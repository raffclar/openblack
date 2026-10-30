/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CameraPathSystem.h"

#include <cctype>

#include <algorithm>
#include <exception>
#include <memory>
#include <string>

#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Camera/CameraPathControl.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/FightCameraModel.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Resources.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

void CameraPathSystem::Start(entt::id_type id)
{
	_path = Locator::resources::value().GetCameraPaths().Handle(id);
	_pathId = id;
	if (!_path)
	{
		return;
	}

	_elapsed = std::chrono::microseconds::zero();
	_state = CameraPathState::PLAYING;
}

void CameraPathSystem::Stop()
{
	_placed.reset();
	_state = CameraPathState::STOPPED;
	_elapsed = std::chrono::microseconds::zero();
	_path = entt::resource<CameraPath>();
}

namespace
{
/// The camera glides onto a placed path over its pause and this long more, then follows it this far behind
constexpr float k_PlacedLagSeconds = 0.3f;
/// The camera moves on by no more than this a frame
constexpr float k_MaxFrameSeconds = 0.1f;
/// The path is let go of as its animation reaches its last of a thousand frames
constexpr float k_AnimationFrames = 1000.0f;

glm::vec3 Place(const glm::mat4& placement, const glm::vec3& point)
{
	return glm::vec3(placement * glm::vec4(point, 1.0f));
}

void SendTo(std::array<camera::Zoomer, 3>& zoomers, const glm::vec3& point, float seconds)
{
	for (int i = 0; i < 3; ++i)
	{
		zoomers.at(static_cast<size_t>(i)).SetDestination(point[i], 0.0f, seconds);
	}
}

glm::vec3 ValueOf(const std::array<camera::Zoomer, 3>& zoomers)
{
	return {zoomers[0].Value(), zoomers[1].Value(), zoomers[2].Value()};
}
} // namespace

void CameraPathSystem::FollowPlaced(std::unique_ptr<CameraPath> path, const glm::mat4& placement, float pauseSeconds,
                                    float speedUp, float animationMs, entt::entity spell)
{
	if (path == nullptr || path->GetPoints().empty() || !Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	// A camera that cannot be left, such as a script's camera, keeps the camera: the path does not start
	if (!camera.GetModel().CanBeLeft())
	{
		return;
	}
	_placed = Placed {.path = std::move(path),
	                  .placement = placement,
	                  .pauseSeconds = pauseSeconds,
	                  .speedUp = speedUp,
	                  .animationMs = animationMs,
	                  .spell = spell,
	                  .seconds = 0.0f,
	                  .origin = {},
	                  .focus = {},
	                  .returnOrigin = camera.GetModel().GetTargetOrigin(),
	                  .returnFocus = camera.GetModel().GetTargetFocus()};
	auto& placed = *_placed;
	const auto origin = camera.GetOrigin();
	const auto focus = camera.GetFocus();
	for (int i = 0; i < 3; ++i)
	{
		placed.origin.at(static_cast<size_t>(i)).SetPosition(origin[i]);
		placed.focus.at(static_cast<size_t>(i)).SetPosition(focus[i]);
	}
	// One glide from where the camera is onto the path's start, arriving still after the pause and a little more
	const auto first = placed.path->SampleAt(std::chrono::milliseconds(0));
	SendTo(placed.focus, Place(placed.placement, first.focus), placed.pauseSeconds + k_PlacedLagSeconds);
	SendTo(placed.origin, Place(placed.placement, first.position), placed.pauseSeconds + k_PlacedLagSeconds);
}

void CameraPathSystem::HandlePlayerControl(const PlayerControl& control)
{
	if (_placed.has_value() &&
	    camera_path::TakesCameraBack(control.movementKey, control.frameMilliseconds, control.grippingLand))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: the player took the camera back from a miracle's path");
		_placed.reset();
	}
}

void CameraPathSystem::UpdatePlaced(float seconds, float gameSeconds)
{
	auto& placed = *_placed;
	// It goes with the miracle that placed it
	if (placed.spell != entt::null &&
	    (!Locator::entitiesRegistry::has_value() || !Locator::entitiesRegistry::value().Valid(placed.spell)))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a miracle's path ended with its miracle");
		EndPlaced();
		return;
	}
	auto& camera = Locator::camera::value();
	// The path goes with the age of the miracle's effect, which moves on by the game's time. The camera glides by its own
	// time, so while the game is paused it comes to where the path stopped and stays there.
	placed.seconds += gameSeconds;
	const float dt = std::min(seconds, k_MaxFrameSeconds);
	if (placed.seconds > placed.pauseSeconds)
	{
		// Playing: every frame the camera is sent afresh to where the path is now, to arrive in a moment, so it follows a
		// little behind
		const float pathMs = (placed.seconds - placed.pauseSeconds) * placed.speedUp * 1000.0f;
		const auto sample = placed.path->SampleAt(std::chrono::milliseconds(static_cast<int64_t>(pathMs)));
		SendTo(placed.focus, Place(placed.placement, sample.focus), k_PlacedLagSeconds);
		SendTo(placed.origin, Place(placed.placement, sample.position), k_PlacedLagSeconds);
		if (pathMs >= placed.animationMs * (k_AnimationFrames - 1.0f) / k_AnimationFrames)
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a miracle's path ended after {:.1f} s", placed.seconds);
			EndPlaced();
			return;
		}
	}
	for (int i = 0; i < 3; ++i)
	{
		placed.origin.at(static_cast<size_t>(i)).Update(dt);
		placed.focus.at(static_cast<size_t>(i)).Update(dt);
	}
	camera.SetOrigin(ValueOf(placed.origin));
	camera.SetFocus(ValueOf(placed.focus));
}

void CameraPathSystem::EndPlaced()
{
	const auto placed = std::exchange(_placed, std::nullopt);
	if (!placed.has_value() || !Locator::camera::has_value())
	{
		return;
	}
	// The player's camera starts again from where the camera now is. The fight view flies back to where it last was;
	// the world camera flies back to where it was going.
	auto& camera = Locator::camera::value();
	const auto origin = camera.GetOrigin();
	const auto focus = camera.GetFocus();
	if (auto* world = dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel()); world != nullptr)
	{
		world->Restart(origin, focus, placed->returnOrigin, placed->returnFocus);
	}
	else if (auto* fight = dynamic_cast<FightCameraModel*>(&camera.GetModel()); fight != nullptr)
	{
		fight->Return({.origin = origin, .focus = focus},
		              {.origin = camera.GetOriginVelocity(), .focus = camera.GetFocusVelocity()});
	}
}

void CameraPathSystem::Update(const std::chrono::microseconds& dt, std::chrono::milliseconds frameGameTime)
{
	if (_placed.has_value() && Locator::camera::has_value())
	{
		UpdatePlaced(std::chrono::duration<float>(dt).count(), std::chrono::duration<float>(frameGameTime).count());
	}
	if (_state == CameraPathState::STOPPED || !_path)
	{
		return;
	}
	if (_state != CameraPathState::PLAYING)
	{
		return;
	}

	// The path spreads its points evenly over its duration, and the camera follows it as the temple's does
	_elapsed += dt;
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(_elapsed);
	const auto sample = _path->SampleAt(elapsed);
	auto& camera = Locator::camera::value();
	camera.SetOrigin(sample.position);
	camera.SetFocus(sample.focus);
	if (elapsed >= _path->GetDuration())
	{
		Stop();
	}
}

void CameraPathSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _pathId);
	reflection::Save(out, static_cast<bool>(_path));
	reflection::Save(out, _elapsed);
	reflection::Save(out, _state);
	reflection::Save(out, _placed.has_value());
	if (!_placed.has_value())
	{
		return;
	}
	// A placed path goes by the name of its file, which the load reads again
	reflection::Save(out, _placed->path->GetDebugName());
	reflection::Save(out, _placed->placement);
	reflection::Save(out, _placed->pauseSeconds);
	reflection::Save(out, _placed->speedUp);
	reflection::Save(out, _placed->animationMs);
	reflection::Save(out, _placed->spell);
	reflection::Save(out, _placed->seconds);
	reflection::Save(out, _placed->origin);
	reflection::Save(out, _placed->focus);
	reflection::Save(out, _placed->returnOrigin);
	reflection::Save(out, _placed->returnFocus);
}

bool CameraPathSystem::LoadState(reflection::BinaryReader& in)
{
	bool hasPath = false;
	bool placed = false;
	reflection::Load(in, _pathId);
	reflection::Load(in, hasPath);
	reflection::Load(in, _elapsed);
	reflection::Load(in, _state);
	reflection::Load(in, placed);
	_path = hasPath && Locator::resources::has_value() ? Locator::resources::value().GetCameraPaths().Handle(_pathId)
	                                                   : entt::resource<CameraPath>();
	_placed.reset();
	if (!placed)
	{
		return !in.Failed();
	}
	std::string file;
	Placed loaded {.path = nullptr, .placement = glm::mat4(1.0f), .pauseSeconds = 0.0f, .speedUp = 1.0f, .animationMs = 0.0f};
	reflection::Load(in, file);
	reflection::Load(in, loaded.placement);
	reflection::Load(in, loaded.pauseSeconds);
	reflection::Load(in, loaded.speedUp);
	reflection::Load(in, loaded.animationMs);
	reflection::Load(in, loaded.spell);
	reflection::Load(in, loaded.seconds);
	reflection::Load(in, loaded.origin);
	reflection::Load(in, loaded.focus);
	reflection::Load(in, loaded.returnOrigin);
	reflection::Load(in, loaded.returnFocus);
	if (in.Failed())
	{
		return false;
	}
	// The file name is as the effect file gives it, under the data folder of the game
	if (Locator::filesystem::has_value())
	{
		std::string name = file;
		std::ranges::replace(name, '\\', '/');
		if (name.starts_with("./"))
		{
			name.erase(0, 2);
		}
		if (name.size() >= 5 && std::equal(name.begin(), name.begin() + 5, "data/",
		                                   [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; }))
		{
			name.erase(0, 5);
		}
		auto& fileSystem = Locator::filesystem::value();
		auto path = std::make_unique<CameraPath>(file);
		try
		{
			if (path->LoadFromFile(fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / name)))
			{
				loaded.path = std::move(path);
				_placed = std::move(loaded);
			}
		}
		catch (const std::exception& error)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The saved camera path {} does not load: {}", file, error.what());
		}
	}
	return true;
}
