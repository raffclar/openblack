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

#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/TownDesire.h"
#include "ECS/VillagerNeeds.h"
#include "ECS/WorldObjects.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerAnimate.h"
#include "VillagerArtefactDance.h"
#include "VillagerBreeding.h"
#include "VillagerBuild.h"
#include "VillagerChecks.h"
#include "VillagerDance.h"
#include "VillagerDiscipleWork.h"
#include "VillagerDisciples.h"
#include "VillagerEaten.h"
#include "VillagerFarmer.h"
#include "VillagerFire.h"
#include "VillagerFisherman.h"
#include "VillagerFlock.h"
#include "VillagerFood.h"
#include "VillagerFootpath.h"
#include "VillagerForester.h"
#include "VillagerHome.h"
#include "VillagerHousewife.h"
#include "VillagerMourning.h"
#include "VillagerMoveToObject.h"
#include "VillagerPhysics.h"
#include "VillagerReactions.h"
#include "VillagerScript.h"
#include "VillagerShepherd.h"
#include "VillagerShieldShelter.h"
#include "VillagerTeleport.h"
#include "VillagerWorship.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace villager_home = openblack::ecs::villager_home;
namespace villager_eaten = openblack::ecs::villager_eaten;
namespace villager_teleport = openblack::ecs::villager_teleport;
namespace villager_shield = openblack::ecs::villager_shield;
namespace villager_fire = openblack::ecs::villager_fire;
namespace villager_flock = openblack::ecs::villager_flock;
namespace villager_dance = openblack::ecs::villager_dance;
namespace villager_artefact_dance = openblack::ecs::villager_artefact_dance;
namespace villager_physics = openblack::ecs::villager_physics;
namespace villager_animate = openblack::ecs::villager_animate;
namespace villager_script = openblack::ecs::villager_script;
namespace villager_build = openblack::ecs::villager_build;
namespace villager_disciple = openblack::ecs::villager_disciple;
namespace villager_disciple_work = openblack::ecs::villager_disciple_work;
namespace villager_food = openblack::ecs::villager_food;
namespace villager_forester = openblack::ecs::villager_forester;
namespace villager_farmer = openblack::ecs::villager_farmer;
namespace villager_move_to_object = openblack::ecs::villager_move_to_object;
namespace villager_shepherd = openblack::ecs::villager_shepherd;
namespace villager_fisherman = openblack::ecs::villager_fisherman;
namespace villager_housewife = openblack::ecs::villager_housewife;
namespace villager_mourning = openblack::ecs::villager_mourning;
namespace villager_checks = openblack::ecs::villager_checks;
namespace villager_breeding = openblack::ecs::villager_breeding;
namespace villager_worship = openblack::ecs::villager_worship;
namespace villager_footpath = openblack::ecs::villager_footpath;

/// A villager with no state does nothing
uint32_t VillagerInvalidState(LivingAction& /*action*/)
{
	return 0;
}

/// Plays the clip it has to its end, then goes on into the state it works towards
uint32_t VillagerWaitForAnimation(LivingAction& action)
{
	const auto& living = Locator::livingActionSystem::value();
	if (living.VillagerIsReadyForNewAnimation(action, 1))
	{
		living.VillagerSetTopStateToFinal(action);
		return 0;
	}
	return 1;
}

/// Stops for a second, worn out or showing its poison, then goes on
uint32_t VillagerPauseForASecond(LivingAction& action)
{
	const auto& living = Locator::livingActionSystem::value();
	if (living.VillagerIsReadyForNewAnimation(action, 1))
	{
		living.VillagerSetTopStateToFinal(action);
	}
	return 1;
}

uint32_t VillagerCreated(LivingAction& action)
{
	if (action.turnsUntilStateChange > 0)
	{
		--action.turnsUntilStateChange;
	}
	else
	{
		Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, VillagerStates::DecideWhatToDo,
		                                                      false);
	}
	return 0;
}

struct VillagerStateTableEntry
{
	std::function<uint32_t(LivingAction&)> state = nullptr;
	std::function<bool(LivingAction&, VillagerStates, VillagerStates)> entryState = nullptr;
	std::function<bool(LivingAction&, VillagerStates)> exitState = nullptr;
	std::function<bool(LivingAction&)> saveState = nullptr;
	std::function<bool(LivingAction&)> loadState = nullptr;
	std::function<bool(LivingAction&)> field0x50 = nullptr;
	std::function<bool(LivingAction&)> field0x60 = nullptr;
	std::function<int(LivingAction&)> transitionAnimation = nullptr;
	std::function<bool(LivingAction&)> validate = nullptr;
};

static const VillagerStateTableEntry k_TodoEntry = {
    .state = [](LivingAction& action) -> uint32_t {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented state function: {}",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(
	                           Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top))));
	    return 0;
    },
    .entryState = [](LivingAction& action, VillagerStates src, VillagerStates dst) -> bool {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented entry state function ({} -> {})",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(src)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(dst)));
	    return false;
    },
    .exitState = [](LivingAction& action, [[maybe_unused]] VillagerStates next) -> bool {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented exit state function)",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)));
	    return false;
    },
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
    .field0x50 = [](LivingAction& action) -> bool {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented field0x50 state function: {}",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(
	                           Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top))));
	    return false;
    },
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
    .validate = [](LivingAction& action) -> bool {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented validate function: {}",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(
	                           Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top))));
	    return false;
    },
};

const static std::array<VillagerStateTableEntry, static_cast<size_t>(VillagerStates::_COUNT)> k_VillagerStateTable = {
    /* INVALID_STATE */ VillagerStateTableEntry {
        .state = &VillagerInvalidState,
    },
    /* MOVE_TO_POS */
    VillagerStateTableEntry {
        .state = &villager_home::MoveToPos,
    },
    /* MOVE_TO_OBJECT */
    VillagerStateTableEntry {
        .state = &villager_move_to_object::MoveToObject,
    },
    /* MOVE_ON_STRUCTURE */ k_TodoEntry,
    /* IN_SCRIPT */
    VillagerStateTableEntry {
        .state = &villager_script::InScript,
        .exitState = &villager_script::ExitInScript,
    },
    /* IN_DANCE */
    VillagerStateTableEntry {
        .state = &villager_dance::InDance,
        .exitState = &villager_dance::ExitInDance,
    },
    /* FLEEING_FROM_OBJECT_REACTION */
    VillagerStateTableEntry {
        .state = &villager_reactions::Fleeing,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* LOOKING_AT_OBJECT_REACTION */
    VillagerStateTableEntry {
        .state = &villager_reactions::Watching,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* FOLLOWING_OBJECT_REACTION */ k_TodoEntry,
    /* INSPECT_OBJECT_REACTION */ k_TodoEntry,
    /* FLYING */
    VillagerStateTableEntry {
        .state = &villager_physics::Flying,
        .exitState = &villager_physics::ExitFlying,
    },
    /* LANDED */
    VillagerStateTableEntry {
        .state = &villager_physics::Landed,
    },
    /* LOOK_AT_FLYING_OBJECT_REACTION */ k_TodoEntry,
    /* SET_DYING */ k_TodoEntry,
    /* DYING */
    VillagerStateTableEntry {
        .state = &villager_fire::Dying,
    },
    /* DEAD */
    VillagerStateTableEntry {
        .state = &villager_fire::Dead,
        .exitState = &villager_fire::ExitDead,
    },
    /* DROWNING */
    VillagerStateTableEntry {
        .state = &villager_physics::Drowning,
    },
    /* DOWNED */
    VillagerStateTableEntry {
        .state = &villager_eaten::LiesStill,
    },
    /* BEING_EATEN */
    VillagerStateTableEntry {
        .state = &villager_eaten::LiesStill,
    },
    /* GOTO_FOOD_REACTION */
    VillagerStateTableEntry {
        .state = &villager_food::GotoFoodReaction,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* ARRIVES_AT_FOOD_REACTION */
    VillagerStateTableEntry {
        .state = &villager_food::ArrivesAtFoodReaction,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* GOTO_WOOD_REACTION */
    VillagerStateTableEntry {
        .state = &villager_forester::GotoWoodReaction,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* ARRIVES_AT_WOOD_REACTION */
    VillagerStateTableEntry {
        .state = &villager_forester::ArrivesAtWoodReaction,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* WAIT_FOR_ANIMATION */
    VillagerStateTableEntry {
        .state = &VillagerWaitForAnimation,
    },
    /* IN_HAND */
    VillagerStateTableEntry {
        .state = &villager_disciple::InHand,
        .exitState = &villager_physics::ExitInHand,
    },
    /* GOTO_PICKUP_BALL_REACTION */ k_TodoEntry,
    /* ARRIVES_AT_PICKUP_BALL_REACTION */ k_TodoEntry,
    /* MOVE_IN_FLOCK */
    VillagerStateTableEntry {
        .state = &villager_flock::MoveInFlock,
    },
    /* MOVE_ALONG_PATH */
    VillagerStateTableEntry {
        .state = &villager_script::MoveAlongPath,
    },
    /* MOVE_ON_PATH */
    VillagerStateTableEntry {
        .state = &villager_footpath::MoveOnPath,
        .exitState = &villager_footpath::ExitMoveOnPath,
    },
    /* FLEEING_AND_LOOKING_AT_OBJECT_REACTION */
    VillagerStateTableEntry {
        .state = &villager_reactions::Watching,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* GOTO_STORAGE_PIT_FOR_DROP_OFF */
    VillagerStateTableEntry {
        .state = &villager_build::GotoStoragePitForDropOff,
    },
    /* ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF */
    VillagerStateTableEntry {
        .state = &villager_build::ArrivesAtStoragePitForDropOff,
    },
    /* GOTO_STORAGE_PIT_FOR_FOOD */
    VillagerStateTableEntry {
        .state = &villager_food::GotoStoragePitForFood,
    },
    /* ARRIVES_AT_STORAGE_PIT_FOR_FOOD */
    VillagerStateTableEntry {
        .state = &villager_food::ArrivesAtStoragePitForFood,
    },
    /* ARRIVES_AT_HOME_WITH_FOOD */ k_TodoEntry,
    /* GO_HOME */
    VillagerStateTableEntry {
        .state = &villager_home::GoHome,
        .exitState = &villager_home::ExitAtHome,
    },
    /* ARRIVES_HOME */
    VillagerStateTableEntry {
        .state = &villager_home::ArrivesHome,
        .exitState = &villager_home::ExitAtHome,
    },
    /* AT_HOME */
    VillagerStateTableEntry {
        .state = &villager_home::AtHome,
        .exitState = &villager_home::ExitAtHome,
    },
    /* ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS */
    VillagerStateTableEntry {
        .state = &villager_build::ArrivesAtStoragePitForBuildingMaterials,
        .entryState = &villager_build::EnterBuilding,
        .exitState = &villager_build::ExitBuilding,
    },
    /* ARRIVES_AT_BUILDING_SITE */
    VillagerStateTableEntry {
        .state = &villager_build::ArrivesAtBuildingSite,
        .entryState = &villager_build::EnterBuilding,
        .exitState = &villager_build::ExitBuilding,
    },
    /* BUILDING */
    VillagerStateTableEntry {
        .state = &villager_build::Building,
        .entryState = &villager_build::EnterBuilding,
        .exitState = &villager_build::ExitBuilding,
    },
    /* GOTO_STORAGE_PIT_FOR_WORSHIP_SUPPLIES */
    VillagerStateTableEntry {
        .state = &villager_worship::GotoStoragePitForWorshipSupplies,
        .exitState = &villager_worship::ExitMoveToWorshipSiteWithSupplies,
    },
    /* ARRIVES_AT_STORAGE_PIT_FOR_WORSHIP_SUPPLIES */
    VillagerStateTableEntry {
        .state = &villager_worship::ArrivesAtStoragePitForWorshipSupplies,
        .exitState = &villager_worship::ExitMoveToWorshipSiteWithSupplies,
    },
    /* GOTO_WORSHIP_SITE_WITH_SUPPLIES */
    VillagerStateTableEntry {
        .state = &villager_worship::GotoWorshipSiteWithSupplies,
        .exitState = &villager_worship::ExitMoveToWorshipSiteWithSupplies,
    },
    /* MOVE_TO_WORSHIP_SITE_WITH_SUPPLIES */
    VillagerStateTableEntry {
        .state = &villager_worship::MoveToWorshipSiteWithSupplies,
        .exitState = &villager_worship::ExitMoveToWorshipSiteWithSupplies,
    },
    /* ARRIVES_AT_WORSHIP_SITE_WITH_SUPPLIES */
    VillagerStateTableEntry {
        .state = &villager_worship::ArrivesAtWorshipSiteWithSupplies,
        .exitState = &villager_worship::ExitMoveToWorshipSiteWithSupplies,
    },
    /* FORESTER_MOVE_TO_FOREST */
    VillagerStateTableEntry {
        .state = &villager_forester::ForesterMoveToForest,
        .exitState = &villager_forester::ExitForesting,
    },
    /* FORESTER_GOTO_FOREST */
    VillagerStateTableEntry {
        .state = &villager_forester::ForesterGotoForest,
        .exitState = &villager_forester::ExitForesting,
    },
    /* FORESTER_ARRIVES_AT_FOREST */
    VillagerStateTableEntry {
        .state = &villager_forester::ForesterArrivesAtForest,
        .exitState = &villager_forester::ExitForesting,
    },
    /* FORESTER_CHOPS_TREE */
    VillagerStateTableEntry {
        .state = &villager_forester::ForesterChopsTree,
        .exitState = &villager_forester::ExitForesting,
    },
    /* FORESTER_CHOPS_TREE_FOR_BUILDING */
    VillagerStateTableEntry {
        .state = &villager_forester::ForesterChopsTreeForBuilding,
        .entryState = &villager_build::EnterBuilding,
        .exitState = &villager_build::ExitBuilding,
    },
    /* FORESTER_FINISHED_FORESTERING */
    VillagerStateTableEntry {
        .state = &villager_forester::ForesterFinishedForestering,
        .exitState = &villager_forester::ExitForesting,
    },
    /* ARRIVES_AT_BIG_FOREST */
    VillagerStateTableEntry {
        .state = &villager_forester::ArrivesAtBigForest,
    },
    /* ARRIVES_AT_BIG_FOREST_FOR_BUILDING */
    VillagerStateTableEntry {
        .state = &villager_forester::ArrivesAtBigForestForBuilding,
        .entryState = &villager_build::EnterBuilding,
        .exitState = &villager_build::ExitBuilding,
    },
    /* FISHERMAN_ARRIVES_AT_FISHING */
    VillagerStateTableEntry {
        .state = &villager_fisherman::FishermanArrivesAtFishing,
        .entryState = &villager_fisherman::EnterFishing,
        .exitState = &villager_fisherman::ExitFishing,
    },
    /* FISHING */
    VillagerStateTableEntry {
        .state = &villager_fisherman::Fishing,
        .entryState = &villager_fisherman::EnterFishing,
        .exitState = &villager_fisherman::ExitFishing,
    },
    /* WAIT_FOR_COUNTER */
    VillagerStateTableEntry {
        .state = &villager_shield::WaitForCounter,
    },
    /* GOTO_WORSHIP_SITE_FOR_WORSHIP */
    VillagerStateTableEntry {
        .state = &villager_worship::GotoWorshipSiteForWorship,
        .exitState = &villager_worship::ExitMoveToWorshipSite,
    },
    /* ARRIVES_AT_WORSHIP_SITE_FOR_WORSHIP */
    VillagerStateTableEntry {
        .state = &villager_worship::ArrivesAtWorshipSiteForWorship,
        .exitState = &villager_worship::ExitMoveToWorshipSite,
    },
    /* WORSHIPPING_AT_WORSHIP_SITE */
    VillagerStateTableEntry {
        .state = &villager_worship::WorshippingAtWorshipSite,
        .exitState = &villager_worship::ExitAtWorshipSite,
    },
    /* GOTO_ALTAR_FOR_REST */
    VillagerStateTableEntry {
        .state = &villager_worship::GotoAltarForRest,
    },
    /* ARRIVES_AT_ALTAR_FOR_REST */
    VillagerStateTableEntry {
        .state = &villager_worship::ArrivesAtAltarForRest,
    },
    /* AT_ALTAR_REST */
    VillagerStateTableEntry {
        .state = &villager_worship::AtAltarRest,
    },
    /* AT_ALTAR_FINISHED_REST */
    VillagerStateTableEntry {
        .state = &villager_worship::AtAltarFinishedRest,
    },
    /* RESTART_WORSHIPPING_AT_WORSHIP_SITE */
    VillagerStateTableEntry {
        .state = &villager_worship::RestartWorshippingAtWorshipSite,
        .exitState = &villager_worship::ExitGetFoodAtWorship,
    },
    /* RESTART_WORSHIPPING_CREATURE */ k_TodoEntry,
    /* FARMER_ARRIVES_AT_FARM */
    VillagerStateTableEntry {
        .state = &villager_farmer::FarmerArrivesAtFarm,
        .entryState = &villager_farmer::EnterFarming,
        .exitState = &villager_farmer::ExitFarming,
    },
    /* FARMER_PLANTS_CROP */
    VillagerStateTableEntry {
        .state = &villager_farmer::FarmerPlantsCrop,
        .entryState = &villager_farmer::EnterFarming,
        .exitState = &villager_farmer::ExitFarming,
    },
    /* FARMER_DIGS_UP_CROP */
    VillagerStateTableEntry {
        .state = &villager_farmer::FarmerDigsUpCrop,
        .entryState = &villager_farmer::EnterFarming,
        .exitState = &villager_farmer::ExitFarming,
    },
    /* MOVE_TO_FOOTBALL_PITCH_CONSTRUCTION */ k_TodoEntry,
    /* FOOTBALL_WALK_TO_POSITION */ k_TodoEntry,
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
    /* CREATED */
    VillagerStateTableEntry {
        .state = &VillagerCreated,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_IN_ABODE_TO_TRADE */ k_TodoEntry,
    /* ARRIVES_IN_ABODE_TO_PICK_UP_EXCESS */ k_TodoEntry,
    /* MAKE_SCARED_STIFF */ k_TodoEntry,
    /* SCARED_STIFF */ k_TodoEntry,
    /* WORSHIPPING_CREATURE */ k_TodoEntry,
    /* SHEPHERD_LOOK_FOR_FLOCK */
    VillagerStateTableEntry {
        .state = &villager_shepherd::ShepherdLookForFlock,
        .exitState = &villager_shepherd::ExitShepherding,
    },
    /* SHEPHERD_MOVE_FLOCK_TO_WATER */
    VillagerStateTableEntry {
        .state = &villager_shepherd::ShepherdMoveFlockToWater,
        .exitState = &villager_shepherd::ExitShepherding,
    },
    /* SHEPHERD_MOVE_FLOCK_TO_FOOD */
    VillagerStateTableEntry {
        .state = &villager_shepherd::ShepherdMoveFlockToFood,
        .exitState = &villager_shepherd::ExitShepherding,
    },
    /* SHEPHERD_MOVE_FLOCK_BACK */
    VillagerStateTableEntry {
        .state = &villager_shepherd::ShepherdMoveFlockBack,
        .exitState = &villager_shepherd::ExitShepherding,
    },
    /* SHEPHERD_DECIDE_WHAT_TO_DO_WITH_FLOCK */
    VillagerStateTableEntry {
        .state = &villager_shepherd::ShepherdDecideWhatToDoWithFlock,
        .exitState = &villager_shepherd::ExitShepherding,
    },
    /* SHEPHERD_WAIT_FOR_FLOCK */
    VillagerStateTableEntry {
        .state = &villager_shepherd::ShepherdWaitForFlock,
        .exitState = &villager_shepherd::ExitShepherding,
    },
    /* SHEPHERD_SLAUGHTER_ANIMAL */
    VillagerStateTableEntry {
        .state = &villager_shepherd::ShepherdSlaughterAnimal,
        .exitState = &villager_shepherd::ExitShepherding,
    },
    /* SHEPHERD_FETCH_STRAY */
    VillagerStateTableEntry {
        .state = &villager_shepherd::ShepherdFetchStray,
        .exitState = &villager_shepherd::ExitShepherding,
    },
    /* SHEPHERD_GOTO_FLOCK */
    VillagerStateTableEntry {
        .state = &villager_shepherd::ShepherdGotoFlock,
        .exitState = &villager_shepherd::ExitShepherding,
    },
    /* HOUSEWIFE_AT_HOME */
    VillagerStateTableEntry {
        .state = &villager_housewife::AtHome,
        .exitState = &villager_home::ExitAtHome,
    },
    /* HOUSEWIFE_GOTO_STORAGE_PIT */
    VillagerStateTableEntry {
        .state = &villager_housewife::GotoStoragePit,
    },
    /* HOUSEWIFE_ARRIVES_AT_STORAGE_PIT */
    VillagerStateTableEntry {
        .state = &villager_housewife::ArrivesAtStoragePit,
    },
    /* HOUSEWIFE_PICKUP_FROM_STORAGE_PIT */
    VillagerStateTableEntry {
        .state = &villager_housewife::PickupFromStoragePit,
    },
    /* HOUSEWIFE_RETURN_HOME_WITH_FOOD */
    VillagerStateTableEntry {
        .state = &villager_housewife::ReturnHomeWithFood,
    },
    /* HOUSEWIFE_MAKE_DINNER */
    VillagerStateTableEntry {
        .state = &villager_housewife::MakeDinner,
    },
    /* HOUSEWIFE_SERVES_DINNER */
    VillagerStateTableEntry {
        .state = &villager_housewife::ServesDinner,
    },
    /* HOUSEWIFE_CLEARS_AWAY_DINNER */
    VillagerStateTableEntry {
        .state = &villager_housewife::ClearsAwayDinner,
    },
    /* HOUSEWIFE_DOES_HOUSEWORK */
    VillagerStateTableEntry {
        .state = &villager_housewife::DoesHousework,
    },
    /* HOUSEWIFE_GOSSIPS_AROUND_STORAGE_PIT */
    VillagerStateTableEntry {
        .state = &villager_housewife::GossipsAroundStoragePit,
    },
    /* HOUSEWIFE_STARTS_GIVING_BIRTH */
    VillagerStateTableEntry {
        .state = &villager_breeding::StartsGivingBirth,
    },
    /* HOUSEWIFE_GIVING_BIRTH */
    VillagerStateTableEntry {
        .state = &villager_breeding::GivingBirth,
    },
    /* HOUSEWIFE_GIVEN_BIRTH */
    VillagerStateTableEntry {
        .state = &villager_breeding::GivenBirth,
    },
    /* CHILD_AT_CRECHE */
    VillagerStateTableEntry {
        .state = &villager_breeding::ChildAtCreche,
    },
    /* CHILD_FOLLOWS_MOTHER */
    VillagerStateTableEntry {
        .state = &villager_breeding::ChildFollowsMother,
    },
    /* CHILD_BECOMES_ADULT */ k_TodoEntry,
    /* SITS_DOWN_TO_DINNER */
    VillagerStateTableEntry {
        .state = &villager_food::DoesNothing,
    },
    /* EAT_FOOD */
    VillagerStateTableEntry {
        .state = &villager_food::EatFood,
    },
    /* EAT_FOOD_AT_HOME */
    VillagerStateTableEntry {
        .state = &villager_food::EatFoodAtHome,
        .exitState = &villager_home::ExitAtHome,
    },
    /* GOTO_BED_AT_HOME */
    VillagerStateTableEntry {
        .state = &villager_home::GotoBedAtHome,
        .exitState = &villager_home::ExitAtHome,
    },
    /* SLEEPING_AT_HOME */
    VillagerStateTableEntry {
        .state = &villager_home::SleepingAtHome,
        .exitState = &villager_home::ExitAtHome,
    },
    /* WAKE_UP_AT_HOME */
    VillagerStateTableEntry {
        .state = &villager_home::GoHome,
        .exitState = &villager_home::ExitAtHome,
    },
    /* START_HAVING_SEX */
    VillagerStateTableEntry {
        .state = &villager_breeding::StartHavingSex,
        .exitState = &villager_breeding::ExitSex,
        .validate = &villager_breeding::SexValidate,
    },
    /* HAVING_SEX */
    VillagerStateTableEntry {
        .state = &villager_breeding::HavingSex,
        .exitState = &villager_breeding::ExitSex,
    },
    /* STOP_HAVING_SEX */
    VillagerStateTableEntry {
        .state = &villager_breeding::StopHavingSex,
        .exitState = &villager_breeding::ExitSex,
    },
    /* START_HAVING_SEX_AT_HOME: nothing ever goes into the states of love at home; they do nothing, as in the game */
    VillagerStateTableEntry {
        .state = &VillagerInvalidState,
        .exitState = &villager_home::ExitAtHome,
    },
    /* HAVING_SEX_AT_HOME */
    VillagerStateTableEntry {
        .state = &VillagerInvalidState,
        .exitState = &villager_home::ExitAtHome,
    },
    /* STOP_HAVING_SEX_AT_HOME */
    VillagerStateTableEntry {
        .state = &VillagerInvalidState,
        .exitState = &villager_home::ExitAtHome,
    },
    /* WAIT_FOR_DINNER */
    VillagerStateTableEntry {
        .state = &villager_food::DoesNothing,
    },
    /* HOMELESS_START */
    VillagerStateTableEntry {
        .state = &villager_home::HomelessStart,
    },
    /* VAGRANT_START */
    VillagerStateTableEntry {
        .state = &villager_home::VagrantStart,
    },
    /* MORN_DEATH */
    VillagerStateTableEntry {
        .state = &villager_breeding::MournMother,
    },
    /* PERFORM_INSPECTION_REACTION */ k_TodoEntry,
    /* APPROACH_OBJECT_REACTION */ k_TodoEntry,
    /* INITIALISE_TELL_OTHERS_ABOUT_OBJECT */ k_TodoEntry,
    /* TELL_OTHERS_ABOUT_INTERESTING_OBJECT */ k_TodoEntry,
    /* APPROACH_VILLAGER_TO_TALK_TO */ k_TodoEntry,
    /* TELL_PARTICULAR_VILLAGER_ABOUT_OBJECT */ k_TodoEntry,
    /* INITIALISE_LOOK_AROUND_FOR_VILLAGER_TO_TELL */ k_TodoEntry,
    /* LOOK_AROUND_FOR_VILLAGER_TO_TELL */ k_TodoEntry,
    /* MOVE_TOWARDS_OBJECT_TO_LOOK_AT */ k_TodoEntry,
    /* INITIALISE_IMPRESSED_REACTION */ k_TodoEntry,
    /* PERFORM_IMPRESSED_REACTION */ k_TodoEntry,
    /* INITIALISE_FIGHT_REACTION */ k_TodoEntry,
    /* PERFORM_FIGHT_REACTION */ k_TodoEntry,
    /* HOMELESS_EAT_DINNER */
    VillagerStateTableEntry {
        .state = &villager_food::DoesNothing,
    },
    /* INSPECT_CREATURE_REACTION */ k_TodoEntry,
    /* PERFORM_INSPECT_CREATURE_REACTION */ k_TodoEntry,
    /* APPROACH_CREATURE_REACTION */ k_TodoEntry,
    /* INITIALISE_BEWILDERED_BY_MAGIC_TREE_REACTION */ k_TodoEntry,
    /* PERFORM_BEWILDERED_BY_MAGIC_TREE_REACTION */ k_TodoEntry,
    /* TURN_TO_FACE_MAGIC_TREE */ k_TodoEntry,
    /* LOOK_AT_MAGIC_TREE */ k_TodoEntry,
    /* DANCE_FOR_EDITING_PURPOSES */ k_TodoEntry,
    /* MOVE_TO_DANCE_POS */
    VillagerStateTableEntry {
        .state = &villager_dance::MoveToDancePos,
    },
    /* INITIALISE_RESPECT_CREATURE_REACTION */ k_TodoEntry,
    /* PERFORM_RESPECT_CREATURE_REACTION */ k_TodoEntry,
    /* FINISH_RESPECT_CREATURE_REACTION */ k_TodoEntry,
    /* APPROACH_HAND_REACTION */ k_TodoEntry,
    /* FLEEING_FROM_CREATURE_REACTION */ k_TodoEntry,
    /* TURN_TO_FACE_CREATURE_REACTION */ k_TodoEntry,
    /* WATCH_FLYING_OBJECT_REACTION */ k_TodoEntry,
    /* POINT_AT_FLYING_OBJECT_REACTION */
    VillagerStateTableEntry {
        .state = &villager_physics::PointAtFlyingObject,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* DECIDE_WHAT_TO_DO */
    VillagerStateTableEntry {
        .state = &villager_home::DecideWhatToDo,
    },
    /* INTERACT_DECIDE_WHAT_TO_DO */ k_TodoEntry,
    /* EAT_OUTSIDE */
    VillagerStateTableEntry {
        .state = &villager_food::DoesNothing,
    },
    /* RUN_AWAY_FROM_OBJECT_REACTION */ k_TodoEntry,
    /* MOVE_TOWARDS_CREATURE_REACTION */ k_TodoEntry,
    /* AMAZED_BY_MAGIC_SHIELD_REACTION */
    VillagerStateTableEntry {
        .state = &villager_shield::AmazedByMagicShield,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* VILLAGER_GOSSIPS */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_ANIMAL */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_WORSHIP_SITE */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_ABODE */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_FIELD */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_FISH_FARM */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_TREE */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_BALL */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_POT */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_FOOTBALL */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_VILLAGER */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_MAGIC_LIVING */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_ROCK */ k_TodoEntry,
    /* ARRIVES_AT_ROCK_FOR_WOOD */ k_TodoEntry,
    /* GOT_WOOD_FROM_ROCK */ k_TodoEntry,
    /* REENTER_BUILDING_STATE */
    VillagerStateTableEntry {
        .state = &villager_build::ReenterBuildingState,
        .entryState = &villager_build::EnterBuilding,
        .exitState = &villager_build::ExitBuilding,
    },
    /* ARRIVE_AT_PUSH_OBJECT */ k_TodoEntry,
    /* TAKE_WOOD_FROM_TREE */ k_TodoEntry,
    /* TAKE_WOOD_FROM_POT */ k_TodoEntry,
    /* TAKE_WOOD_FROM_TREE_FOR_BUILDING */ k_TodoEntry,
    /* TAKE_WOOD_FROM_POT_FOR_BUILDING */ k_TodoEntry,
    /* SHEPHERD_TAKE_ANIMAL_FOR_SLAUGHTER */
    VillagerStateTableEntry {
        .state = &villager_shepherd::ShepherdTakeAnimalForSlaughter,
        .exitState = &villager_shepherd::ExitShepherding,
    },
    /* SHEPHERD_TAKES_CONTROL_OF_FLOCK */
    VillagerStateTableEntry {
        .state = &villager_shepherd::ShepherdTakesControlOfFlock,
        .exitState = &villager_shepherd::ExitShepherding,
    },
    /* SHEPHERD_RELEASES_CONTROL_OF_FLOCK */
    VillagerStateTableEntry {
        .state = &villager_shepherd::ShepherdReleasesControlOfFlock,
        .exitState = &villager_shepherd::ExitShepherding,
    },
    /* DANCE_BUT_NOT_WORSHIP */ k_TodoEntry,
    /* FAINTING_REACTION */ k_TodoEntry,
    /* START_CONFUSED_REACTION */ k_TodoEntry,
    /* CONFUSED_REACTION */ k_TodoEntry,
    /* AFTER_TAP_ON_ABODE */
    VillagerStateTableEntry {
        .state = &villager_home::AfterTapOnAbode,
    },
    /* WEAK_ON_GROUND */
    VillagerStateTableEntry {
        .state = &villager_food::DoesNothing,
    },
    /* SCRIPT_WANDER_AROUND_POSITION */
    VillagerStateTableEntry {
        .state = &villager_script::ScriptWanderAroundPosition,
        .exitState = &villager_script::ExitInScript,
    },
    /* SCRIPT_PLAY_ANIM */
    VillagerStateTableEntry {
        .state = &villager_script::ScriptPlayAnim,
        .exitState = &villager_script::ExitInScript,
    },
    /* GO_TOWARDS_TELEPORT_REACTION */
    VillagerStateTableEntry {
        .state = &villager_teleport::GoTowardsTeleportReaction,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* TELEPORT_REACTION */
    VillagerStateTableEntry {
        .state = &villager_teleport::TeleportReaction,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* DANCE_WHILE_REACTING */ k_TodoEntry,
    /* CONTROLLED_BY_CREATURE */ k_TodoEntry,
    /* POINT_AT_DEAD_PERSON */
    VillagerStateTableEntry {
        .state = &villager_mourning::PointAtDeadPerson,
        .exitState = &villager_reactions::ExitReaction,
        .validate = &villager_mourning::ValidateMourning,
    },
    /* GO_TOWARDS_DEAD_PERSON */
    VillagerStateTableEntry {
        .state = &villager_mourning::GoTowardsDeadPerson,
        .exitState = &villager_reactions::ExitReaction,
        .validate = &villager_mourning::ValidateMourning,
    },
    /* LOOK_AT_DEAD_PERSON */
    VillagerStateTableEntry {
        .state = &villager_mourning::LookAtDeadPerson,
        .exitState = &villager_reactions::ExitReaction,
        .validate = &villager_mourning::ValidateMourning,
    },
    /* MOURN_DEAD_PERSON */
    VillagerStateTableEntry {
        .state = &villager_mourning::MournDeadPerson,
        .exitState = &villager_reactions::ExitReaction,
        .validate = &villager_mourning::ValidateMourning,
    },
    /* NOTHING_TO_DO */
    VillagerStateTableEntry {
        .state = &villager_home::NothingToDo,
    },
    /* ARRIVES_AT_WORKSHOP_FOR_DROP_OFF */
    VillagerStateTableEntry {
        .state = &villager_disciple_work::ArrivesAtWorkshopForDropOff,
    },
    /* ARRIVES_AT_STORAGE_PIT_FOR_WORKSHOP_MATERIALS */
    VillagerStateTableEntry {
        .state = &villager_disciple_work::ArrivesAtStoragePitForWorkshopMaterials,
    },
    /* SHOW_POISONED */
    VillagerStateTableEntry {
        .state = &villager_food::ShowPoisoned,
    },
    /* HIDING_AT_WORSHIP_SITE */
    VillagerStateTableEntry {
        .state = &villager_worship::HidingAtWorshipSite,
        .exitState = &villager_worship::ExitAtWorshipSite,
    },
    /* CROWD_REACTION */ k_TodoEntry,
    /* REACT_TO_FIRE */
    VillagerStateTableEntry {
        .state = &villager_fire::ReactToFire,
        .exitState = &villager_fire::ExitReaction,
        .validate = &villager_fire::ReactionValidate,
    },
    /* PUT_OUT_FIRE_BY_BEATING */
    VillagerStateTableEntry {
        .state = &villager_fire::PutOutFireByBeating,
        .entryState = &villager_fire::EnterPutOutFire,
        .exitState = &villager_fire::ExitPutOutFire,
        .validate = &villager_fire::ReactionValidate,
    },
    /* PUT_OUT_FIRE_WITH_WATER */
    VillagerStateTableEntry {
        .state = &villager_fire::PutOutFireWithWater,
        .entryState = &villager_fire::EnterPutOutFire,
        .exitState = &villager_fire::ExitPutOutFire,
        .validate = &villager_fire::ReactionValidate,
    },
    /* GET_WATER_TO_PUT_OUT_FIRE */
    VillagerStateTableEntry {
        .state = &villager_fire::PutOutFireWithWater,
        .entryState = &villager_fire::EnterPutOutFire,
        .exitState = &villager_fire::ExitPutOutFire,
        .validate = &villager_fire::ReactionValidate,
    },
    /* ON_FIRE */
    VillagerStateTableEntry {
        .state = &villager_fire::OnFire,
        .entryState = &villager_fire::EnterOnFire,
        .exitState = &villager_fire::ExitOnFire,
    },
    /* MOVE_AROUND_FIRE */
    VillagerStateTableEntry {
        .state = &villager_fire::MoveAroundFire,
        .entryState = &villager_fire::EnterPutOutFire,
        .exitState = &villager_fire::ExitPutOutFire,
        .validate = &villager_fire::ReactionValidate,
    },
    /* DISCIPLE_NOTHING_TO_DO */
    VillagerStateTableEntry {
        .state = &villager_disciple::DiscipleNothingToDo,
        .entryState = &villager_disciple::EnterDiscipleNothingToDo,
    },
    /* FOOTBALL_MOVE_TO_BALL */ k_TodoEntry,
    /* ARRIVES_AT_STORAGE_PIT_FOR_TRADER_PICK_UP */
    VillagerStateTableEntry {
        .state = &villager_disciple_work::TraderFetch,
    },
    /* ARRIVES_AT_STORAGE_PIT_FOR_TRADER_DROP_OFF */
    VillagerStateTableEntry {
        .state = &villager_disciple_work::TraderDeliver,
    },
    /* BREEDER_DISCIPLE */
    VillagerStateTableEntry {
        .state = &villager_breeding::BreederDisciple,
        .entryState = &villager_breeding::EnterBreeder,
        .exitState = &villager_breeding::ExitBreeder,
        .validate = &villager_breeding::SexValidate,
    },
    /* MISSIONARY_DISCIPLE */
    VillagerStateTableEntry {
        .state = &villager_disciple_work::MissionaryDisciple,
    },
    /* REACT_TO_BREEDER */
    VillagerStateTableEntry {
        .state = &villager_breeding::ReactToBreeder,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* SHEPHERD_CHECK_ANIMAL_FOR_SLAUGHTER */
    VillagerStateTableEntry {
        .state = &villager_shepherd::ShepherdCheckAnimalForSlaughter,
        .exitState = &villager_shepherd::ExitShepherding,
    },
    /* INTERACT_DECIDE_WHAT_TO_DO_FOR_OTHER_VILLAGER */ k_TodoEntry,
    /* ARTIFACT_DANCE */
    VillagerStateTableEntry {
        .state = &villager_artefact_dance::ArtefactDance,
        .exitState = &villager_artefact_dance::ExitDance,
    },
    /* FLEEING_FROM_PREDATOR_REACTION */ k_TodoEntry,
    /* WAIT_FOR_WOOD */ k_TodoEntry,
    /* INSPECT_OBJECT */
    VillagerStateTableEntry {
        .state = &villager_disciple::InspectObject,
    },
    /* GO_HOME_AND_CHANGE */
    VillagerStateTableEntry {
        .state = &villager_home::GoHomeAndChange,
        .exitState = &villager_home::ExitGoHomeAndChange,
    },
    /* WAIT_FOR_MATE */
    VillagerStateTableEntry {
        .state = &villager_reactions::WaitForMate,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* GO_AND_HIDE_IN_NEARBY_BUILDING */ k_TodoEntry,
    /* LOOK_TO_SEE_IF_IT_IS_SAFE */ k_TodoEntry,
    /* SLEEP_IN_TENT */
    VillagerStateTableEntry {
        .state = &villager_home::SleepInTent,
    },
    /* PAUSE_FOR_A_SECOND */
    VillagerStateTableEntry {
        .state = &VillagerPauseForASecond,
    },
    /* PANIC_REACTION */ k_TodoEntry,
    /* GET_FOOD_AT_WORSHIP_SITE */
    VillagerStateTableEntry {
        .state = &villager_worship::GetFoodAtWorshipSite,
        .exitState = &villager_worship::ExitGetFoodAtWorship,
    },
    /* GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY */
    VillagerStateTableEntry {
        .state = &villager_home::GotoCongregateInTownAfterEmergency,
    },
    /* CONGREGATE_IN_TOWN_AFTER_EMERGENCY */
    VillagerStateTableEntry {
        .state = &villager_home::CongregateInTownAfterEmergency,
    },
    /* SCRIPT_IN_CROWD */
    VillagerStateTableEntry {
        .state = &villager_build::ScriptInCrowd,
    },
    /* GO_AND_CHILLOUT_OUTSIDE_HOME */
    VillagerStateTableEntry {
        .state = &villager_home::GoAndChilloutOutsideHome,
    },
    /* SIT_AND_CHILLOUT */
    VillagerStateTableEntry {
        .state = &villager_home::SitAndChillout,
        .entryState = &villager_home::EnterSitAndChillOut,
    },
    /* SCRIPT_GO_AND_MOVE_ALONG_PATH */ k_TodoEntry,
    /* GO_HOME_FROM_WORSHIP */
    VillagerStateTableEntry {
        .state = &villager_worship::GoHomeFromWorship,
        .exitState = &villager_home::ExitAtHome,
    },
    /* ARRIVES_HOME_FROM_WORSHIP */
    VillagerStateTableEntry {
        .state = &villager_home::ArrivesHome,
        .exitState = &villager_home::ExitAtHome,
    },
    /* SLEEP_IN_TENT_FROM_WORSHIP */
    VillagerStateTableEntry {
        .state = &villager_home::SleepInTent,
    },
    /* GO_TOWARDS_TELEPORT_REACTION_QUICKLY: running to a teleport stone is walking to it, faster */
    VillagerStateTableEntry {
        .state = &villager_teleport::GoTowardsTeleportReaction,
        .exitState = &villager_reactions::ExitReaction,
    },
    /* GO_AND_CHILLOUT_IN_TOWN */
    VillagerStateTableEntry {
        .state = &villager_home::GoAndChilloutInTown,
    },
    /* WAIT_FOR_ARTIFACT_DANCE */
    VillagerStateTableEntry {
        .state = &villager_artefact_dance::WaitForArtefactDance,
        .entryState = &villager_artefact_dance::EnterWaitForArtefactDance,
        .exitState = &villager_artefact_dance::ExitDance,
    },
    /* BREEDER_JUST_LANDED */
    VillagerStateTableEntry {
        .state = &villager_breeding::BreederJustLanded,
    },
};

void LivingActionSystem::Update()
{
	auto& registry = Locator::entitiesRegistry::value();

	registry.Each<LivingAction>([](LivingAction& action) { ++action.turnsSinceStateChange; });

	// What a hand holds, or what flies in the physics, does nothing of its own. Each villager in turn looks at whether
	// its states still hold, then, unless a clip into or out of a state holds it, looks at its needs and acts.
	registry.Each<Villager, LivingAction>(
	    [this, &registry](entt::entity entity, Villager& villager, LivingAction& action) {
		    // Magic food wears off
		    villager_food::ProcessFoodSpeedUp(entity);
		    VillagerCallValidate(action, LivingAction::Index::Top);
		    VillagerCallValidate(action, LivingAction::Index::Final);
		    // A villager carried off by a tornado does nothing of its own
		    if (registry.AnyOf<CarriedByTornado>(entity))
		    {
			    return;
		    }
		    if (villager.transitionPlaying)
		    {
			    if (villager_animate::IsReadyForNewAnimation(action))
			    {
				    villager_animate::FinishedIntoOutOfAnimation(action);
			    }
			    return;
		    }
		    // The life its state costs and the checks on its needs come before its state does its part
		    villager_checks::CheckEveryTime(action);
		    VillagerCallState(action, LivingAction::Index::Top);
	    },
	    entt::exclude<InHand, InPhysics>);
	// A villager in the hand has only its hand state to do: taking up the work of what it is held near
	registry.Each<Villager, LivingAction, InHand>([this](Villager&, LivingAction& action, InHand&) {
		if (VillagerGetState(action, LivingAction::Index::Top) == VillagerStates::InHand)
		{
			VillagerCallState(action, LivingAction::Index::Top);
		}
	});
	// TODO(#476): same calls but for other types of living
}

VillagerStates LivingActionSystem::VillagerGetState(const LivingAction& action, LivingAction::Index index) const
{
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
}

namespace
{
/// Keeps a villager out of sight while its top state hides it
void UpdateHiddenByState(entt::entity villager, VillagerStates state)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::infoConstants::has_value() || !registry.AllOf<Villager>(villager))
	{
		return;
	}
	const bool hidden = villager_animate::IsHiddenByState(state);
	if (hidden != registry.AllOf<HiddenByState>(villager))
	{
		if (hidden)
		{
			registry.Assign<HiddenByState>(villager);
		}
		else
		{
			registry.Remove<HiddenByState>(villager);
		}
		registry.SetDirty();
	}
}

/// A villager's town counts what its people are doing for it: a villager leaving a state that counts towards one of the
/// town's desires is taken off that desire's count, and one going into such a state is added, by the state's weight
void CountWhatTownIsDoing(entt::entity villager, VillagerStates previous, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* person = registry.TryGet<const Villager>(villager);
	if (person == nullptr || !Locator::infoConstants::has_value() || person->town == entt::null ||
	    !registry.Valid(person->town))
	{
		return;
	}
	auto* desire = registry.TryGet<TownDesire>(person->town);
	if (desire == nullptr)
	{
		return;
	}
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	const auto count = [&](VillagerStates state, float sign) {
		if (state == VillagerStates::InvalidState)
		{
			return;
		}
		const auto& row = table.at(static_cast<size_t>(state));
		if (row.isFinalState == 0)
		{
			return;
		}
		const auto change = ecs::town_desire::DoingNowChange({.desire = row.field0x4, .weight = row.field0x8}, sign);
		if (change.has_value())
		{
			desire->doingNow.at(change->desire) += change->amount;
			desire->doingNowCount.at(change->desire) += change->count;
		}
	};
	count(previous, -1.0f);
	count(next, 1.0f);
}

/// The state a villager's top state works towards: the top state itself when that is a final one
VillagerStates FinalStateOf(const LivingAction& action)
{
	const auto top = static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Top)));
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	if (table.at(static_cast<size_t>(top)).isFinalState != 0)
	{
		return top;
	}
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Final)));
}
} // namespace

VillagerStates LivingActionSystem::VillagerGetFinalState(const LivingAction& action) const
{
	return FinalStateOf(action);
}

void LivingActionSystem::SetStateDirectly(LivingAction& action, LivingAction::Index index, VillagerStates state) const
{
	if (index == LivingAction::Index::Top)
	{
		action.turnsSinceStateChange = 0;
	}
	const auto previous = static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	action.states.at(static_cast<size_t>(index)) = static_cast<uint8_t>(state);
	CountWhatTownIsDoing(Locator::entitiesRegistry::value().ToEntity(action), previous, state);
	if (index == LivingAction::Index::Top)
	{
		UpdateHiddenByState(Locator::entitiesRegistry::value().ToEntity(action), state);
	}
}

void LivingActionSystem::VillagerSetState(LivingAction& action, LivingAction::Index index, VillagerStates state,
                                          bool skipTransition) const
{
	const auto previousState = static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	[[maybe_unused]] auto& registry = Locator::entitiesRegistry::value();
	SPDLOG_LOGGER_TRACE(spdlog::get("ai"), "Villager #{}: Setting state {} -> {}", static_cast<int>(registry.ToEntity(action)),
	                    k_VillagerStateStrings.at(static_cast<size_t>(previousState)),
	                    k_VillagerStateStrings.at(static_cast<size_t>(state)));

	// Set as it is, with none of the state's rules: the final or remembered states, and a top state put straight in
	if (index != LivingAction::Index::Top || skipTransition || !Locator::infoConstants::has_value())
	{
		if (previousState != state)
		{
			SetStateDirectly(action, index, state);
		}
		return;
	}
	SetTopState(action, state);
}

bool LivingActionSystem::ExitAllowed(LivingAction& action, VillagerStates next) const
{
	// Both the top state and the state it works towards must let the villager go
	const auto top = VillagerGetState(action, LivingAction::Index::Top);
	const auto final = FinalStateOf(action);
	bool allowed = !VillagerCallExitState(action, LivingAction::Index::Top, next);
	if (final != top)
	{
		const auto& entry = k_VillagerStateTable.at(static_cast<size_t>(final));
		if (entry.exitState && entry.exitState(action, next))
		{
			allowed = false;
		}
	}
	return allowed;
}

bool LivingActionSystem::EnterState(LivingAction& action, VillagerStates state, VillagerStates workedTowards) const
{
	const auto& entry = k_VillagerStateTable.at(static_cast<size_t>(state));
	if (entry.entryState && !entry.entryState(action, workedTowards, state))
	{
		return false;
	}
	// A new top state lets go of the state the villager was heading for first: whatever it goes on to next is set after
	// this, as a walk sets where it is heading once it is walking
	if (VillagerGetState(action, LivingAction::Index::Final) != VillagerStates::InvalidState)
	{
		SetStateDirectly(action, LivingAction::Index::Final, VillagerStates::InvalidState);
	}
	SetStateDirectly(action, LivingAction::Index::Top, state);
	return true;
}

LivingActionSystem::SetResult LivingActionSystem::ChangeTopState(LivingAction& action, VillagerStates current,
                                                                 std::optional<VillagerStates> destination) const
{
	const auto next = destination.value_or(current);
	if (!ExitAllowed(action, next))
	{
		return SetResult::Refused;
	}
	const auto outOf = villager_animate::OutOfClip(action, next);
	// Both states are told what the villager was working towards before the change, so going on from a state passed
	// through on the way (walking, pausing) into the state it was heading for doesn't count as starting it afresh
	const auto previous = FinalStateOf(action);
	if (!EnterState(action, current, previous))
	{
		return SetResult::EntryFailed;
	}
	if (destination.has_value())
	{
		const auto& entry = k_VillagerStateTable.at(static_cast<size_t>(*destination));
		if (entry.entryState && !entry.entryState(action, previous, *destination))
		{
			return SetResult::EntryFailed;
		}
		SetStateDirectly(action, LivingAction::Index::Final, *destination);
	}
	const auto villager = Locator::entitiesRegistry::value().ToEntity(action);
	villager_reactions::SetStateSpeed(villager, current);
	if (outOf.has_value())
	{
		villager_animate::SetAnim(villager, static_cast<int32_t>(*outOf), true);
		return SetResult::Done;
	}
	villager_animate::SetStateAnim(villager);
	if (const auto into = villager_animate::IntoClip(action, next); into.has_value())
	{
		villager_animate::SetAnim(villager, static_cast<int32_t>(*into), true);
	}
	return SetResult::Done;
}

bool LivingActionSystem::PausesBefore(LivingAction& action, VillagerStates state) const
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	if (VillagerGetState(action, LivingAction::Index::Top) == VillagerStates::PauseForASecond ||
	    table.at(static_cast<size_t>(state)).canPauseForASecond == 0 || registry.AllOf<ScriptControlled>(villager) ||
	    !registry.AllOf<Villager>(villager) || !Locator::gameRandom::has_value())
	{
		return false;
	}
	const auto& person = registry.Get<const Villager>(villager);
	const auto& info =
	    Locator::infoConstants::value().villager.at(static_cast<size_t>(GVillagerInfo::Find(person.tribe, person.number)));
	const float life = ecs::world_objects::LifeOf(villager);
	const bool poisoned = registry.AllOf<Poisoned>(villager);
	const float draw = Locator::gameRandom::value().GameFloatRand(1.0f);
	return ecs::villager_needs::PausesForASecond(draw, life, poisoned, info.pauseForASecondChance) &&
	       ChangeTopState(action, VillagerStates::PauseForASecond, state) == SetResult::Done;
}

void LivingActionSystem::SetTopState(LivingAction& action, VillagerStates state) const
{
	VillagerChangeTopState(action, state);
}

bool LivingActionSystem::VillagerChangeTopState(LivingAction& action, VillagerStates state) const
{
	if (PausesBefore(action, state))
	{
		return true;
	}
	const auto result = ChangeTopState(action, state, std::nullopt);
	if (result == SetResult::EntryFailed)
	{
		// A state that won't be gone into leaves the villager deciding what to do
		EnterState(action, VillagerStates::DecideWhatToDo, FinalStateOf(action));
	}
	return result != SetResult::Refused;
}

bool LivingActionSystem::VillagerSetCurrentAndDestinationState(LivingAction& action, VillagerStates current,
                                                               VillagerStates destination) const
{
	const auto result = ChangeTopState(action, current, destination);
	if (result == SetResult::EntryFailed)
	{
		// A state that won't be gone into leaves the villager deciding what to do
		EnterState(action, VillagerStates::DecideWhatToDo, FinalStateOf(action));
	}
	return result == SetResult::Done;
}

void LivingActionSystem::VillagerPlayAnimThenSetState(LivingAction& action, VillagerStates next) const
{
	// The clip it plays now plays on to its end, then it goes on into the next state
	const auto previous = FinalStateOf(action);
	if (!ExitAllowed(action, next) || !EnterState(action, VillagerStates::WaitForAnimation, previous))
	{
		return;
	}
	const auto& entry = k_VillagerStateTable.at(static_cast<size_t>(next));
	if (entry.entryState && !entry.entryState(action, previous, next))
	{
		return;
	}
	SetStateDirectly(action, LivingAction::Index::Final, next);
}

void LivingActionSystem::VillagerSetTopStateToFinal(LivingAction& action) const
{
	SetTopState(action, VillagerGetState(action, LivingAction::Index::Final));
}

bool LivingActionSystem::VillagerIsReadyForNewAnimation(const LivingAction& action, uint32_t times) const
{
	return villager_animate::IsReadyForNewAnimation(action, times);
}

bool LivingActionSystem::VillagerCanBeDirected(entt::entity villager) const
{
	return villager_script::CanBeDirected(villager);
}

void LivingActionSystem::VillagerSetScriptState(entt::entity villager, VillagerStates state) const
{
	villager_script::SetScriptState(villager, state);
}

void LivingActionSystem::VillagerScriptMoveTo(entt::entity villager, glm::vec2 goal) const
{
	villager_script::MoveTo(villager, goal);
}

void LivingActionSystem::VillagerSetScriptAnimation(entt::entity villager, AnimId clip, uint32_t plays) const
{
	villager_script::SetScriptAnimation(villager, clip, plays);
}

void LivingActionSystem::VillagerSetScriptPlace(entt::entity villager, glm::vec2 place) const
{
	villager_script::SetScriptPlace(villager, place);
}

void LivingActionSystem::VillagerSetScriptNumber(entt::entity villager, float number) const
{
	villager_script::SetScriptNumber(villager, number);
}

void LivingActionSystem::VillagerSetScriptNumbers(entt::entity villager, uint32_t first, uint32_t second) const
{
	villager_script::SetScriptNumbers(villager, first, second);
}

bool LivingActionSystem::VillagerHasPlayedScriptAnimation(entt::entity villager) const
{
	return villager_script::HasPlayedScriptAnimation(villager);
}

void LivingActionSystem::VillagerFace(entt::entity villager, glm::vec2 point) const
{
	villager_script::Face(villager, point);
}

std::optional<float> LivingActionSystem::VillagerYAngle(entt::entity villager) const
{
	return villager_script::YAngle(villager);
}

void LivingActionSystem::VillagerSetYAngle(entt::entity villager, float angle) const
{
	villager_script::SetYAngle(villager, angle);
}

void LivingActionSystem::VillagerOverrideAnimation(entt::entity villager, int32_t clip) const
{
	villager_script::OverrideAnimation(villager, clip);
}

void LivingActionSystem::VillagerSetAge(entt::entity villager, uint32_t age) const
{
	villager_script::SetAge(villager, age);
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

bool LivingActionSystem::VillagerCallEntryState(LivingAction& action, LivingAction::Index index, VillagerStates src,
                                                VillagerStates dst) const
{
	const auto state = action.states.at(static_cast<size_t>(index));
	const auto& entry = k_VillagerStateTable.at(static_cast<size_t>(state));
	const auto& callback = entry.entryState;
	if (!callback)
	{
		return false;
	}
	return callback(action, src, dst);
}

bool LivingActionSystem::VillagerCallExitState(LivingAction& action, LivingAction::Index index, VillagerStates next) const
{
	const auto& state = action.states.at(static_cast<size_t>(index));
	const auto& entry = k_VillagerStateTable.at(static_cast<size_t>(state));
	const auto& callback = entry.exitState;
	if (!callback)
	{
		return false;
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
	const auto state = action.states.at(static_cast<size_t>(index));
	const auto& entry = k_VillagerStateTable.at(static_cast<size_t>(state));
	const auto& callback = entry.validate;
	if (!callback)
	{
		return false;
	}
	return callback(action);
}
