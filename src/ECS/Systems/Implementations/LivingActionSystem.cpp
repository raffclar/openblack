/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LivingActionSystem.h"

#include <bitset>

#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/LivingPhysics.h"
#include "ECS/LivingTurn.h"
#include "ECS/LivingWalkPath.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerBirth.h"
#include "ECS/Villager/VillagerBuild.h"
#include "ECS/Villager/VillagerChild.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerEmergency.h"
#include "ECS/Villager/VillagerFarmer.h"
#include "ECS/Villager/VillagerFisherman.h"
#include "ECS/Villager/VillagerFlock.h"
#include "ECS/Villager/VillagerFood.h"
#include "ECS/Villager/VillagerForester.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerInteract.h"
#include "ECS/Villager/VillagerMourning.h"
#include "ECS/Villager/VillagerOriginalFns.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerSoul.h"
#include "ECS/Villager/VillagerStateTable.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerDrowning.h"
#include "Enums.h"
#include "GameClock.h"
#include "Locator.h"
#include "VillagerFire.h"
#include "VillagerReactions.h"
#include "VillagerResourceReactions.h"
#include "VillagerShield.h"
#include "VillagerTeleport.h"
#include "VillagerWorship.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

uint32_t VillagerInvalidState(LivingAction& action)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("ai"), "Villager #{}: Stuck in an invalid state",
	                    static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)));
	assert(false);
	return 0;
}

namespace
{
/// The walk's result for openblack's walk (the step PathfindingSystem::Step has just taken, living_turn::MoveToStep):
/// 0xA (arrived) only from ARRIVED when already there and from FINAL_STEP, both after putting the object on the goal.
/// PathfindingSystem sets FINAL_STEP when there (its step 5) and puts the villager on the goal on the next turn
/// (ApplyStepGoal<FinalStep>, step 4b; ARRIVED the same): arrived when the tag is there and the villager stands on its
/// goal, as the original returns 0xA one turn after STEP_THROUGH set FINAL_STEP. Anything else is a walk still going
/// (0 / 1 / 6 / 7: MOVE_TO_POS does nothing). There is no "abandoned" result (PathfindingSystem's AbandonMove goes on
/// as STEP_THROUGH).
uint32_t WallHugMoveToResult(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return 0;
	}
	const auto at = glm::xz(transform->position);
	if (const auto* finalStep = registry.TryGet<const MoveStateFinalStepTag>(entity);
	    finalStep != nullptr && finalStep->stepGoal == at)
	{
		return 0xA;
	}
	if (const auto* arrived = registry.TryGet<const MoveStateArrivedTag>(entity); arrived != nullptr && arrived->stepGoal == at)
	{
		return 0xA;
	}
	return 1;
}

// state 1 MOVE_TO_POS: the walk and, only on 0xA, the final state. Returns the walk's result
uint32_t VillagerMoveToPos(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.ToEntity(action);

	// a villager in a hand does not walk (0). (approximate) only the local hand is asked (HandSystem::GetHeldObject)
	if (Locator::handSystem::has_value())
	{
		if (const auto held = Locator::handSystem::value().GetHeldObject(); held.has_value() && *held == entity)
		{
			return 0;
		}
	}
	ecs::living_turn::MoveToStep(entity);
	const auto result = WallHugMoveToResult(entity);
	if (result == 0xA)
	{
		registry.Remove<MoveStateFinalStepTag, MoveStateArrivedTag>(entity);
		// SetTopStateToFinal: the pause roll, the exit of MOVE_TO_POS (the circle hug reset; not ported: taken as 1) and
		// FINAL's exit / entry
		const auto final = static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Final)));
		if (final != VillagerStates::InvalidState)
		{
			ecs::villager::SetTopStateToFinal(entity);
		}
		else
		{
			// a walk set up by openblack's own code with FINAL 0 (the debug tools' "Move To Point"), where the original
			// would SetTopState(0): back to deciding through the compat path
			Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top,
			                                                      VillagerStates::DecideWhatToDo, true);
		}
	}
	return result;
}
// FLYING / IN_HAND: the physics and the hand move the villager; nothing to decide meanwhile
uint32_t VillagerCarried([[maybe_unused]] LivingAction& action)
{
	return 0;
}

// 11 LANDED. Its clip is VillagerLandedClip. Always 1
uint32_t VillagerLanded(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.ToEntity(action);
	auto* v = registry.TryGet<Villager>(entity);
	if (v == nullptr)
	{
		return 1;
	}
	// still "in the hand" (put down gently, the physics kept the flag) and a disciple
	if ((v->flags & Villager::k_FlagInHand) != 0 && (v->flags & Villager::k_FlagDisciple) != 0)
	{
		const uint8_t disciple = v->discipleType;
		// TODO(creature): on the first turn, the player who last dropped it may make the creature copy it (the disciple's
		// action), plus a further player call (not identified) when its disciple type changed
		// wait for the landing clip (return 1)
		if (!ecs::VillagerAnimationDone(entity, action.turnsSinceStateChange))
		{
			return 1;
		}
		// the in-hand reactions it started go; the in-hand flag off
		ecs::effects::reactions::RemoveAllReactionsOfTypeInitiatedBy(entity, openblack::Reaction::ReactToVillagerInHand);
		v->flags = static_cast<uint16_t>(v->flags & ~Villager::k_FlagInHand);
		if (disciple != 0)
		{
			// TODO: objects close enough to interact with around it; found -> the town change, then the disciple's begin
			// state or an inspection of the object (INSPECT_OBJECT), whose object sends it to 171-175 CHECK_INTERACT_WITH_*
			// (VillagerInteract.h), return 1; nothing found -> the plain landing below. Not ported: the plain landing, as when
			// nothing is found
		}
	}
	// the in-hand reactions it started go; the clip, then DECIDE_WHAT_TO_DO
	ecs::effects::reactions::RemoveAllReactionsOfTypeInitiatedBy(entity, openblack::Reaction::ReactToVillagerInHand);
	ecs::villager::PlayAnimThenSetState(entity, VillagerStates::DecideWhatToDo);
	return 1;
}
} // namespace

using ecs::villager::VillagerStateTableEntry;

namespace
{
/// A table row the original has and openblack has not ported: its state function does nothing (0) and warns once per
/// state; the entry, exit and validate slots are empty (as "no function": 1), and LivingActionSystem::VillagerCallEntry
/// / VillagerCallExit / VillagerCallValidate warn once if the original has one there (VillagerOriginalFns.h)
uint32_t TodoState(LivingAction& action)
{
	static std::bitset<256> warned;
	const auto state = static_cast<size_t>(action.states.at(static_cast<size_t>(LivingAction::Index::Top)));
	if (!warned.test(state))
	{
		warned.set(state);
		SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented state function: {} {}",
		                   static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)), state,
		                   k_VillagerStateStrings.at(std::min<size_t>(state, k_VillagerStateStrings.size() - 1)));
	}
	return 0;
}

/// the worship exits keep the old convention (false = it may leave): the row adapts them to the original's
/// (1 = it may leave)
uint32_t OldExit(bool refused)
{
	return refused ? 0u : 1u;
}
} // namespace

static const VillagerStateTableEntry k_TodoEntry = {
    .state = &TodoState,
    .saveState = [](LivingAction& action) -> bool {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented save state function: {}",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(
	                           Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top))));
	    return false;
    },
    .loadState = [](LivingAction& action) -> bool {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented load state function: {}",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(
	                           Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top))));
	    return false;
    },
    // the town emergency's answer has no default: villager::ReactsToTownEmergency reads the original's column
    // for every row, ported or not (k_TownEmergencyReaction, VillagerOriginalFns.h)
    .field0x60 = [](LivingAction& action) -> bool {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented field0x60 state function: {}",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(
	                           Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top))));
	    return false;
    },
    .transitionAnimation = [](LivingAction& action) -> int {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented transition animation function: {}",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(
	                           Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top))));
	    return -1;
    },
};

/// A not-ported row whose exit is the shared ExitReaction: the state is a TODO, its exit is the shared one
static VillagerStateTableEntry TodoWithExitReaction()
{
	auto entry = k_TodoEntry;
	entry.exitState = &ecs::villager_reactions::ExitReaction;
	return entry;
}

/// A not-ported row whose state function walks (living_turn::k_WalkStates: it reaches the walk directly or through
/// MOVE_TO_POS, the move to an object or the dance): the walk's step
/// (living_turn::MoveToStep), then the TODO. (approximate) The rest of the function and the use of MoveTo's result are
/// not ported (a villager walking a footpath, state 29, stops at the node)
static VillagerStateTableEntry TodoWalk(VillagerStateTableEntry entry = k_TodoEntry)
{
	entry.state = [](LivingAction& action) -> uint32_t {
		ecs::living_turn::MoveToStep(Locator::entitiesRegistry::value().ToEntity(action));
		return TodoState(action);
	};
	return entry;
}

const static std::array<VillagerStateTableEntry, static_cast<size_t>(VillagerStates::_COUNT)> k_VillagerStateTable = {
    /* INVALID_STATE */ VillagerStateTableEntry {
        .state = &VillagerInvalidState,
    },
    /* MOVE_TO_POS */
    VillagerStateTableEntry {
        .state = &VillagerMoveToPos,
    },
    /* MOVE_TO_OBJECT */ TodoWalk(),
    /* MOVE_ON_STRUCTURE */ TodoWalk(),
    // the script states (ECS/Villager/VillagerScript.h); save / load not ported
    /* IN_SCRIPT */
    {.state = &ecs::villager::StateInScript,
     .entryState = &ecs::villager::EnterInScript,
     .exitState = &ecs::villager::ExitInScript,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    /* IN_DANCE */ TodoWalk(),
    /* FLEEING_FROM_OBJECT_REACTION */ TodoWithExitReaction(),
    /* LOOKING_AT_OBJECT_REACTION */ TodoWithExitReaction(),
    /* FOLLOWING_OBJECT_REACTION */ TodoWithExitReaction(),
    /* INSPECT_OBJECT_REACTION */ TodoWithExitReaction(),
    /* FLYING */ {.state = &VillagerCarried},
    /* LANDED */ {.state = &VillagerLanded},
    /* LOOK_AT_FLYING_OBJECT_REACTION */ k_TodoEntry,
    // the death (ECS/Villager/VillagerDeath.cpp): no entry; DEAD's exit can never leave
    /* SET_DYING */
    VillagerStateTableEntry {
        .state = &ecs::villager::SetDyingState,
    },
    /* DYING */
    VillagerStateTableEntry {
        .state = &ecs::villager::Dying,
    },
    /* DEAD */
    VillagerStateTableEntry {
        .state = &ecs::villager::Dead,
        .exitState = &ecs::villager::CannotExitState,
    },
    // the water's state (ECS/VillagerDrowning). Its entry and exit only accept
    /* DROWNING */
    {.state = &openblack::ecs::VillagerDrowningState,
     .entryState = [](LivingAction&, VillagerStates, VillagerStates) -> uint32_t { return 1; },
     .exitState = [](LivingAction&, VillagerStates) -> uint32_t { return 1; }},
    /* DOWNED */ {.state = &VillagerCarried}, // caught by a predator: the animal AI drives it (ECS/AnimalPredators)
    /* BEING_EATEN */ {.state = &VillagerCarried},
    // reactions 7 / 12 (VillagerResourceReactions.cpp): the shared ExitReaction; validate ReactionValidate
    // (VillagerCallValidate); always reacts to the town emergency
    /* GOTO_FOOD_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_resource_reactions::GotoFoodReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_FOOD_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_resource_reactions::ArrivesAtFoodReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* GOTO_WOOD_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_resource_reactions::GotoWoodReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_WOOD_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_resource_reactions::ArrivesAtWoodReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // no entry or exit; always reacts to the town emergency; save / load not ported
    /* WAIT_FOR_ANIMATION */
    {.state = &ecs::villager::WaitForAnimation,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState,
     .field0x50 = k_TodoEntry.field0x50},
    /* IN_HAND */ {.state = &VillagerCarried},
    /* GOTO_PICKUP_BALL_REACTION */ k_TodoEntry,
    /* ARRIVES_AT_PICKUP_BALL_REACTION */ k_TodoEntry,
    // no entry, exit or validate (ECS/Villager/VillagerFlock.h); save / load not ported (k_TodoEntry's)
    /* MOVE_IN_FLOCK */
    {.state = &ecs::villager::MoveInFlock, .saveState = k_TodoEntry.saveState, .loadState = k_TodoEntry.loadState},
    // entry EnterInScript, exit ExitInScript (ECS/LivingWalkPath.h); save / load not ported (k_TodoEntry's)
    /* MOVE_ALONG_PATH */
    {.state = &ecs::living::MoveAlongPath,
     .entryState = &ecs::villager::EnterInScript,
     .exitState = &ecs::villager::ExitInScript,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    /* MOVE_ON_PATH */ TodoWalk(),
    /* FLEEING_AND_LOOKING_AT_OBJECT_REACTION */ k_TodoEntry,
    // carrying (VillagerResources.cpp): no entry, no exit; always reacts to the town emergency
    /* GOTO_STORAGE_PIT_FOR_DROP_OFF */
    VillagerStateTableEntry {
        .state = &ecs::villager::GotoStoragePitForDropOffState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF (clip 347) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtStoragePitForDropOff,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // the food (VillagerFood.cpp, the storage pit): no entry; always reacts to the town emergency
    /* GOTO_STORAGE_PIT_FOR_FOOD */
    VillagerStateTableEntry {
        .state = &ecs::villager::GotoStoragePitForFood,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_STORAGE_PIT_FOR_FOOD */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtStoragePitForFood,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_HOME_WITH_FOOD (the housewife's); exit ExitAtHome */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtHomeWithFood,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // the home (VillagerHome.cpp): the exit of 35..38, 118..121 is ExitAtHome (it leaves home unless the next
    // state stays at home)
    /* GO_HOME = DoGoingHome(37, 238); always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoHomeState,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_HOME: always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesHomeState,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* AT_HOME: the home decision (clip -4: not drawn) */
    VillagerStateTableEntry {
        .state = &ecs::villager::AtHome,
        .exitState = &ecs::villager::ExitAtHome,
    },
    // the builders (VillagerBuild.cpp): entry EnterBuilding (0 = refused -> 163), exit ExitBuilding; always reacts to
    // the town emergency; save / load not ported
    /* ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS (clip 340) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtStoragePitForBuildingMaterials,
        .entryState = &ecs::villager::EnterBuilding,
        .exitState = &ecs::villager::ExitBuilding,
        .saveState = k_TodoEntry.saveState,
        .loadState = k_TodoEntry.loadState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_BUILDING_SITE (clip 348) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtBuildingSite,
        .entryState = &ecs::villager::EnterBuilding,
        .exitState = &ecs::villager::ExitBuilding,
        .saveState = k_TodoEntry.saveState,
        .loadState = k_TodoEntry.loadState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* BUILDING (the building clip) */
    VillagerStateTableEntry {
        .state = &ecs::villager::BuildingState,
        .entryState = &ecs::villager::EnterBuilding,
        .exitState = &ecs::villager::ExitBuilding,
        .saveState = k_TodoEntry.saveState,
        .loadState = k_TodoEntry.loadState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* GOTO_STORAGE_PIT_FOR_WORSHIP_SUPPLIES */ k_TodoEntry,
    /* ARRIVES_AT_STORAGE_PIT_FOR_WORSHIP_SUPPLIES */ k_TodoEntry,
    /* GOTO_WORSHIP_SITE_WITH_SUPPLIES */ k_TodoEntry,
    /* MOVE_TO_WORSHIP_SITE_WITH_SUPPLIES */ k_TodoEntry,
    /* ARRIVES_AT_WORSHIP_SITE_WITH_SUPPLIES */ k_TodoEntry,
    // the foresters (VillagerForester.cpp): exit ExitForesting for 47-50 and 52; always reacts to the town
    // emergency
    /* FORESTER_MOVE_TO_FOREST (MOVE_TO_POS first; the walk's validate not ported) */
    VillagerStateTableEntry {
        .state = [](LivingAction& action) -> uint32_t {
	        return ecs::villager::ForesterMoveToForest(action, VillagerMoveToPos(action));
        },
        .exitState = &ecs::villager::ExitForesting,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FORESTER_GOTO_FOREST: the wood desire check */
    VillagerStateTableEntry {
        .state = &ecs::villager::ForesterGotoForest,
        .exitState = &ecs::villager::ExitForesting,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FORESTER_ARRIVES_AT_FOREST (clip 217) */
    VillagerStateTableEntry {
        .state = [](LivingAction& action) -> uint32_t { return ecs::villager::ForesterArrivesAtForest(action); },
        .exitState = &ecs::villager::ExitForesting,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FORESTER_CHOPS_TREE (clip 217) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ForesterChopsTree,
        .exitState = &ecs::villager::ExitForesting,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // (not ported) never entered in the shipped game: no code changes to this state
    /* FORESTER_CHOPS_TREE_FOR_BUILDING */ k_TodoEntry,
    /* FORESTER_FINISHED_FORESTERING */
    VillagerStateTableEntry {
        .state = [](LivingAction& action) -> uint32_t { return ecs::villager::ForesterFinishedForestering(action); },
        .exitState = &ecs::villager::ExitForesting,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_BIG_FOREST (no entry, no exit) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtBigForest,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // (not ported) never entered in the shipped game
    /* ARRIVES_AT_BIG_FOREST_FOR_BUILDING */ k_TodoEntry,
    // the fishermen (VillagerFisherman.cpp): entry EnterFishing, exit ExitFishing; always reacts to the town
    // emergency
    /* FISHERMAN_ARRIVES_AT_FISHING */
    VillagerStateTableEntry {
        .state = &ecs::villager::FishermanArrivesAtFishing,
        .entryState = &ecs::villager::EnterFishing,
        .exitState = &ecs::villager::ExitFishing,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FISHING (clip 262 P_FISHERMAN) */
    VillagerStateTableEntry {
        .state = &ecs::villager::Fishing,
        .entryState = &ecs::villager::EnterFishing,
        .exitState = &ecs::villager::ExitFishing,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // no entry or exit: the states that park a villager for a while (the amazed villager of VillagerShield.cpp) need
    // it to come back to their final state
    /* WAIT_FOR_COUNTER */ {.state = &ecs::villager::WaitForCounter},
    // the worship states (VillagerWorship.cpp). 58 is the original's footpath walk: openblack
    // walks with the WallHug inside 59, so GotoWorshipSiteForWorship sets 59 straight away and 58 is only entered when a
    // reaction's state is popped (PopFromPrevious resumes 59 as 58), where its own state function,
    // GotoWorshipSiteForWorship, starts the walk again (approximate: the footpath walk is not ported).
    // (their exits keep the old convention, false = it may leave: OldExit adapts them)
    /* GOTO_WORSHIP_SITE_FOR_WORSHIP */
    {.state = &ecs::villager_worship::GotoWorshipSiteForWorshipState,
     .exitState = [](LivingAction& a,
                     VillagerStates n) { return OldExit(ecs::villager_worship::ExitMoveToWorshipSite(a, n)); }},
    /* ARRIVES_AT_WORSHIP_SITE_FOR_WORSHIP */
    {.state = &ecs::villager_worship::ArrivesAtWorshipSiteForWorship,
     .exitState = [](LivingAction& a,
                     VillagerStates n) { return OldExit(ecs::villager_worship::ExitMoveToWorshipSite(a, n)); }},
    /* WORSHIPPING_AT_WORSHIP_SITE */
    {.state = &ecs::villager_worship::WorshippingAtWorshipSite,
     .exitState = [](LivingAction& a, VillagerStates n) { return OldExit(ecs::villager_worship::ExitAtWorshipSite(a, n)); }},
    /* GOTO_ALTAR_FOR_REST */ k_TodoEntry,
    /* ARRIVES_AT_ALTAR_FOR_REST */ k_TodoEntry,
    /* AT_ALTAR_REST */ k_TodoEntry,
    /* AT_ALTAR_FINISHED_REST */ k_TodoEntry,
    /* RESTART_WORSHIPPING_AT_WORSHIP_SITE */ k_TodoEntry,
    /* RESTART_WORSHIPPING_CREATURE */ TodoWalk(),
    // the farmers (VillagerFarmer.cpp): entry EnterFarming (0 = refused -> 163), exit ExitFarming; always reacts to
    // the town emergency
    /* FARMER_ARRIVES_AT_FARM (clip 259 P_FARMER_SOWING_SEEDS) */
    VillagerStateTableEntry {
        .state = &ecs::villager::FarmerArrivesAtFarm,
        .entryState = &ecs::villager::EnterFarming,
        .exitState = &ecs::villager::ExitFarming,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FARMER_PLANTS_CROP (clip 259) */
    VillagerStateTableEntry {
        .state = &ecs::villager::FarmerPlantsCrop,
        .entryState = &ecs::villager::EnterFarming,
        .exitState = &ecs::villager::ExitFarming,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FARMER_DIGS_UP_CROP (clip 257 P_FARMER_HARVESTING) */
    VillagerStateTableEntry {
        .state = &ecs::villager::FarmerDigsUpCrop,
        .entryState = &ecs::villager::EnterFarming,
        .exitState = &ecs::villager::ExitFarming,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* MOVE_TO_FOOTBALL_PITCH_CONSTRUCTION */ k_TodoEntry,
    /* FOOTBALL_WALK_TO_POSITION */ TodoWalk(),
    /* FOOTBALL_WAIT_FOR_KICK_OFF */ k_TodoEntry,
    /* FOOTBALL_ATTACKER */ k_TodoEntry,
    /* FOOTBALL_GOALIE */ k_TodoEntry,
    /* FOOTBALL_DEFENDER */ k_TodoEntry,
    /* FOOTBALL_WON_GOAL */ k_TodoEntry,
    /* FOOTBALL_LOST_GOAL */ k_TodoEntry,
    /* START_MOVE_TO_PICK_UP_BALL_FOR_DEAD_BALL */ k_TodoEntry,
    /* ARRIVED_AT_PICK_UP_BALL_FOR_DEAD_BALL */ k_TodoEntry,
    /* ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_START */ k_TodoEntry,
    /* ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_END */ k_TodoEntry,
    /* FOOTBALL_MATCH_PAUSED */ k_TodoEntry,
    /* FOOTBALL_WATCH_MATCH */ k_TodoEntry,
    /* FOOTBALL_MEXICAN_WAVE */ k_TodoEntry,
    /* CREATED (no entry or exit; always reacts to the town emergency) */
    VillagerStateTableEntry {
        .state = &ecs::villager::VillagerCreated,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_IN_ABODE_TO_TRADE */ k_TodoEntry,
    /* ARRIVES_IN_ABODE_TO_PICK_UP_EXCESS */ k_TodoEntry,
    /* MAKE_SCARED_STIFF */ k_TodoEntry,
    /* SCARED_STIFF */ k_TodoEntry,
    /* WORSHIPPING_CREATURE */ TodoWalk(),
    /* SHEPHERD_LOOK_FOR_FLOCK */ k_TodoEntry,
    /* SHEPHERD_MOVE_FLOCK_TO_WATER */ k_TodoEntry,
    /* SHEPHERD_MOVE_FLOCK_TO_FOOD */ k_TodoEntry,
    /* SHEPHERD_MOVE_FLOCK_BACK */ k_TodoEntry,
    /* SHEPHERD_DECIDE_WHAT_TO_DO_WITH_FLOCK */ k_TodoEntry,
    /* SHEPHERD_WAIT_FOR_FLOCK */ k_TodoEntry,
    /* SHEPHERD_SLAUGHTER_ANIMAL */ k_TodoEntry,
    /* SHEPHERD_FETCH_STRAY */ k_TodoEntry,
    /* SHEPHERD_GOTO_FLOCK */ k_TodoEntry,
    /* HOUSEWIFE_AT_HOME */ k_TodoEntry,
    /* HOUSEWIFE_GOTO_STORAGE_PIT */ k_TodoEntry,
    /* HOUSEWIFE_ARRIVES_AT_STORAGE_PIT */ k_TodoEntry,
    /* HOUSEWIFE_PICKUP_FROM_STORAGE_PIT */ k_TodoEntry,
    /* HOUSEWIFE_RETURN_HOME_WITH_FOOD */ k_TodoEntry,
    /* HOUSEWIFE_MAKE_DINNER */ k_TodoEntry,
    /* HOUSEWIFE_SERVES_DINNER */ k_TodoEntry,
    /* HOUSEWIFE_CLEARS_AWAY_DINNER */ k_TodoEntry,
    /* HOUSEWIFE_DOES_HOUSEWORK */ k_TodoEntry,
    /* HOUSEWIFE_GOSSIPS_AROUND_STORAGE_PIT */ k_TodoEntry,
    /* HOUSEWIFE_STARTS_GIVING_BIRTH (VillagerBirth.cpp; no entry or exit; (inferred) no code sets 110: the woman's
       special turn calls the function itself) */
    VillagerStateTableEntry {
        .state = &ecs::villager::HousewifeStartsGivingBirthState,
    },
    /* HOUSEWIFE_GIVING_BIRTH (VillagerBirth.cpp; no entry or exit) */
    VillagerStateTableEntry {
        .state = &ecs::villager::HousewifeGivingBirthState,
    },
    /* HOUSEWIFE_GIVEN_BIRTH (VillagerBirth.cpp; no entry or exit) */
    VillagerStateTableEntry {
        .state = &ecs::villager::HousewifeGivenBirth,
    },
    /* CHILD_AT_CRECHE (VillagerChild.cpp; no entry or exit; always reacts to the town emergency) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ChildAtCreche,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* CHILD_FOLLOWS_MOTHER (VillagerDecide.cpp; no entry or exit; always reacts to the town emergency) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ChildFollowsMother,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* CHILD_BECOMES_ADULT (VillagerAge.cpp; (inferred) only a script sets 115) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ChildBecomesAdultState,
    },
    /* SITS_DOWN_TO_DINNER */ k_TodoEntry,
    /* EAT_FOOD (VillagerFood.cpp; clip 254 EatDinner); always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::EatFood,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* EAT_FOOD_AT_HOME (clip -4) */
    VillagerStateTableEntry {
        .state = &ecs::villager::EatFoodAtHome,
        .exitState = &ecs::villager::ExitAtHome,
    },
    /* GOTO_BED_AT_HOME (clip -4) */
    VillagerStateTableEntry {
        .state = &ecs::villager::GotoBedAtHome,
        .exitState = &ecs::villager::ExitAtHome,
    },
    /* SLEEPING_AT_HOME (clip -4) */
    VillagerStateTableEntry {
        .state = &ecs::villager::SleepingAtHome,
        .exitState = &ecs::villager::ExitAtHome,
    },
    /* WAKE_UP_AT_HOME = GO_HOME (no code sets 121); always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::WakeUpAtHome,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* START_HAVING_SEX */ k_TodoEntry,
    /* HAVING_SEX */ k_TodoEntry,
    /* STOP_HAVING_SEX */ k_TodoEntry,
    /* START_HAVING_SEX_AT_HOME */ k_TodoEntry,
    /* HAVING_SEX_AT_HOME */ k_TodoEntry,
    /* STOP_HAVING_SEX_AT_HOME */ k_TodoEntry,
    /* WAIT_FOR_DINNER */ k_TodoEntry,
    /* HOMELESS_START (VillagerHome.cpp); always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::HomelessStart,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* VAGRANT_START: always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::VagrantStartState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* MORN_DEATH = GO_HOME (no entry or exit; the mourning into / out-of clip) */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoHomeState,
    },
    /* PERFORM_INSPECTION_REACTION */ k_TodoEntry,
    /* APPROACH_OBJECT_REACTION */ k_TodoEntry,
    /* INITIALISE_TELL_OTHERS_ABOUT_OBJECT */ k_TodoEntry,
    /* TELL_OTHERS_ABOUT_INTERESTING_OBJECT */ k_TodoEntry,
    /* APPROACH_VILLAGER_TO_TALK_TO */ k_TodoEntry,
    /* TELL_PARTICULAR_VILLAGER_ABOUT_OBJECT */ k_TodoEntry,
    /* INITIALISE_LOOK_AROUND_FOR_VILLAGER_TO_TELL */ k_TodoEntry,
    /* LOOK_AROUND_FOR_VILLAGER_TO_TELL */ k_TodoEntry,
    /* MOVE_TOWARDS_OBJECT_TO_LOOK_AT */ TodoWalk(),
    /* INITIALISE_IMPRESSED_REACTION */ k_TodoEntry,
    /* PERFORM_IMPRESSED_REACTION */ k_TodoEntry,
    /* INITIALISE_FIGHT_REACTION */ k_TodoEntry,
    /* PERFORM_FIGHT_REACTION */ k_TodoEntry,
    /* HOMELESS_EAT_DINNER */ k_TodoEntry,
    /* INSPECT_CREATURE_REACTION */ k_TodoEntry,
    /* PERFORM_INSPECT_CREATURE_REACTION */ k_TodoEntry,
    /* APPROACH_CREATURE_REACTION */ k_TodoEntry,
    /* INITIALISE_BEWILDERED_BY_MAGIC_TREE_REACTION */ k_TodoEntry,
    /* PERFORM_BEWILDERED_BY_MAGIC_TREE_REACTION */ k_TodoEntry,
    /* TURN_TO_FACE_MAGIC_TREE */ k_TodoEntry,
    /* LOOK_AT_MAGIC_TREE */ k_TodoEntry,
    /* DANCE_FOR_EDITING_PURPOSES */ TodoWalk(),
    /* MOVE_TO_DANCE_POS */ TodoWalk(),
    /* INITIALISE_RESPECT_CREATURE_REACTION */ k_TodoEntry,
    /* PERFORM_RESPECT_CREATURE_REACTION */ k_TodoEntry,
    /* FINISH_RESPECT_CREATURE_REACTION */ k_TodoEntry,
    /* APPROACH_HAND_REACTION */ k_TodoEntry,
    /* FLEEING_FROM_CREATURE_REACTION */ k_TodoEntry,
    /* TURN_TO_FACE_CREATURE_REACTION */ k_TodoEntry,
    /* WATCH_FLYING_OBJECT_REACTION */ k_TodoEntry,
    /* POINT_AT_FLYING_OBJECT_REACTION */ k_TodoEntry,
    /* DECIDE_WHAT_TO_DO (VillagerDecide.cpp; no entry or exit) */
    VillagerStateTableEntry {
        .state = &ecs::villager::DecideWhatToDo,
    },
    /* INTERACT_DECIDE_WHAT_TO_DO */ k_TodoEntry,
    /* EAT_OUTSIDE */ k_TodoEntry,
    /* RUN_AWAY_FROM_OBJECT_REACTION */ k_TodoEntry,
    /* MOVE_TOWARDS_CREATURE_REACTION */ TodoWalk(),
    // the shield's state (VillagerShield.cpp); its exit is the shared ExitReaction
    /* AMAZED_BY_MAGIC_SHIELD_REACTION */
    {.state = &ecs::villager_shield::AmazedByMagicShieldReaction, .exitState = &ecs::villager_reactions::ExitReaction},
    /* VILLAGER_GOSSIPS */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_ANIMAL */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_WORSHIP_SITE */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_ABODE (pending: VillagerInteract.h) */
    k_TodoEntry,
    // the disciple's inspection (VillagerInteract.cpp): no entry / exit; always reacts to the town emergency
    /* CHECK_INTERACT_WITH_FIELD */
    {.state = &ecs::villager::CheckInteractWithField},
    /* CHECK_INTERACT_WITH_FISH_FARM */
    {.state = &ecs::villager::CheckInteractWithFishFarm},
    /* CHECK_INTERACT_WITH_TREE */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_BALL */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_POT */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_FOOTBALL */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_VILLAGER */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_MAGIC_LIVING */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_ROCK */ k_TodoEntry,
    /* ARRIVES_AT_ROCK_FOR_WOOD */ k_TodoEntry,
    /* GOT_WOOD_FROM_ROCK */ k_TodoEntry,
    /* REENTER_BUILDING_STATE (also the "after" state of the building rows: a builder resumes here after a
       reaction) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ReenterBuildingState,
        .entryState = &ecs::villager::EnterBuilding,
        .exitState = &ecs::villager::ExitBuilding,
        .saveState = k_TodoEntry.saveState,
        .loadState = k_TodoEntry.loadState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // (not ported) never entered in the shipped game: the clear-area check finds no pushable object
    /* ARRIVE_AT_PUSH_OBJECT */ k_TodoEntry,
    // (VillagerForester.cpp): reached only from the interaction tables; no entry, no exit; always reacts to the
    // town emergency
    /* TAKE_WOOD_FROM_TREE (the foresting clip) */
    VillagerStateTableEntry {
        .state = &ecs::villager::TakeWoodFromTree,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* TAKE_WOOD_FROM_POT (always 1) */
    VillagerStateTableEntry {
        .state = &ecs::villager::TakeWoodFromPot,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // (not ported) 188 / 189 never entered in the shipped game
    /* TAKE_WOOD_FROM_TREE_FOR_BUILDING */ k_TodoEntry,
    /* TAKE_WOOD_FROM_POT_FOR_BUILDING */ k_TodoEntry,
    /* SHEPHERD_TAKE_ANIMAL_FOR_SLAUGHTER */ k_TodoEntry,
    /* SHEPHERD_TAKES_CONTROL_OF_FLOCK */ k_TodoEntry,
    /* SHEPHERD_RELEASES_CONTROL_OF_FLOCK */ k_TodoEntry,
    /* DANCE_BUT_NOT_WORSHIP */ TodoWalk(),
    /* FAINTING_REACTION */ k_TodoEntry,
    /* START_CONFUSED_REACTION */ k_TodoEntry,
    /* CONFUSED_REACTION */ k_TodoEntry,
    /* AFTER_TAP_ON_ABODE (no entry / exit; the clip 273, a yawn, is VillagerAnimationTable.h's) */
    VillagerStateTableEntry {
        .state = &ecs::villager::AfterTapOnAbode,
    },
    /* WEAK_ON_GROUND */ k_TodoEntry,
    /* SCRIPT_WANDER_AROUND_POSITION */ k_TodoEntry,
    // the script's clip (VillagerAnimations' AnimFn::Script); save / load not ported
    /* SCRIPT_PLAY_ANIM */
    {.state = &ecs::villager::ScriptPlayAnim,
     .entryState = &ecs::villager::EnterPlayAnim,
     .exitState = &ecs::villager::ExitPlayAnim,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    // the teleport stones' states (VillagerTeleport.cpp); their exit ExitReactToTeleport
    /* GO_TOWARDS_TELEPORT_REACTION */
    {.state = &ecs::villager_teleport::GoToTeleportReaction, .exitState = &ecs::villager_teleport::ExitReactToTeleport},
    /* TELEPORT_REACTION */
    {.state = &ecs::villager_teleport::TeleportReaction, .exitState = &ecs::villager_teleport::ExitReactToTeleport},
    /* DANCE_WHILE_REACTING */ TodoWalk(TodoWithExitReaction()),
    /* CONTROLLED_BY_CREATURE */ k_TodoEntry,
    // the mourning (ECS/Villager/VillagerMourning.cpp): exit ExitReaction, validate ReactionValidate, both for
    // REACT_TO_DEATH's reaction
    /* POINT_AT_DEAD_PERSON */
    VillagerStateTableEntry {
        .state = &ecs::villager_mourning::PointAtDeadPerson,
        .exitState = &ecs::villager_mourning::ExitReaction,
        .validate = &ecs::villager_mourning::ReactionValidate,
    },
    /* GO_TOWARDS_DEAD_PERSON */
    VillagerStateTableEntry {
        .state = &ecs::villager_mourning::GoTowardsDeadPerson,
        .exitState = &ecs::villager_mourning::ExitReaction,
        .validate = &ecs::villager_mourning::ReactionValidate,
    },
    /* LOOK_AT_DEAD_PERSON */
    VillagerStateTableEntry {
        .state = &ecs::villager_mourning::LookAtDeadPerson,
        .exitState = &ecs::villager_mourning::ExitReaction,
        .validate = &ecs::villager_mourning::ReactionValidate,
    },
    /* MOURN_DEAD_PERSON (the mourning into / out-of clip) */
    VillagerStateTableEntry {
        .state = &ecs::villager_mourning::MournDeadPerson,
        .exitState = &ecs::villager_mourning::ExitReaction,
        .validate = &ecs::villager_mourning::ReactionValidate,
    },
    /* NOTHING_TO_DO (no entry or exit; always reacts to the town emergency) */
    VillagerStateTableEntry {
        .state = &ecs::villager::NothingToDo,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_WORKSHOP_FOR_DROP_OFF */ k_TodoEntry,
    /* ARRIVES_AT_STORAGE_PIT_FOR_WORKSHOP_MATERIALS */ k_TodoEntry,
    /* SHOW_POISONED (VillagerFood.cpp; clip 342) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ShowPoisoned,
    },
    /* HIDING_AT_WORSHIP_SITE */
    {.state = &ecs::villager_worship::HidingAtWorshipSite,
     .exitState = [](LivingAction& a, VillagerStates n) { return OldExit(ecs::villager_worship::ExitAtWorshipSite(a, n)); }},
    /* CROWD_REACTION */ TodoWithExitReaction(),
    // the fire's states (VillagerFire.cpp): 216, 217, 218 and 220 enter and exit with EnterPutOutFire /
    // ExitPutOutFire, 219 with EnterOnFire / ExitOnFire. 215's
    // exit is the shared ExitReaction
    /* REACT_TO_FIRE */ {.state = &ecs::villager_fire::ReactToFire, .exitState = &ecs::villager_reactions::ExitReaction},
    /* PUT_OUT_FIRE_BY_BEATING */
    {.state = &ecs::villager_fire::PutOutFireByBeating,
     .entryState = &ecs::villager_fire::EnterPutOutFire,
     .exitState = &ecs::villager_fire::ExitPutOutFire},
    /* PUT_OUT_FIRE_WITH_WATER */
    {.state = &ecs::villager_fire::PutOutFireWithWater,
     .entryState = &ecs::villager_fire::EnterPutOutFire,
     .exitState = &ecs::villager_fire::ExitPutOutFire},
    /* GET_WATER_TO_PUT_OUT_FIRE */
    {.state = &ecs::villager_fire::PutOutFireWithWater,
     .entryState = &ecs::villager_fire::EnterPutOutFire,
     .exitState = &ecs::villager_fire::ExitPutOutFire},
    /* ON_FIRE */
    {.state = &ecs::villager_fire::OnFire,
     .entryState = &ecs::villager_fire::EnterOnFire,
     .exitState = &ecs::villager_fire::ExitOnFire},
    /* MOVE_AROUND_FIRE */
    {.state = &ecs::villager_fire::MoveAroundFire,
     .entryState = &ecs::villager_fire::EnterPutOutFire,
     .exitState = &ecs::villager_fire::ExitPutOutFire},
    /* DISCIPLE_NOTHING_TO_DO, entry EnterDiscipleNothingToDo (VillagerDisciple.cpp; no exit); save / load not ported
       (k_TodoEntry's); always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::DiscipleNothingToDo,
        .entryState = &ecs::villager::EnterDiscipleNothingToDo,
        .saveState = k_TodoEntry.saveState,
        .loadState = k_TodoEntry.loadState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FOOTBALL_MOVE_TO_BALL */ TodoWalk(),
    /* ARRIVES_AT_STORAGE_PIT_FOR_TRADER_PICK_UP */ k_TodoEntry,
    /* ARRIVES_AT_STORAGE_PIT_FOR_TRADER_DROP_OFF */ k_TodoEntry,
    /* BREEDER_DISCIPLE */ k_TodoEntry,
    /* MISSIONARY_DISCIPLE */ k_TodoEntry,
    /* REACT_TO_BREEDER */ k_TodoEntry,
    /* SHEPHERD_CHECK_ANIMAL_FOR_SLAUGHTER */ k_TodoEntry,
    /* INTERACT_DECIDE_WHAT_TO_DO_FOR_OTHER_VILLAGER */ k_TodoEntry,
    /* ARTIFACT_DANCE */ TodoWalk(),
    /* FLEEING_FROM_PREDATOR_REACTION */ k_TodoEntry,
    // (not ported) never entered in the shipped game: SetupWaitForWood needs a game flag that is never set
    /* WAIT_FOR_WOOD */ k_TodoEntry,
    /* INSPECT_OBJECT */ k_TodoEntry,
    /* GO_HOME_AND_CHANGE, exit ExitGoHomeAndChange (VillagerHome.cpp: the grown-up child's adult mesh); always reacts
       to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoHomeAndChange,
        .exitState = &ecs::villager::ExitGoHomeAndChange,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* WAIT_FOR_MATE */ k_TodoEntry,
    /* GO_AND_HIDE_IN_NEARBY_BUILDING */ k_TodoEntry,
    /* LOOK_TO_SEE_IF_IT_IS_SAFE */ k_TodoEntry,
    /* SLEEP_IN_TENT (VillagerHome.cpp; clip 381 and the tent's in / out clip of VillagerAnimationTable.h); always
       reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::SleepInTentState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* PAUSE_FOR_A_SECOND (no entry, exit, save or load; always reacts to the town emergency; its clip function is in
       VillagerAnimationTable.h) */
    VillagerStateTableEntry {
        .state = &ecs::villager::PauseForASecond,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* PANIC_REACTION */ k_TodoEntry,
    /* GET_FOOD_AT_WORSHIP_SITE */ k_TodoEntry,
    /* GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY (no entry or exit) */
    VillagerStateTableEntry {
        .state = &ecs::villager::GotoCongregateInTownAfterEmergency,
    },
    /* CONGREGATE_IN_TOWN_AFTER_EMERGENCY (no entry / exit; the town emergency clip is VillagerAnimationTable.h's) */
    VillagerStateTableEntry {
        .state = &ecs::villager::CongregateInTownAfterEmergency,
    },
    /* SCRIPT_IN_CROWD */ k_TodoEntry,
    /* GO_AND_CHILLOUT_OUTSIDE_HOME (VillagerDecide.cpp) */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoAndChilloutOutsideHome,
    },
    /* SIT_AND_CHILLOUT, entry EnterSitAndChillOut (its sit-down clip functions are in VillagerAnimationTable.h) */
    VillagerStateTableEntry {
        .state = &ecs::villager::SitAndChillout,
        .entryState = &ecs::villager::EnterSitAndChillOut,
    },
    /* SCRIPT_GO_AND_MOVE_ALONG_PATH */ k_TodoEntry,
    // (k_VillagerStateStrings mislabels 248..254; the names here are the enum's / info.dat's)
    /* 248 GO_HOME_FROM_WORSHIP = DoGoingHome(249, 250) (VillagerHome.cpp); exit ExitAtHome; always reacts to the
       town emergency */
    VillagerStateTableEntry {
        .state = [](LivingAction& action) -> uint32_t {
	        return ecs::villager::DoGoingHome(Locator::entitiesRegistry::value().ToEntity(action),
	                                          VillagerStates::ArrivesHomeFromWorship, VillagerStates::SleepInTentFromWorship);
        },
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* 249 ARRIVES_HOME_FROM_WORSHIP = ARRIVES_HOME; exit ExitAtHome */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesHomeState,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* 250 SLEEP_IN_TENT_FROM_WORSHIP = SLEEP_IN_TENT */
    VillagerStateTableEntry {
        .state = &ecs::villager::SleepInTentState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* 251 GO_TOWARDS_TELEPORT_REACTION_QUICKLY (= 201's; exit ExitReactToTeleport) */
    {.state = &ecs::villager_teleport::GoToTeleportReaction, .exitState = &ecs::villager_teleport::ExitReactToTeleport},
    /* 252 GO_AND_CHILLOUT_IN_TOWN (VillagerDecide.cpp; only scripts set it) */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoAndChilloutInTown,
    },
    /* 253 WAIT_FOR_ARTIFACT_DANCE */ k_TodoEntry,
    /* 254 BREEDER_JUST_LANDED */ k_TodoEntry,
};

LivingActionSystem::LivingActionSystem()
{
	ecs::living::RegisterPhysicsHandlers();
}

void LivingActionSystem::Update()
{
	// the villagers' turn is in the one living list with the animals (living_turn::ProcessLiving, ECS/LivingTurn.h); this
	// is what follows it: the souls of the dead, (approximate) once a game turn with its ms
	// (game_clock::MsPerTurn); the original runs it every frame with the frame's time step
	ecs::villager_soul::Update(game_clock::MsPerTurn());
}

VillagerStates LivingActionSystem::VillagerGetState(const LivingAction& action, LivingAction::Index index) const
{
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
}

void LivingActionSystem::VillagerSetState(LivingAction& action, LivingAction::Index index, VillagerStates state,
                                          bool skipTransition) const
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.ToEntity(action);
	const auto previousState = static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	SPDLOG_LOGGER_TRACE(spdlog::get("ai"), "Villager #{}: Setting state {} -> {}", static_cast<int>(entity),
	                    k_VillagerStateStrings.at(static_cast<size_t>(previousState)),
	                    k_VillagerStateStrings.at(static_cast<size_t>(state)));

	if (index != LivingAction::Index::Top)
	{
		// the PREVIOUS rule and the town modifier
		ecs::villager::SetState(entity, index, state);
		return;
	}
	if (!skipTransition)
	{
		// the pause, the exits, the entry, the refusals, the speed and the clips
		ecs::villager::SetTopState(entity, state);
		return;
	}
	// The changes that bypass the exit and entry functions (the hand, the physics, the animals, the water, MOVE_TO_POS
	// and LANDED): the state is set straight (FINAL cleared with its town modifier, the counter 0) and the clips and speed
	// as before. (approximate) in the original those go through SetTopState with their own entry / exit functions
	// (in the hand, ...), not ported yet. Repeating the same order changes nothing, as
	// openblack had it
	if (previousState == state)
	{
		return;
	}
	ecs::villager::SetState(entity, LivingAction::Index::Top, state);
	ecs::OnVillagerStateChanged(entity, previousState, state);
}

uint32_t LivingActionSystem::VillagerCallState(LivingAction& action, LivingAction::Index index) const
{
	const auto state = action.states.at(static_cast<size_t>(index));
	const auto& entry = k_VillagerStateTable.at(static_cast<size_t>(state));
	const auto& callback = entry.state;
	if (!callback)
	{
		return 0;
	}
	return callback(action);
}

namespace
{
enum class Slot : uint8_t
{
	Entry,
	Exit,
	Validate,
};

/// One warning per state and slot when the original has a function there and the row has none
void WarnMissing(LivingAction& action, VillagerStates row, Slot slot)
{
	static std::array<std::bitset<256>, 3> warned;
	const auto index = std::min<size_t>(static_cast<size_t>(row), 254);
	const auto& original = ecs::villager::k_OriginalStateFns.at(index);
	const uint8_t function = slot == Slot::Entry ? original.entry : slot == Slot::Exit ? original.exit : original.validate;
	auto& bits = warned.at(static_cast<size_t>(slot));
	if (function == 0 || bits.test(index))
	{
		return;
	}
	bits.set(index);
	SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented {} function of {} {}: taken as 1",
	                   static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                   slot == Slot::Entry  ? "entry"
	                   : slot == Slot::Exit ? "exit"
	                                        : "validate",
	                   index, k_VillagerStateStrings.at(index));
}
} // namespace

uint32_t LivingActionSystem::VillagerCallEntry(LivingAction& action, VillagerStates row, VillagerStates final,
                                               VillagerStates next) const
{
	const auto& callback = k_VillagerStateTable.at(static_cast<size_t>(row)).entryState;
	if (!callback)
	{
		WarnMissing(action, row, Slot::Entry);
		return 1;
	}
	return callback(action, final, next);
}

uint32_t LivingActionSystem::VillagerCallExit(LivingAction& action, VillagerStates row, VillagerStates next) const
{
	const auto& callback = k_VillagerStateTable.at(static_cast<size_t>(row)).exitState;
	if (!callback)
	{
		WarnMissing(action, row, Slot::Exit);
		return 1;
	}
	return callback(action, next);
}

int LivingActionSystem::VillagerCallOutOfAnimation(LivingAction& action, LivingAction::Index index) const
{
	const auto state = action.states.at(static_cast<size_t>(index));
	const auto& entry = k_VillagerStateTable.at(static_cast<size_t>(state));
	const auto& callback = entry.transitionAnimation;
	if (!callback)
	{
		return -1;
	}
	return callback(action);
}

bool LivingActionSystem::VillagerCallValidate(LivingAction& action, LivingAction::Index index) const
{
	const auto state = static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	const auto& callback = k_VillagerStateTable.at(static_cast<size_t>(state)).validate;
	// the reaction rows' validate is ReactionValidate (201, 202, 251, 215-218,
	// 220, 6-30, 140-196...: every row whose original validate is it, VillagerOriginalFns.h). The villager's state turn
	// calls this slot for its two states before the state function; the result is unused there
	if (!callback && ecs::villager::k_OriginalStateFns.at(std::min<size_t>(static_cast<size_t>(state), 254)).validate ==
	                     ecs::villager::k_ReactionValidate)
	{
		return ecs::villager_reactions::ReactionValidate(action);
	}
	if (!callback)
	{
		WarnMissing(action, state, Slot::Validate);
		return false;
	}
	return callback(action);
}
