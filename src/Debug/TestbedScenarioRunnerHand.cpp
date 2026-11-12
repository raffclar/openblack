/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed runner: the commands of the hand of the player and of the pointer: clicks, taps, gestures and drags.

#include <algorithm>
#include <array>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#include <SDL_events.h>
#include <SDL_mouse.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/SkyDome.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Commands/PlayerCommands.h"
#include "Common/PortableMath.h"
#include "Creature/CaveCreature.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureScriptPlay.h"
#include "Debug/FightAutopilotInterface.h"
#include "Debug/TestbedHandSweep.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLeashRope.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FootballSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "Gestures/GesturePaths.h"
#include "Gui/ScreenFit.h"
#include "Help/TipBubble.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Input/InjectedInput.h"
#include "Input/InputDeviceInterface.h"
#include "Input/PointerSettle.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/GameMusic.h"
#include "TestbedScenarioRunner.h"
#include "TestbedScenarioRunnerParts.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::runner_parts;
using openblack::ecs::components::Creature;
using openblack::ecs::components::Transform;

namespace
{
glm::vec2 WindowSize()
{
	return Locator::windowing::has_value() ? glm::vec2(Locator::windowing::value().GetSize()) : glm::vec2(1.0f);
}

/// Sends a move of the pointer to where it is, by so much, with the buttons held, as the mouse does
void PushMotion(const input::GameActionInterface::ScriptedPointer& pointer, glm::ivec2 moved)
{
	SDL_Event event {};
	event.type = SDL_MOUSEMOTION;
	event.motion.windowID = Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
	event.motion.state = pointer.buttons;
	event.motion.x = pointer.position.x;
	event.motion.y = pointer.position.y;
	event.motion.xrel = moved.x;
	event.motion.yrel = moved.y;
	input::MarkInjected(event);
	input::Device().PushEvent(event);
}

/// What the player's hand holds, for the log
std::string HeldByHand()
{
	if (!Locator::handGrabSystem::has_value())
	{
		return {};
	}
	const auto held = Locator::handGrabSystem::value().GetHeld();
	return held.has_value() ? fmt::format(", holding {}", static_cast<uint32_t>(*held)) : std::string {};
}
} // namespace

std::string Runner::ClickScroll(const Command& command)
{
	if (!Locator::handGrabSystem::has_value())
	{
		return "no hand";
	}
	// The scroll nearest the point, as the player clicks the one that the camera looks at
	const auto at = MapPoint(_middle, command.point);
	auto& registry = Locator::entitiesRegistry::value();
	auto nearest = entt::entity {entt::null};
	float best = std::numeric_limits<float>::max();
	registry.Each<const ecs::components::ScriptHighlight, const ecs::components::Transform>(
	    [&](entt::entity entity, const ecs::components::ScriptHighlight&, const ecs::components::Transform& transform) {
		    const auto distance = glm::distance(glm::vec2(transform.position.x, transform.position.z), at);
		    if (distance < best)
		    {
			    best = distance;
			    nearest = entity;
		    }
	    });
	if (nearest == entt::null)
	{
		return "no scroll";
	}
	// A short press on the scroll taps it, which replays the reminder of a challenge that has started, and clicks it for
	// the scripts
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	Locator::handGrabSystem::value().DoTap(player, nearest);
	Locator::handGrabSystem::value().ClickThing(nearest);
	return fmt::format("clicked the scroll {} m from the point", best);
}

std::string Runner::ClickCreatureAt(const Command& command)
{
	if (!Locator::handGrabSystem::has_value())
	{
		return "no hand";
	}
	// The creature nearest the point, as the player clicks the one under the pointer
	const auto at = MapPoint(_middle, command.point);
	auto& registry = Locator::entitiesRegistry::value();
	auto nearest = entt::entity {entt::null};
	float best = std::numeric_limits<float>::max();
	registry.Each<const ecs::components::Creature, const ecs::components::Transform>(
	    [&](entt::entity entity, const ecs::components::Creature&, const ecs::components::Transform& transform) {
		    const auto distance = glm::distance(glm::vec2(transform.position.x, transform.position.z), at);
		    if (distance < best)
		    {
			    best = distance;
			    nearest = entity;
		    }
	    });
	if (nearest == entt::null)
	{
		return "no creature";
	}
	Locator::handGrabSystem::value().ClickThing(nearest);
	return fmt::format("clicked the creature {} m from the point", best);
}

std::string Runner::ClickCreatureNear(const Command& command)
{
	if (!Locator::handGrabSystem::has_value())
	{
		return "no hand";
	}
	const auto at = MapPoint(_middle, command.point);
	auto& registry = Locator::entitiesRegistry::value();
	auto nearest = entt::entity {entt::null};
	float best = std::numeric_limits<float>::max();
	registry.Each<const ecs::components::Creature, const ecs::components::Transform>(
	    [&](entt::entity entity, const ecs::components::Creature&, const ecs::components::Transform& transform) {
		    const auto distance = glm::distance(glm::vec2(transform.position.x, transform.position.z), at);
		    if (distance < best)
		    {
			    best = distance;
			    nearest = entity;
		    }
	    });
	if (nearest == entt::null)
	{
		return "no creature";
	}
	Locator::handGrabSystem::value().ClickThing(nearest);
	return fmt::format("clicked the creature {} {} m from the point", entt::to_integral(nearest), best);
}

std::string Runner::TapRewardNear(const Command& command)
{
	if (!Locator::handGrabSystem::has_value())
	{
		return "no hand";
	}
	const auto at = MapPoint(_middle, command.point);
	auto& registry = Locator::entitiesRegistry::value();
	auto nearest = entt::entity {entt::null};
	float best = std::numeric_limits<float>::max();
	registry.Each<const ecs::components::Reward, const ecs::components::Transform>(
	    [&](entt::entity entity, const ecs::components::Reward&, const ecs::components::Transform& transform) {
		    const auto distance = glm::distance(glm::vec2(transform.position.x, transform.position.z), at);
		    if (distance < best)
		    {
			    best = distance;
			    nearest = entity;
		    }
	    });
	if (nearest == entt::null)
	{
		return "no reward chest";
	}
	// A short press on the chest taps it open and clicks it for the scripts
	auto& hand = Locator::handGrabSystem::value();
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	hand.DoTap(player, nearest);
	hand.ClickThing(nearest);
	return fmt::format("tapped the reward chest {} m from the point", best);
}

std::string Runner::HandTakeNear(const Command& command)
{
	if (!Locator::handGrabSystem::has_value())
	{
		return "no hand";
	}
	// The mobile object or the scaffold nearest the point, as the player picks up the one under the pointer
	constexpr float k_Reach = 10.0f;
	const auto at = MapPoint(_middle, command.point);
	auto& registry = Locator::entitiesRegistry::value();
	auto nearest = entt::entity {entt::null};
	float best = k_Reach;
	const auto closer = [&](entt::entity entity, const ecs::components::Transform& transform) {
		const auto distance = glm::distance(glm::vec2(transform.position.x, transform.position.z), at);
		if (distance < best)
		{
			best = distance;
			nearest = entity;
		}
	};
	registry.Each<const ecs::components::MobileObject, const ecs::components::Transform>(
	    [&closer](entt::entity entity, const ecs::components::MobileObject&, const ecs::components::Transform& transform) {
		    closer(entity, transform);
	    });
	registry.Each<const ecs::components::Scaffold, const ecs::components::Transform>(
	    [&closer](entt::entity entity, const ecs::components::Scaffold&, const ecs::components::Transform& transform) {
		    closer(entity, transform);
	    });
	if (nearest == entt::null)
	{
		return "no mobile object near the point";
	}
	auto& hand = Locator::handGrabSystem::value();
	const auto why = hand.WhyNotTake(nearest);
	return hand.TakeIntoHand(nearest) ? fmt::format("took the mobile object {} m from the point", best)
	                                  : fmt::format("the hand can't take it: {}", why);
}

std::string Runner::HandTapObject(size_t index)
{
	const auto object = ObjectAt(index);
	if (!object.has_value() || !Locator::handSystem::has_value() || !Locator::abodeKnockSystem::has_value())
	{
		return "nothing to tap";
	}
	// A knock on a building by this computer's hand, from where the hand is, as the Action button pressed on it makes
	auto& registry = Locator::entitiesRegistry::value();
	const auto hand =
	    Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	const auto* transform = registry.TryGet<const Transform>(hand);
	// A rock tall enough breaks under the tap of this computer's player's hand
	if (ecs::object_physics::CanTapRock(*object) && Locator::dynamicsSystem::has_value())
	{
		const auto player =
		    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
		ecs::object_physics::TapRock(Locator::dynamicsSystem::value(), *object,
		                             transform != nullptr ? transform->position : glm::vec3(0.0f), player);
		return "broke the rock";
	}
	// A tap breaks one scaffold off a scaffold
	if (registry.AllOf<ecs::components::Scaffold>(*object) && Locator::workshopSystem::has_value())
	{
		auto& workshops = Locator::workshopSystem::value();
		if (!workshops.ValidToTap(*object))
		{
			return "the scaffold takes no tap";
		}
		return workshops.Tap(*object, transform != nullptr ? transform->position : glm::vec3(0.0f))
		           ? "broke the scaffold"
		           : "the scaffold did not break";
	}
	const bool took =
	    Locator::abodeKnockSystem::value().Tap(*object, transform != nullptr ? transform->position : glm::vec3(0.0f), true);
	return took ? "knocked" : "it takes no knock";
}

std::string Runner::ChargeIconWithSeedHeld(const Command& command)
{
	if (!Locator::infoConstants::has_value() || !Locator::spellIconSystem::has_value() || !Locator::magicSystem::has_value() ||
	    !Locator::worshipSiteSystem::has_value())
	{
		return "no miracles";
	}
	const auto& info = Locator::infoConstants::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto seedType = static_cast<SpellSeedType>(command.value);
	std::optional<entt::entity> found;
	registry.Each<const ecs::components::WorshipIcon>([&](entt::entity entity, const ecs::components::WorshipIcon& icon) {
		const auto* site = registry.Valid(icon.site) ? registry.TryGet<const ecs::components::WorshipSite>(icon.site) : nullptr;
		if (!found.has_value() && site != nullptr && site->player == command.player && icon.seed == seedType)
		{
			found = entity;
		}
	});
	if (!found.has_value())
	{
		return "no such icon";
	}
	auto& icon = registry.Get<ecs::components::WorshipIcon>(*found);
	const auto type = magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(info, icon.seed), magic::k_BasePowerUpLevel);
	const float cost = magic::GetChantsRequiredToCreate(info, type);
	// The icon is charged full and a tap puts its miracle in the hand
	icon.charging = false;
	icon.store = cost;
	Locator::spellIconSystem::value().Tap(*found, command.player);
	const auto held = Locator::magicSystem::value().GetHeldSeed();
	if (!held.has_value() || std::ranges::find(icon.seedsOut, *held) == icon.seedsOut.end())
	{
		return "the hand did not take the seed";
	}
	// The seed keeps only a part of its charge, and the icon charges again for the hand that holds the seed
	auto& seed = registry.Get<ecs::components::SpellSeed>(*held);
	seed.chantStore = cost * command.amount;
	icon.charging = true;
	icon.charger = command.player;
	icon.powerUp = magic::k_BasePowerUpLevel;
	icon.store = 0.0f;
	Locator::worshipSiteSystem::value().ReturnChants(icon.site, cost);
	return fmt::format("the seed in the hand holds {:.0f} of {:.0f}, and its icon charges", seed.chantStore, cost);
}

std::string Runner::TakeIconSeed(const Command& command)
{
	if (!Locator::infoConstants::has_value() || !Locator::spellIconSystem::has_value() || !Locator::magicSystem::has_value() ||
	    !Locator::worshipSiteSystem::has_value())
	{
		return "no miracles";
	}
	const auto& info = Locator::infoConstants::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto seedType = static_cast<SpellSeedType>(command.value);
	std::optional<entt::entity> found;
	registry.Each<const ecs::components::WorshipIcon>([&](entt::entity entity, const ecs::components::WorshipIcon& icon) {
		const auto* site = registry.Valid(icon.site) ? registry.TryGet<const ecs::components::WorshipSite>(icon.site) : nullptr;
		if (!found.has_value() && site != nullptr && site->player == command.player && icon.seed == seedType)
		{
			found = entity;
		}
	});
	if (!found.has_value())
	{
		return "no such icon";
	}
	auto& icon = registry.Get<ecs::components::WorshipIcon>(*found);
	const auto& seedInfo = magic::GetSpellSeedInfo(info, icon.seed);
	const float cost =
	    magic::GetChantsRequiredToCreate(info, magic::GetMagicTypeFromPowerUpLevel(seedInfo, magic::k_BasePowerUpLevel));
	// A seed that the hand holds goes first. Then the icon is charged full and a tap puts its miracle in the hand.
	if (Locator::magicSystem::value().GetHeldSeedOf(command.player).has_value())
	{
		Locator::magicSystem::value().DiscardHeldSeedOf(command.player);
	}
	icon.charging = false;
	icon.store = cost;
	Locator::spellIconSystem::value().Tap(*found, command.player);
	const auto held = Locator::magicSystem::value().GetHeldSeed();
	if (!held.has_value() || std::ranges::find(icon.seedsOut, *held) == icon.seedsOut.end())
	{
		return "the hand did not take the seed";
	}
	// The site gets prayer power for the power-ups, from the cost of the highest level that the seed has
	const int top = magic::GetNumPowerUpLevels(seedInfo) - 2;
	const float topCost = magic::GetChantsRequiredToCreate(info, magic::GetMagicTypeFromPowerUpLevel(seedInfo, top));
	Locator::worshipSiteSystem::value().ReturnChants(icon.site, topCost * command.amount);
	return fmt::format("the seed is in the hand, and the site gets {:.0f}", topCost * command.amount);
}

std::string Runner::TapSiteIcon(const Command& command)
{
	if (!Locator::infoConstants::has_value() || !Locator::spellIconSystem::has_value() ||
	    !Locator::worshipSiteSystem::has_value())
	{
		return "no miracles";
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto seedType = static_cast<SpellSeedType>(command.value);
	std::optional<entt::entity> found;
	registry.Each<const ecs::components::WorshipIcon>([&](entt::entity entity, const ecs::components::WorshipIcon& icon) {
		const auto* site = registry.Valid(icon.site) ? registry.TryGet<const ecs::components::WorshipSite>(icon.site) : nullptr;
		if (!found.has_value() && site != nullptr && site->player == command.player && icon.seed == seedType)
		{
			found = entity;
		}
	});
	if (!found.has_value())
	{
		return "no such icon";
	}
	const auto& icon = registry.Get<const ecs::components::WorshipIcon>(*found);
	if (command.amount > 0.0f)
	{
		const auto& info = Locator::infoConstants::value();
		const float cost = magic::GetChantsRequiredToCreate(
		    info, magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(info, seedType), magic::k_BasePowerUpLevel));
		Locator::worshipSiteSystem::value().ReturnChants(icon.site, cost * command.amount);
	}
	const bool counted = Locator::spellIconSystem::value().Tap(*found, command.player);
	return fmt::format("tapped the icon in place {}: {}", icon.place, counted ? "it counts" : "it does not count");
}

std::string Runner::HandTakeFireBall()
{
	if (!Locator::magicSystem::has_value())
	{
		return "no miracles";
	}
	// The first fireball the hand may hold now, the hand straight over it looking down
	std::optional<glm::vec3> target;
	Locator::entitiesRegistry::value().Each<const ecs::components::MagicFireBall>(
	    [this, &target](entt::entity entity, const ecs::components::MagicFireBall& ball) {
		    if (!target.has_value() && ball.handTarget.has_value())
		    {
			    target = ball.handTarget;
			    _fireBallTaken = entity;
		    }
	    });
	if (!target.has_value())
	{
		return "no fireball to take hold of";
	}
	constexpr float k_HandAbove = 10.0f;
	const glm::vec3 hand = *target + glm::vec3(0.0f, k_HandAbove, 0.0f);
	Locator::magicSystem::value().DriveHand(ecs::systems::MagicSystemInterface::HandFrame {.handPosition = hand,
	                                                                                       .point = *target,
	                                                                                       .rayOrigin = hand,
	                                                                                       .rayDirection = {0.0f, -1.0f, 0.0f},
	                                                                                       .cameraForward = {0.0f, -1.0f, 0.0f},
	                                                                                       .overWorld = true});
	// The hand takes its place over the next frames, then taps or presses
	_fireBallTakeFrames = 0;
	return fmt::format("the hand goes over it at ({:.1f}, {:.1f}, {:.1f})", target->x, target->y, target->z);
}

void Runner::FinishTakingFireBall()
{
	constexpr int k_FramesToReach = 2;
	if (!_fireBallTakeFrames.has_value() || ++*_fireBallTakeFrames < k_FramesToReach || !Locator::magicSystem::has_value())
	{
		return;
	}
	_fireBallTakeFrames.reset();
	auto& magic = Locator::magicSystem::value();
	std::string result;
	if (magic.IsHandBusy())
	{
		const bool taken = magic.PressAction();
		magic.ReleaseAction();
		result = taken ? "the press was taken" : "the press wasn't taken";
	}
	else
	{
		result = magic.TapFireBall(_fireBallTaken) ? "the tap went to the turn" : "the hand could not tap it";
	}
	magic.DriveHand(std::nullopt);
	Log(fmt::format("{:.1f}s: hand on the fireball: {}", _seconds, result));
}

std::string Runner::DrawGesture(GestureType gesture, glm::vec2 offset)
{
	if (!Locator::gestureSystem::has_value())
	{
		return "no gestures";
	}
	auto& gestures = Locator::gestureSystem::value();
	// Across the middle of the screen, or moved from it by a part of the screen's width and height, a little under half
	// its height across
	const auto aspect = gestures.GetScreenAspect();
	const glm::vec2 middle {gesture::k_ReferenceHeight * aspect * (0.5f + offset.x),
	                        gesture::k_ReferenceHeight * (0.5f + offset.y)};
	constexpr float k_Size = 320.0f;
	auto path = gesture::TraceGesture(gestures.GetTemplates(), gesture, middle, k_Size, aspect);
	if (!path.has_value())
	{
		// Without the game's templates, the circle and the scribble are drawn as a hand would draw them
		if (gesture == GestureType::Circle)
		{
			path = gesture::TraceCircle(middle, k_Size * 0.5f, true);
		}
		else if (gesture == GestureType::Scribble)
		{
			path = gesture::TraceScribble(middle, k_Size, 5);
		}
		else
		{
			return "no template to draw it from";
		}
	}
	// A circle sizes a storm or a shield readied by holding the Action button
	const bool holdingAction = gesture == GestureType::Circle;
	gestures.DrawPath(std::move(*path), holdingAction);
	// With a miracle in the hand the button goes down on it too, as the player's does, readying it to be cast
	_drawingPressedAction = holdingAction && Locator::magicSystem::has_value() && Locator::magicSystem::value().IsHandBusy() &&
	                        Locator::magicSystem::value().PressAction();
	if (_drawingPressedAction)
	{
		Log(fmt::format("{:.1f}s: pressed for the circle: {}", _seconds,
		                HandResultName(Locator::magicSystem::value().GetLastHandResult())));
	}
	_drawing = gesture;
	const auto last = gestures.GetLastRecognised();
	_recognisedBefore = last.has_value() ? last->number : 0;
	return fmt::format("drawing gesture {}", static_cast<int>(gesture));
}

void Runner::WatchGesture()
{
	if (!_drawing.has_value() || !Locator::gestureSystem::has_value())
	{
		return;
	}
	const auto& gestures = Locator::gestureSystem::value();
	const auto recognised = gestures.GetLastRecognised();
	// Recognised, or drawn to the end without being recognised
	const bool done = recognised.has_value() && recognised->number != _recognisedBefore;
	// A button held for the gesture stays down until the path is drawn to its end
	if ((!done || _drawingPressedAction) && gestures.IsDrawingPath())
	{
		return;
	}
	if (_drawingPressedAction && Locator::magicSystem::has_value())
	{
		auto& magic = Locator::magicSystem::value();
		magic.ReleaseAction();
		Log(fmt::format("{:.1f}s: let go after the circle: {}", _seconds, HandResultName(magic.GetLastHandResult())));
		_drawingPressedAction = false;
	}
	if (!done)
	{
		Log(fmt::format("{:.1f}s: gesture {} not recognised", _seconds, static_cast<int>(*_drawing)));
		_drawing.reset();
		return;
	}
	std::string what;
	if (const auto& event = recognised->event; event.has_value())
	{
		using EventKind = ecs::systems::GestureEvent::Kind;
		if (event->kind == EventKind::Circle)
		{
			what =
			    fmt::format(", a circle at {:.0f}, {:.0f} of radius {:.1f}", event->centre.x, event->centre.z, event->radius);
		}
		else if (event->kind == EventKind::PowerUp)
		{
			what = fmt::format(", power up to level {}", event->powerUpLevel);
		}
		else
		{
			what = ", the held seed is dropped";
		}
	}
	const auto creature = CreatureAt(0);
	const auto leash = Locator::leashSystem::has_value() && creature.has_value()
	                       ? Locator::leashSystem::value().TypeOf(*creature)
	                       : LeashType::None;
	Log(fmt::format("{:.1f}s: gesture {} recognised ({}){}; leash {}, picker {}", _seconds,
	                static_cast<int>(recognised->request.gesture), gesture::Name(recognised->request.purpose), what,
	                leash == LeashType::None ? "off" : creature_leash::Name(leash),
	                gestures.IsLeashPickerOpen() ? "open" : "closed"));
	_drawing.reset();
}

std::optional<glm::ivec2> Runner::OpponentOnScreen(size_t creature, glm::vec2 size) const
{
	const auto self = CreatureAt(creature);
	if (!self.has_value() || !Locator::creatureFightSystem::has_value() || !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto opponent = Locator::creatureFightSystem::value().OpponentOf(*self);
	if (!opponent.has_value())
	{
		return std::nullopt;
	}
	return BodyOnScreen(*opponent, size);
}

std::optional<glm::ivec2> Runner::BodyOnScreen(entt::entity creature, glm::vec2 size)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::camera::has_value() || !registry.AllOf<ecs::components::Transform, ecs::components::Creature>(creature))
	{
		return std::nullopt;
	}
	// The middle of the body is half the height of the creature above its feet
	const auto& feet = registry.Get<const ecs::components::Transform>(creature).position;
	const auto height = creature_feedback::k_HeightAtSizeOne *
	                    ecs::components::ShownSize(registry.Get<const ecs::components::Creature>(creature));
	glm::vec3 screen;
	if (!Locator::camera::value().ProjectWorldToScreen(feet + glm::vec3(0.0f, height * 0.5f, 0.0f),
	                                                   {0.0f, 0.0f, size.x, size.y}, screen))
	{
		return std::nullopt;
	}
	return glm::ivec2(glm::round(glm::vec2(screen)));
}

std::optional<glm::ivec2> Runner::AltarOnScreen(glm::vec2 size, bool foodPot) const
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(k_ScenarioTownId);
	if (found == towns.end() || !registry.Valid(found->second) || !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto site = registry.Get<const ecs::components::Town>(found->second).worshipSite;
	const auto* worship = registry.Valid(site) ? registry.TryGet<const ecs::components::WorshipSite>(site) : nullptr;
	const auto target = worship != nullptr ? (foodPot ? worship->foodPot : worship->altar) : entt::null;
	if (!registry.Valid(target) || !registry.AllOf<ecs::components::Transform>(target))
	{
		return std::nullopt;
	}
	// The middle of the altar's foot: the line from the camera to it meets the altar's low top first. The food pot is
	// low, so its middle is the place where it stands.
	const auto& foot = registry.Get<const ecs::components::Transform>(target).position;
	glm::vec3 screen;
	if (!Locator::camera::value().ProjectWorldToScreen(foot, {0.0f, 0.0f, size.x, size.y}, screen))
	{
		return std::nullopt;
	}
	return glm::ivec2(glm::round(glm::vec2(screen)));
}

std::optional<glm::ivec2> Runner::ObjectOnScreen(size_t object, float height, glm::vec2 size, size_t pile) const
{
	auto entity = ObjectAt(object);
	if (!entity.has_value() || !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	// A pile of a storage pit: 1 to 5 are its wood piles, 6 is its food pile
	if (pile != 0)
	{
		const auto* pit = Locator::entitiesRegistry::value().TryGet<const ecs::components::StoragePit>(*entity);
		if (pit == nullptr || pile > pit->woodPiles.size() + 1)
		{
			return std::nullopt;
		}
		entity = pile <= pit->woodPiles.size() ? pit->woodPiles.at(pile - 1) : pit->foodPile;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const ecs::components::Transform>(*entity);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	// A thing in a creature's hand is where the last frame drew it in the hand, not where its place stays
	auto position = transform->position;
	if (const auto* held = registry.TryGet<const ecs::components::HeldByCreature>(*entity);
	    held != nullptr && held->drawn.has_value())
	{
		position = held->drawn->position;
	}
	glm::vec3 screen;
	if (!Locator::camera::value().ProjectWorldToScreen(position + glm::vec3(0.0f, height, 0.0f), {0.0f, 0.0f, size.x, size.y},
	                                                   screen))
	{
		return std::nullopt;
	}
	return glm::ivec2(glm::round(glm::vec2(screen)));
}

std::vector<glm::ivec2> Runner::RopeOnScreen(size_t creature, glm::vec2 size) const
{
	std::vector<glm::ivec2> pixels;
	const auto entity = CreatureAt(creature);
	if (!entity.has_value() || !Locator::camera::has_value() || !Locator::leashSystem::has_value())
	{
		return pixels;
	}
	const auto* leash = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureLeash>(*entity);
	const auto* drawn = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureLeashRope>(*entity);
	if (leash == nullptr || !leash->worn.has_value() || drawn == nullptr)
	{
		return pixels;
	}
	// The rope runs from the collar to the thing it is tied to, and it sags below the straight line between them. The
	// points of the straight line are tried from its middle out, away from the collar and the thing. Below each point
	// on the screen, the middle of the first run of pixels that touch the rope is taken.
	const auto& camera = Locator::camera::value();
	const auto& rope = drawn->rope;
	auto& leashes = Locator::leashSystem::value();
	constexpr int k_Steps = 16;
	constexpr int k_MostPixelsBelow = 200;
	for (int tried = 0; tried < k_Steps - 1; ++tried)
	{
		const int step = (k_Steps / 2) + ((tried % 2 == 0) ? tried / 2 : -((tried + 1) / 2));
		const auto point = rope.end + (rope.start - rope.end) * (static_cast<float>(step) / static_cast<float>(k_Steps));
		glm::vec3 screen;
		if (!camera.ProjectWorldToScreen(point, {0.0f, 0.0f, size.x, size.y}, screen))
		{
			continue;
		}
		const auto pixel = glm::ivec2(glm::round(glm::vec2(screen)));
		std::optional<int> first;
		int last = 0;
		for (int below = 0; below < k_MostPixelsBelow; ++below)
		{
			const ecs::systems::LeashSystemInterface::RopeFrame frame {
			    .viewProjection = camera.GetViewProjectionMatrix(), .screenSize = size, .cursor = pixel + glm::ivec2(0, below)};
			const bool touches = leashes.RopeUnderCursor(PlayerNames::PLAYER_ONE, frame).has_value();
			if (touches)
			{
				first = first.value_or(below);
				last = below;
			}
			else if (first.has_value())
			{
				break;
			}
		}
		if (first.has_value())
		{
			pixels.push_back(pixel + glm::ivec2(0, (*first + last) / 2));
		}
	}
	return pixels;
}

std::optional<glm::ivec2> Runner::TipBubbleOnScreen(glm::vec2 size)
{
	if (!Locator::tipBubbleSystem::has_value() || !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto& bubble = Locator::tipBubbleSystem::value();
	const auto anchor = bubble.GetAnchor();
	if (!bubble.IsUp() || !anchor.has_value())
	{
		return std::nullopt;
	}
	// The bubble is placed on the screen as the interface draws it
	const auto clip = Locator::camera::value().GetViewProjectionMatrix() * glm::vec4(*anchor, 1.0f);
	if (clip.w <= 0.0f)
	{
		return std::nullopt;
	}
	const glm::vec2 pixel {(clip.x / clip.w + 1.0f) * size.x * 0.5f, (1.0f - clip.y / clip.w) * size.y * 0.5f};
	const auto placement = help::tip_bubble::Place(
	    pixel, clip.w, size.x, gui::ReferenceScale(static_cast<int32_t>(size.x), static_cast<int32_t>(size.y)));
	if (!placement.has_value())
	{
		return std::nullopt;
	}
	return glm::ivec2(glm::round((placement->min + placement->max) * 0.5f));
}

bool Runner::NextRopePixel()
{
	if (!_ropeSearch.has_value() || !Locator::gameActionSystem::has_value())
	{
		return false;
	}
	const auto pixels = RopeOnScreen(_ropeSearch->creature, WindowSize());
	if (_ropeSearch->next >= pixels.size())
	{
		return false;
	}
	auto& actions = Locator::gameActionSystem::value();
	auto pointer = actions.GetScriptedPointer().value_or(input::GameActionInterface::ScriptedPointer {});
	const auto to = pixels.at(_ropeSearch->next);
	++_ropeSearch->next;
	const auto moved = to - pointer.position;
	pointer.position = to;
	_framesSincePointerMoved = 0;
	actions.SetScriptedPointer(pointer);
	PushMotion(pointer, moved);
	return true;
}

std::optional<glm::ivec2> Runner::CaveCreatureOnScreen(glm::vec2 size)
{
	if (!Locator::creatureCaveSystem::has_value() || !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto& cave = Locator::creatureCaveSystem::value();
	const auto copy = cave.GetCaveCreature();
	if (!copy.has_value() || !Locator::temple::has_value())
	{
		return std::nullopt;
	}
	// The copy is a thing of the temple's rooms, and its creature is a thing of the land
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& copies = Locator::temple::value().GetRooms();
	if (!copies.Valid(*copy))
	{
		return std::nullopt;
	}
	const auto* transform = copies.TryGet<const ecs::components::Transform>(*copy);
	const auto* original = copies.TryGet<const ecs::components::CreatureCopy>(*copy);
	const auto* mesh = original != nullptr && registry.Valid(original->creature)
	                       ? registry.TryGet<const ecs::components::Mesh>(original->creature)
	                       : nullptr;
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (transform == nullptr || mesh == nullptr || !meshes.Contains(mesh->id))
	{
		return std::nullopt;
	}
	// From the creature's feet up to the middle of its body's bounding box, the first pixel where the cave finds the
	// creature under the pointer, as it is posed now
	const auto& camera = Locator::camera::value();
	const auto model = cave_creature::Placement(transform->position, transform->rotation, transform->scale);
	const auto middle = glm::vec3(model * glm::vec4(meshes.Handle(mesh->id)->GetBoundingBox().Center(), 1.0f));
	glm::vec3 feet;
	glm::vec3 top;
	const glm::vec4 viewport {0.0f, 0.0f, size.x, size.y};
	if (!camera.ProjectWorldToScreen(transform->position, viewport, feet) ||
	    !camera.ProjectWorldToScreen(middle, viewport, top))
	{
		return std::nullopt;
	}
	constexpr int k_Steps = 64;
	for (int step = 0; step <= k_Steps; ++step)
	{
		const auto pixel = glm::round(glm::mix(glm::vec2(feet), glm::vec2(top), static_cast<float>(step) / k_Steps));
		const auto [eye, near] = camera.OnNearPlane(pixel / size);
		if (cave.CursorOverCaveCreature({
		        .worldToClip = camera.GetViewProjectionMatrix(),
		        .resolution = size,
		        .near = camera.GetNearClip(),
		        .xScale = camera.GetProjectionMatrix()[0][0],
		        .camera = eye,
		        .cursor = pixel,
		    }))
		{
			return glm::ivec2(pixel);
		}
	}
	return std::nullopt;
}

std::string Runner::GivePointerCommand(const Command& command)
{
	if (!Locator::gameActionSystem::has_value())
	{
		return "no controls";
	}
	auto& actions = Locator::gameActionSystem::value();
	const auto size = WindowSize();
	auto pointer = actions.GetScriptedPointer().value_or(input::GameActionInterface::ScriptedPointer {
	    .position = glm::ivec2(actions.GetMousePosition()),
	});
	KeepDebugWindowsOffTheMouse(true);
	_ownsPointer = true;
	switch (command.kind)
	{
	case Kind::PointerOnOpponent:
	{
		const auto to = OpponentOnScreen(command.creature, size);
		if (!to.has_value())
		{
			return "no opponent on the screen";
		}
		const auto moved = *to - pointer.position;
		pointer.position = *to;
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerOnCreature:
	{
		const auto self = CreatureAt(command.creature);
		const auto to = self.has_value() ? BodyOnScreen(*self, size) : std::nullopt;
		if (!to.has_value())
		{
			return "no creature on the screen";
		}
		const auto moved = *to - pointer.position;
		pointer.position = *to;
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerOnCaveCreature:
	{
		const auto to = CaveCreatureOnScreen(size);
		if (!to.has_value())
		{
			return "no creature in the cave on the screen";
		}
		const auto moved = *to - pointer.position;
		pointer.position = *to;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerOnRope:
	{
		// The thing that the rope is tied to, or the creature, can hide a pixel of the rope from the pick of the hand. The
		// pointer tries the pixels in turn until the pick is the rope.
		actions.SetScriptedPointer(pointer);
		_ropeSearch = RopeSearch {.creature = command.creature, .next = 0};
		if (!NextRopePixel())
		{
			_ropeSearch.reset();
			return "no tied rope on the screen";
		}
		break;
	}
	case Kind::PointerOnTotem:
	{
		const auto totem = TotemAt(command.object);
		const auto* transform =
		    totem.has_value() ? Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(*totem) : nullptr;
		glm::vec3 screen;
		if (transform == nullptr || !Locator::camera::has_value() ||
		    !Locator::camera::value().ProjectWorldToScreen(transform->position + glm::vec3(0.0f, command.amount, 0.0f),
		                                                   {0.0f, 0.0f, size.x, size.y}, screen))
		{
			return "no totem on the screen";
		}
		const auto to = glm::ivec2(glm::round(glm::vec2(screen)));
		const auto moved = to - pointer.position;
		pointer.position = to;
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerOnTipBubble:
	{
		const auto to = TipBubbleOnScreen(size);
		if (!to.has_value())
		{
			return "no tip bubble on the screen";
		}
		const auto moved = *to - pointer.position;
		pointer.position = *to;
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerOnAltar:
	{
		const auto to = AltarOnScreen(size, command.value == 1);
		if (!to.has_value())
		{
			return command.value == 1 ? "no food pot on the screen" : "no altar on the screen";
		}
		const auto moved = *to - pointer.position;
		pointer.position = *to;
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerOnObject:
	{
		const auto to = ObjectOnScreen(command.object, command.amount, size, command.value);
		if (!to.has_value())
		{
			return "no object on the screen";
		}
		const auto moved = *to - pointer.position;
		pointer.position = *to;
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerTo:
	{
		const auto to = glm::ivec2(glm::round(command.point * size));
		const auto moved = to - pointer.position;
		pointer.position = to;
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerPress:
	case Kind::PointerRelease:
	{
		const bool press = command.kind == Kind::PointerPress;
		// A press waits for the game to know what the pointer is over, so it acts as a press of a hand at rest
		if (press && (!input::pointer_settle::PressMayStart(_framesSincePointerMoved) || _ropeSearch.has_value()))
		{
			_pressWaiting = command;
			break;
		}
		const auto button = static_cast<uint8_t>(command.value);
		pointer.buttons = press ? (pointer.buttons | SDL_BUTTON(button)) : (pointer.buttons & ~SDL_BUTTON(button));
		actions.SetScriptedPointer(pointer);
		SDL_Event event {};
		event.type = press ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
		event.button.windowID = Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
		event.button.button = button;
		event.button.state = press ? SDL_PRESSED : SDL_RELEASED;
		event.button.clicks = 1;
		event.button.x = pointer.position.x;
		event.button.y = pointer.position.y;
		input::MarkInjected(event);
		input::Device().PushEvent(event);
		_handWatchSeconds = 0.5f;
		break;
	}
	case Kind::PointerSweep:
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		_sweep = PointerSweep {.pixelsPerSecond = command.point * size / command.amount, .secondsLeft = command.amount};
		break;
	case Kind::WheelTurn:
	{
		actions.SetScriptedPointer(pointer);
		SDL_Event event {};
		event.type = SDL_MOUSEWHEEL;
		event.wheel.windowID = Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
		event.wheel.y = static_cast<int32_t>(command.value) * (command.ctrl ? -1 : 1);
		event.wheel.preciseY = static_cast<float>(event.wheel.y);
		input::MarkInjected(event);
		input::Device().PushEvent(event);
		break;
	}
	default:
		break;
	}
	return HandOnScreen();
}

void Runner::UpdatePointer(float seconds)
{
	++_framesSincePointerMoved;
	if (_ropeSearch.has_value() && input::pointer_settle::PressMayStart(_framesSincePointerMoved))
	{
		const bool onRope = Locator::pickingSystem::has_value() && Locator::pickingSystem::value().GetPick().rope;
		if (onRope || !NextRopePixel())
		{
			const auto line = fmt::format("{:.2f}s: {}: {}", _seconds,
			                              onRope ? "the pick of the hand is the rope" : "no pixel of the rope takes the pick",
			                              HandOnScreen());
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
			Log(line);
			_ropeSearch.reset();
		}
	}
	// The debug windows get the mouse again when the scenario's hand demonstration ends
	if (_demoOwnsPointer && (!Locator::handDemoSystem::has_value() || !Locator::handDemoSystem::value().IsReplaying()))
	{
		_demoOwnsPointer = false;
		if (!_ownsPointer)
		{
			KeepDebugWindowsOffTheMouse(false);
		}
	}
	if (_pressWaiting.has_value() && !_ropeSearch.has_value() && input::pointer_settle::PressMayStart(_framesSincePointerMoved))
	{
		const auto waiting = std::exchange(_pressWaiting, std::nullopt);
		static_cast<void>(GivePointerCommand(*waiting));
	}
	if (!Locator::gameActionSystem::has_value())
	{
		return;
	}
	auto& actions = Locator::gameActionSystem::value();
	// Once the commands are done and the buttons let go, the mouse is the player's again
	if (const auto pointer = actions.GetScriptedPointer(); _ownsPointer && pointer.has_value() && pointer->buttons == 0 &&
	                                                       !_sweep.has_value() && _timeline.done && _handWatchSeconds <= 0.0f)
	{
		actions.SetScriptedPointer(std::nullopt);
		KeepDebugWindowsOffTheMouse(false);
		_ownsPointer = false;
	}
	if (_handWatchSeconds > 0.0f || _sweep.has_value())
	{
		_handWatchSeconds -= seconds;
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Testbed {:.3f}s: {}", _seconds, HandOnScreen());
	}
	if (_sweep.has_value())
	{
		_framesSincePointerMoved = 0;
		auto pointer = actions.GetScriptedPointer().value_or(input::GameActionInterface::ScriptedPointer {});
		const auto step = std::min(seconds, _sweep->secondsLeft);
		const auto exact = _sweep->pixelsPerSecond * step + _sweep->remainder;
		const auto moved = glm::ivec2(exact);
		_sweep->remainder = exact - glm::vec2(moved);
		// While the camera turns with the mouse its pointer is held, and only the movement comes through
		if (!actions.IsCursorFrozen())
		{
			pointer.position = glm::clamp(pointer.position + moved, glm::ivec2(0), glm::ivec2(WindowSize()) - 1);
		}
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		if (_sweepMovesHeldHand)
		{
			MoveHeldHand(pointer.position);
		}
		_sweep->secondsLeft -= step;
		if (_sweep->secondsLeft <= 0.0f)
		{
			_sweep.reset();
			const bool holds = Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GetHeld().has_value();
			const auto line =
			    fmt::format("{:.2f}s: moved: {}{}", _seconds, HandOnScreen(),
			                _sweepMovesHeldHand ? (holds ? ", the hand still holds" : ", the hand is empty") : "");
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
			Log(line);
		}
	}
}

void Runner::MoveHeldHand(glm::ivec2 pointer)
{
	if (!_heldHand.has_value() || !Locator::camera::has_value() || !Locator::terrainSystem::has_value() ||
	    !Locator::magicSystem::has_value())
	{
		return;
	}
	glm::vec3 origin {0.0f};
	glm::vec3 direction {0.0f};
	Locator::camera::value().DeprojectScreenToWorld(glm::vec2(pointer) / WindowSize(), origin, direction);
	const auto at = testbed::hand_sweep::OnPlane(origin, direction, _heldHand->point.value_or(_heldHand->handPosition).y);
	if (!at.has_value())
	{
		return;
	}
	const glm::vec3 point {at->x, Locator::terrainSystem::value().GetHeightAt(*at), at->y};
	const glm::vec3 hand = point + glm::vec3(0.0f, _heldHandHeight, 0.0f);
	_heldHand->handPosition = hand;
	_heldHand->point = point;
	_heldHand->rayOrigin = hand + glm::vec3(0.0f, k_HandEyeHeight, 0.0f);
	Locator::magicSystem::value().DriveHand(*_heldHand);
}

std::string Runner::HandOnScreen() const
{
	if (!Locator::gameActionSystem::has_value() || !Locator::handSystem::has_value() || !Locator::camera::has_value())
	{
		return {};
	}
	const auto cursor = Locator::gameActionSystem::value().GetMousePosition();
	const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
	const auto& transform = Locator::entitiesRegistry::value().Get<Transform>(hand);
	const auto& position = transform.position;
	const auto size = WindowSize();
	glm::vec3 screen {0.0f};
	Locator::camera::value().ProjectWorldToScreen(position, {0.0f, 0.0f, size.x, size.y}, screen);
	const auto& camera = Locator::camera::value();
	const auto cues = camera.GetModel().GetHandCues();
	constexpr std::array<std::string_view, 4> k_DragModes {"pan", "edge rotate", "pitch", "pitch from the top"};
	const auto drag = !cues.dragging              ? std::string_view("none")
	                  : cues.dragMode.has_value() ? k_DragModes.at(static_cast<size_t>(*cues.dragMode))
	                                              : std::string_view("undecided");
	return fmt::format("cursor ({}, {}), hand ({:.0f}, {:.0f}) at ({:.1f}, {:.1f}, {:.1f}), {:.1f} from the camera, scale "
	                   "{:.3f}, hints {:#x}, drag {}, camera heading {:.3f} pitch {:.3f}{}{}",
	                   cursor.x, cursor.y, screen.x, screen.y, position.x, position.y, position.z,
	                   glm::distance(position, camera.GetOrigin()), transform.scale.y, cues.tricons, drag,
	                   camera.GetRotation().y, camera.GetRotation().x,
	                   Locator::gameActionSystem::value().IsCursorFrozen() ? ", held" : "", HeldByHand());
}
