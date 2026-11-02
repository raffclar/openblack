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

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Camera/CameraPathControl.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/FightCameraModel.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "Locator.h"
#include "Resources/Resources.h"

using namespace openblack;
using namespace openblack::ecs::systems;

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
                                    const std::shared_ptr<particles::CameraPathLink>& link)
{
	if (path == nullptr || path->GetPoints().empty() || link == nullptr || !Locator::camera::has_value())
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
	                  .link = link,
	                  .glided = false,
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
}

void CameraPathSystem::ShowPlaced(float turnFraction)
{
	if (!_placed.has_value())
	{
		return;
	}
	const auto link = _placed->link.lock();
	// The atom is drawn from its second step on
	if (link == nullptr || !link->shown)
	{
		return;
	}
	const auto frame = camera_path::DrawnFrame(link->previousFrame, link->currentFrame, turnFraction, link->frames, link->loop);
	link->drawnMilliseconds = camera_path::PathMilliseconds(link->cycleMilliseconds, frame);
	if (!link->ended && camera_path::EndsPath(frame, link->frames))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a miracle's path came to the last frame of its animation");
		link->ended = true;
	}
}

void CameraPathSystem::CheckPlaced()
{
	if (!_placed.has_value())
	{
		return;
	}
	const auto link = _placed->link.lock();
	if (link == nullptr || link->ended)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Camera: a miracle's path let go of the camera");
		EndPlaced();
	}
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

void CameraPathSystem::UpdatePlaced(float seconds)
{
	auto& placed = *_placed;
	auto& camera = Locator::camera::value();
	const float dt = std::min(seconds, k_MaxFrameSeconds);
	const auto link = placed.link.lock();
	// The time on the path is that of the frame that the last draw showed
	const auto pathTime = std::chrono::milliseconds(link != nullptr ? link->drawnMilliseconds : 0);
	if (link != nullptr && link->playing)
	{
		// Playing: every frame the camera is sent afresh to where the path is now, to arrive in a moment, so it follows a
		// little behind
		const auto sample = placed.path->SampleAt(pathTime);
		SendTo(placed.focus, Place(placed.placement, sample.focus), k_PlacedLagSeconds);
		SendTo(placed.origin, Place(placed.placement, sample.position), k_PlacedLagSeconds);
	}
	else if (!placed.glided)
	{
		// One glide from where the camera is onto the path's start, arriving still after the pause and a little more
		const auto sample = placed.path->SampleAt(pathTime);
		const float glideSeconds = placed.pauseSeconds + k_PlacedLagSeconds;
		SendTo(placed.focus, Place(placed.placement, sample.focus), glideSeconds);
		SendTo(placed.origin, Place(placed.placement, sample.position), glideSeconds);
		placed.glided = true;
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

void CameraPathSystem::Update(const std::chrono::microseconds& dt, std::chrono::milliseconds /*frameGameTime*/)
{
	if (_placed.has_value() && Locator::camera::has_value())
	{
		UpdatePlaced(std::chrono::duration<float>(dt).count());
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
	// The path of a miracle holds the camera of this computer and goes with an atom that the save does not link to it. A
	// load gives the camera back to the player, so the save keeps only that there is no such path.
	out.BeginPresentation();
	reflection::Save(out, false);
	out.EndPresentation();
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
	// An older save kept the placed path: read past it. The camera goes back to the player.
	std::string file;
	glm::mat4 placement(1.0f);
	float pauseSeconds = 0.0f;
	float speedUp = 1.0f;
	float animationMs = 0.0f;
	entt::entity spell {entt::null};
	float seconds = 0.0f;
	std::array<camera::Zoomer, 3> origin;
	std::array<camera::Zoomer, 3> focus;
	glm::vec3 returnOrigin(0.0f);
	glm::vec3 returnFocus(0.0f);
	reflection::Load(in, file);
	reflection::Load(in, placement);
	reflection::Load(in, pauseSeconds);
	reflection::Load(in, speedUp);
	reflection::Load(in, animationMs);
	reflection::Load(in, spell);
	reflection::Load(in, seconds);
	reflection::Load(in, origin);
	reflection::Load(in, focus);
	reflection::Load(in, returnOrigin);
	reflection::Load(in, returnFocus);
	return !in.Failed();
}
