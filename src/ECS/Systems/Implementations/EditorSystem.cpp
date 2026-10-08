/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "EditorSystem.h"

#include <cmath>

#include <algorithm>

#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/EditorCameraModel.h"
#include "Camera/ScriptCamera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Editor/EditorClock.h"
#include "Editor/EditorEntities.h"
#include "Input/HandDemo.h"
#include "Locator.h"

namespace openblack::ecs::systems
{

using components::Transform;

namespace
{
bool InTemple()
{
	return Locator::temple::has_value() && Locator::temple::value().Active();
}

/// A script's camera or a hand demo is moving the view, so the editor's cameras keep off it
bool ScriptDrivesCamera()
{
	return (Locator::scriptState::has_value() && script_camera::Drives()) ||
	       (Locator::inputState::has_value() && hand_demo::IsPlaying());
}
} // namespace

EditorSystem::EditorSystem() = default;
EditorSystem::~EditorSystem() = default;

void EditorSystem::SetOpen(bool open)
{
	if (open == _open)
	{
		return;
	}
	_open = open;
	if (!open)
	{
		SetCameraMode(CameraMode::Free);
		_tool = Tool::Select;
	}
}

bool EditorSystem::OwnsCamera() const
{
	return _cameraModel != nullptr && Locator::camera::has_value() && &Locator::camera::value().GetModel() == _cameraModel;
}

void EditorSystem::ReleaseCamera()
{
	if (OwnsCamera() && _playerCameraModel != nullptr)
	{
		Locator::camera::value().SetModel(std::move(_playerCameraModel));
		_cameraModel = nullptr;
	}
}

void EditorSystem::SetCameraMode(CameraMode mode)
{
	const auto selected = _selection.Get();
	if (mode != CameraMode::Free && (!selected.has_value() || InTemple() || ScriptDrivesCamera() ||
	                                 !Locator::camera::has_value() || !Locator::entitiesRegistry::has_value()))
	{
		mode = CameraMode::Free;
	}
	_cameraMode = mode;
	if (mode == CameraMode::Free)
	{
		ReleaseCamera();
		return;
	}
	const auto modelMode = mode == CameraMode::Orbit ? EditorCameraModel::Mode::Orbit : EditorCameraModel::Mode::Follow;
	if (OwnsCamera())
	{
		_cameraModel->SetMode(modelMode);
		return;
	}
	// The camera may still be with another model that took it from the editor's, as the temple's does; wait for it back
	if (_cameraModel != nullptr)
	{
		return;
	}
	auto& camera = Locator::camera::value();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<Transform>(*selected);
	const auto target = transform != nullptr ? transform->position : camera.GetFocus();
	auto model = std::make_unique<EditorCameraModel>(modelMode, camera.GetOrigin(), target);
	_cameraModel = model.get();
	_playerCameraModel = camera.SetModel(std::move(model));
	AimCamera();
}

void EditorSystem::AimCamera()
{
	if (!OwnsCamera() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto selected = _selection.Get();
	const auto& registry = Locator::entitiesRegistry::value();
	if (!selected.has_value())
	{
		return;
	}
	if (const auto* transform = registry.TryGet<Transform>(*selected))
	{
		_cameraModel->SetTarget(transform->position, editor::FacingOf(transform->rotation),
		                        editor::HeightOf(registry, *selected));
	}
}

void EditorSystem::TurnCamera(glm::vec2 radians)
{
	if (OwnsCamera())
	{
		_cameraModel->Turn(radians);
	}
}

void EditorSystem::ZoomCamera(float steps)
{
	if (OwnsCamera())
	{
		_cameraModel->Zoom(steps);
	}
}

void EditorSystem::FrameSelection()
{
	const auto selected = _selection.Get();
	if (!selected.has_value() || !Locator::camera::has_value() || !Locator::entitiesRegistry::has_value() || InTemple() ||
	    ScriptDrivesCamera())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<Transform>(*selected);
	if (transform == nullptr)
	{
		return;
	}
	const auto height = editor::HeightOf(registry, *selected);
	const auto distance = std::clamp(height * 3.0f, 15.0f, 400.0f);
	if (OwnsCamera())
	{
		// Each step of zoom is a tenth closer
		_cameraModel->Zoom(std::log(distance / _cameraModel->GetOrbit().distance) / std::log(0.9f));
		return;
	}
	auto& camera = Locator::camera::value();
	const auto focus = transform->position + glm::vec3(0.0f, height * 0.5f, 0.0f);
	auto orbit = editor::OrbitFrom(camera.GetOrigin(), focus);
	orbit.pitch = 0.45f;
	orbit.distance = distance;
	camera.GetModel().SetFlight(editor::OrbitOrigin(focus, orbit), focus);
}

void EditorSystem::StepTurn()
{
	if (!editor::clock::Available() || _stepFrom.has_value())
	{
		return;
	}
	_stepFrom = editor::clock::Turn();
	editor::clock::SetPaused(false);
}

void EditorSystem::Update([[maybe_unused]] std::chrono::microseconds dt)
{
	if (_stepFrom.has_value() && editor::clock::Available())
	{
		if (editor::clock::Turn() != *_stepFrom)
		{
			editor::clock::SetPaused(true);
			_stepFrom.reset();
		}
	}
	if (!_open)
	{
		return;
	}
	if (Locator::entitiesRegistry::has_value())
	{
		const auto& registry = Locator::entitiesRegistry::value();
		_selection.Validate([&registry](entt::entity entity) { return registry.Valid(entity); });
	}
	if (_cameraMode != CameraMode::Free)
	{
		// The picked thing went, the temple took the camera, or a script's camera or a hand demo moves the view
		if (_selection.Empty() || InTemple() || ScriptDrivesCamera())
		{
			SetCameraMode(CameraMode::Free);
		}
		else if (!OwnsCamera() && _cameraModel != nullptr)
		{
			// Still away with another model
		}
		else
		{
			AimCamera();
		}
	}
	else if (OwnsCamera())
	{
		// Given back to the editor's model after the editor had let go, as on leaving the temple
		ReleaseCamera();
	}
}

} // namespace openblack::ecs::systems
