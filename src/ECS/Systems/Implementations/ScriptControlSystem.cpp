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
#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>

#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCameraModel.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
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
	camera.SetModel(std::make_unique<ScriptCameraModel>(camera.GetOrigin(), camera.GetFocus(), std::move(groundAt)));
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
	if (GetScriptCamera(camera) != nullptr)
	{
		// The player gets a camera of their own, starting from where the script left it and looking the same way
		camera.SetModel(std::make_unique<DefaultWorldCameraModel>(camera.GetOrigin(), camera.GetFocus()));
	}
	// Inside the temple, the script's camera the player came in from gives way all the same
	else if (!Locator::temple::has_value() || !Locator::temple::value().Active() ||
	         !Locator::temple::value().ReleaseOutsideScriptCamera())
	{
		ScriptMessage("We are in the wrong camera mode! - exception happened?");
	}
	_cameraOwner = 0;
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

void ScriptControlSystem::Reset()
{
	_cameraOwner = 0;
	_gameSpeedOwner = 0;
}
