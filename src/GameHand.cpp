/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand of the player of this computer: its frame, its place and turn on the land, its tool tips, its gestures and
// its sounds

#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/CreatureCaveTrophies.h"
#include "3D/DayNightClock.h"
#include "3D/FlatLand.h"
#include "3D/HandAnimation.h"
#include "3D/HandCycleChoice.h"
#include "3D/HandMorph.h"
#include "3D/HandNavigationPose.h"
#include "3D/HandOrientation.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightFrame.h"
#include "3D/LandLightTable.h"
#include "3D/MapCoords.h"
#include "3D/SkyDome.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/WaterRings.h"
#include "Audio/AtmosAudio.h"
#include "Audio/AudioManagerInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/FightCameraModel.h"
#include "Camera/ScriptCameraModel.h"
#include "Commands/PlayerCommands.h"
#include "Game.h"
#include "GameDetail.h"
#include "Sound/ClipSounds.h"
#include "Sound/GameMusic.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/HelpSpeech.h"
#include "Sound/ScriptSoundEffect.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Common/RandomNumberManager.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureFightHud.h"
#include "Creature/CreatureHandRules.h"
#include "Creature/CreatureRig.h"
#include "Creature/LeashOrders.h"
#include "Debug/DebugGuiInterface.h"
#include "Debug/FrameStatsLog.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/DrawnPlace.h"
#include "ECS/FloatingNumber.h"
#include "ECS/LearningMarker.h"
#include "ECS/Map.h"
#include "ECS/ProfileAlignment.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FilmCreatureSystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/FloatingNumberSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandPressSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PuzzleGameSystemInterface.h"
#include "ECS/Systems/PuzzleTotemSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/SimulationSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TattooMenuSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TownDesireFlags.h"
#include "ECS/VillageTotem.h"
#include "ECS/VillagerLabelSource.h"
#include "ECS/WorldObjects.h"
#include "EngineConfig.h"
#include "Gestures/GestureTrailBuilder.h"
#include "Graphics/FrameBuffer.h"
#include "Gui/GameInterface.h"
#include "Gui/HandToolTips.h"
#include "Gui/MultiplayerFrontEnd.h"
#include "Gui/ScreenFit.h"
#include "Gui/StartupRules.h"
#include "Gui/StatisticsBoxRules.h"
#include "Gui/WorldToolTipRules.h"
#include "Gui/WorldToolTips.h"
#include "Hand/HandBlend.h"
#include "Hand/HandFeel.h"
#include "Hand/HandGrabRules.h"
#include "Hand/HandReach.h"
#include "Hand/HandTrace.h"
#include "Hand/HandVisibility.h"
#include "Hand/ScriptHandState.h"
#include "Help/DialogueText.h"
#include "Help/HelpProfile.h"
#include "Help/SingAlong.h"
#include "Help/Spirits.h"
#include "Help/TurnCheer.h"
#include "Input/GameActionMapInterface.h"
#include "Input/LandGrip.h"
#include "Input/PlatformKeys.h"
#include "Locator.h"
#include "Magic/TownBelief.h"
#include "Online/NetworkLand.h"
#include "Online/NetworkSession.h"
#include "Online/OnlineMapLand.h"
#include "Particles/LiquidParticles.h"
#include "Profiler.h"
#include "Resources/GameAssets.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptInterfaceLevels.h"
#include "Simulation/FrameBody.h"
#include "Simulation/ProfileMenu.h"
#include "Simulation/TurnBody.h"
#include "Simulation/WorldStart.h"
#include "Video/VideoRules.h"

using namespace openblack;
using namespace openblack::game_detail;

namespace
{

/// Notes in the hand's debugging trace that a part of the frame put the hand at a position (see hand_trace::HandTrace)
void TraceHand(std::string_view writer, const glm::vec3& position)
{
	if (auto* trace = Locator::handSystem::value().GetTrace(); trace != nullptr)
	{
		trace->Note(writer, position);
	}
}

/// The cursor that the hand follows, in the pixels of the window
[[nodiscard]] glm::ivec2 HandCursor()
{
	return Locator::handPressSystem::value().GetMousePosition();
}

/// The eye and the cursor's point on the near plane, which the hand's line of sight goes through. None when there is
/// no window.
[[nodiscard]] std::optional<Camera::NearPlanePoint> HandCursorLine(const Camera& camera)
{
	if (!Locator::windowing::has_value())
	{
		return std::nullopt;
	}
	const auto size = Locator::windowing::value().GetSize();
	if (size.x <= 0 || size.y <= 0)
	{
		return std::nullopt;
	}
	return camera.OnNearPlane(static_cast<glm::vec2>(HandCursor()) / static_cast<glm::vec2>(size));
}

/// What the buttons of the mouse do with the hand after the last event
[[nodiscard]] const input::HandButtons& HandButtons()
{
	return Locator::handPressSystem::value().GetHandButtons();
}

/// The hand's splash, its sound and the scare it gives the fish are this high over the sea
constexpr float k_HandSplashHeight = 0.2f;

/// Whether the hand turns to the face of a model it rests on: not a tree's, a dead tree's, a totem statue's, a seed's or
/// a shield's, whose faces leave the hand turned to the land
bool FeelsModel(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AnyOf<ecs::components::Tree, ecs::components::DeadTree, ecs::components::SpellSeed,
	                   ecs::components::MagicShield>(object))
	{
		return false;
	}
	const auto* info = ecs::world_objects::InfoOf(object);
	return info == nullptr || info->type != ObjectType::TotemStatue;
}

/// The creature a script's camera is about, any player's: the thing it looks at, else the one it follows, when that is
/// a creature
std::optional<entt::entity> ScriptCameraCreature()
{
	if (!Locator::scriptControlSystem::has_value() || !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto* camera = Locator::scriptControlSystem::value().GetScriptCamera(Locator::camera::value());
	const auto thing = camera != nullptr ? camera->GetFocusedThing() : std::nullopt;
	if (!thing.has_value())
	{
		return std::nullopt;
	}
	const auto entity = static_cast<entt::entity>(*thing);
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) && registry.AllOf<ecs::components::Creature>(entity) ? std::optional(entity) : std::nullopt;
}

/// Whether the hand is within the local player's influence, as what it offers and does asks it: at the point picked,
/// which is the position of the thing under the cursor, or the land under it with nothing there (anywhere, with neither).
/// While the hand keeps some of the player's influence past the border it counts as inside however far out it is.
bool HandInInfluence()
{
	// One answer for what the hand offers and what it does
	if (Locator::handGrabSystem::has_value())
	{
		return Locator::handGrabSystem::value().IsInInfluence();
	}
	const auto point = Locator::pickingSystem::has_value() ? Locator::pickingSystem::value().GetPick().point : std::nullopt;
	return !point.has_value() || Locator::influenceSystem::value().IsHandInInfluence(LocalInputPlayer(), *point);
}

/// Whether the hand points at the water where the fish of a fish farm show, with the hand in the local player's
/// influence: the hand can take fish there, though the farm has no model to point at
bool HandOverFish()
{
	const auto point = Locator::pickingSystem::has_value() ? Locator::pickingSystem::value().GetPick().point : std::nullopt;
	if (!point.has_value() || !Locator::fishFarmSystem::has_value() || !HandInInfluence())
	{
		return false;
	}
	// Water is a cell of the land with water in it, or a place off the land
	const auto cell = map_coords::CellOf(*point);
	if (map_coords::InBounds(cell) && Locator::terrainSystem::has_value())
	{
		const auto* land = Locator::terrainSystem::value().FindCell(glm::u16vec2(cell));
		if (land != nullptr && land->properties.hasWater == 0)
		{
			return false;
		}
	}
	return Locator::fishFarmSystem::value().FarmWithFishAt({point->x, point->z}).has_value();
}

/// An ease of the hand takes the fixed steps of the input of this frame (see input_steps)
template <typename Ease>
void StepEase(Ease& ease)
{
	for (uint32_t i = 0; i < Locator::time::value().GetInputSteps().count; ++i)
	{
		ease.Update(input_steps::k_StepSeconds);
	}
}

} // namespace

void Game::UpdateGestures(const Camera& camera, glm::ivec2 screenSize, float deltaSeconds)
{
	if (!Locator::gestureSystem::has_value() || screenSize.x <= 0 || screenSize.y <= 0)
	{
		return;
	}
	const auto size = static_cast<glm::vec2>(screenSize);
	const auto rayAt = [&camera, size](glm::vec2 pixel) {
		glm::vec3 origin;
		glm::vec3 direction;
		camera.DeprojectScreenToWorld(pixel / size, origin, direction);
		return std::pair {origin, direction};
	};
	ecs::systems::GestureSystemInterface::Frame frame {
	    .seconds = deltaSeconds,
	    // Not over a debug window, nor while a cut scene has the screen
	    .overWorld = !Locator::debugGui::value().StealsFocus() && !Locator::debugGui::value().IsMouseOverWindow() &&
	                 Locator::cinematicDirectorSystem::value().IsInterfaceActive(),
	    .actionHeld = Locator::gameActionSystem::value().Get(input::BindableActionMap::ACTION),
	    .player = LocalInputPlayer(),
	    .view = {.screenSize = size,
	             .cameraRight = camera.GetRight(),
	             .cameraEye = camera.GetOrigin(),
	             .cameraForward = camera.GetForward()},
	    .cameraShaking = Locator::explosionSystem::has_value() && Locator::explosionSystem::value().IsShaking(),
	};
	// The places of the pointer that the interface got in this frame
	const auto places = Locator::gameActionSystem::value().GetMousePlaces();
	frame.places.assign(places.begin(), places.end());
	// The fight view that shows a fight in its arena, and the camera of Creature Mode, let the hand draw on while they
	// move
	const auto& model = camera.GetModel();
	const bool fights = Locator::creatureFightSystem::has_value();
	frame.cameraLetsHandDraw = (dynamic_cast<const FightCameraModel*>(&model) != nullptr && fights &&
	                            Locator::creatureFightSystem::value().IsCameraOnFightInArena()) ||
	                           dynamic_cast<const CreatureCameraModel*>(&model) != nullptr;
	frame.moveHeldInFight = fights && Locator::creatureFightSystem::value().DirectsFight() &&
	                        Locator::gameActionSystem::value().Get(input::BindableActionMap::MOVE);
	// The land or the sea under a point of the screen, as the interface takes it to be under the cursor
	frame.view.landAt = [&camera, size](glm::vec2 pixel) -> std::optional<glm::vec3> {
		if (!Locator::pickingSystem::has_value())
		{
			return std::nullopt;
		}
		const auto [eye, nearPoint] = camera.OnNearPlane(pixel / size);
		return Locator::pickingSystem::value().LandUnderPixel(eye, nearPoint,
		                                                      ecs::systems::PickingSystemInterface::Sea::Anywhere);
	};
	// A point of the screen as far in front of the camera as another point
	frame.view.atDepthOf = [rayAt, forward = camera.GetForward()](glm::vec2 pixel, glm::vec3 sameDepthAs) {
		const auto [origin, direction] = rayAt(pixel);
		float distance = 0.0f;
		if (glm::intersectRayPlane(origin, direction, sameDepthAs, -forward, distance))
		{
			return origin + (direction * distance);
		}
		return sameDepthAs;
	};
	// Where a recognised gesture's trail is laid under a pixel: where the line from the camera through it meets the land,
	// or the sea's level no further than 7500 units from the camera across the land, at the land's height there;
	// otherwise 400 units from the camera towards the pixel
	frame.view.trailPointUnder = [&camera, rayAt, size, eye = camera.GetOrigin()](glm::ivec2 pixel) {
		const auto direction = rayAt(glm::vec2(pixel)).second;
		const auto onNear = camera.OnNearPlane(glm::vec2(pixel) / size);
		if (Locator::pickingSystem::has_value())
		{
			if (const auto hit = Locator::pickingSystem::value().LandOrSeaAlong(onNear.eye, onNear.point, onNear.eye))
			{
				return *hit;
			}
		}
		return eye + (direction * gesture::k_TrailSkyDistance);
	};
	Locator::gestureSystem::value().Update(frame);
	// A path drawn for the testbed is drawn by the hand, which follows it. The hand moves the mouse, which sends a
	// message at the place.
	if (const auto drawing = Locator::gestureSystem::value().GetDrawingPoint())
	{
		const auto pixel = glm::ivec2(*drawing * (size.y / gesture::k_ReferenceHeight));
		Locator::handPressSystem::value().MovePointer(pixel);
		Locator::gameActionSystem::value().DrawnMouseMessage(pixel);
	}
}

bool Game::HandOverThing() const
{
	// In the temple the hand only ever wiggles
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		return false;
	}
	if (!Locator::pickingSystem::has_value())
	{
		return false;
	}
	// Only a thing with a model of its own (a bubble's is drawn by the miracles): a fish farm, found through its fish,
	// has none
	const auto picked = Locator::pickingSystem::value().GetPick().object;
	return picked.has_value() && Locator::entitiesRegistry::value().Valid(*picked) &&
	       Locator::entitiesRegistry::value().AnyOf<ecs::components::Mesh, ecs::components::OneOffSpellSeed>(*picked);
}

void Game::PickUnderCursor(float seconds)
{
	if (!Locator::pickingSystem::has_value() || !Locator::windowing::has_value())
	{
		return;
	}
	const auto screenSize = Locator::windowing::value().GetSize();
	if (screenSize.x <= 0 || screenSize.y <= 0)
	{
		return;
	}
	const auto& camera = Locator::camera::value();
	const auto resolution = static_cast<glm::vec2>(screenSize);
	const auto cursor = static_cast<glm::vec2>(HandCursor());
	const auto [eye, nearPoint] = camera.OnNearPlane(cursor / resolution);
	Locator::pickingSystem::value().PickUnderCursor({
	    .view =
	        {
	            .worldToClip = camera.GetViewProjectionMatrix(),
	            .resolution = resolution,
	            .near = camera.GetNearClip(),
	            .xScale = camera.GetProjectionMatrix()[0][0],
	            .camera = eye,
	            .cursor = cursor,
	            .up = camera.GetUp(),
	        },
	    .nearPoint = nearPoint,
	    .seconds = seconds,
	    // Gripping the land, the interface holds what it picked
	    .locked = _handCameraState && _handPose == hand_navigation_pose::Pose::Grip,
	});
}

void Game::UpdateHandInterface()
{
	if (!Locator::gameInterface::has_value())
	{
		return;
	}
	Locator::gameInterface::value().SetCreaturePanel(std::nullopt);
	// The temple places the hand's tooltip itself
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	if (inTemple)
	{
		return;
	}
	// The creature that the hand holds, or else the creature under the hand, any player's, while a hand holds it
	const auto& creatureHand = Locator::creatureHandSystem::value();
	const auto pointed = creatureHand.GetCreature().has_value() ? creatureHand.GetCreature() : _creatureUnderHand;
	auto creature = creature_panel::HandPanelCreature(pointed, pointed.has_value() && creatureHand.IsLocked(*pointed));
	// Without one, the creature Creature Mode follows shows near the top of the screen, without the reward
	std::optional<float> reward = creatureHand.GetLastFeedbackSum();
	if (!creature.has_value() && Locator::creatureModeSystem::has_value())
	{
		creature = Locator::creatureModeSystem::value().GetCreature();
		reward.reset();
	}
	// or the creature a script's camera follows or looks at, any player's
	if (!creature.has_value())
	{
		creature = ScriptCameraCreature();
		reward.reset();
	}
	if (!creature.has_value())
	{
		return;
	}
	const auto* needs = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureNeeds>(*creature);
	if (needs != nullptr)
	{
		// The reward is the hand's, which it keeps showing after letting go until it takes hold again
		Locator::gameInterface::value().SetCreaturePanel(creature_panel::FromNeeds(needs->needs, reward));
	}
}

void Game::UpdateHandToolTipAnchor()
{
	// The temple places the hand's tooltip itself
	if (!Locator::gameInterface::has_value() || !Locator::windowing::has_value() ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	// Not while the pointer is over a debug window
	if (Locator::debugGui::value().IsMouseOverWindow())
	{
		Locator::gameInterface::value().SetHandOnScreen(std::nullopt);
		return;
	}
	const auto handEntity =
	    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	const auto* handTransform = Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(handEntity);
	if (handTransform == nullptr)
	{
		Locator::gameInterface::value().SetHandOnScreen(std::nullopt);
		return;
	}
	// At the hand as drawn this frame, wherever the pointer is
	const auto& camera = Locator::camera::value();
	const auto hand = gui::world_tooltip::HandOnScreen(camera.GetViewProjectionMatrix(), camera.GetNearClip(),
	                                                   handTransform->position, Locator::windowing::value().GetSize());
	Locator::gameInterface::value().SetHandOnScreen(hand.has_value() ? std::optional(glm::vec2(*hand)) : std::nullopt);
}

void Game::ProcessHandToolTipTurn()
{
	if (!Locator::gameInterface::has_value() || (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	auto& toolTips = Locator::gameInterface::value().GetToolTips();
	// The shown tooltip's display time runs down only while the player may turn the camera
	const bool displayTimeRuns = !Locator::cameraHelpSystem::has_value() ||
	                             (Locator::cameraHelpSystem::value().Get().features & camera_help::feature::k_Rotate) != 0;
	// When the hand takes a miracle, or the miracle in the hand gets a new power-up level, the hand shows its name at once
	if (const auto text =
	        Locator::magicSystem::has_value() ? Locator::magicSystem::value().TakeHeldMiracleToolTip() : std::nullopt;
	    text.has_value())
	{
		if (const auto index = gui::world_tooltip::ToolTipOfText(*text); index.has_value())
		{
			toolTips.Force(*index, 0.0f);
		}
	}
	// As the player's own totem moves on the screen, the hand shows the share it is held at, over anything else
	if (const auto tip = Locator::villageTotemSystem::has_value()
	                         ? Locator::villageTotemSystem::value().TakeShareToolTip(LocalInputPlayer())
	                         : std::nullopt;
	    tip.has_value() && Locator::entitiesRegistry::value().Valid(tip->totem) &&
	    OnScreen(ecs::drawn_place::DrawnTransformOf(
	                 Locator::entitiesRegistry::value(), tip->totem,
	                 Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(tip->totem))
	                 .position)
	        .has_value())
	{
		toolTips.Force(ecs::village_totem::k_ShareToolTip, tip->percent);
	}
	// Let go of the creature having told it next to nothing, the hand offers to focus on it, over anything else
	if (Locator::creatureHandSystem::value().TakeFocusTip())
	{
		toolTips.Force(creature_leash_orders::k_FocusCreatureTip, 1.0f);
	}
	// As the hand takes food or wood from a pile, it shows the amount it holds, over anything else
	if (Locator::handGrabSystem::has_value())
	{
		if (const auto amount = Locator::handGrabSystem::value().TakeAmountToolTip(); amount.has_value())
		{
			const auto held = gui::world_tooltip::HeldAmount(*amount);
			toolTips.SubmitNumbered(held.index, gui::ToolTipAction::None, held.first, std::nullopt, true);
		}
	}
	const auto pointed = Locator::pickingSystem::has_value() ? Locator::pickingSystem::value().GetPick().object : std::nullopt;
	const bool shown =
	    !Locator::gameInterface::value().IsDialogOpen() && Locator::cinematicDirectorSystem::value().IsInterfaceActive();
	if (shown)
	{
		SubmitHandToolTips(toolTips, pointed);
	}
	toolTips.ProcessTurn(displayTimeRuns);
}

void Game::SubmitHandToolTips(gui::ToolTips& toolTips, std::optional<entt::entity> pointed)
{
	namespace tips = gui::hand_tooltip;
	const auto submitAll = [&toolTips](const std::vector<tips::Offer>& offers) {
		for (const auto& offer : offers)
		{
			toolTips.Submit(offer.index, offer.action, offer.arrows);
		}
	};
	// Over the land, or over a thing that the hand can do nothing with, the hand shows how a drag moves the camera. When
	// the middle of the screen shows the land, the help counts it.
	const auto overNothing = [&submitAll]() {
		const auto camera = tips::CameraNow();
		const bool overFish = HandOverFish();
		if (const auto event = tips::OverNothingHelpEvent(camera, overFish);
		    event.has_value() && Locator::helpProfileSystem::has_value())
		{
			Locator::helpProfileSystem::value().Report(*event);
		}
		submitAll({tips::OverNothing(camera, overFish)});
	};
	// What a tap does with the leash. Each time the hand offers to focus on the creature, the help counts it.
	const auto leashTip = [pointed]() {
		const auto tip = Locator::leashSystem::value().ToolTip(LocalInputPlayer(), pointed);
		if (tip == creature_leash_orders::k_FocusCreatureTip && Locator::helpProfileSystem::has_value())
		{
			Locator::helpProfileSystem::value().Report(help::profile::k_FocusCreatureTip);
		}
		return tip;
	};
	// While the hand grips the land, it shows "Zoom" when the Action button is held too. In other conditions it shows
	// what a tap does with the leash, or how the drag moves the camera. The grip starts with a drag of the land and
	// continues until the Move button is let go. Both buttons together stop the drag but not the grip.
	const bool bothButtons = Locator::gameActionSystem::has_value() &&
	                         Locator::gameActionSystem::value().Get(input::UnbindableActionMap::TWO_BUTTON_CLICK);
	const bool moveHeld = bothButtons || (Locator::gameActionSystem::has_value() &&
	                                      Locator::gameActionSystem::value().Get(input::BindableActionMap::MOVE));
	_tipsLandGrip = input::land_grip::GripsLand(
	    Locator::camera::has_value() && Locator::camera::value().GetModel().GetHandCues().dragging, _tipsLandGrip, moveHeld);
	if (_tipsLandGrip)
	{
		// The Action button counts as held from its press until it is let go, by itself or with the Move button
		const bool actionHeld = bothButtons || (Locator::gameActionSystem::has_value() &&
		                                        Locator::gameActionSystem::value().Get(input::BindableActionMap::ACTION));
		const auto camera = tips::CameraNow();
		if (const auto zoom = tips::GrippingLand(actionHeld, camera); zoom.index == tips::k_Zoom)
		{
			submitAll({zoom});
			return;
		}
		if (const auto tip = leashTip())
		{
			toolTips.Submit(*tip, gui::ToolTipAction::Apply, gui::ToolTipArrows::k_None);
			return;
		}
		overNothing();
		return;
	}
	// With a miracle in the hand, poured or not, the hand offers to cast it where it may be cast: on the thing under the
	// pointer, or on the land where there is none. In other places it shows what it shows over the land.
	const auto held = Locator::magicSystem::has_value() ? Locator::magicSystem::value().HeldCastAt(pointed) : std::nullopt;
	if (held.has_value())
	{
		if (const auto tip = gui::world_tooltip::HeldMiracle(magic::OfferFor(*held)); tip.has_value())
		{
			toolTips.Submit(*tip, gui::ToolTipAction::Apply, gui::ToolTipArrows::k_None);
			return;
		}
	}
	// With the leash held, the hand says what a tap of the Action button does with it, before anything else
	if (const auto tip = leashTip())
	{
		toolTips.Submit(*tip, gui::ToolTipAction::Apply, gui::ToolTipArrows::k_None);
		return;
	}
	if (held.has_value())
	{
		overNothing();
		return;
	}
	// While the player's creature duels, the hand offers to block over it, to attack over its opponent, and to manoeuvre
	// anywhere else, each by the Action button
	if (const auto tip = Locator::creatureFightSystem::value().HandTip(_creatureUnderHand))
	{
		toolTips.Submit(creature_fight::ToolTipIndex(*tip), gui::ToolTipAction::Apply, gui::ToolTipArrows::k_None);
		return;
	}
	// While the hand holds a thing, it offers to give it, to put it into a store, or to drop and throw it. A hand that
	// holds the button down to throw offers nothing.
	if (Locator::handGrabSystem::has_value())
	{
		const auto& grab = Locator::handGrabSystem::value();
		if (const auto thing = grab.GetHeld(); thing.has_value())
		{
			if (grab.IsAboutToThrow())
			{
				return;
			}
			if (const auto offers = tips::Holding(HandInInfluence(), tips::UseAt(*thing, pointed), tips::DropOf(*thing));
			    offers.has_value())
			{
				submitAll(*offers);
				return;
			}
			overNothing();
			return;
		}
	}
	// Over the player's own creature, the hand shows that it can take hold of it to stroke or slap it. It can hold other
	// players' creatures too, but the game only offers it for the player's own.
	const auto over = _creatureUnderHand.has_value() ? _creatureUnderHand : Locator::creatureHandSystem::value().GetCreature();
	if (over.has_value())
	{
		// Within the influence, over a creature that takes hold, the mouse shows the Action button; in other places the
		// tooltip has no mouse
		const auto tip = Locator::creatureHandSystem::has_value()
		                     ? Locator::creatureHandSystem::value().InteractTipOver(*over, HandInInfluence())
		                     : creature_hand::InteractTip::None;
		if (tip != creature_hand::InteractTip::None)
		{
			toolTips.Submit(creature_panel::k_InteractToolTip,
			                tip == creature_hand::InteractTip::WithAction ? gui::ToolTipAction::Apply
			                                                              : gui::ToolTipAction::None,
			                gui::ToolTipArrows::k_None);
			return;
		}
		overNothing();
		return;
	}
	// While the hand grips the tip bubble of a sign, or the pointer is on the bubble and on nothing else, the hand offers
	// to scroll its words
	if (Locator::tipBubbleSystem::has_value())
	{
		const auto& bubble = Locator::tipBubbleSystem::value();
		if (bubble.IsGripped() || (!pointed.has_value() && bubble.IsUnderHand()))
		{
			submitAll({tips::BubbleOffer()});
			return;
		}
	}
	if (!pointed.has_value() || !tips::IsInteractable(*pointed))
	{
		overNothing();
		return;
	}
	// Over a building that is not fully built or not fully repaired, the wood that it wants and how far it is built take
	// turns, wherever the hand is
	if (const auto building = tips::BuildingOf(*pointed); building.has_value())
	{
		if (building->wood.words == tips::k_WoodRequired)
		{
			toolTips.SubmitNumbered(tips::k_WoodRequired, gui::ToolTipAction::None, building->wood.number, std::nullopt);
		}
		else
		{
			toolTips.SubmitInWords(tips::k_WoodRequired, building->wood.words, static_cast<float>(building->wood.number));
		}
		toolTips.SubmitNumbered(tips::k_BuildingCompleted, gui::ToolTipAction::None, building->completed, std::nullopt);
		return;
	}
	const bool inInfluence = HandInInfluence();
	if (const auto flagTip = Locator::townDesireSystem::value().GetFlagToolTip(*pointed);
	    flagTip.has_value() && gui::world_tooltip::Shows(gui::world_tooltip::Subject::DesireFlag, inInfluence))
	{
		// Over a flag of a town's store, the hand shows how much the town wants its thing, in the flag's own words
		toolTips.SubmitInWords(ecs::town_desire_flags::k_NeedToolTip, flagTip->words, flagTip->percent);
		return;
	}
	// Over a temple, a village centre, a store, an altar or a pile, the hand says what it holds or can do
	if (const auto submissions = gui::world_tooltip::Over(*pointed, inInfluence); !submissions.empty())
	{
		for (const auto& submission : submissions)
		{
			if (submission.wordsOnly)
			{
				toolTips.Submit(submission.numbers.index, submission.action, gui::ToolTipArrows::k_None, submission.force);
			}
			else
			{
				toolTips.SubmitNumbered(submission.numbers.index, submission.action, submission.numbers.first,
				                        submission.numbers.second, submission.force);
			}
		}
		return;
	}
	// Within the influence, the way into a temple that is not the player's offers nothing
	if (inInfluence && Locator::entitiesRegistry::value().AllOf<ecs::components::TempleEntrance>(*pointed))
	{
		return;
	}
	// A scroll that a script put in the world offers to start it, or to remind the player of it, anywhere
	if (const auto scroll = tips::ScrollOf(*pointed); scroll.has_value())
	{
		submitAll({*scroll});
		return;
	}
	// The rest needs the hand in the player's influence
	if (!inInfluence)
	{
		overNothing();
		return;
	}
	// Over a miracle icon, whoever's it is, the hand names its miracle at once. A worship site's icon also shows how full
	// its miracle is.
	if (const auto icon = tips::SpellIconOf(*pointed); icon.has_value())
	{
		const auto charge =
		    Locator::spellIconSystem::has_value() ? Locator::spellIconSystem::value().ShownCharge(*pointed) : std::nullopt;
		toolTips.ForceWithCharge(icon->index, charge);
		return;
	}
	// Over a one-shot globe at rest, the hand names its miracle at once. A globe in flight is caught as any thing is.
	if (const auto globe = tips::GlobeOf(*pointed); globe.has_value())
	{
		if (tips::IsMoving(*pointed))
		{
			submitAll(tips::OverTakeable(true, std::nullopt));
		}
		else
		{
			toolTips.ForceWithCharge(globe->index, std::nullopt);
		}
		return;
	}
	// Over a thing that the hand can take: "Catch" while it moves, else "Pick Up" and what a tap does to it. Over a
	// scaffold, the hand shows how many scaffolds it is worth in place of "Pick Up".
	if (Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().WhyNotTake(*pointed).empty())
	{
		const bool moving = tips::IsMoving(*pointed);
		auto offers = tips::OverTakeable(moving, tips::TakeableTapOf(*pointed));
		if (const auto value = tips::ScaffoldValueOf(*pointed); value.has_value() && !moving)
		{
			toolTips.SubmitNumbered(gui::world_tooltip::k_ScaffoldValue, gui::ToolTipAction::Apply, *value, std::nullopt);
			offers.erase(offers.begin());
		}
		submitAll(offers);
		return;
	}
	if (const auto tap = tips::TapOf(*pointed); tap.has_value())
	{
		submitAll({*tap});
		return;
	}
	overNothing();
}

void Game::UpdateHandFrame(std::chrono::microseconds deltaTime)
{
	// The draw of the hand sees the start and the end of a hold of a creature before the hand moves. First the camera of
	// the hold turns and the hand takes its pose.
	if (Locator::creatureHandSystem::has_value())
	{
		Locator::creatureHandSystem::value().HoldFrame(Locator::time::value().GetInputSteps());
		Locator::creatureHandSystem::value().DrawFrame(Locator::time::value().GetInputSteps());
	}
	auto& profiler = Locator::profiler::value();
	auto& camera = Locator::camera::value();
	const auto& config = Locator::config::value();
	auto& clock = Locator::time::value();
	const auto gameTime = std::chrono::duration<float, std::milli>(clock.GetFrameGameTime());
	// Update Uniforms
	{
		auto profilerScopedUpdateUniforms = profiler.BeginScoped(Profiler::Stage::UpdateUniforms);

		// The hand follows the pointer through the view that is seen: an inspector's override while one is shown
		ShowInspectorOverride(true);
		// Update Hand and intersection point
		// Upright where nothing is under the cursor
		ecs::components::Transform intersectionTransform {
		    .position = glm::vec3(0.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)};
		// The point the interface picks under the cursor, which the hand's influence is tested at
		std::optional<map_coords::MapCoords> handPick;
		UpdateHandCursor(deltaTime, profiler, camera, intersectionTransform, handPick);

		// The hand's debugging trace records this frame, starting from where the hand was left before it
		if (auto* trace = Locator::handSystem::value().GetTrace(); trace != nullptr && trace->Enabled())
		{
			trace->BeginFrame(_frameCount, std::chrono::duration<float>(deltaTime).count());
			const auto handEntity =
			    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
			if (const auto* transform = Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(handEntity))
			{
				trace->Note("frame start", transform->position);
			}
		}

		// Update Hand
		UpdateDrawnHand(deltaTime, profiler, camera, gameTime, intersectionTransform);

		// The leashes' ropes swing from where the hand now is
		{
			auto creatureLeash = profiler.BeginScoped(Profiler::Stage::CreatureLeashUpdate);
			Locator::leashSystem::value().Update(std::chrono::duration<float>(gameTime).count());
		}

		// Holding a miracle's seed, the hand takes the still pose of its hold, sways with the seed and tips with a pour. A
		// thing it picked up it holds the same way.
		const auto heldSeed = magic::HandHoldPoser::Find();
		auto heldThing = heldSeed;
		if (!heldThing.has_value() && Locator::handGrabSystem::has_value())
		{
			if (const auto held = Locator::handGrabSystem::value().GetHeldPose())
			{
				heldThing = magic::HandHoldPoser::HeldSeed {
				    .entity = held->object,
				    .hold = held->hold,
				    .hang = held->hang,
				    .reach = held->reach,
				    .yRotate = 0.0f,
				    .effectInFingers = false,
				};
			}
		}
		bool holdingSeed = false;
		PoseHeldThing(deltaTime, camera, config, heldSeed, heldThing, holdingSeed);
		// What the hands of the other players of a network game hold moves toward their reported places
		if (Locator::handGrabSystem::has_value())
		{
			Locator::handGrabSystem::value().PlaceOthersHeldForFrame(
			    static_cast<int32_t>(Locator::time::value().GetFrameGameTime().count()));
			// A thing that this computer threw shows on the flight of its lead until the turn of the command
			Locator::handGrabSystem::value().DrawThrownForFrame();
		}
		// The phantom buildings of the scaffolds: at the drawn hand of the player, and on the building sites
		if (Locator::workshopSystem::has_value())
		{
			const auto handEntity =
			    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
			const auto* handTransform = Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(handEntity);
			Locator::workshopSystem::value().PlaceGhostsForFrame(
			    handTransform != nullptr ? std::optional(handTransform->position) : std::nullopt);
		}
		{
			// The globes show their miracles where they are now, a globe in the hand where the hand holds it, each turned
			// to face the camera as it is drawn
			auto miracleGlobes = profiler.BeginScoped(Profiler::Stage::MiracleGlobesUpdate);
			Locator::miracleFxSystem::value().UpdateGlobes(Locator::time::value().GetFrameRealTime(),
			                                               Locator::time::value().GetFrameGameTime(), simulation::LandDrawn());
		}

		// The animation of the hand in this frame, and what is below the hand, for the blend of the drawn hand
		auto handCycle = HandAnimation::Cycle::Wiggle;
		bool nothingUnderHand = true;
		bool gripsTotem = false;
		bool pulls = false;
		// Animate the hand: gripping while it drags the land, otherwise its normal pose. Turning the camera with the
		// middle button leaves the hand idle.
		AnimateHand(deltaTime, camera, config, heldThing, holdingSeed, handCycle, nothingUnderHand, gripsTotem, pulls);
		// What the hand does in this frame, which starts its blends and decides whether the camera's icons show
		const auto mode = hand_blend::ModeOf({
		    .shown = IsHandDrawn(),
		    .insideTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
		    .holds = heldThing.has_value(),
		    .holdsSeed = heldSeed.has_value(),
		    .pulls = pulls,
		    .knocks = _handKnocking,
		    .gripsTotem = gripsTotem,
		    .touchesCreature = _handOnCreature.has_value(),
		    .movesCamera = _handCameraState,
		});
		// The camera's helper icons by the hand, where it is drawn this frame, now that it is posed
		UpdateHandTricons(IsHandDrawn(), std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count(), mode);
		// The tooltip by the drawn hand too, which stays with it while it is held still and the pointer moves on
		UpdateHandToolTipAnchor();
		// Taking or letting go of a seed cross-fades the drawn hand, and the seed's in-hand effect sits in its fingers
		BlendHeldSeed(camera, gameTime, heldSeed, heldThing, handCycle, nothingUnderHand, mode);

		// The hand shows its player's alignment, catching up with it each frame once it has moved far enough. Its skin is
		// blended again as it goes into or out of the player's influence, tested where the cursor picks the land. The
		// point is held while the hand grips the land, and in the temple.
		// TODO(raffclar): what the game picks in the temple, which needs it run under a debugger
		{
			const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
			Locator::handSystem::value().UpdateAlignmentMorph(handPick, HandButtons().gripping || inTemple);
		}

		// The trees bend away from where the hand now is, and rustle
		{
			auto actions = profiler.BeginScoped(Profiler::Stage::VegetationUpdate);
			auto& vegetation = Locator::vegetation::value();
			vegetation.UpdateBendPoints(IsHandDrawn());
			vegetation.Rustle(gameTime);
		}

		ShowInspectorOverride(false);
	}
}

void Game::UpdateHandCursor(std::chrono::microseconds deltaTime, Profiler& profiler, Camera& camera,
                            ecs::components::Transform& intersectionTransform, std::optional<map_coords::MapCoords>& handPick)
{
	const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
	const auto scale = glm::vec3(50.0f, 50.0f, 50.0f);
	if (screenSize.x > 0 && screenSize.y > 0)
	{
		glm::vec3 rayOrigin;
		glm::vec3 rayDirection;
		camera.DeprojectScreenToWorld(static_cast<glm::vec2>(HandCursor()) / static_cast<glm::vec2>(screenSize), rayOrigin,
		                              rayDirection);

		_cursorWorldPosition.reset();
		_cursorLand.reset();
		_handRestOnThing.reset();
		// The hand that goes into the temple starts its distance again from where it is, with no speed, and
		// stands straight up. The hand that comes out of the temple does the same, and ignores the last sparkle
		// of a fight.
		if (const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active(); inTemple != _handInTemple)
		{
			_handInTemple = inTemple;
			_handHoverZoomer.Reset(_handDistance);
			if (inTemple)
			{
				_handTempleNormal.Reset(glm::vec3(0.0f, 1.0f, 0.0f));
			}
			else
			{
				_handUp.Reset(glm::vec3(0.0f, 1.0f, 0.0f));
				_handDropsSparkle = true;
			}
		}
		if (Locator::temple::has_value() && Locator::temple::value().Active())
		{
			// In the temple, the hand goes where the cursor meets the room, turning to its surface
			if (const auto hit = Locator::temple::value().GetCursorHit())
			{
				_handTempleNormal.SetDestination(hit->normal, k_HandTempleTurnTime);
				StepEase(_handTempleNormal);
				const auto normal = _handTempleNormal.GetValue();
				_cursorWorldPosition = hit->point;
				intersectionTransform.position = hit->point;
				intersectionTransform.rotation =
				    glm::length(normal) > 0.0f
				        ? glm::mat3_cast(glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f), glm::normalize(normal)))
				        : glm::mat3(1.0f);
			}
		}
		else if (!glm::any(glm::isnan(rayOrigin) || glm::isnan(rayDirection)))
		{
			// The leash keys, and the Action button tapping leash posts, and with the leash held giving orders and
			// tying the leash to things.
			// Not while the debug windows have the keyboard or mouse, as when typing in a text field: the controls
			// aren't updated then, so a key just pressed would read as pressed again every frame.
			if (!Locator::debugGui::value().StealsFocus())
			{
				auto& leashes = Locator::leashSystem::value();
				leashes.HandleInput(rayOrigin, rayDirection, Locator::handPressSystem::value().TakeActionPress());
			}
			// The gestures drawn with the hand: circles and power-ups for the miracles, the leash's gestures, and
			// the scribble that shakes off what the hand holds
			{
				auto gestures = profiler.BeginScoped(Profiler::Stage::GestureUpdate);
				UpdateGestures(camera, screenSize, std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
			}
			// What the interface picked under the cursor as the last frame was drawn: the object nearest the camera, or
			// else the land or the sea. The hand's influence is tested at that point.
			const auto& picking = Locator::pickingSystem::value();
			const auto& pick = picking.GetPick();
			_cursorOnObject = pick.object.has_value();
			if (pick.point.has_value())
			{
				handPick = map_coords::FromMetres({pick.point->x, pick.point->z});
			}
			// The hand rests over what was picked: before a villager or an animal, on a model it feels the triangles
			// of, or else on the land, turned to its slope
			const auto [eye, nearPoint] =
			    camera.OnNearPlane(static_cast<glm::vec2>(HandCursor()) / static_cast<glm::vec2>(screenSize));
			const auto cursorDirection =
			    hand_feel::CursorDirection(eye, nearPoint, hand_feel::HoverLimit(HandHoverInputs(eye)));
			_handSurfaceUp.reset();
			_handOverObject = false;
			_handRestNearer = 0.0f;
			_handOverSpellIcon = false;
			// Over the help's text bubble or the leash's rope, the hand stands straight up; over the bubble it
			// eases to the bubble's distance. While the hand holds the rope, the rope counts as under the cursor.
			const bool rope =
			    pick.rope || Locator::leashSystem::value().RopeHand(LocalInputPlayer()) == leash_snap::Hand::Holding;
			_handStandsStraight = !pick.object.has_value() && (pick.textBubble || rope);
			_handBubbleDistance =
			    !pick.object.has_value() && pick.textBubble && !rope ? std::optional(pick.distance) : std::nullopt;
			// Only the empty hand offers to turn the camera
			_handMayOfferTurn = !pick.object.has_value() && !pick.textBubble && !rope &&
			                    !Locator::handGrabSystem::value().GetHeld().has_value();
			std::optional<glm::vec3> rest;
			if (pick.object.has_value())
			{
				const auto& registry = Locator::entitiesRegistry::value();
				const auto object = *pick.object;
				const auto& grab = Locator::handGrabSystem::value();
				const auto held = grab.GetHeld();
				const auto* mesh = registry.TryGet<const ecs::components::Mesh>(object);
				auto& meshes = Locator::resources::value().GetMeshes();
				const bool posed = mesh != nullptr && meshes.Contains(mesh->id) && meshes.Handle(mesh->id)->IsBoned();
				const auto feel = hand_feel::FeelOf({
				    .living =
				        registry.AnyOf<ecs::components::Villager, ecs::components::Animal, ecs::components::Creature>(object),
				    .creature = registry.AllOf<ecs::components::Creature>(object),
				    .posedModel = posed,
				    .animatedStatic = registry.AllOf<ecs::components::AnimatedStatic>(object),
				    .ignored = registry.AllOf<ecs::components::CarriedByTornado>(object) || held == object,
				    .spellIcon = registry.AllOf<ecs::components::IconSeed>(object),
				});
				// Holding something, the hand feels along the line to the cursor's point on the land, raised by
				// its lift for what it holds
				std::optional<hand_feel::RaisedGround> raisedGround;
				if (held.has_value())
				{
					raisedGround =
					    hand_feel::RaisedGround {.ground = pick.land.value_or(_handPosition), .lift = grab.GetCursorRaise()};
				}
				const auto feltDirection = hand_feel::FeelDirection(eye, cursorDirection, raisedGround);
				if (feel == hand_feel::Feel::AtIcon)
				{
					// The hand rests nearer than a miracle's icon by its own height, turned to the land
					rest = registry.Get<const ecs::components::Transform>(object).position;
					_handRestNearer = hand_feel::k_IconGap;
					_handOverSpellIcon = true;
				}
				else if (feel == hand_feel::Feel::AtPosition)
				{
					rest = hand_feel::RestAtPosition(eye, nearPoint, feltDirection,
					                                 registry.Get<const ecs::components::Transform>(object).position,
					                                 ecs::systems::object_measures::TwoDRadius(registry, object));
					_handOverObject = true;
				}
				else if (feel == hand_feel::Feel::OnModel)
				{
					_handOverObject = true;
					if (const auto felt = picking.FeelModel(object, eye, feltDirection))
					{
						std::optional<hand_feel::Holding> holding;
						if (held.has_value())
						{
							holding = hand_feel::Holding {
							    .radius = ecs::systems::object_measures::TwoDRadius(registry, *held),
							    .creature = std::nullopt,
							};
							if (registry.AllOf<ecs::components::Creature>(object))
							{
								const auto& at = registry.Get<const ecs::components::Transform>(object).position;
								holding->creature = hand_feel::CreatureReach {
								    .centre = {at.x, at.z},
								    .radius = ecs::systems::object_measures::TwoDRadius(registry, object),
								};
							}
						}
						const auto onModel = hand_feel::RestOnModel(eye, cursorDirection, feltDirection, felt->point,
						                                            felt->normal, FeelsModel(object), holding);
						rest = onModel.point;
						_handSurfaceUp = onModel.up;
					}
				}
			}
			_cursorLand = pick.land;
			_handRestOnThing = rest;
			// The cursor's point at the furthest the hand may be this frame, which it stands towards the camera's
			// focus from
			_handLimitPoint = hand_feel::PointAlongCursor(eye, nearPoint, hand_feel::HoverLimit(HandHoverInputs(eye)));
			const auto restPoint = rest.has_value() ? rest : pick.point;
			if (restPoint.has_value())
			{
				const auto up = _handSurfaceUp.has_value()
				                    ? glm::normalize(*_handSurfaceUp)
				                    : Locator::terrainSystem::value().GetNormalAt({restPoint->x, restPoint->z});
				intersectionTransform.position = *restPoint;
				intersectionTransform.rotation = glm::mat3_cast(glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f), up));
				_cursorWorldPosition = *restPoint;
			}
		}
		intersectionTransform.scale = scale;
	}
}

void Game::UpdateDrawnHand(std::chrono::microseconds deltaTime, Profiler& profiler, Camera& camera,
                           const std::chrono::duration<float, std::milli>& gameTime,
                           const ecs::components::Transform& intersectionTransform)
{
	// Put away, it stays where it was and does nothing until it comes back. The creature's panel is wanted afresh
	// every frame, so with the hand away (a cut scene, a menu) it isn't left up from before
	if (!IsHandDrawn() && Locator::gameInterface::has_value())
	{
		Locator::gameInterface::value().SetCreaturePanel(std::nullopt);
	}
	if (IsHandDrawn())
	{
		const glm::mat4 modelRotationCorrection = glm::eulerAngleX(glm::radians(90.0f));

		const auto handEntity =
		    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
		auto& handTransform = Locator::entitiesRegistry::value().Get<ecs::components::Transform>(handEntity);
		if (!_stillHandAt.has_value())
		{
			_stillHandAt = handTransform.position;
		}
		UpdateHandNavigation(handTransform);
		UpdateHandKnock(handTransform);
		if (Locator::temple::has_value() && Locator::temple::value().Active())
		{
			if (!HandButtons().gripping)
			{
				handTransform.rotation = glm::eulerAngleY(camera.GetRotation().y) * modelRotationCorrection;
				handTransform.rotation = intersectionTransform.rotation * handTransform.rotation;
			}
		}
		else
		{
			OrientHand(handTransform, glm::mat3(glm::eulerAngleY(camera.GetRotation().y) * modelRotationCorrection),
			           _handSurfaceUp.value_or(glm::vec3(0.0f, 1.0f, 0.0f)));
		}
		PlaceHand(handTransform, std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
		TraceHand("navigation", handTransform.position);
		// A tool's run can keep the hand where it started, so that the hand gives no input
		if (Locator::simulation::value().GetSettings().stillHand)
		{
			handTransform.position = _stillHandAt.value_or(handTransform.position);
		}
		// Held to a creature, the hand rests on its body under the cursor, stroking and slapping it
		{
			auto creatureHand = profiler.BeginScoped(Profiler::Stage::CreatureHandUpdate);
			const auto screenSize =
			    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
			_handOnCreature.reset();
			_creatureUnderHand.reset();
			if (screenSize.x > 0 && screenSize.y > 0)
			{
				auto& hands = Locator::creatureHandSystem::value();
				glm::vec3 rayOrigin;
				glm::vec3 rayDirection;
				camera.DeprojectScreenToWorld(static_cast<glm::vec2>(HandCursor()) / static_cast<glm::vec2>(screenSize),
				                              rayOrigin, rayDirection);
				// The hand isn't over the world while it is over a debug window, or in the temple
				const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
				if (!inTemple && !Locator::debugGui::value().IsMouseOverWindow())
				{
					_creatureUnderHand = hands.CreatureUnderCursor();
				}
				if (hands.GetCreature().has_value())
				{
					_handOnCreature = hands.Update(rayOrigin, rayDirection, static_cast<glm::vec2>(HandCursor()),
					                               static_cast<float>(screenSize.x), Locator::time::value().GetInputSteps());
				}
				if (_handOnCreature.has_value())
				{
					handTransform.position = _handOnCreature->position;
					// Its back turns to the body it rests on, or stands it upright beside it
					handTransform.rotation =
					    hand_orientation::StandOnSlope(_handOnLevelLand, _handHeading, _handOnCreature->up);
					TraceHand("on creature", handTransform.position);
				}
			}
			UpdateHandInterface();
		}
		{
			auto magic = profiler.BeginScoped(Profiler::Stage::MagicUpdate);
			// Food and wood pouring from the hand lift it and tip it forward, and what it pours comes from there
			const auto pour = Locator::magicSystem::value().GetHandPour(Locator::time::value().GetTurnFraction());
			// The hand is drawn where a scenario puts it, and stays where a pour that holds it began
			const auto driven = Locator::magicSystem::value().GetDrivenHand();
			if (driven.has_value())
			{
				handTransform.position = driven->handPosition;
			}
			handTransform.position = pour.pinned.value_or(handTransform.position);
			handTransform.position.y += pour.raise;
			TraceHand("scenario or pour", handTransform.position);
			// A seed in the hand lifts it, by how it is held; the pour tips it as the hand is posed
			// (magic::HandHoldPoser)
			if (const auto held = magic::HandHoldPoser::Find())
			{
				// Measured to the land under the cursor, or under the hand a scenario puts
				const auto land = driven.has_value() ? driven->point : _cursorWorldPosition;
				handTransform.position.y += magic::HandHoldPoser::Lift(
				    *held, x87::Distance<x87::SumOrder::Zyx>(camera.GetOrigin(), land.value_or(handTransform.position)));
				TraceHand("seed lift", handTransform.position);
			}
			else if (const auto screenSize =
			             Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
			         Locator::handGrabSystem::has_value() && screenSize.x > 0 && screenSize.y > 0)
			{
				// A thing the hand takes or holds lifts it, and makes ready to throw, its spring drags it
				glm::vec3 rayOrigin;
				glm::vec3 rayDirection;
				camera.DeprojectScreenToWorld(static_cast<glm::vec2>(HandCursor()) / static_cast<glm::vec2>(screenSize),
				                              rayOrigin, rayDirection);
				const auto land = _cursorWorldPosition.value_or(handTransform.position);
				handTransform.position = Locator::handGrabSystem::value().UpdateFrame({
				    .target = handTransform.position,
				    .rayOrigin = rayOrigin,
				    .rayDirection = rayDirection,
				    .camera = camera.GetOrigin(),
				    .cursorGround = _cursorWorldPosition,
				    .handSize = HandAnimation::SizeAtDistance(x87::Distance<x87::SumOrder::Zyx>(camera.GetOrigin(), land)),
				    .seconds = std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count(),
				    .gameMs = static_cast<uint32_t>(std::lround(gameTime.count())),
				    .inputSteps = Locator::time::value().GetInputSteps(),
				    .nowMs = machine_clock::Ticks(),
				    .turn = Locator::time::value().GetTurn(),
				});
				TraceHand("grab", handTransform.position);
			}
			UpdateMagicHand(handTransform.position,
			                std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
		}
		// Holding a town's totem, the mouse slides it up and down and the hand stays on its icon
		// A puzzle totem too
		const bool townTotem =
		    Locator::villageTotemSystem::has_value() && Locator::villageTotemSystem::value().GetGripped().has_value();
		const bool puzzleTotem =
		    Locator::puzzleTotemSystem::has_value() && Locator::puzzleTotemSystem::value().GetGripped().has_value();
		if (townTotem || puzzleTotem)
		{
			const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(0);
			// The pointer is put back on the hand every frame, so how far it went from there is how far the mouse
			// moved
			auto& handPress = Locator::handPressSystem::value();
			const auto& totemPointer = handPress.GetTotemPointer();
			const int up = totemPointer.has_value() ? totemPointer->y - handPress.GetMousePosition().y : 0;
			std::optional<ecs::systems::VillageTotemSystemInterface::HandHold> hold;
			if (townTotem)
			{
				auto& totems = Locator::villageTotemSystem::value();
				totems.Slide(static_cast<float>(up), static_cast<float>(screenSize.y));
				hold = totems.GetHandHold();
			}
			else
			{
				auto& totems = Locator::puzzleTotemSystem::value();
				totems.Slide(static_cast<float>(up), static_cast<float>(screenSize.y));
				hold = totems.GetHandHold();
			}
			if (hold.has_value())
			{
				handTransform.position = hold->position;
				TraceHand("totem", handTransform.position);
				// It faces the way it always does, tipped forwards over the icon
				handTransform.rotation =
				    hand_orientation::TippedHand(hand_orientation::TippedRows(_handHeadingAngle, hold->tilt));
				// The pointer goes where the hand is on the screen, while it is on it
				glm::vec3 screen;
				if (camera.ProjectWorldToScreen(hold->position, glm::vec4(0.0f, 0.0f, glm::vec2(screenSize)), screen))
				{
					const glm::ivec2 at {std::lrint(screen.x), std::lrint(screen.y)};
					if (at.x >= 0 && at.y >= 0 && at.x < screenSize.x && at.y < screenSize.y)
					{
						// Only put when it isn't there already
						if (at != handPress.GetMousePosition())
						{
							Locator::gameActionSystem::value().WarpCursor(at);
							handPress.MovePointer(at);
						}
						handPress.SetTotemPointer(at);
					}
				}
			}
		}
	}
	// A fireball pressed long enough is taken, which catches it
	Locator::magicSystem::value().HoldFireBall(machine_clock::Ticks(), Locator::time::value().GetTurn());
	// A press on a creature that waits for a tap and lasts long enough sends the hold
	if (Locator::creatureHandSystem::has_value())
	{
		Locator::creatureHandSystem::value().WaitFrame(machine_clock::Ticks(), Locator::time::value().GetTurn());
	}
	{
		// The hand and the piles show their miracles
		auto miracleFx = profiler.BeginScoped(Profiler::Stage::MiracleFxUpdate);
		Locator::miracleFxSystem::value().Update(std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count(),
		                                         std::chrono::duration<float>(gameTime).count());
	}
	Locator::entitiesRegistry::value().SetDirty();
}

void Game::PoseHeldThing(std::chrono::microseconds deltaTime, const Camera& camera, const EngineConfig& config,
                         const std::optional<magic::HandHoldPoser::HeldSeed>& heldSeed,
                         const std::optional<magic::HandHoldPoser::HeldSeed>& heldThing, bool& holdingSeed)
{
	const auto handEntity =
	    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	if (auto* handTransform = Locator::entitiesRegistry::value().TryGet<ecs::components::Transform>(handEntity))
	{
		const magic::HandHoldPoser::Frame holdFrame {
		    .dt = deltaTime,
		    .cursor = HandCursor(),
		    .camera = camera.GetOrigin(),
		    .heading = _handHeadingAngle,
		    .modelCorrection = glm::mat3(glm::eulerAngleX(glm::radians(90.0f))),
		    // A pour tips the hand; offering what it holds to a creature turns it over too
		    .tilt = Locator::magicSystem::value().GetHandPour(Locator::time::value().GetTurnFraction()).tilt +
		            (heldSeed.has_value() || !Locator::handGrabSystem::has_value()
		                 ? 0.0f
		                 : Locator::handGrabSystem::value().GetOfferTurn()),
		    // A thing the hand picked up isn't turned about for a right hand as a seed is
		    .handScale = HandDrawnScale(camera.GetOrigin(), handTransform->position, heldThing.has_value()),
		    .rightHanded = heldSeed.has_value() && config.rightHandedHand,
		};
		holdingSeed = _handHold.Pose(heldThing, holdFrame, _handAnimation.get(), *handTransform);
	}
}

void Game::AnimateHand(std::chrono::microseconds deltaTime, const Camera& camera, const EngineConfig& config,
                       const std::optional<magic::HandHoldPoser::HeldSeed>& heldThing, bool holdingSeed,
                       HandAnimation::Cycle& handCycle, bool& nothingUnderHand, bool& gripsTotem, bool& pulls)
{
	if (_handAnimation)
	{
		using HandState = HandAnimation::State;
		using HandCycle = HandAnimation::Cycle;
		using hand_navigation_pose::Pose;
		// Held to a creature the hand is in its own state, which plays its cycle without leaning
		const auto state = _handOnCreature.has_value() ? HandState::Creature
		                   : _handCameraState          ? HandState::Camera
		                                               : HandState::Normal;
		// Over the rope of the tied leash the fingers open to pinch it, and holding it they pinch it
		const auto ropeHand = Locator::leashSystem::value().RopeHand(LocalInputPlayer());
		const auto cycle = hand_cycle::Choose({
		    .pose = _handPose,
		    .cameraState = _handCameraState,
		    .overThing = HandOverThing(),
		    .overSpellIcon = _handOverSpellIcon,
		    .rope = ropeHand,
		    .creature = _handOnCreature.has_value() ? std::optional(_handOnCreature->cycle) : std::nullopt,
		});
		// Pulling a thing free, the hand takes the pose the thing is held with
		std::optional<ecs::systems::HandGrabSystemInterface::PullPose> pull;
		if (!holdingSeed && Locator::handGrabSystem::has_value())
		{
			pull = Locator::handGrabSystem::value().GetPullPose();
		}
		const auto pullCycle = pull.has_value() ? magic::hand_hold::HoldCycle(pull->hold) : std::nullopt;
		const auto* pullClip = pullCycle.has_value() ? _handAnimation->GetAnimation(static_cast<size_t>(*pullCycle)) : nullptr;
		// Holding a town's totem, the hand takes its side hold on the icon, closed by how wide the icon is
		auto totemHold =
		    Locator::villageTotemSystem::has_value() ? Locator::villageTotemSystem::value().GetHandHold() : std::nullopt;
		if (!totemHold.has_value() && Locator::puzzleTotemSystem::has_value())
		{
			totemHold = Locator::puzzleTotemSystem::value().GetHandHold();
		}
		handCycle = cycle;
		gripsTotem = totemHold.has_value();
		pulls = pull.has_value();
		nothingUnderHand =
		    ropeHand == leash_snap::Hand::Away &&
		    !(Locator::magicSystem::has_value() && Locator::magicSystem::value().OrbUnderHand().has_value()) &&
		    !(Locator::pickingSystem::has_value() && Locator::pickingSystem::value().GetPick().object.has_value());
		const auto* sideHold =
		    totemHold.has_value() ? _handAnimation->GetAnimation(static_cast<size_t>(HandCycle::HoldSide)) : nullptr;
		if (sideHold != nullptr)
		{
			_handAnimation->UpdateHeld(deltaTime, HandCycle::HoldSide,
			                           ecs::village_totem::GripTimeMs(totemHold->closure, sideHold->duration), HandCursor());
		}
		else if (pullClip != nullptr)
		{
			const float handSize = HandAnimation::SizeAtDistance(
			    magic::hand_hold::DistanceFromView(camera.GetOrigin(), _handPosition, heldThing.has_value()));
			_handAnimation->UpdateHeld(deltaTime, *pullCycle,
			                           magic::hand_hold::TugTimeMs(pull->hold, pullClip->duration, pull->reach, handSize),
			                           HandCursor());
		}
		else if (_handKnocking)
		{
			// Knocking on a house, the hand plays its tap once through, without leaning
			const auto* tap = _handAnimation->GetAnimation(static_cast<size_t>(HandCycle::TapHouse));
			const auto timeMs = _handKnock.Play(deltaTime, tap != nullptr ? tap->duration : 0);
			_handAnimation->UpdateHeld(deltaTime, HandCycle::TapHouse, timeMs, HandCursor());
			_handAnimation->SettleCursor(HandCursor());
		}
		else if (!holdingSeed)
		{
			_handAnimation->Update(deltaTime, state, cycle, HandCursor(), leash_snap::PinchShare(ropeHand));
		}

		const auto handEntity =
		    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
		auto& registry = Locator::entitiesRegistry::value();
		auto* hand = registry.TryGet<ecs::components::Hand>(handEntity);
		if (hand != nullptr)
		{
			hand->boneMatrices = _handAnimation->GetBoneMatrices();
		}
		// The hand keeps about the same size on screen however far away it is
		if (auto* handTransform = registry.TryGet<ecs::components::Transform>(handEntity))
		{
			// The game mirrors the mesh's left hand along its x axis to make a right hand
			const auto scale = HandDrawnScale(camera.GetOrigin(), handTransform->position, heldThing.has_value());
			handTransform->scale = glm::vec3(config.rightHandedHand ? -scale : scale, scale, scale);
			// Showing it turns the camera as it drags the land, the hand is drawn a third of its height higher, the
			// place it holds staying where it is
			if (hand != nullptr && _handCameraState && _handPose == Pose::Rotate &&
			    glm::determinant(handTransform->rotation) != 0.0f && scale != 0.0f)
			{
				const auto lift =
				    hand_navigation_pose::k_RotateLiftShare * k_HandHeight * HandAnimation::SizeAtDistance(_handDistance);
				const auto toModel =
				    glm::inverse(handTransform->rotation * glm::mat3(glm::scale(glm::mat4(1.0f), handTransform->scale)));
				const auto raise = glm::translate(glm::mat4(1.0f), toModel * glm::vec3(0.0f, lift, 0.0f));
				for (auto& bone : hand->boneMatrices)
				{
					bone = raise * bone;
				}
			}
		}
	}
}

void Game::BlendHeldSeed(const Camera& camera, const std::chrono::duration<float, std::milli>& gameTime,
                         const std::optional<magic::HandHoldPoser::HeldSeed>& heldSeed,
                         const std::optional<magic::HandHoldPoser::HeldSeed>& heldThing, HandAnimation::Cycle handCycle,
                         bool nothingUnderHand, hand_blend::HandMode mode)
{
	const auto handEntity =
	    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	if (auto* handTransform = Locator::entitiesRegistry::value().TryGet<ecs::components::Transform>(handEntity))
	{
		const float handSize = HandAnimation::SizeAtDistance(
		    magic::hand_hold::DistanceFromView(camera.GetOrigin(), handTransform->position, heldThing.has_value()));
		// Each change of what the hand does starts a blend of the drawn hand from how it looked before
		const auto* grab = Locator::handGrabSystem::has_value() ? &Locator::handGrabSystem::value() : nullptr;
		const auto changes = _handBlend.Step({
		    .mode = mode,
		    .cycle = handCycle,
		    .held = heldThing.has_value() ? std::optional(heldThing->entity) : std::nullopt,
		    .nothingUnderHand = nothingUnderHand,
		});
		const auto placed = handTransform->position;
		// The hand that goes into the normal mode starts its hover distance and its up again. When the frame
		// knows the mode only now, the reset applies from the next frame.
		if (changes.left.has_value() && hand_blend::ResetsAfterPlacing(*changes.left, mode))
		{
			_handHoverZoomer.Reset(_handDistance);
			_handUp.Reset(glm::vec3(0.0f, 1.0f, 0.0f));
			_handDropsSparkle = true;
		}
		if (auto* hand = Locator::entitiesRegistry::value().TryGet<ecs::components::Hand>(handEntity))
		{
			// Only a thing in the hand hangs from the blended place of the hand, not a seed
			_handHold.Fade(changes, mode != hand_blend::HandMode::Hidden, mode == hand_blend::HandMode::Holding,
			               heldThing.has_value() ? heldThing->entity : entt::null, HandStepSeconds(), *handTransform,
			               hand->boneMatrices);
		}
		TraceHand("stage fade", handTransform->position);
		EndHandTrace(placed, *handTransform, mode, grab);
		// The land behind the hand's middle, as posed for this frame, which the hand reaches about when pressed
		// with nothing under the cursor
		if (Locator::pickingSystem::has_value())
		{
			std::optional<glm::vec3> middle;
			if (const auto* hand = Locator::entitiesRegistry::value().TryGet<ecs::components::Hand>(handEntity))
			{
				const auto model = glm::translate(glm::mat4(1.0f), handTransform->position) *
				                   glm::mat4(handTransform->rotation) * glm::scale(glm::mat4(1.0f), handTransform->scale);
				std::vector<glm::vec3> bonePlaces;
				bonePlaces.reserve(hand->boneMatrices.size());
				for (const auto& bone : hand->boneMatrices)
				{
					bonePlaces.emplace_back(model * bone[3]);
				}
				middle = hand_reach::Middle(bonePlaces);
			}
			Locator::pickingSystem::value().FindBehindHand(camera.GetOrigin(), middle);
		}
		_handHold.PlaceHandEffect(heldSeed, _handAnimation.get(), *handTransform, handSize);
		// The trails' sheets of light and the chain behind the gesturing hand move on every frame
		Locator::particleRuleSystem::value().UpdateFrame(
		    std::chrono::duration<float>(gameTime).count(),
		    {.position = handTransform->position,
		     .size = handSize,
		     .cameraPosition = camera.GetOrigin(),
		     .gesturing = Locator::gestureSystem::has_value() && Locator::gestureSystem::value().IsGesturing(),
		     .selecting = Locator::gestureSystem::has_value() && Locator::gestureSystem::value().IsSelecting()});
	}
	// The temples' leashes, the picked one round the wrist of the hand as it is drawn, after the blend of its pose
	Locator::leashSystem::value().ShowPosts(IsHandDrawn());
	// A tribe's power behind a miracle spins its name round the hand, or up from the place of the cast. Its step is
	// the whole milliseconds of the game clock in this frame, times a thousandth.
	Locator::miracleFxSystem::value().UpdateTribalPower(gameTime.count() * 0.001f);
}

void Game::LoadHandAnimation()
{
	if (auto animation = game_assets::LoadHandAnimation(); animation != nullptr)
	{
		_handAnimation = std::move(animation);
	}
}

void Game::PlaceHand(ecs::components::Transform& handTransform, float deltaSeconds)
{
	const auto& camera = Locator::camera::value();
	const auto eye = camera.GetOrigin();
	// How far the hand reaches from the camera, as the land's scripts allow
	const auto handReach = Locator::cameraHelpSystem::value().Get().handReach;

	// In the temple the hand hangs on the line of sight through the cursor, a little short of where it
	// meets the room, slowly away from the camera and quickly towards it
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		if (_cursorWorldPosition)
		{
			const auto toRoom = *_cursorWorldPosition - eye;
			const auto roomDistance = glm::length(toRoom);
			if (roomDistance > 0.0f)
			{
				_handRayDirection = toRoom / roomDistance;
			}
			// With no window, the line goes from the camera straight to the room
			const auto line =
			    HandCursorLine(camera).value_or(Camera::NearPlanePoint {.eye = eye, .point = *_cursorWorldPosition});
			const auto target = hand_feel::TempleHandDistance(line.eye, line.point, *_cursorWorldPosition);
			const auto easeTime = _handHoverZoomer.GetDestination() <= target ? 0.4f : 0.2f;
			_handHoverZoomer.SetDestination(target, easeTime);
			StepEase(_handHoverZoomer);
			// The hand's distance from the camera
			_handDistance = _handHoverZoomer.GetValue();
		}
		handTransform.position = eye + _handRayDirection * _handDistance;
		return;
	}

	// In the clear view, the hand stays on the land at the thing that the camera turns round. The camera puts it there
	// after the turn moved the hand, so this comes first.
	if (const auto* world = dynamic_cast<const DefaultWorldCameraModel*>(&camera.GetModel()); world != nullptr)
	{
		if (const auto hand = world->GetClearViewHand(); hand.has_value())
		{
			_handPosition = *hand;
			_handCrossFade.Update(deltaSeconds);
			handTransform.position = _handCrossFade.Apply(_handPosition);
			return;
		}
	}

	// Tapping a house it knocked on, the hand stays where it knocked
	if (_handKnocking)
	{
		// When the knock ends, the hover distance starts again from here with no speed
		_handHoverZoomer.Reset(_handDistance);
		_handPosition = _handKnock.Point().value_or(_handPosition);
		_handCrossFade.Update(deltaSeconds);
		handTransform.position = _handCrossFade.Apply(_handPosition);
		return;
	}

	// Gripping the land, the camera keeps the land the hand gripped under the cursor, and the hand stays on the land it
	// gripped, so it moves with it. Its hover carries on from how far the gripped land was from the camera.
	const bool gripsLand = _handCameraState && _handPose == hand_navigation_pose::Pose::Grip;
	if (gripsLand)
	{
		if (!_handWasGripping)
		{
			_handGripPoint = _cursorWorldPosition.value_or(_handPosition);
		}
		_handPosition = _handGripPoint;
		_handDistance = glm::clamp(glm::distance(eye, _handGripPoint), k_HandMinDistance, handReach);
		_handHoverZoomer.Reset(_handDistance);
		_handHoldZoomer.Reset(_handDistance);
		_handWasGripping = true;
		_handCrossFade.Update(deltaSeconds);
		handTransform.position = _handCrossFade.Apply(_handPosition);
		return;
	}
	_handWasGripping = false;

	// Dragging by the edge of the screen, before the drag is decided or as it turns or tilts the camera, the hand keeps
	// on the line of sight through the cursor, holding how far it is from the camera over 0.4 seconds but never past
	// the land
	if (_handCameraState)
	{
		const auto screenSize =
		    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
		if (screenSize.x > 0 && screenSize.y > 0)
		{
			glm::vec3 rayOrigin;
			glm::vec3 rayDirection;
			camera.DeprojectScreenToWorld(static_cast<glm::vec2>(HandCursor()) / static_cast<glm::vec2>(screenSize), rayOrigin,
			                              rayDirection);
			if (!glm::any(glm::isnan(rayDirection)))
			{
				_handRayDirection = glm::normalize(rayDirection);
			}
		}
		_handHoldZoomer.SetDestination(glm::distance(eye, _handPosition), k_HandHoldTime);
		StepEase(_handHoldZoomer);
		const auto landDistance = _cursorWorldPosition.has_value() ? glm::distance(eye, *_cursorWorldPosition) : handReach;
		_handDistance = glm::clamp(glm::min(_handHoldZoomer.GetValue(), landDistance), k_HandMinDistance, handReach);
		_handHoverZoomer.Reset(_handDistance);
		_handPosition = eye + _handRayDirection * _handDistance;
		_handCrossFade.Update(deltaSeconds);
		handTransform.position = _handCrossFade.Apply(_handPosition);
		return;
	}
	_handHoldZoomer.Reset(_handDistance);

	// While a press on a body is held in a fight, the hand goes to a sparkle on the opponent's body that the last
	// drawing found. That point takes the place of the cursor's point.
	auto sparkle =
	    Locator::creatureFightSystem::has_value() ? Locator::creatureFightSystem::value().TakeHandSparkle() : std::nullopt;
	// The hand that went into the normal mode ignores the sparkle that the drawing found before
	if (std::exchange(_handDropsSparkle, false))
	{
		sparkle.reset();
	}

	// The cursor's point on the near plane, which the hand's line of sight goes through
	std::optional<glm::vec3> cursorNearPoint;
	auto lineEye = eye;
	if (const auto line = HandCursorLine(camera); line.has_value())
	{
		lineEye = line->eye;
		cursorNearPoint = line->point;
	}

	// The game puts the origin of the hand, by its fingertips, on the line of sight through the cursor, so the hand is
	// always under the cursor on screen. How far along it depends on the land the cursor is over. Turning the camera
	// with the mouse holds the cursor still, so the hand stays where it is on screen and eases to the land coming under
	// it.
	if (_cursorLand.has_value() || _handRestOnThing.has_value() || _handBubbleDistance.has_value())
	{
		// Over the land the hand points at it; over a thing, at where it rests on the thing
		// Over the help's text bubble it eases to the bubble, even with the sky behind it
		const auto toward = _handRestOnThing.value_or(_cursorLand.value_or(eye)) - eye;
		if (const auto length = glm::length(toward); length > 0.0f)
		{
			_handRayDirection = toward / length;
		}

		// The furthest the hand may be is the land less its height, so its fingers hang down to the land, or the
		// water over the sea; holding something, the land itself. Over a thing it eases to where it rests on the thing,
		// touching it.
		auto inputs = HandHoverInputs(lineEye);
		inputs.restOnThing = _handRestOnThing;
		inputs.restNearer = _handRestNearer;
		inputs.bubbleDistance = _handBubbleDistance;
		if (sparkle.has_value())
		{
			inputs.cursorPoint = *sparkle;
		}
		else if (cursorNearPoint.has_value())
		{
			inputs.cursorPoint = hand_feel::PointAlongCursor(lineEye, *cursorNearPoint, hand_feel::HoverLimit(inputs));
		}
		const auto hover = hand_feel::HoverDistancesOf(inputs);

		// The hand eases out slowly away from the camera and quickly towards it, but not while it pulls a thing out of
		// the ground: the pull places it then. While the camera watches a fight or the hand offers what it holds to a
		// creature, it eases out more slowly.
		const bool pulling = Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().IsPulling();
		const bool watchesFight = dynamic_cast<const FightCameraModel*>(&camera.GetModel()) != nullptr;
		const bool offers = Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().OffersToCreature();
		// The hover eases at the fixed steps of the input
		for (uint32_t i = 0; i < Locator::time::value().GetInputSteps().count; ++i)
		{
			hand_feel::EaseHover(_handHoverZoomer, hover.target, input_steps::k_StepSeconds, pulling, watchesFight || offers);
		}
		_handDistance = glm::clamp(glm::min(_handHoverZoomer.GetValue(), hover.limit), k_HandMinDistance, handReach);
	}
	// The hand is on the line of sight through the cursor at its distance, or on the fight's sparkle
	_handPosition = cursorNearPoint.has_value() ? hand_feel::PointAlongCursor(lineEye, *cursorNearPoint, _handDistance)
	                                            : eye + _handRayDirection * _handDistance;
	_handPosition = sparkle.value_or(_handPosition);
	_handCrossFade.Update(deltaSeconds);
	handTransform.position = _handCrossFade.Apply(_handPosition);
}

float Game::HandDrawnScale(glm::vec3 eye, glm::vec3 handPosition, bool holding) const
{
	const auto distance = magic::hand_hold::DistanceFromView(eye, handPosition, holding);
	// Held to a creature, its size is multiplied by the creature's scale before the size becomes the drawn scale
	const float size = HandAnimation::SizeAtDistance(distance);
	return _handAnimation->DrawnScale(_handOnCreature.has_value() ? _handOnCreature->scale * size : size);
}

hand_feel::Hover Game::HandHoverInputs(glm::vec3 eye) const
{
	std::optional<HoldType> held;
	bool offers = false;
	if (Locator::handGrabSystem::has_value())
	{
		const auto& grab = Locator::handGrabSystem::value();
		if (const auto pose = grab.GetHeldPose())
		{
			held = pose->hold;
		}
		offers = grab.OffersToCreature();
	}
	const bool holding = hand_feel::HoldReachesLand(held, offers);
	const auto overSea =
	    _cursorLand.has_value() && Locator::terrainSystem::value().GetHeightAt(glm::xz(*_cursorLand)) < k_HandSeaAltitude;
	return {
	    .camera = eye,
	    .land = _cursorLand,
	    .landIsSea = overSea,
	    .holding = holding,
	    .restOnThing = std::nullopt,
	    .cursorPoint = std::nullopt,
	    .handHeight = k_HandHeight * HandAnimation::SizeAtDistance(_handDistance),
	    .currentDistance = _handDistance,
	    .minDistance = k_HandMinDistance,
	    .reach = Locator::cameraHelpSystem::value().Get().handReach,
	};
}

void Game::UpdateHandKnock(const ecs::components::Transform& handTransform)
{
	if (!Locator::abodeKnockSystem::has_value())
	{
		return;
	}
	auto& knocks = Locator::abodeKnockSystem::value();
	// The houses' read-out of their people runs by the frame's game time, so it stops while the game pauses and
	// follows the game speed
	knocks.Update(Locator::time::value().GetFrameGameTime());
	// A knock while the tap plays doesn't start it over, the hand already being where it knocked
	if (knocks.TakeHandKnock())
	{
		_handKnock.Knock(_handPosition);
	}
	// Holding something comes first; the tap starts over once the hand lets go
	bool holds = magic::HandHoldPoser::Find().has_value();
	if (!holds && Locator::handGrabSystem::has_value())
	{
		holds = Locator::handGrabSystem::value().GetHeldPose().has_value();
	}
	const bool knocking = _handKnock.Hold(holds);
	// Starting and ending the tap, the hand fades from where it was drawn
	if (knocking != _handKnocking)
	{
		_handCrossFade.Start(handTransform.position);
	}
	_handKnocking = knocking;
}

void Game::UpdateHandNavigation(const ecs::components::Transform& handTransform)
{
	using namespace hand_navigation_pose;
	auto cues = Locator::camera::value().GetModel().GetHandCues();
	// A hand demonstration shows the camera hints its recording had
	if (Locator::handDemoSystem::has_value() && Locator::handDemoSystem::value().IsReplaying())
	{
		cues.tricons = Locator::handDemoSystem::value().GetHints();
	}
	// Dragging the land is the hand's camera state; turning the camera with the middle button or both buttons isn't
	const bool cameraState = HandButtons().gripping && !HandButtons().rotating;
	auto pose = Pose::Idle;
	if (cameraState)
	{
		// Other cameras than the world's don't sort their drags: the hand grips the land at once
		const bool gripsLand = !cues.dragging || cues.dragMode == camera_drag::DragMode::Pan || cues.clearViewGrip;
		pose = WhileDragging(gripsLand, cues.tricons);
	}
	else if (!_cursorOnObject && !_creatureUnderHand.has_value() && !_handOnCreature.has_value())
	{
		pose = WhileHovering(cues.tricons).value_or(Pose::Idle);
	}
	// Changing state or pose, the hand fades from where it was drawn to its new place
	if (cameraState != _handCameraState || pose != _handPose)
	{
		_handCrossFade.Start(handTransform.position);
	}
	_handCameraState = cameraState;
	_handPose = pose;
}

void Game::UpdateHandTricons(bool handDrawn, float seconds, hand_blend::HandMode mode)
{
	if (!Locator::gameInterface::has_value())
	{
		return;
	}
	Locator::gameInterface::value().SetHandTricons(std::nullopt);
	// Shown from the world camera, or from a tutorial demonstration's recording in any camera, out of the temple
	const auto& camera = Locator::camera::value();
	const auto cues = camera.GetModel().GetHandCues();
	const bool demonstration = Locator::handDemoSystem::has_value() && Locator::handDemoSystem::value().IsReplaying();
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	// The world camera makes its icons each frame, also while the hand is not drawn. The scripts read them, so they are
	// kept. A demonstration puts its recorded icons in their place.
	std::optional<hand_tricons::WorldCameraFrame> worldFrame;
	std::optional<hand_tricons::WorldCameraIcons> worldIcons;
	if (demonstration)
	{
		_cameraIcons = Locator::handDemoSystem::value().GetHints();
	}
	else if (cues.icons.has_value() && !inTemple)
	{
		worldFrame = *cues.icons;
		worldFrame->handInInfluence = HandInInfluence();
		// A box of the menu on the screen stops the camera input, so no icon shows
		worldFrame->inputOff = FrontEndBoxShows();
		worldFrame->tickMs = machine_clock::Ticks();
		worldIcons = hand_tricons::WorldCamera(*worldFrame, _triconAngle);
		_triconAngle = worldIcons->rotateAngle;
		_cameraIcons = worldIcons->icons;
	}
	if (!handDrawn || (!demonstration && !worldIcons.has_value()) || !Locator::windowing::has_value() || inTemple)
	{
		return;
	}
	const auto& actions = Locator::gameActionSystem::value();
	gui::GameInterface::HandTricons tricons {
	    .handStateShowsIcons = true,
	    .cinemaBars = Locator::cinematicDirectorSystem::value().IsWideScreenOn(),
	    .seconds = seconds,
	};
	if (demonstration)
	{
		const auto& demo = Locator::handDemoSystem::value();
		const auto held = demo.GetHeldButtons();
		_triconAngle = demo.GetHintAngle();
		tricons.icons = demo.GetHints();
		tricons.demonstration = true;
		tricons.moveHeld = held.move;
		tricons.actionHeld = held.action;
	}
	else
	{
		const auto& frame = *worldFrame;
		tricons.icons = worldIcons->icons;
		tricons.leans = cues.clearView <= hand_tricons::k_ClearViewLeans;
		tricons.cameraBusy =
		    frame.gripping || frame.rotatingAroundMouse || frame.turn != 0.0f || frame.tilt != 0.0f || frame.zoom != 0.0f;
		// The cross brightens with a thing under the hand, and shows at once with the action button
		if (actions.Get(input::BindableActionMap::ACTION))
		{
			tricons.crossNudge = hand_tricons::CrossNudge::Action;
		}
		else if (_cursorOnObject)
		{
			tricons.crossNudge = hand_tricons::CrossNudge::OverThing;
		}
	}
	tricons.rotateAngle = _triconAngle;

	const auto screen = glm::vec2(Locator::windowing::value().GetSize());
	// Where the posed hand is drawn: by its first bone, at the wrist
	const auto handEntity =
	    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	auto& registry = Locator::entitiesRegistry::value();
	const auto* handTransform = registry.TryGet<ecs::components::Transform>(handEntity);
	if (handTransform == nullptr)
	{
		return;
	}
	const auto* hand = registry.TryGet<ecs::components::Hand>(handEntity);
	const auto anchor = hand_tricons::AnchorPoint(handTransform->position, handTransform->rotation, handTransform->scale,
	                                              hand != nullptr ? std::span<const glm::mat4>(hand->boneMatrices)
	                                                              : std::span<const glm::mat4>());
	const auto clip = camera.GetViewProjectionMatrix() * glm::vec4(anchor, 1.0f);
	// Not with the hand behind the camera
	if (clip.w < camera.GetNearClip())
	{
		return;
	}
	tricons.hand = {static_cast<int>((clip.x / clip.w + 1.0f) * screen.x * 0.5f),
	                static_cast<int>((1.0f - clip.y / clip.w) * screen.y * 0.5f)};
	// They lean towards the cursor the hand follows this frame, after any warp onto the edge's turning ring
	tricons.cursor = HandCursor();
	// The same size on the screen at any distance: a part of the focal length across
	tricons.halfSize = hand_tricons::IconHalfSize(camera.GetProjectionMatrix()[0][0], glm::ivec2(screen));
	// The mode of the hand decides whether they show
	const bool scoopsLockedThing =
	    Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GetLockedOn().has_value();
	tricons.handStateShowsIcons = hand_blend::AllowsCameraIcons(mode, scoopsLockedThing);
	Locator::gameInterface::value().SetHandTricons(tricons);
}

void Game::OrientHand(ecs::components::Transform& handTransform, const glm::mat3& facingCamera, glm::vec3 surfaceUp)
{
	using namespace hand_orientation;
	const auto& camera = Locator::camera::value();
	const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();

	// The hand faces along the line of sight through the cursor, laid flat
	if (screenSize.x > 0 && screenSize.y > 0)
	{
		glm::vec3 rayOrigin;
		glm::vec3 rayDirection;
		camera.DeprojectScreenToWorld(static_cast<glm::vec2>(HandCursor()) / static_cast<glm::vec2>(screenSize), rayOrigin,
		                              rayDirection);
		if (!glm::any(glm::isnan(rayDirection)))
		{
			// Both forms of the heading change together, only when the line of sight leans across the land
			if (LeansAcrossLand(rayDirection))
			{
				_handHeadingAngle = HeadingAngle(rayDirection);
			}
			_handHeading = HeadingAlongRay(rayDirection, _handHeading);
		}
	}
	const auto cameraHeading = HeadingAlongRay(camera.GetForward(), _handHeading);

	// Its up eases over 0.4 seconds to the slope of the land under it, or to the face of what the cursor is on, given
	// afresh only as the cursor moves across the screen. Offering to turn the camera at the edge of the screen it
	// stands up towards where the camera looks instead. Dragging the land it holds its up, or stands straight towards
	// the camera's focus showing it turns the camera; letting go it stands up straight and eases from there.
	using hand_navigation_pose::Pose;
	const bool cursorMovedAcross = HandCursor().x != _handLastCursorX;
	_handLastCursorX = HandCursor().x;
	// A hand that holds a thing keeps the rules of the hand that holds, also while it drags the land
	const bool holdsSeed = magic::HandHoldPoser::Find().has_value();
	const bool holds =
	    holdsSeed || (Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GetHeld().has_value());
	const bool cameraUp = _handCameraState && !holds;
	// A pour tips the hand that holds the miracle. Its up is then given afresh each frame.
	const float pourTilt = holdsSeed && Locator::magicSystem::has_value()
	                           ? Locator::magicSystem::value().GetHandPour(Locator::time::value().GetTurnFraction()).tilt
	                           : 0.0f;
	// While the camera turns by the ring around the middle of the screen, the hand that holds stands towards the focus
	const auto cues = camera.GetModel().GetHandCues();
	const bool ringTurn = holds && cues.dragging && cues.dragMode == camera_drag::DragMode::EdgeRotate;
	if (cameraUp)
	{
		if (_handPose == Pose::Rotate)
		{
			_handUp.Reset(hand_navigation_pose::UpTowardsFocus(_handPosition, camera.GetFocus()));
		}
	}
	else
	{
		if (_handUpWasHeld)
		{
			_handUp.Reset(glm::vec3(0.0f, 1.0f, 0.0f));
		}
		if (cursorMovedAcross || pourTilt != 0.0f)
		{
			// The face of a model the hand rests on, unless it is lower over the thing than half its own height; else the
			// land where the hand is
			const float handHeight = k_HandHeight * HandAnimation::SizeAtDistance(_handDistance);
			// The land's height under the hand is taken at the hand's map position
			const auto handPoint = map_coords::FromMetres(glm::xz(handTransform.position));
			const float aboveLand =
			    Locator::terrainSystem::has_value()
			        ? handTransform.position.y - Locator::terrainSystem::value().GetAltitude(handPoint.x, handPoint.z)
			        : 0.0f;
			auto up = !hand_feel::TurnsToLand(_handSurfaceUp.has_value(), _handOverObject, aboveLand, handHeight)
			              ? surfaceUp
			              : (Locator::terrainSystem::has_value()
			                     ? Locator::terrainSystem::value().GetNormalAt(glm::xz(handTransform.position))
			                     : glm::vec3(0.0f, 1.0f, 0.0f));
			// Over the help's text bubble or the leash's rope it stands straight up. Offering to turn the camera over the
			// land with nothing in it, it stands towards the camera's focus from the cursor's point at its furthest.
			if (_handStandsStraight)
			{
				up = glm::vec3(0.0f, 1.0f, 0.0f);
			}
			else if (_handPose == Pose::Rotate && _handMayOfferTurn)
			{
				up = hand_navigation_pose::UpTowardsFocus(_handLimitPoint.value_or(_handPosition), camera.GetFocus());
			}
			if (ringTurn && !_handStandsStraight)
			{
				up = hand_navigation_pose::UpTowardsFocusOnRing(_handLimitPoint.value_or(_handPosition), camera.GetFocus());
			}
			// A pour turns straight up by its tilt about the line from the hand to the camera, over all else
			if (pourTilt != 0.0f)
			{
				up = hand_feel::PourUp(camera.GetOrigin(), handTransform.position, pourTilt);
			}
			_handUp.SetDestination(hand_feel::UnitUp(up), k_UpEaseSeconds);
		}
		StepEase(_handUp);
	}
	_handUpWasHeld = cameraUp;

	// Tapping a house, the hand stands straight up
	if (_handKnocking)
	{
		_handUp.Reset(glm::vec3(0.0f, 1.0f, 0.0f));
	}
	const auto up = hand_feel::UnitUp(_handUp.GetValue());
	const auto onLevelLand = TurnToHeading(facingCamera, cameraHeading, _handHeading);
	_handOnLevelLand = onLevelLand;
	handTransform.rotation = glm::length(up) > 0.0f ? StandOnSlope(onLevelLand, _handHeading, up) : onLevelLand;
}

float Game::HandStepSeconds() const
{
	return static_cast<float>(static_cast<int32_t>(CameraStepTime().count())) * 0.001f;
}

void Game::EndHandTrace(const glm::vec3& placed, const ecs::components::Transform& hand, hand_blend::HandMode mode,
                        const ecs::systems::HandGrabSystemInterface* grab)
{
	auto* trace = Locator::handSystem::value().GetTrace();
	auto* frame = trace != nullptr ? trace->Current() : nullptr;
	if (frame == nullptr)
	{
		return;
	}
	frame->cursor = glm::vec2(HandCursor());
	frame->placed = placed;
	frame->drawn = hand.position;
	// The hand shows where its fingertips are as posed, which its pose can carry away from its origin
	frame->visible = hand.position;
	if (_handAnimation && _handAnimation->IsLoaded())
	{
		const auto model =
		    glm::translate(glm::mat4(1.0f), hand.position) * glm::mat4(hand.rotation) * glm::scale(glm::mat4(1.0f), hand.scale);
		frame->visible = glm::vec3(model * glm::vec4(_handAnimation->LeafBoneCentre(), 1.0f));
		// The wrist as it is drawn: after the blend of the drawn pose, which the animation's own bones do not have
		const auto handEntity =
		    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
		const auto* drawn = Locator::entitiesRegistry::value().TryGet<const ecs::components::Hand>(handEntity);
		const auto& bones = drawn != nullptr ? drawn->boneMatrices : _handAnimation->GetBoneMatrices();
		if (!bones.empty())
		{
			frame->wrist = glm::vec3(model * bones.front()[3]);
		}
	}
	const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
	glm::vec3 screen;
	if (screenSize.x > 0 && screenSize.y > 0 &&
	    Locator::camera::value().ProjectWorldToScreen(frame->visible, glm::vec4(0.0f, 0.0f, glm::vec2(screenSize)), screen))
	{
		frame->screen = glm::vec2(screen);
	}
	if (screenSize.x > 0 && screenSize.y > 0 &&
	    Locator::camera::value().ProjectWorldToScreen(frame->wrist, glm::vec4(0.0f, 0.0f, glm::vec2(screenSize)), screen))
	{
		frame->wristScreen = glm::vec2(screen);
	}
	frame->stage = static_cast<uint8_t>(mode);
	if (const auto picked = Locator::pickingSystem::value().GetPick().object)
	{
		frame->picked = static_cast<uint32_t>(entt::to_integral(*picked));
	}
	frame->cursorLand = _cursorLand;
	frame->restOnThing = _handRestOnThing;
	if (grab != nullptr)
	{
		frame->busy = grab->IsBusy();
		if (const auto held = grab->GetHeld())
		{
			frame->held = static_cast<uint32_t>(entt::to_integral(*held));
			if (const auto* transform = Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(*held))
			{
				frame->heldAt = transform->position;
			}
		}
	}
	trace->EndFrame();
}

void Game::UpdateMagicHand(const glm::vec3& handPosition, float deltaSeconds)
{
	const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
	ecs::systems::MagicSystemInterface::HandFrame frame {.handPosition = handPosition, .point = _cursorWorldPosition};
	if (screenSize.x > 0 && screenSize.y > 0)
	{
		Locator::camera::value().DeprojectScreenToWorld(
		    static_cast<glm::vec2>(HandCursor()) / static_cast<glm::vec2>(screenSize), frame.rayOrigin, frame.rayDirection);
	}
	frame.cameraForward = Locator::camera::value().GetForward();
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	frame.overWorld = !inTemple && !Locator::debugGui::value().IsMouseOverWindow();
	auto& magic = Locator::magicSystem::value();
	// The hand's movement, and the spin it gives a miracle, are measured at the steps of the input
	magic.UpdateHand(frame, Locator::time::value().GetInputSteps());
	magic.Update(deltaSeconds);
	Locator::magicShieldSystem::value().Update(deltaSeconds);
	// The seeds above the stone hands and worship icons spin as the globes' do, by the game time of the frame: they stand
	// still while the game is paused
	if (Locator::spellIconSystem::has_value())
	{
		Locator::spellIconSystem::value().Update(
		    std::chrono::duration<float>(Locator::time::value().GetFrameGameTime()).count());
	}
}

void Game::PlayHandGrabSound()
{
	if (!_cursorWorldPosition || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto position = *_cursorWorldPosition;

	// Land is a cell of the landscape without water
	bool isLand = false;
	const auto cell = glm::floor(glm::vec2(position.x, position.z) / 10.0f);
	if (cell.x >= 0.0f && cell.y >= 0.0f && cell.x < 512.0f && cell.y < 512.0f)
	{
		const auto* landCell = Locator::terrainSystem::value().FindCell(glm::u16vec2(cell));
		isLand = landCell != nullptr && landCell->properties.hasWater == 0;
	}

	const auto feedback = hand_grab::GripFeedbackOf(isLand, !Locator::cinematicDirectorSystem::value().IsInterfaceActive());
	if (feedback == hand_grab::GripFeedback::None)
	{
		return;
	}
	if (feedback == hand_grab::GripFeedback::Dust)
	{
		// The next turn makes a puff of dust where the hand grips the land, on each computer of a network game. The sound
		// plays at once, only on this computer.
		Locator::playerCommandSystem::value().Submit(player_commands::GripDust {
		    .point = {position.x, Locator::terrainSystem::value().GetHeightAt(glm::xz(position)), position.z}});
		// One of G_HandGrabLand_01 to _06, centred on the listener. The number comes from this computer's own seed, so
		// that the synced numbers of the world do not change.
		const auto sample = 4 + Locator::gameRandom::value().LocalRand(6);
		const auto id = fmt::format("InGame.sad/{}", sample);
		// The grip is a sound of its own to the voice rules
		audio::StartGameSoundEffect(entt::hashed_string(id.c_str()), {.voiceOwner = audio::VoiceOwner::OwnSound});
	}
	else
	{
		// While the game runs, the hand splashes where it goes in: a ring on the water, in the land's brightest light
		if (!Locator::time::value().IsPaused())
		{
			const auto angle = Locator::gameRandom::value().CrtRandom(0.0f, glm::two_pi<float>());
			if (Locator::waterRingSystem::has_value())
			{
				Locator::waterRingSystem::value().Add(
				    water_rings::HandSplash(glm::vec2(position.x, position.z), angle, FrameLandLight(255)));
			}
			// The fish near where the hand went in dart away
			Locator::fishFarmSystem::value().Scare({position.x, k_HandSplashHeight, position.z});
			// The shoals of the fish puzzle get the scare at the next turn, on each computer of a network game
			if (Locator::puzzleGameSystem::has_value() && Locator::puzzleGameSystem::value().HasFishPuzzle())
			{
				Locator::playerCommandSystem::value().Submit(
				    player_commands::FishScare {.point = {position.x, k_HandSplashHeight, position.z}});
			}
		}
		// G_HandInWater_01 to _10 in turn, on the water's surface where the hand went in
		const auto id = fmt::format("InGame.sad/{}", 99 + _handInWaterSample);
		_handInWaterSample = (_handInWaterSample + 1) % 10;
		audio::PlayGameSoundEffect(entt::hashed_string(id.c_str()), glm::vec3(position.x, k_HandSplashHeight, position.z));
	}
}

bool Game::IsHandDrawn() const
{
	return hand_visibility::IsShown(Locator::gameInterface::has_value() && Locator::gameInterface::value().IsDialogOpen(),
	                                Locator::cinematicDirectorSystem::value().IsInterfaceActive(),
	                                Locator::temple::has_value() && Locator::temple::value().Active());
}
