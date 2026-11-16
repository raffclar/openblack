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
#include <utility>

#include <LHVM.h>

#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/DoubleClickFlight.h"
#include "Camera/EditorCameraModel.h"
#include "Common/PortableMath.h"
#include "ECS/Components/Transform.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Editor/EditorEntities.h"
#include "Editor/EditorNetworkEdits.h"
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
	ApplyCameraMoveSpeed();
}

void EditorSystem::SetCameraMoveSpeed(float speed)
{
	_cameraMoveSpeed = ClampKeyboardMoveSpeed(speed);
	ApplyCameraMoveSpeed();
}

void EditorSystem::ApplyCameraMoveSpeed() const
{
	if (Locator::camera::has_value())
	{
		Locator::camera::value().SetKeyboardMoveSpeed(_open ? _cameraMoveSpeed : k_KeyboardMoveSpeedDefault);
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
	if (mode != CameraMode::Free &&
	    (!selected.has_value() || InTemple() || !Locator::camera::has_value() || !Locator::entitiesRegistry::has_value()))
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
	if (!selected.has_value() || !Locator::camera::has_value() || !Locator::entitiesRegistry::has_value() || InTemple())
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
		_cameraModel->Zoom(portable_math::Log(distance / _cameraModel->GetOrbit().distance) / portable_math::Log(0.9f));
		return;
	}
	auto& camera = Locator::camera::value();
	const auto focus = transform->position + glm::vec3(0.0f, height * 0.5f, 0.0f);
	auto orbit = editor::OrbitFrom(camera.GetOrigin(), focus);
	orbit.pitch = 0.45f;
	orbit.distance = distance;
	camera.GetModel().SetFlight(editor::OrbitOrigin(focus, orbit), focus, camera_fly::k_HalfWayRise);
}

void EditorSystem::StepTurn()
{
	if (!Locator::time::has_value() || _stepFrom.has_value())
	{
		return;
	}
	auto& time = Locator::time::value();
	_stepFrom = time.GetTurn();
	time.SetPaused(false);
}

void EditorSystem::AnswerRequest(editor::Request request, entt::entity entity)
{
	const auto awaited = _history.AwaitedFrom(request);
	if (!awaited.has_value() || !Locator::entitiesRegistry::has_value())
	{
		// No change waits for the answer, so the request only closes
		_history.Answer(request, {.entity = entity});
		return;
	}
	_history.Answer(request, editor::AnswerOf(Locator::entitiesRegistry::value(), entity, *awaited));
}

void EditorSystem::SaveState(reflection::BinaryWriter& out) const
{
	// The debugger of the script machine. A load of the scripts clears it, so this section puts it back after them.
	const auto debugger = Locator::vm::has_value() ? Locator::vm::value().GetDebuggerState() : lhvm::LHVM::DebuggerState {};
	reflection::Save(out, debugger.breakpoints);
	reflection::Save(out, debugger.heldTasks);
	reflection::Save(out, debugger.resumeFrom);
	// The run of the land script stepper. Only the panels of this computer use the message and the count of the changes.
	const auto run = editor::script_acts::KeepLandScriptRun(_landScript);
	reflection::Save(out, run.name);
	reflection::Save(out, run.text);
	reflection::Save(out, run.stepper.states);
	reflection::Save(out, run.stepper.breakpoints);
	reflection::Save(out, run.stepper.next);
	reflection::SavePresentation(out, run.message);
	reflection::SavePresentation(out, run.changes);
}

bool EditorSystem::LoadState(reflection::BinaryReader& in)
{
	lhvm::LHVM::DebuggerState debugger;
	reflection::Load(in, debugger.breakpoints);
	reflection::Load(in, debugger.heldTasks);
	reflection::Load(in, debugger.resumeFrom);
	editor::script_acts::KeptLandScriptRun run;
	reflection::Load(in, run.name);
	reflection::Load(in, run.text);
	reflection::Load(in, run.stepper.states);
	reflection::Load(in, run.stepper.breakpoints);
	reflection::Load(in, run.stepper.next);
	reflection::Load(in, run.message);
	reflection::Load(in, run.changes);
	if (in.Failed())
	{
		return false;
	}
	if (Locator::vm::has_value())
	{
		Locator::vm::value().SetDebuggerState(std::move(debugger));
	}
	// The load reset the registry before this section, so the run starts at the count of the world that it now holds
	const auto resets = Locator::entitiesRegistry::has_value()
	                        ? std::optional<uint32_t> {Locator::entitiesRegistry::value().GetResets()}
	                        : std::nullopt;
	return editor::script_acts::RestoreLandScriptRun(_landScript, run, resets);
}

void EditorSystem::Update([[maybe_unused]] std::chrono::microseconds dt)
{
	// A land or a saved world that loads can give a new thing the entity of an old one, so the picks and the history
	// start again
	if (Locator::entitiesRegistry::has_value())
	{
		const auto resets = Locator::entitiesRegistry::value().GetResets();
		if (_registryResets.has_value() && *_registryResets != resets)
		{
			_selection.Clear();
			_history.Clear();
			// A world that the editor loaded keeps the paths that the editor found in it again
			if (_drawnPaths.keepAtNextReset)
			{
				_drawnPaths.keepAtNextReset = false;
			}
			else
			{
				_drawnPaths = {};
			}
		}
		_registryResets = resets;
	}
	// The land script stepper starts again with the script of the new land. A turn does the same check before its act.
	editor::script_acts::RestartLandScriptAtNewWorld();
	// A request that gets no answer, such as a command that the game server refused, does not hold undo and redo
	if (_history.Waiting() && Locator::time::has_value())
	{
		_history.ForgetOldRequests(Locator::time::value().GetTurn());
	}
	if (_stepFrom.has_value() && Locator::time::has_value())
	{
		auto& time = Locator::time::value();
		if (time.GetTurn() != *_stepFrom)
		{
			time.SetPaused(true);
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
		// The picked thing went, or the temple took the camera
		if (_selection.Empty() || InTemple())
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
