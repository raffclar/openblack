/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cmath>
#include <cstdint>
#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMTypes.h>
#include <SaveSlots.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraEdits.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ScreenPick.h"
#include "Blast/CameraShake.h"
#include "CHLApi.h"
#include "CHLApi/NativeHelpers.h"
#include "CHLApi/Natives.h"
#include "Camera/Camera.h"
#include "Camera/ForcedTurnPoint.h"
#include "Camera/ScriptCameraModel.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureHandGlow.h"
#include "Creature/CreatureScriptAgendas.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Dances.h"
#include "ECS/FollowHeadingThing.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ReportedCamera.h"
#include "ECS/ScriptDestination.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FootballSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MistSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlaygroundTripSystemInterface.h"
#include "ECS/Systems/PuzzleTotemSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/ThingOnScreen.h"
#include "ECS/WalkerPlacement.h"
#include "ECS/WorldObjects.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gui/ScriptTexts.h"
#include "Help/Spirits.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Physics/Body.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptCameraLookups.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptLessonNatives.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "ScriptHeaders/ScriptSwitchChanges.h"
#include "Story/PlaygroundTrip.h"

namespace openblack::chlapi
{

using openblack::Locator;
using openblack::ecs::components::Transform;

/// A script gives the camera an order: the turns count it, so that a view report from before the order does not tell
/// that the camera has arrived
void CountCameraOrder()
{
	++Locator::entitiesRegistry::value().Context().scriptCameraOrders;
}

/// The script's camera, for a command that gives it an order
ScriptCameraModel* OrderedScriptCamera()
{
	CountCameraOrder();
	return Locator::scriptControlSystem::value().GetScriptCamera(Locator::camera::value());
}

/// The script's camera the camera commands move: with a warning when the script has none
ScriptCameraModel* ScriptCamera()
{
	auto* camera = OrderedScriptCamera();
	if (camera == nullptr)
	{
		ScriptMessage("We are in the wrong camera mode! - exception happened?");
		return nullptr;
	}
	// While a two-thing camera has taken over, the script's own commands don't reach the camera
	if (camera->InDualView())
	{
		ScriptMessage("We are in the wrong camera mode!");
		return nullptr;
	}
	return camera;
}

/// A thing a script's camera follows or looks at, as the camera sees it while the thing is there: where it is drawn,
/// raised by half its height, and the way it faces when it walks. A flock is at the place of its leader, as a follow
/// camera finds it.
ScriptCameraModel::ThingLookup FollowedThing(entt::entity thing)
{
	return [thing]() -> std::optional<script_camera::FollowedThing> {
		const auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(thing))
		{
			return std::nullopt;
		}
		if (ecs::script_flocks::IsFlock(registry, thing))
		{
			const auto target = ecs::CameraTargetOf(registry, thing);
			if (!target.has_value())
			{
				return std::nullopt;
			}
			return script_camera::FollowedFlock(target->position, target->height);
		}
		const auto* transform = registry.TryGet<const Transform>(thing);
		if (transform == nullptr)
		{
			return std::nullopt;
		}
		// TODO(opening): a creature's height is its body's
		float height = 0.0f;
		if (const auto* mesh = registry.TryGet<const ecs::components::Mesh>(thing); mesh != nullptr)
		{
			const auto& meshes = Locator::resources::value().GetMeshes();
			if (meshes.Contains(mesh->id))
			{
				height = meshes.Handle(mesh->id)->GetBoundingBox().Size().y * std::abs(transform->scale.y);
			}
		}
		const auto* wallHug = registry.TryGet<const ecs::components::WallHug>(thing);
		// A villager is where its drawing glides to this frame, or in the opening hand's grip while it holds it
		auto drawnAt =
		    ecs::components::DrawnPosition(transform->position, registry.TryGet<const ecs::components::VillagerPose>(thing));
		if (const auto* highDetail = registry.TryGet<const ecs::components::HighDetail>(thing); highDetail != nullptr)
		{
			drawnAt = ecs::high_detail_rules::DrawnAt(highDetail->heldAt, drawnAt);
		}
		return script_camera::FollowedThing {
		    .point = drawnAt + glm::vec3(0.0f, height * 0.5f, 0.0f),
		    .gameAngle = wallHug != nullptr ? std::optional(wallHug->gameAngle) : std::nullopt,
		    .height = height,
		};
	};
}

/// The camera commands that move it warn of a script doing so in the temple
ScriptCameraModel* ScriptCameraToMove()
{
	if (PlayerInsideTemple())
	{
		ScriptMessage("Script moving camera in citadel");
	}
	return ScriptCamera();
}

/// Puts the camera at a position and focus at once, still, whatever has the camera. When the kept view of the camera
/// has this position and focus, nothing changes. The land's draw list is made again on the next frame.
void PlaceCamera(const glm::vec3& origin, const glm::vec3& focus)
{
	CountCameraOrder();
	Locator::camera::value().Place(origin, focus);
	RemakeDrawList();
}

void SetCameraPosition() // 001 SET_CAMERA_POSITION
{
	const auto position = PopVec();
	// Without the script's camera the position is quietly dropped
	if (auto* camera = OrderedScriptCamera(); camera != nullptr)
	{
		camera->SetOrigin(position);
		RemakeDrawList();
	}
}

void SetCameraFocus() // 002 SET_CAMERA_FOCUS
{
	const auto position = PopVec();
	if (auto* camera = ScriptCameraToMove(); camera != nullptr)
	{
		camera->SetFocus(position);
		RemakeDrawList();
	}
}

void MoveCameraPosition() // 003 MOVE_CAMERA_POSITION
{
	const auto time = Popf();
	const auto position = PopVec();
	if (auto* camera = ScriptCameraToMove(); camera != nullptr)
	{
		camera->MoveOrigin(position, time);
	}
}

void MoveCameraFocus() // 004 MOVE_CAMERA_FOCUS
{
	const auto time = Popf();
	const auto position = PopVec();
	if (auto* camera = ScriptCameraToMove(); camera != nullptr)
	{
		camera->MoveFocus(position, time);
	}
}

void GetCameraPosition() // 005 GET_CAMERA_POSITION
{
	const auto reported = ecs::ReportedPlayerCamera(PlayerNames::PLAYER_ONE);
	PushVec(reported.has_value() ? reported->origin : glm::vec3(0.0f));
}

void GetCameraFocus() // 006 GET_CAMERA_FOCUS
{
	const auto reported = ecs::ReportedPlayerCamera(PlayerNames::PLAYER_ONE);
	PushVec(reported.has_value() ? reported->focus : glm::vec3(0.0f));
}

void GameThingFieldOfView() // 011 GAME_THING_FIELD_OF_VIEW
{
	const auto object = PopObject();
	const bool valid = object != entt::null && Locator::entitiesRegistry::value().Valid(object);
	if (!valid)
	{
		ObjectNoLongerValid();
	}
	const auto view = ecs::thing_on_screen::ScreenView(PlayerNames::PLAYER_ONE);
	Pushb(valid && !PlayerInsideTemple() && view.has_value() && ecs::thing_on_screen::OnScreen(*view, object));
}

void PosFieldOfView() // 012 POS_FIELD_OF_VIEW
{
	const auto position = PopVec();
	const auto view = ecs::thing_on_screen::ScreenView(PlayerNames::PLAYER_ONE);
	Pushb(!PlayerInsideTemple() && view.has_value() && screen_pick::PointOnScreen(*view, position));
}

/// The way a creature's head looks: towards the point that it looks at, when it looks at a point that is not where it
/// stands; else the way its body faces
static GameHeading CreatureLookHeading(entt::entity creature, const Transform& transform)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* animation = registry.TryGet<const ecs::components::CreatureAnimation>(creature);
	if (animation != nullptr && animation->lookAt.has_value() && *animation->lookAt != transform.position)
	{
		const float dx = animation->lookAt->x - transform.position.x;
		const float dz = animation->lookAt->z - transform.position.z;
		return GameHeading::Of(dx, dz);
	}
	if (const auto* locomotion = registry.TryGet<const ecs::components::CreatureLocomotion>(creature);
	    locomotion != nullptr && locomotion->started)
	{
		return locomotion->heading;
	}
	return GameHeading(script::property_rules::PlacedAngles(transform.rotation).y);
}

void StartCameraControl() // 030 START_CAMERA_CONTROL
{
	const auto task = CurrentTask();
	const bool templeScript = TaskType(task) & (lhvm::ScriptType::TempleHelp | lhvm::ScriptType::TempleSpecial);
	const bool insideTemple = PlayerInsideTemple();
	// Following a creature is left for the script's camera, which takes over from where the camera is
	if (!insideTemple && Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().IsActive())
	{
		Locator::creatureModeSystem::value().Leave();
	}
	const bool taken = Locator::scriptControlSystem::value().StartCameraControl(
	    Locator::camera::value(), {.task = task, .templeScript = templeScript, .insideTemple = insideTemple},
	    [](float x, float z) { return Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)); });
	// A script that retries a refused take each turn does not stop the arrival of the camera for the other scripts
	if (script_camera::TakeIsCameraOrder(taken, insideTemple))
	{
		CountCameraOrder();
	}
	if (taken)
	{
		ApplySwitchChanges(script::switches::CameraTaken(insideTemple));
	}
	Pushb(taken);
}

void EndCameraControl() // 031 END_CAMERA_CONTROL
{
	// Only the task that has the camera gives it back. For any other task the camera does not change.
	if (Locator::scriptControlSystem::value().EndCameraControl(Locator::camera::value(), CurrentTask()))
	{
		CountCameraOrder();
		ApplySwitchChanges(script::switches::CameraReleased());
	}
}

void SetWidescreen() // 032 SET_WIDESCREEN
{
	const auto enabled = static_cast<bool>(Pop().intVal);
	// Only the task that brought the bars in may change them, or any while none has
	auto& director = Locator::cinematicDirectorSystem::value();
	const auto owner = director.GetWideScreenOwner();
	const auto task = Locator::vm::value().GetCurrentTaskNumber();
	if (enabled && owner == task)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script asking for widescreen it has control of");
	}
	if (owner == 0 || owner == task)
	{
		director.SetWideScreen(enabled, task);
	}
}

/// A thing a script tells to face a point turns to it: a villager or another living thing at once, with its walk; a
/// creature is left to its own turning; anything else that stands on the land is turned at once about the upright,
/// keeping any lean it has
static void FacePoint(entt::entity object, glm::vec3 point)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (IsDirectableVillager(object))
	{
		Locator::livingActionSystem::value().VillagerFace(object, glm::vec2(point.x, point.z));
		return;
	}
	auto* transform = registry.TryGet<Transform>(object);
	if (transform == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Jonty - Thing must be living to face position!");
		return;
	}
	if (registry.AllOf<ecs::components::Creature>(object))
	{
		// The script takes control of the creature, which gives up what it was doing and turns to face the point,
		// keeping its head turned to it until the script lets it go
		if (Locator::creatureMindSystem::has_value())
		{
			auto& minds = Locator::creatureMindSystem::value();
			minds.SetScriptFocus(object, creature_mind::Gaze {.point = point});
			minds.CarryOutForScript(object, creature_mind::FaceForScript(point));
		}
		return;
	}
	const float angle = script::property_rules::FacingAngle(glm::vec2(transform->position.x, transform->position.z),
	                                                        glm::vec2(point.x, point.z));
	if (IsLivingThing(object))
	{
		TurnLivingThing(object, *transform, angle);
		return;
	}
	auto angles = ecs::placed_turn::AnglesOf(registry, object);
	angles.y = angle;
	TurnPlacedThing(object, angles, false);
}

void SetFocus() // 034 SET_FOCUS
{
	const auto position = PopVec();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing no longer valid");
		return;
	}
	if (IsScriptContainer(registry, object))
	{
		// Each of its members turns to face the point
		for (const auto member : ContainerMembers(registry, object))
		{
			if (registry.Valid(member))
			{
				FacePoint(member, position);
			}
		}
		return;
	}
	FacePoint(object, position);
}

void HasCameraArrived() // 035 HAS_CAMERA_ARRIVED
{
	if (PlayerInsideTemple())
	{
		ScriptMessage("Script camera in citadel");
	}
	// The view report of the player tells whether the camera has arrived
	const auto view = ScriptView();
	Pushb(view.has_value() && script_camera::ArrivedByReport(view->cameraArrived, view->cameraOrders,
	                                                         Locator::entitiesRegistry::value().Context().scriptCameraOrders));
}

void FocusFollow() // 049 FOCUS_FOLLOW
{
	// The same as SET_FOCUS_FOLLOW: the script's camera glides round to look at the thing and keeps looking at it
	SetFocusFollow();
}

/// The thing a following command is given, and the script's camera, each complained of when missing
std::pair<entt::entity, ScriptCameraModel*> FollowCommand(entt::entity thing)
{
	auto* camera = OrderedScriptCamera();
	if (camera == nullptr)
	{
		ScriptMessage("Script camera has been removed! - Exception happened?");
	}
	const bool valid = thing != entt::null && Locator::entitiesRegistry::value().Valid(thing);
	if (!valid)
	{
		ObjectNoLongerValid();
	}
	// While a two-thing camera has taken over, the script's own commands don't reach the camera
	if (valid && camera != nullptr && camera->InDualView())
	{
		ScriptMessage("We are in the wrong camera mode!");
		camera = nullptr;
	}
	return {valid ? thing : entt::null, camera};
}

void PositionFollow() // 050 POSITION_FOLLOW
{
	const auto [thing, camera] = FollowCommand(PopObject());
	if (thing != entt::null && camera != nullptr)
	{
		// As SET_POSITION_FOLLOW, but the script's camera moves behind the thing as it follows, and does not go there at
		// once
		camera->Follow({.id = static_cast<uint32_t>(thing), .lookup = FollowedThing(thing)}, false);
	}
}

void ConvertCameraPosition() // 081 CONVERT_CAMERA_POSITION
{
	// Where one of the camera editor's numbered cameras is. The game pushes whatever its stack held for a number the file
	// doesn't have; here that is nowhere
	const auto camera = camera_edits::FindCamera(Pop().intVal);
	const auto position = camera.has_value() ? camera->position : std::array<float, 3> {};
	Pushv(position[0]);
	Pushv(position[1]);
	Pushv(position[2]);
}

void ConvertCameraFocus() // 082 CONVERT_CAMERA_FOCUS
{
	// What one of the camera editor's numbered cameras looks at, nowhere for a number the file doesn't have
	const auto camera = camera_edits::FindCamera(Pop().intVal);
	const auto focus = camera.has_value() ? camera->focus : std::array<float, 3> {};
	Pushv(focus[0]);
	Pushv(focus[1]);
	Pushv(focus[2]);
}

/// One of a two-thing camera's things, as the camera sees it while the thing is there: where the game holds it, its
/// height and its radius on the ground (a marker or a group counts as 30 wide). A flock is at its own place on the land.
ScriptCameraModel::DualSubject DualSubjectOf(entt::entity thing)
{
	return {.id = static_cast<uint32_t>(thing), .lookup = [thing]() -> std::optional<script_camera::DualThing> {
		        const auto& registry = Locator::entitiesRegistry::value();
		        if (ecs::script_flocks::IsFlock(registry, thing))
		        {
			        if (!Locator::terrainSystem::has_value())
			        {
				        return std::nullopt;
			        }
			        const auto& place = registry.Get<const ecs::components::Flock>(thing).place;
			        return script_camera::DualFlock(map_coords::ToWorld(Locator::terrainSystem::value(), place));
		        }
		        const auto* transform = registry.Valid(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
		        if (transform == nullptr)
		        {
			        return std::nullopt;
		        }
		        const bool modelled =
		            !registry.AllOf<ecs::components::ScriptMarker>(thing) && !IsScriptContainer(registry, thing);
		        return script_camera::DualThing {
		            .position = transform->position,
		            .height = IsWorldObject(thing) ? ScriptHeightOf(thing) : 0.0f,
		            .radius = modelled ? ecs::systems::object_measures::TwoDRadius(registry, thing)
		                               : script_camera::k_DualNoModelRadius,
		        };
	        }};
}

/// The two things a two-thing camera command is given, the second popped first; none, complained of, when either is
/// missing
std::optional<std::pair<entt::entity, entt::entity>> PopDualThings()
{
	const auto second = PopObject();
	const auto first = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (first == entt::null || second == entt::null || !registry.Valid(first) || !registry.Valid(second))
	{
		ScriptMessage("Thing invalid for dual cam");
		return std::nullopt;
	}
	return std::pair {first, second};
}

void StartDualCamera() // 093 START_DUAL_CAMERA
{
	const auto things = PopDualThings();
	auto* camera = OrderedScriptCamera();
	if (things.has_value() && camera != nullptr)
	{
		// The camera keeps both things in view, from the side of the line between them, until the script lets it go
		camera->StartDualView(DualSubjectOf(things->first), DualSubjectOf(things->second));
	}
}

void UpdateDualCamera() // 094 UPDATE_DUAL_CAMERA
{
	const auto things = PopDualThings();
	auto* camera = OrderedScriptCamera();
	if (things.has_value() && camera != nullptr)
	{
		camera->UpdateDualView(DualSubjectOf(things->first), DualSubjectOf(things->second));
	}
}

void ReleaseDualCamera() // 095 RELEASE_DUAL_CAMERA
{
	// The script's own camera takes over again, from wherever the two-thing camera left it
	if (auto* camera = OrderedScriptCamera(); camera != nullptr)
	{
		camera->ReleaseDualView();
	}
}

void CreateDualCameraWithPoint() // 105 CREATE_DUAL_CAMERA_WITH_POINT
{
	const auto point = PopVec();
	const auto thing = PopObject();
	// The game does not check the thing. A camera on a thing that is not there lets go at the next turn.
	if (auto* camera = OrderedScriptCamera(); camera != nullptr)
	{
		// The camera keeps the thing and the point in view, from the side of the line between them, until the script lets
		// it go
		camera->StartDualViewWithPoint(DualSubjectOf(thing), point);
	}
}

/// The way a thing faces, as a camera sent to face it reads it: a walker by its game angle, a creature by its body's
/// turn, anything else along the first heading
float CameraFacingOf(entt::entity object, const Transform& transform)
{
	const auto& registry = Locator::entitiesRegistry::value();
	constexpr float k_QuarterTurn = std::numbers::pi_v<float> * 0.5f;
	if (registry.AllOf<ecs::components::Creature>(object))
	{
		// Its body is turned a quarter turn on from the way it faces
		return script_camera::CreatureFacing(LivingAngleOf(object, transform) + k_QuarterTurn);
	}
	if (const auto* walk = registry.TryGet<const ecs::components::WallHug>(object); walk != nullptr)
	{
		return gutils::ConvertGameAngleToScawenAngle(walk->gameAngle);
	}
	if (const auto* animal = registry.TryGet<const ecs::components::Animal>(object); animal != nullptr)
	{
		return gutils::ConvertGameAngleToScawenAngle(static_cast<uint16_t>(gutils::ConvertAngle3DToGame(animal->heading)));
	}
	return 0.0f;
}

/// Where a camera facing a thing from a distance stands and looks; none, complained of, without the thing
std::optional<script_camera::View> FaceViewOf(entt::entity object, float distance)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = object != entt::null && registry.Valid(object) ? registry.TryGet<const Transform>(object) : nullptr;
	if (transform == nullptr)
	{
		ScriptMessage("no object to face");
		return std::nullopt;
	}
	const float facing = CameraFacingOf(object, *transform);
	if (script_camera::IsWrongFacing(facing))
	{
		ScriptMessage("Invalid heading");
	}
	return script_camera::FaceView(
	    {.position = transform->position, .height = IsWorldObject(object) ? ScriptHeightOf(object) : 0.0f, .facing = facing},
	    distance);
}

/// The script's camera for a command that sends it to face a thing, complained of when it is missing, in the temple or
/// under a two-thing camera
ScriptCameraModel* ScriptCameraToFace()
{
	auto* camera = OrderedScriptCamera();
	if (camera == nullptr)
	{
		ScriptMessage("Script camera has been removed!");
	}
	if (PlayerInsideTemple())
	{
		ScriptMessage("Script moving camera in citadel");
	}
	if (camera != nullptr && camera->InDualView())
	{
		ScriptMessage("We are in the wrong camera mode!");
		return nullptr;
	}
	return camera;
}

void SetCameraToFaceObject() // 106 SET_CAMERA_TO_FACE_OBJECT
{
	const auto distance = Popf();
	const auto object = PopObject();
	if (auto* camera = ScriptCameraToFace(); camera != nullptr)
	{
		// The camera is put at once in front of the thing, by the way it faces, looking at it
		if (const auto view = FaceViewOf(object, distance); view.has_value())
		{
			camera->SetOrigin(view->origin);
			camera->SetFocus(view->focus);
		}
		// Do this also when there is no thing to face
		RemakeDrawList();
	}
}

void MoveCameraToFaceObject() // 107 MOVE_CAMERA_TO_FACE_OBJECT
{
	const auto time = Popf();
	const auto distance = Popf();
	const auto object = PopObject();
	if (auto* camera = ScriptCameraToFace(); camera != nullptr)
	{
		// The camera glides over the time to in front of the thing, by the way it faces now, looking at it
		if (const auto view = FaceViewOf(object, distance); view.has_value())
		{
			camera->MoveOrigin(view->origin, time);
			camera->MoveFocus(view->focus, time);
		}
	}
}

void RunCameraPath() // 119 RUN_CAMERA_PATH
{
	const auto track = Pop().intVal;
	auto* camera = OrderedScriptCamera();
	if (camera == nullptr)
	{
		ScriptMessage("Script camera has been removed! - Exception happened?");
		return;
	}
	// The script's camera runs the camera editor's track of that number; one the file doesn't have runs nothing
	camera->RunTrack(camera_edits::FindTrack(track), track);
}

void WidescreenTransistionFinished() // 132 WIDESCREEN_TRANSISTION_FINISHED
{
	Pushb(Locator::cinematicDirectorSystem::value().IsWideScreenTransitionFinished());
}

void SetCameraZone() // 142 SET_CAMERA_ZONE
{
	const auto fileName = PopString();
	// The world camera is kept inside the file's fence and under its height limits from now on
	if (!Locator::cameraZoneSystem::value().SetZones(fileName))
	{
		ScriptMessage(fmt::format("Couldn't load zone file-.\\Data\\Zones\\{}", fileName));
	}
}

void FocusAndPositionFollow() // 178 FOCUS_AND_POSITION_FOLLOW
{
	const auto distance = Popf();
	const auto [thing, camera] = FollowCommand(PopObject());
	if (thing != entt::null && camera != nullptr)
	{
		// As SET_FOCUS_AND_POSITION_FOLLOW, but the script's camera moves to the distance as it follows, and does not go
		// there at once
		camera->FollowAndLookAt({.id = static_cast<uint32_t>(thing), .lookup = FollowedThing(thing)}, distance, false);
	}
}

void CameraProperties() // 180 CAMERA_PROPERTIES
{
	const auto relative = Pop().intVal != 0;
	// Degrees to radians by the game's own float
	constexpr float k_DegreesToRadians = 0.017453292f;
	const auto angle = Popf() * k_DegreesToRadians;
	const auto glideScale = Popf();
	const auto distance = Popf();
	auto* camera = OrderedScriptCamera();
	if (camera == nullptr)
	{
		ScriptMessage("Script camera has been removed! - Exception happened?");
		return;
	}
	// How the script's camera follows a thing: from how far, how quickly, at what heading, and whether the heading turns
	// with the thing
	camera->SetFollowSettings(distance, glideScale, angle, relative);
}

void ShakeCamera() // 201 SHAKE_CAMERA
{
	const auto duration = Popf();
	const auto amplitude = Popf();
	const auto radius = Popf();
	const auto position = PopVec();
	// The shake lasts the seconds rounded to whole milliseconds. It moves the camera along all axes
	Locator::explosionSystem::value().AddShake(position, radius, amplitude, camera_shake::ScriptMilliseconds(duration), false);
}

void SetFixedCamRotation() // 209 SET_FIXED_CAM_ROTATION
{
	// Any value other than 0 gives the player's camera the point to turn about and look at. The value 0 removes the point.
	const auto point = PopVec();
	const bool fixed = Pop().uintVal != 0;
	if (!Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	if (!forced_turn::TakesPoint(camera.GetModel()))
	{
		ScriptMessage("Wrong camera mode");
		return;
	}
	camera.SetForcedTurnPoint(fixed ? std::optional(point) : std::nullopt);
}

void SetFade() // 241 SET_FADE
{
	// The game takes each as a whole number, the colour's as bytes and the seconds as a small signed one
	const auto time = static_cast<int8_t>(static_cast<int32_t>(Popf()));
	const auto blue = static_cast<uint8_t>(static_cast<int32_t>(Popf()));
	const auto green = static_cast<uint8_t>(static_cast<int32_t>(Popf()));
	const auto red = static_cast<uint8_t>(static_cast<int32_t>(Popf()));
	Locator::cinematicDirectorSystem::value().FadeTo(red, green, blue, time);
}

void SetFadeIn() // 242 SET_FADE_IN
{
	const auto duration = static_cast<int8_t>(static_cast<int32_t>(Popf()));
	Locator::cinematicDirectorSystem::value().FadeBackToNormal(duration);
}

void FadeFinished() // 243 FADE_FINISHED
{
	Pushb(Locator::cinematicDirectorSystem::value().IsFadeFinished());
}

void SetFocusFollow() // 276 SET_FOCUS_FOLLOW
{
	const auto [thing, camera] = FollowCommand(PopObject());
	if (thing != entt::null && camera != nullptr)
	{
		// The script's camera keeps looking at the thing
		camera->LookAt({.id = static_cast<uint32_t>(thing), .lookup = FollowedThing(thing)});
	}
}

void SetPositionFollow() // 277 SET_POSITION_FOLLOW
{
	const auto [thing, camera] = FollowCommand(PopObject());
	if (thing != entt::null && camera != nullptr)
	{
		// The script's camera is put behind the thing, as far as eight times its height, and follows it
		camera->Follow({.id = static_cast<uint32_t>(thing), .lookup = FollowedThing(thing)}, true);
	}
}

void SetFocusAndPositionFollow() // 278 SET_FOCUS_AND_POSITION_FOLLOW
{
	const auto distance = Popf();
	const auto [thing, camera] = FollowCommand(PopObject());
	if (thing != entt::null && camera != nullptr)
	{
		// The script's camera is put at a distance from the thing, follows it and looks at it
		camera->FollowAndLookAt({.id = static_cast<uint32_t>(thing), .lookup = FollowedThing(thing)}, distance, true);
	}
}

void SetCameraLens() // 279 SET_CAMERA_LENS
{
	// "reset camera lens": the lens glides back to the normal one over the time
	const auto time = Popf();
	Locator::camera::value().SetLens(glm::radians(Locator::config::value().cameraXFov), time);
}

void MoveCameraLens() // 280 MOVE_CAMERA_LENS
{
	// "set camera lens": the lens glides to a field of view in degrees over the time, at once without one
	const auto time = Popf();
	const auto degrees = Popf();
	// Degrees to radians by the game's own float
	constexpr float k_DegreesToRadians = 0.017453292f;
	Locator::camera::value().SetLens(degrees * k_DegreesToRadians, time);
}

void StoreCameraDetails() // 283 STORE_CAMERA_DETAILS
{
	// The script keeps where the camera is and where it looks as the frame draws it, whatever has the camera. It reads
	// the camera from the last view report, as the other camera reads of the scripts do.
	const auto view = ScriptView();
	Locator::scriptControlSystem::value().StoreCameraDetails({
	    .origin = view.has_value() ? view->eye : glm::vec3(0.0f),
	    .focus = view.has_value() ? view->focus : glm::vec3(0.0f),
	});
}

void RestoreCameraDetails() // 284 RESTORE_CAMERA_DETAILS
{
	// The camera goes back at once to the position and focus that the script kept last
	const auto details = Locator::scriptControlSystem::value().GetCameraDetails();
	PlaceCamera(details.origin, details.focus);
}

void SetCameraPosFocLens() // 286 SET_CAMERA_POS_FOC_LENS
{
	const auto lens = Popf();
	const auto focus = PopVec();
	const auto position = PopVec();
	// The camera goes at once to the position and focus, whatever has the camera
	PlaceCamera(position, focus);
	// The lens changes at once. The script gives it in radians: this command does not change degrees to radians.
	Locator::camera::value().SetLens(lens, 0.0f);
}

void MoveCameraPosFocLens() // 287 MOVE_CAMERA_POS_FOC_LENS
{
	const auto time = Popf();
	const auto lens = Popf();
	const auto focus = PopVec();
	const auto position = PopVec();
	// Only the script camera moves. The position, the focus and the lens move for the same time. The script gives the
	// lens in radians: this command does not change degrees to radians. The land's draw list does not change.
	if (auto* camera = ScriptCameraToMove(); camera != nullptr)
	{
		camera->MoveOrigin(position, time);
		camera->MoveFocus(focus, time);
		Locator::camera::value().SetLens(lens, time);
	}
}

void SetGraphicsClipping() // 299 SET_GRAPHICS_CLIPPING
{
	// Only the first argument counts: whether the camera clips close
	Pop();
	const auto close = Pop().uintVal != 0;
	Locator::cinematicDirectorSystem::value().SetCloseClipping(close);
}

void SetFocusOnObject() // 302 SET_FOCUS_ON_OBJECT
{
	const auto target = PopObject();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	const auto* targetTransform =
	    target != entt::null && registry.Valid(target) ? registry.TryGet<const Transform>(target) : nullptr;
	if (object == entt::null || !registry.Valid(object) || targetTransform == nullptr)
	{
		ScriptMessage("Thing not found!");
		return;
	}
	if (registry.AllOf<ecs::components::Creature>(object))
	{
		// The script takes control of the creature, which turns to face the thing, holding still two seconds, and keeps
		// its head turned to it until the script lets it go
		if (Locator::creatureMindSystem::has_value())
		{
			auto& minds = Locator::creatureMindSystem::value();
			minds.SetScriptFocus(object, creature_mind::Gaze {.object = static_cast<uint32_t>(target)});
			minds.CarryOutForScript(object, creature_mind::FaceThingForScript(static_cast<uint32_t>(target)));
		}
		return;
	}
	// Anything else turns at once to face where the thing stands, as SET_FOCUS turns it to face a point; a marker or a
	// group doesn't turn
	if (!registry.AllOf<ecs::components::ScriptMarker>(object) && !IsScriptContainer(registry, object))
	{
		FacePoint(object, targetTransform->position);
	}
}

void ReleaseObjectFocus() // 303 RELEASE_OBJECT_FOCUS
{
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing not found!");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(object))
	{
		ScriptMessage("Thing not creature!");
		return;
	}
	// The creature's head is free to look about again
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().SetScriptFocus(object, std::nullopt);
	}
}

void GetStoredCameraPosition() // 314 GET_STORED_CAMERA_POSITION
{
	// The camera position that STORE_CAMERA_DETAILS kept last. Before the first store, it is at zero.
	PushVec(Locator::scriptControlSystem::value().GetCameraDetails().origin);
}

void GetStoredCameraFocus() // 315 GET_STORED_CAMERA_FOCUS
{
	// The camera focus that STORE_CAMERA_DETAILS kept last. Before the first store, it is at zero.
	PushVec(Locator::scriptControlSystem::value().GetCameraDetails().focus);
}

void GameThingCanViewCamera() // 339 GAME_THING_CAN_VIEW_CAMERA
{
	const auto degrees = Popf();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = object != entt::null && registry.Valid(object) ? registry.TryGet<const Transform>(object) : nullptr;
	if (transform == nullptr || !IsWorldObject(object))
	{
		ScriptMessage(script::lesson_natives::k_ThingNotValid);
		Pushb(false);
		return;
	}
	// The camera is where the player's report put it
	const auto camera = ecs::ReportedPlayerCamera(PlayerNames::PLAYER_ONE);
	const auto eye = camera.has_value() ? camera->origin : glm::vec3(0.0f);
	if (registry.AllOf<ecs::components::Creature>(object))
	{
		// A creature looks with its head, not with the way its body faces
		const auto look = CreatureLookHeading(object, *transform);
		const auto at = map_coords::FromMetres(glm::vec2(transform->position.x, transform->position.z));
		Pushb(script::lesson_natives::CreatureSeesCamera(at, map_coords::FromMetres(glm::vec2(eye.x, eye.z)), look.GameAngle(),
		                                                 degrees));
		return;
	}
	// The thing sees from its place on the map in the way it faces
	const auto place = glm::vec3(map_coords::Quantise(transform->position.x), transform->position.y,
	                             map_coords::Quantise(transform->position.z));
	const float yAngle =
	    IsLivingThing(object) ? LivingAngleOf(object, *transform) : ecs::placed_turn::AnglesOf(registry, object).y;
	Pushb(script::lesson_natives::InViewCone(place, yAngle, script::lesson_natives::HalfViewAngle(degrees), eye));
}

/// The script's camera for a command that follows the hand of a computer god: with a warning when the script has none
ScriptCameraModel* ComputerHandCamera()
{
	auto* camera = OrderedScriptCamera();
	if (camera == nullptr)
	{
		ScriptMessage("Script camera has been removed!");
		return nullptr;
	}
	// While a two-thing camera has taken over, the script's own commands do not reach the camera
	if (camera->InDualView())
	{
		ScriptMessage("We are in the wrong camera mode!");
		return nullptr;
	}
	return camera;
}

/// The place of the hand of a computer god, as a camera reads it in each frame
ScriptCameraModel::HandLookup ComputerHandPlace(PlayerNames player)
{
	return [player]() -> std::optional<glm::vec3> {
		if (!Locator::computerPlayerSystem::has_value())
		{
			return std::nullopt;
		}
		return Locator::computerPlayerSystem::value().HandPositionForView(player);
	};
}

void SetFocusFollowComputerPlayer() // 372 SET_FOCUS_FOLLOW_COMPUTER_PLAYER
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (auto* camera = ComputerHandCamera(); camera != nullptr)
	{
		// The script's camera keeps looking at the hand of the god
		camera->LookAtComputerHand(ComputerHandPlace(player));
	}
}

void SetPositionFollowComputerPlayer() // 373 SET_POSITION_FOLLOW_COMPUTER_PLAYER
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (auto* camera = ComputerHandCamera(); camera != nullptr)
	{
		// The script's camera follows the hand of the god
		camera->FollowComputerHand(ComputerHandPlace(player));
	}
}

void GetFacingCameraPosition() // 377 GET_FACING_CAMERA_POSITION
{
	const auto distance = Popf();
	// The point in front of the camera of the player, from the last view report. Before the first report, the eye and
	// the focus are both the middle of the world's axes.
	const auto view = ScriptView();
	const auto eye = view.has_value() ? view->eye : glm::vec3(0.0f);
	const auto focus = view.has_value() ? view->focus : glm::vec3(0.0f);
	PushVec(script_camera::FacingPoint(eye, focus, distance));
}

void SetClippingWindow() // 437 SET_CLIPPING_WINDOW
{
	// The screen goes black but for a box, its place and size shares of the screen, the black fading in over the time
	const auto time = Popf();
	const auto height = Popf();
	const auto width = Popf();
	const auto down = Popf();
	const auto across = Popf();
	Locator::cinematicDirectorSystem::value().OpenClipWindow({across, down}, {width, height}, time);
}

void ClearClippingWindow() // 438 CLEAR_CLIPPING_WINDOW
{
	// The black round the box fades out over the time
	const auto time = Popf();
	Locator::cinematicDirectorSystem::value().CloseClipWindow(time);
}

ScriptCameraModel::Lookups ScriptCameraLookups()
{
	return {
	    .thing = [](uint32_t id) { return FollowedThing(static_cast<entt::entity>(id)); },
	    .dual = [](uint32_t id) { return DualSubjectOf(static_cast<entt::entity>(id)).lookup; },
	    .track = [](int32_t number) { return camera_edits::FindTrack(number); },
	};
}

} // namespace openblack::chlapi
