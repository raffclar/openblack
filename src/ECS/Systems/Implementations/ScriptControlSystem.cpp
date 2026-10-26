/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ScriptControlSystem.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraTrack.h"
#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraLimits.h"
#include "Camera/DanceCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCameraModel.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "EngineConfig.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptCameraLookups.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// Given back, the camera's lens returns to normal over this long
constexpr float k_LensBackSeconds = 0.5f;

void ScriptMessage(std::string_view message)
{
	if (const auto logger = spdlog::get("scripting"))
	{
		SPDLOG_LOGGER_WARN(logger, "{}", message);
	}
}
} // namespace

bool ScriptControlSystem::StartCameraControl(Camera& camera, const CameraRequest& request, GroundHeight groundAt)
{
	if (request.insideTemple)
	{
		// The temple's camera stays as it is, and only its own scripts take control of it
		if (!request.templeScript)
		{
			return false;
		}
		_cameraOwner = request.task;
		return true;
	}
	// Another script's camera can't be left until that script gives it back
	if (GetScriptCamera(camera) != nullptr)
	{
		ScriptMessage("Note-Camera control Failed");
		return false;
	}
	auto previous =
	    camera.SetModel(std::make_unique<ScriptCameraModel>(camera.GetOrigin(), camera.GetFocus(), std::move(groundAt)));
	// The camera of a dance stays under the script's camera, and comes back when the script ends
	if (dynamic_cast<DanceCameraModel*>(previous.get()) != nullptr)
	{
		_danceCamera = std::move(previous);
	}
	_cameraOwner = request.task;
	return true;
}

bool ScriptControlSystem::EndCameraControl(Camera& camera, uint32_t task)
{
	if (_cameraOwner != task)
	{
		return false;
	}
	ReleaseCamera(camera);
	return true;
}

ScriptCameraModel* ScriptControlSystem::GetScriptCamera(Camera& camera) const
{
	return dynamic_cast<ScriptCameraModel*>(&camera.GetModel());
}

void ScriptControlSystem::ReleaseCamera(Camera& camera)
{
	if (GetScriptCamera(camera) != nullptr && _danceCamera != nullptr)
	{
		// The camera of a dance comes back and holds the view where the script left it
		camera.SetModel(std::move(_danceCamera));
	}
	else if (const auto* script = GetScriptCamera(camera); script != nullptr)
	{
		// The player gets a camera of their own. The camera does not stop where the script left it: it glides on, from
		// the speed that it has, to where the script sent it. The player's camera starts with the time of its first
		// glide.
		const auto origin = script->GetTargetOrigin();
		const auto focus = script->GetTargetFocus();
		camera.GlideTo(origin, focus, script->GetOriginSpeed(), script->GetFocusSpeed(), camera_limits::k_FlightEaseSeconds);
		camera.SetModel(std::make_unique<DefaultWorldCameraModel>(origin, focus));
	}
	// Inside the temple, the script's camera the player came in from gives way all the same
	else if (!Locator::temple::has_value() || !Locator::temple::value().Active() ||
	         !Locator::temple::value().ReleaseOutsideScriptCamera())
	{
		ScriptMessage("We are in the wrong camera mode! - exception happened?");
	}
	_danceCamera.reset();
	// The lens glides back to the normal one over half a second
	if (Locator::config::has_value())
	{
		camera.SetLens(glm::radians(Locator::config::value().cameraXFov), k_LensBackSeconds);
	}
	_cameraOwner = 0;
	// The script's cinema is over: all the villagers that it drew in high detail are drawn as usual again
	if (Locator::highDetailSystem::has_value())
	{
		Locator::highDetailSystem::value().ReleaseAll();
	}
	// Make the land's draw list again on the next frame
	if (Locator::objectDrawListSystem::has_value())
	{
		Locator::objectDrawListSystem::value().RequestRebuild(1);
	}
}

void ScriptControlSystem::StartGameSpeed(uint32_t task)
{
	if (_gameSpeedOwner == 0 || _gameSpeedOwner == task)
	{
		_gameSpeedOwner = task;
	}
}

bool ScriptControlSystem::EndGameSpeed(uint32_t task)
{
	if (_gameSpeedOwner != 0 && _gameSpeedOwner != task)
	{
		return false;
	}
	_gameSpeedOwner = 0;
	return true;
}

ScriptControlSystemInterface::Released ScriptControlSystem::TaskStopped(Camera& camera, uint32_t task)
{
	Released released;
	if (_cameraOwner == task)
	{
		ReleaseCamera(camera);
		released.camera = true;
	}
	if (_gameSpeedOwner == task)
	{
		_gameSpeedOwner = 0;
		released.gameSpeed = true;
	}
	return released;
}

void ScriptControlSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _cameraOwner);
	reflection::Save(out, _gameSpeedOwner);
	reflection::Save(out, _cameraDetails);
	// The script's camera, with what it follows, looks at and runs
	const auto* model =
	    Locator::camera::has_value() ? dynamic_cast<const ScriptCameraModel*>(&Locator::camera::value().GetModel()) : nullptr;
	reflection::Save(out, model != nullptr ? std::optional(model->GetSaved()) : std::nullopt);
}

bool ScriptControlSystem::LoadState(reflection::BinaryReader& in)
{
	reflection::Load(in, _cameraOwner);
	reflection::Load(in, _gameSpeedOwner);
	reflection::Load(in, _cameraDetails);
	std::optional<ScriptCameraModel::Saved> saved;
	reflection::Load(in, saved);
	if (in.Failed())
	{
		return false;
	}
	// A script that held the camera holds it again, from where the camera is, as its camera was
	if (saved.has_value() && Locator::camera::has_value() && Locator::terrainSystem::has_value())
	{
		auto& camera = Locator::camera::value();
		auto model = std::make_unique<ScriptCameraModel>(camera.GetOrigin(), camera.GetFocus(), [](float x, float z) {
			return Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
		});
		model->Restore(*saved, chlapi::ScriptCameraLookups());
		camera.SetModel(std::move(model));
	}
	return true;
}

void ScriptControlSystem::Reset()
{
	_cameraOwner = 0;
	_danceCamera.reset();
	_gameSpeedOwner = 0;
}
