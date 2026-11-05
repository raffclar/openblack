/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "OwnReportSystem.h"

#include <cstdint>

#include <algorithm>
#include <limits>
#include <optional>

#include "Camera/Camera.h"
#include "Camera/ScriptCameraModel.h"
#include "Commands/PlayerCommands.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Gui/GameInterface.h"
#include "Hand/OwnHandReport.h"
#include "Hand/ViewReportRules.h"
#include "Help/HelpProfile.h"
#include "Input/BindableActions.h"
#include "Input/GameActionMapInterface.h"
#include "Input/KeyRebinding.h"
#include "Input/LandGrip.h"
#include "Input/MouseWheel.h"
#include "Locator.h"
#include "Online/NetworkSession.h"
#include "ScriptHeaders/ScriptLessonNatives.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

bool OwnReportSystem::StepLandGrip(bool dragging, bool moveHeld)
{
	_state.landGrip = input::land_grip::GripsLand(dragging, _state.landGrip, moveHeld);
	return _state.landGrip;
}

bool OwnReportSystem::StepCameraTurned(uint32_t turn, bool controlsTurned)
{
	_state.cameraTurned = view_report::CameraTurnedInTurn(_state.cameraTurned, _state.cameraTurnTurn == turn, controlsTurned);
	_state.cameraTurnTurn = turn;
	return _state.cameraTurned;
}

void OwnReportSystem::ForgetViewReport()
{
	_state.lastViewReport.reset();
}

bool OwnReportSystem::SendViewReport(const player_commands::ViewReport& report, PlayerCommandSystemInterface& commands)
{
	if (_state.lastViewReport.has_value() && !view_report::ChangedEnough(*_state.lastViewReport, report))
	{
		return false;
	}
	_state.lastViewReport = report;
	commands.Submit(report);
	return true;
}

bool OwnReportSystem::TakeHelpTurn(uint32_t turn, bool helpThere)
{
	if (_state.helpReportTurn == turn || !helpThere)
	{
		return false;
	}
	_state.helpReportTurn = turn;
	return true;
}

void OwnReportSystem::ReportHand(uint32_t worldCameraIcons)
{
	// The hand system keeps what this computer last sent of its hand, and decides what goes in this turn
	auto* reporter = Locator::handSystem::value().GetOwnReporter();
	if (reporter == nullptr)
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	const auto* transform = registry.Valid(hand) ? registry.TryGet<const ecs::components::Transform>(hand) : nullptr;
	const auto start = reporter->Start(Locator::time::value().GetTurn(), hand,
	                                   transform != nullptr ? std::optional(transform->position) : std::nullopt);
	if (!start.has_value())
	{
		return;
	}
	if (start->newHand)
	{
		ForgetViewReport();
	}
	ReportView(worldCameraIcons);
	hand_report::OwnInputs inputs {
	    .handPoint = start->handPoint,
	    .spellNeedsReport =
	        [] { return Locator::magicSystem::has_value() && Locator::magicSystem::value().SpellNeedsHandReportEachTurn(); },
	};
	if (Locator::pickingSystem::has_value())
	{
		const auto& pick = Locator::pickingSystem::value().GetPick();
		inputs.picked = pick.object.has_value() && registry.Valid(*pick.object) ? *pick.object : entt::entity {entt::null};
		inputs.pickPoint = pick.point;
	}
	const auto* session = Locator::networkGame::has_value() ? Locator::networkGame::value().Session() : nullptr;
	inputs.origin = session != nullptr ? std::optional(session->Origin()) : std::nullopt;
	// The camera is the view that the last frame drew
	const auto& camera = Locator::camera::value();
	inputs.camera = camera.GetOrigin(Camera::Interpolation::Drawn);
	inputs.focus = camera.GetFocus(Camera::Interpolation::Drawn);
	const auto sends = reporter->Finish(inputs);
	if (sends.pick.has_value())
	{
		Locator::playerCommandSystem::value().Submit(*sends.pick);
	}
	if (sends.report.has_value())
	{
		Locator::playerCommandSystem::value().Submit(*sends.report);
		// The send time gives the latency of the report at the turn that does it
		Locator::handSystem::value().ReportSent(*sends.report);
	}
}

void OwnReportSystem::ReportView(uint32_t worldCameraIcons)
{
	// What the scripts and the advisors' remarks read of the view: the camera as the frame draws it, the window it draws
	// into, its arrival, the distance from the fence of the camera zones, and the grip of the hand on the land
	auto& camera = Locator::camera::value();
	const auto pose = camera.DrawnPose(Camera::Interpolation::Current);
	const auto window = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(0);
	const auto pixels = [](int size) {
		return static_cast<uint16_t>(std::clamp(size, 0, static_cast<int>(std::numeric_limits<uint16_t>::max())));
	};
	// The grip of the land starts with a drag of the land and continues until the Move button is let go. Both buttons
	// together stop the drag but not the grip, so the scripts see the zoom with the land gripped.
	const bool bothButtons = Locator::gameActionSystem::has_value() &&
	                         Locator::gameActionSystem::value().Get(input::UnbindableActionMap::TWO_BUTTON_CLICK);
	const bool moveHeld = bothButtons || (Locator::gameActionSystem::has_value() &&
	                                      Locator::gameActionSystem::value().Get(input::BindableActionMap::MOVE));
	const auto cues = camera.GetModel().GetHandCues();
	const bool gripsLand = StepLandGrip(cues.dragging, moveHeld);
	// The turn of the camera counts for the whole game turn, so that the scripts see it at all frame rates
	const bool cameraTurned = StepCameraTurned(Locator::time::value().GetTurn(), cues.controlsTurned);
	player_commands::ViewReport report {
	    .eye = pose.eye,
	    .focus = pose.focus,
	    .lens = camera.GetLens(),
	    .nearClip = camera.GetNearClip(),
	    .screenWidth = pixels(window.x),
	    .screenHeight = pixels(window.y),
	    .aspect = camera.GetAspect(),
	    .inclusionDistance =
	        Locator::cameraZoneSystem::has_value() ? Locator::cameraZoneSystem::value().GetInclusionDistance() : 0.0f,
	    .cameraOrders = Locator::entitiesRegistry::value().Context().scriptCameraOrders,
	    .cameraArrived = false,
	    .gripsLand = gripsLand,
	    .otherButton = bothButtons,
	    .mouseWheel = script::lesson_natives::MouseHasWheel(platform_mouse::SystemSaysWheel(),
	                                                        Locator::gameActionSystem::has_value() &&
	                                                            Locator::gameActionSystem::value().WheelTurned()),
	    .cameraTurned = cameraTurned,
	    // The icons that the last draw kept, or the world camera's own when the draw did not run
	    .cameraIcons = static_cast<uint16_t>(
	        (Locator::gameInterface::has_value() ? Locator::gameInterface::value().GetDrawnCameraIcons() : std::nullopt)
	            .value_or(worldCameraIcons)),
	    .mouseButtons = platform_mouse::SystemMouseButtons(),
	};
	// The keys held, by the game's number of each key
	if (Locator::gameActionSystem::has_value())
	{
		const auto& actions = Locator::gameActionSystem::value();
		for (uint32_t code = 0; code < report.keysHeld.size(); ++code)
		{
			const auto key = input::rebinding::KeyOfGameCode(code);
			report.keysHeld.set(code, key.has_value() && actions.IsKeyHeld(static_cast<int>(*key)));
		}
	}
	if (const auto* script = Locator::scriptControlSystem::value().GetScriptCamera(camera); script != nullptr)
	{
		report.cameraArrived = script->Arrived();
	}
	else
	{
		report.cameraArrived =
		    script_camera::PlayerCameraArrived(camera.GetOrigin(), camera.GetOrigin(Camera::Interpolation::Target),
		                                       camera.GetFocus(), camera.GetFocus(Camera::Interpolation::Target));
	}
	// The report goes only when the view changed enough since the last report that went
	SendViewReport(report, Locator::playerCommandSystem::value());
}

void OwnReportSystem::ReportHelp(const HelpInputs& inputs)
{
	// The report goes once a turn, after the turns of a frame that played one, and only when something in it changed
	if (!TakeHelpTurn(Locator::time::value().GetTurn(), Locator::helpProfileSystem::has_value()))
	{
		return;
	}
	const bool cursorOnPoint =
	    Locator::pickingSystem::has_value() && Locator::pickingSystem::value().GetPick().point.has_value();
	if (const auto report =
	        Locator::helpProfileSystem::value().MakeReport(inputs.frontEndBoxShows(), inputs.worldCameraView(), cursorOnPoint);
	    report.has_value())
	{
		Locator::playerCommandSystem::value().Submit(*report);
	}
}
