/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerResourceReactions.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <string>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerReaction.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Life.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerForester.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/Villager/VillagerTrace.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerReactions.h"

// The villagers' food and wood reactions (VillagerResourceReactions.h)

namespace openblack::ecs::villager_resource_reactions
{
using namespace components;
namespace tq = town_queries;

namespace
{
/// The villager's own interest threshold for a food object
constexpr float k_OwnFoodThreshold = 0.25f;
/// A sped-up pile's boost
constexpr float k_SpeedUpBoost = 2.0f;
/// The drop-off threshold on arriving with food
constexpr float k_FoodDropOffThreshold = 0.1f;
/// The falling log's wait, in seconds
constexpr float k_FallSeconds = 100.0f;
/// How near the falling log the villager waits
constexpr float k_FallReach = 0.5f;
/// The turns waited at the falling log
constexpr uint16_t k_FallWaitTurns = 10;
/// The look-at mode at the falling log
constexpr uint32_t k_LookMode = 1;
/// SET_INTERACT_DESIRE's value (a feature script command; reset when a playground game starts).
/// TODO(SET_INTERACT_DESIRE): FeatureScriptCommands::SetInteractDesire is an empty stub and no Land 1-5 script uses it
constexpr float k_InteractDesire = 0.0f;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, line);
	}
}

/// The villager's town: a valid town entity, or null
entt::entity TownOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original only reads the link: stands for the missing town unlinking
	if (v == nullptr || v->town == entt::null || !ecs::IsAvailable(v->town) || !Entities().AllOf<Town>(v->town))
	{
		return entt::null;
	}
	return v->town;
}

const ReactionInfo& Info(Reaction type)
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(type));
}

/// In the physics: PhysicsObjects::IsFlying (a resting proxy is in the list without the in-physics flag)
bool InPhysics(entt::entity object)
{
	return physics::PhysicsObjects::IsFlying(object);
}

/// In the hand (fire::traits::InHand)
bool InHand(entt::entity object)
{
	return fire::traits::InHand(object);
}

/// Whether a reaction object (a dead tree, a pile or a pot) is available
bool IsAvailable(entt::entity object)
{
	return object != entt::null && Entities().Valid(object) && fire::traits::IsAvailable(object);
}

/// Sped up: a pot's or pile's speedUp flag; nothing else is
bool IsSpeedUp(entt::entity object)
{
	const auto* pot = Entities().TryGet<const Pot>(object);
	return pot != nullptr && pot->speedUp;
}

/// A dead tree: the DeadTree entities
bool IsDeadTree(entt::entity object)
{
	return Entities().AllOf<DeadTree>(object);
}

/// A pot from a building site; nothing else is. (inferred) the flag is the building site's pile
/// (building_sites::SiteOfPile), its writer was not traced
bool IsAPotFromABuildingSite(entt::entity object)
{
	return Entities().AllOf<Pot>(object) && building_sites::SiteOfPile(object) != entt::null;
}

/// The resource type: dead trees and trees -> WOOD; food piles -> FOOD; wood piles -> WOOD; a pot -> its GPotInfo
/// resourceType ((inferred) copied at creation); none for the rest
ResourceType ResourceTypeOf(entt::entity object)
{
	auto& registry = Entities();
	if (registry.AnyOf<DeadTree, Tree>(object))
	{
		return ResourceType::Wood;
	}
	if (const auto* info = object::PotInfoOf(object); info != nullptr)
	{
		switch (info->potType)
		{
		case PotType::PileFood:
			return ResourceType::Food;
		case PotType::PileWood:
			return ResourceType::Wood;
		default:
			return info->resourceType;
		}
	}
	return ResourceType::None;
}

/// Removing wood: a dead tree's (ecs::RemoveWood), the piles' and pots' (object_resources::RemoveResource)
uint32_t RemoveWoodFrom(entt::entity object, uint32_t amount)
{
	if (Entities().AllOf<DeadTree>(object))
	{
		return RemoveWood(object, amount);
	}
	// (note) a live Tree would come here too, while the original's tree keeps the base removal;
	// unreachable: a reaction-12 initiator is a DeadTree, a pile or a pot, never a standing tree
	return object_resources::RemoveResource(object, ResourceType::Wood, amount);
}

/// The carried tree type: a dead tree's or tree's (ecs::TreeCarriedType), 0 for the rest
uint8_t CarriedTreeTypeOf(entt::entity object)
{
	if (Entities().AnyOf<DeadTree, Tree>(object))
	{
		return static_cast<uint8_t>(TreeCarriedType(object));
	}
	return 0;
}

/// The distance in metres from the villager to the object
float DistanceTo(entt::entity villager, entt::entity object)
{
	return tq::GetDistanceInMetres(tq::PosOf(villager), tq::PosOf(object));
}

/// The distance modifier of the distance to the object against a maximum
float DistanceModifierTo(entt::entity villager, entt::entity object, float maximum)
{
	return gutils::GetDistanceModifier(DistanceTo(villager, object), maximum);
}

/// The object's working point for the villager (the same for dead trees and pots), in metres
glm::vec2 WorkingPos(entt::entity object, entt::entity villager)
{
	const auto pos = object::GetWorkingPos(object, villager);
	return tq::ToMetres({pos.x, pos.z});
}

/// The disciple test of 20 and GotWoodDecideWhatToDo: a disciple whose type ignores needs
bool DiscipleGoesBack(const Villager& v)
{
	return (v.flags & Villager::k_FlagDisciple) != 0 && villager::DiscipleIgnoresNeeds(v.discipleType);
}

/// The shared walk of 19 and 21: the object available -> SetupMoveToWithHug(its working point, final); else
/// StopReactingAndSetState. 1
uint32_t LivingGotoReaction(entt::entity villager, entt::entity object, VillagerStates final)
{
	if (!IsAvailable(object))
	{
		villager_reactions::StopReactingAndSetState(villager);
		return 1;
	}
	villager::SetupMoveToWithHug(villager, WorkingPos(object, villager), final);
	return 1;
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

float FoodInterestScore(float boost, float frac, float want, float stateInterest, float distanceModifier, float interact)
{
	// in this order
	float v = boost * frac;
	v = v * want;
	v = v * stateInterest;
	v = v * distanceModifier;
	return v + interact;
}

float TownInterestScore(float desire, float frac, float stateInterest, float distanceModifier, float interact)
{
	float v = desire * frac;
	v = v * stateInterest;
	v = v * distanceModifier;
	return interact + v;
}

float FoodDropOffScore(float desire, float frac, float distanceModifier)
{
	const float v = desire * frac;
	return v * distanceModifier;
}

float CapacityFraction(int16_t capacity, uint32_t maxCarried)
{
	// one float rounding (24-bit FPU)
	const auto max = static_cast<int32_t>(maxCarried);
	return max != 0 ? static_cast<float>(capacity) / static_cast<float>(max) : 0.0f;
}

uint32_t ReactionFallLimit(uint32_t msPerTurn)
{
	// 1000 / ms per turn, x 100.0, truncated. (openblack, guard) no division by 0
	const uint32_t perSecond = 1000u / std::max<uint32_t>(msPerTurn, 1u);
	const float turns = static_cast<float>(perSecond) * k_FallSeconds;
	return static_cast<uint32_t>(static_cast<int32_t>(std::trunc(turns)));
}

bool IsForesterWorkState(VillagerStates final)
{
	// 47 <= s <= 50 (signed on the byte) or s == 52
	const auto s = static_cast<int32_t>(static_cast<uint8_t>(final));
	return (s >= 0x2F && s <= 0x32) || s == 0x34;
}

// ---- the type table's slots ---------------------------------------------------------------------------------------

bool IsInterestedInFoodObject(entt::entity villager, entt::entity object)
{
	const auto* v = VillagerOf(villager);
	// not available, in the physics or in the hand -> 0
	if (v == nullptr || !IsAvailable(object) || InPhysics(object) || InHand(object))
	{
		return false;
	}
	const auto& info = villager::InfoOf(villager);
	// GDM(distance, ReactionInfo[7] maxReactionDistance (35))
	const float d = DistanceModifierTo(villager, object, Info(Reaction::ReactToFood).maxReactionDistance);
	// the final state's food interest
	const float s = villager::state_info::FoodInterest(villager::state_info::StateInfo(villager::GetFinalState(villager)));
	const float want = villager::GetDesireForFood(villager);
	// (float)(int16)food capacity / (int)MaxFoodCarried
	const float frac = CapacityFraction(villager::GetFoodCapacity(villager), info.maxFoodCarried);
	// sped up ? 2.0 : 1.0
	const float boost = IsSpeedUp(object) ? k_SpeedUpBoost : 1.0f;
	// > 0.25 -> 1
	const float own = FoodInterestScore(boost, frac, want, s, d, k_InteractDesire);
	if (own > k_OwnFoodThreshold)
	{
		villager::TraceFormatted(villager, "react 7: {} own {:.9f} -> interested", static_cast<uint32_t>(object), own);
		return true;
	}
	// no town -> 0
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return false;
	}
	// the town's food desire x frac x s x d + interact > the food desire info's trigger
	const float desire = town_desire::GetDesire(town, TownDesireInfo::ForFood);
	const float value = TownInterestScore(desire, frac, s, d, k_InteractDesire);
	const float trigger = Locator::infoConstants::value().townDesire.at(0).desireTriggersVillagerAction;
	villager::TraceFormatted(villager, "react 7: {} own {:.9f} town {:.9f} (trigger {:.9f})", static_cast<uint32_t>(object),
	                         own, value, trigger);
	return value > trigger;
}

bool IsInterestedInWoodObject(entt::entity villager, entt::entity object)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return false;
	}
	// the town first
	const auto town = TownOf(villager);
	// not available or in the hand -> 0
	if (!IsAvailable(object) || InHand(object))
	{
		return false;
	}
	// no wood capacity (a negative one goes on) -> 0
	if (villager::GetWoodCapacity(villager) == 0)
	{
		return false;
	}
	// no town -> 0
	if (town == entt::null)
	{
		return false;
	}
	const auto& info = villager::InfoOf(villager);
	// life at or below DamageThresholdToGoHome (0.3) -> 0
	if (!(life::LifeOf(villager) > info.damageThresholdToGoHome))
	{
		return false;
	}
	// a dead tree, less wood held than the capacity and IsVillagerAvailable -> 1
	// (below 125 held any log in reach is wanted, whatever the town needs)
	if (IsDeadTree(object) && v->resourceHeld.at(1) < villager::GetWoodCapacity(villager) &&
	    villager::IsVillagerAvailable(villager))
	{
		villager::TraceFormatted(villager, "react 12: {} dead tree, held {} -> interested", static_cast<uint32_t>(object),
		                         v->resourceHeld.at(1));
		return true;
	}
	// the building site valid and needing no wood -> 0
	const auto site = v->buildingSite;
	if (site != entt::null && building_sites::IsBuildingSiteValid(town, site) &&
	    !(building_sites::GetWoodNeededToBuild(site) > 0.0f))
	{
		return false;
	}
	// (a building site's pot || a disciple that fetches wood) && IsVillagerAvailable -> 1
	// (villager::DiscipleFetchesWood: FORESTER, BUILDER and CRAFTSMAN)
	const bool disciple = villager::DiscipleFetchesWood(v->discipleType);
	if ((IsAPotFromABuildingSite(object) || disciple) && villager::IsVillagerAvailable(villager))
	{
		return true;
	}
	// GDM(d, ReactionInfo[12] maxReactionDistance (35))
	const float d = DistanceModifierTo(villager, object, Info(Reaction::ReactToWood).maxReactionDistance);
	// the final state's wood interest
	const float s = villager::state_info::WoodInterest(villager::state_info::StateInfo(villager::GetFinalState(villager)));
	// (float)(int16)wood capacity / (int)MaxWoodCarried
	const float frac = CapacityFraction(villager::GetWoodCapacity(villager), info.maxWoodCarried);
	// the town's raw wood desire x frac x s x d + interact > the wood desire info's trigger
	const float raw = town_desire::GetRawDesire(town, TownDesireInfo::ForWood);
	const float value = TownInterestScore(raw, frac, s, d, k_InteractDesire);
	const float trigger = Locator::infoConstants::value().townDesire.at(1).desireTriggersVillagerAction;
	villager::TraceFormatted(villager, "react 12: {} town {:.9f} (trigger {:.9f})", static_cast<uint32_t>(object), value,
	                         trigger);
	return value > trigger;
}

uint8_t ReactToFoodPriority(entt::entity villager, uint32_t reaction, [[maybe_unused]] uint32_t other)
{
	const auto* r = effects::reactions::Find(reaction);
	// the initiator object
	if (r == nullptr || r->initiator == entt::null || !Entities().Valid(r->initiator))
	{
		return 0;
	}
	const auto object = r->initiator;
	// in the hand or the physics -> 0
	if (InHand(object) || InPhysics(object))
	{
		return 0;
	}
	// no food -> 0
	if (object_resources::GetResource(object, ResourceType::Food) == 0)
	{
		return 0;
	}
	// farther than ReactionInfo[7] maxDistanceToRunAwayFromObject (100) -> 0
	const auto& info = Info(Reaction::ReactToFood);
	if (!(DistanceTo(villager, object) <= info.maxDistanceToRunAwayFromObject))
	{
		return 0;
	}
	// IsInterestedInFoodObject ? the priority (60) : 0
	return IsInterestedInFoodObject(villager, object) ? static_cast<uint8_t>(info.priority) : 0;
}

uint8_t ReactToWoodPriority(entt::entity villager, uint32_t reaction, [[maybe_unused]] uint32_t other)
{
	const auto* r = effects::reactions::Find(reaction);
	if (r == nullptr || r->initiator == entt::null || !Entities().Valid(r->initiator))
	{
		return 0;
	}
	// in the hand -> 0; no physics, resource or distance test
	if (InHand(r->initiator))
	{
		return 0;
	}
	// IsInterestedInWoodObject ? the priority (55) : 0
	return IsInterestedInWoodObject(villager, r->initiator) ? static_cast<uint8_t>(Info(Reaction::ReactToWood).priority) : 0;
}

void SetupReactToFood(entt::entity villager, entt::entity object, uint32_t reaction)
{
	villager_reactions::AddReaction(villager, reaction, VillagerStates::GotoFoodReaction);
	villager_reactions::SetSlotObject(villager, object);
}

void SetupReactToWood(entt::entity villager, entt::entity object, uint32_t reaction)
{
	villager_reactions::AddReaction(villager, reaction, VillagerStates::GotoWoodReaction);
	villager_reactions::SetSlotObject(villager, object);
}

uint32_t StandardTurnsToReact([[maybe_unused]] entt::entity villager, [[maybe_unused]] entt::entity object, uint8_t type,
                              float distance)
{
	return effects::reactions::StandardTurnsToReact(type, distance);
}

uint32_t StandardTurnsBeforeAgain([[maybe_unused]] entt::entity villager, [[maybe_unused]] entt::entity object, uint8_t type,
                                  float distance)
{
	return effects::reactions::StandardTurnsBeforeReactingAgain(type, distance);
}

uint32_t TurnsBeforeReactingToWoodAgain(entt::entity villager, entt::entity object, uint8_t type, float distance)
{
	// the forester's work -> 0; else the standard
	if (IsForesterWorkState(villager::GetFinalState(villager)))
	{
		return 0;
	}
	return StandardTurnsBeforeAgain(villager, object, type, distance);
}

// ---- the states --------------------------------------------------------------------------------------------------

uint32_t GotoFoodReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto object = villager_reactions::SlotObject(villager);
	// no object or not available -> StopReactingAndSetState, 1
	if (!IsAvailable(object))
	{
		TraceIf(villager, "react 19: object gone -> stop");
		villager_reactions::StopReactingAndSetState(villager);
		return 1;
	}
	// the walk with FINAL 20
	villager::TraceFormatted(villager, "react 19: walk to {} -> 20", static_cast<uint32_t>(object));
	return LivingGotoReaction(villager, object, VillagerStates::ArrivesAtFoodReaction);
}

uint32_t ArrivesAtFoodReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto object = villager_reactions::SlotObject(villager);
	// no object or not available -> StopReactingAndSetState, 1
	if (!IsAvailable(object))
	{
		TraceIf(villager, "react 20: object gone -> stop");
		villager_reactions::StopReactingAndSetState(villager);
		return 1;
	}
	// a food pile (GPotInfo potType 1: the pit's Food Pile, Hand Food, Magic Food) and capacity > 0 ->
	// GetResourceFrom(o, FOOD, cap). A plain Food Pot (potType 0) gives nothing
	const int16_t cap = villager::GetFoodCapacity(villager);
	uint16_t took = 0;
	if (object::IsPileFood(object) && cap > 0)
	{
		took = villager::GetResourceFrom(villager, object, ResourceType::Food, cap);
	}
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// no food held -> 163, 1
	if (v->resourceHeld.at(0) <= 0)
	{
		villager::TraceFormatted(villager, "react 20: took {} -> 163", took);
		villager_reactions::SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// hungry -> 117 EAT_FOOD, 1
	if (villager::IsHungry(villager))
	{
		villager::TraceFormatted(villager, "react 20: took {} -> 117", took);
		villager_reactions::SetTopState(villager, VillagerStates::EatFood);
		return 1;
	}
	// a disciple held at its job -> 163 (villager::DiscipleHeldAtJob)
	if (DiscipleGoesBack(*v))
	{
		villager_reactions::SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// the food drop-off point (may make the town's MagicFood pot);
	// GDM(distance to it, MaxDistCarryFoodPit (100))
	const auto& info = villager::InfoOf(villager);
	const auto pos = villager::GetResourceDropoffPos(villager, ResourceType::Food);
	const float dm = gutils::GetDistanceModifier(tq::GetDistanceInMetres(tq::PosOf(villager), pos), info.maxDistCarryFoodPit);
	// (float)(int16)food held / (int)MaxFoodCarried: the load, not the capacity
	const float frac = CapacityFraction(v->resourceHeld.at(0), info.maxFoodCarried);
	// no town -> 163
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		villager_reactions::SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// the town's CalculateDesireForFood (with the low-food warning) x frac x dm > 0.1 -> GotoStoragePitForDropOff
	// (its result)
	const float desire = town_desire::CalculateDesireForFood(town);
	const float score = FoodDropOffScore(desire, frac, dm);
	if (score > k_FoodDropOffThreshold)
	{
		villager::TraceFormatted(villager, "react 20: took {} drop {:.9f} -> 31", took, score);
		return villager::GotoStoragePitForDropOff(villager);
	}
	// else 163, 1
	villager::TraceFormatted(villager, "react 20: took {} drop {:.9f} -> 163", took, score);
	villager_reactions::SetTopState(villager, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t GotoWoodReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto object = villager_reactions::SlotObject(villager);
	// no object, not available, in the hand or on fire -> stop, 1
	if (!IsAvailable(object) || InHand(object) || fire::IsOnFire(object))
	{
		TraceIf(villager, "react 21: object gone -> stop");
		villager_reactions::StopReactingAndSetState(villager);
		return 1;
	}
	// the walk with FINAL 22
	villager::TraceFormatted(villager, "react 21: walk to {} -> 22", static_cast<uint32_t>(object));
	return LivingGotoReaction(villager, object, VillagerStates::ArrivesAtWoodReaction);
}

uint32_t ArrivesAtWoodReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto object = villager_reactions::SlotObject(villager);
	// no object, not available, in the hand or on fire -> 163, 1 (leaving the reaction stops it: 163 is not
	// reactive)
	if (!IsAvailable(object) || InHand(object) || fire::IsOnFire(object))
	{
		TraceIf(villager, "react 22: object gone -> 163");
		villager_reactions::SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// still falling
	if (InPhysics(object))
	{
		// falling for longer than ReactionFallLimit since the reaction was recorded -> 163
		const auto slotType = villager_reactions::SlotReaction(villager) != 0
		                          ? Entities().Get<const VillagerReactionSlot>(villager).type
		                          : Reaction::ReactToWood;
		const auto limit = ReactionFallLimit(game_clock::MsPerTurn());
		const auto since = villager::CurrentTurn() -
		                   effects::reactions::RecordTurn(villager, static_cast<uint8_t>(static_cast<int>(slotType)));
		if (since > limit)
		{
			villager::TraceFormatted(villager, "react 22: falling {} turns > {} -> gave up 163", since, limit);
			villager_reactions::SetTopState(villager, VillagerStates::DecideWhatToDo);
			return 1;
		}
		// at the working point (within 0.5)
		const auto wp = WorkingPos(object, villager);
		if (villager::AreWeThere(villager, wp, k_FallReach))
		{
			// still turning to look at it -> 1
			if (villager::LookAtPos(villager, tq::PosOf(object), k_LookMode) == 0)
			{
				return 1;
			}
			// wait 10 turns (57, back to 22)
			TraceIf(villager, "react 22: falling wait 57");
			villager::SetupWaitForCounter(villager, k_FallWaitTurns, villager::GetFinalState(villager));
			return 1;
		}
		// else walk to the working point
		villager::SetupMoveToWithHug(villager, wp, villager::GetFinalState(villager));
		return 1;
	}
	// a wood object and wood capacity > 0
	if (ResourceTypeOf(object) == ResourceType::Wood)
	{
		const int16_t cap = villager::GetWoodCapacity(villager);
		if (cap > 0)
		{
			// n = the wood removed, up to the capacity; n > 0 (signed)
			const auto tree = CarriedTreeTypeOf(object);
			const auto n = static_cast<int32_t>(RemoveWoodFrom(object, static_cast<uint32_t>(cap)));
			if (n > 0)
			{
				// PickupWood(n, the carried tree type). (openblack) the tree type
				// is read before the removal: RemoveWood may delete the log that gave its last wood (the original reads
				// it after, from the object still in memory)
				villager::TraceFormatted(villager, "react 22: took {} type {}", n, tree);
				villager::PickupWood(villager, static_cast<int16_t>(n), tree);
			}
		}
	}
	// then GotWoodDecideWhatToDo (always)
	return villager::GotWoodDecideWhatToDo(villager);
}
} // namespace openblack::ecs::villager_resource_reactions
