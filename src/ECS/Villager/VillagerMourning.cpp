/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerMourning.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <string>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerMourningState.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/VillagerStateSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerTrace.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"

// The mourning of a dead villager and its orphans (VillagerMourning.h).

namespace openblack::ecs::villager_mourning
{
using namespace components;
namespace tq = openblack::ecs::town_queries;
using Index = LivingAction::Index;

namespace
{
/// How many Livings took each reaction, by reaction id (Locator::villagerStateSystem)
std::unordered_map<uint32_t, uint32_t>& Takers()
{
	if (!Locator::villagerStateSystem::has_value())
	{
		std::fputs("ecs::villager_mourning: no villagerStateSystem in the locator (Locator::villagerStateSystem)\n", stderr);
		std::abort();
	}
	return Locator::villagerStateSystem::value().MourningTakers();
}

/// 1000 / game_clock::MsPerTurn (100), an unsigned division (the original faults on 0; 0 here)
uint32_t TurnsPerSecond()
{
	const auto ms = game_clock::MsPerTurn();
	return ms != 0 ? 1000u / ms : 0u;
}

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The villager's mourning state if it has one (any id, alive or not)
VillagerMourningState* FindState(entt::entity villager)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Entities();
	return registry.Valid(villager) ? registry.TryGet<VillagerMourningState>(villager) : nullptr;
}

/// The villager's mourning state, added with its defaults if it has none. The villager must exist.
VillagerMourningState& StateOf(entt::entity villager)
{
	if (auto* state = Entities().TryGet<VillagerMourningState>(villager))
	{
		return *state;
	}
	return Entities().AssignState<VillagerMourningState>(villager);
}

entt::entity EntityOf(const LivingAction& action)
{
	return Entities().ToEntity(action);
}

LivingAction* ActionOf(entt::entity villager)
{
	return Entities().TryGet<LivingAction>(villager);
}

const ReactionInfo& DeathReactionInfo()
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(openblack::Reaction::ReactToDeath));
}

/// Takes the reaction (a first step of the original is not ported): StorePreviousState when it follows none,
/// SetTopState(state), leaving the dance (pending: dance), the reaction kept and one more taker
void AddReaction(entt::entity villager, uint32_t reaction, VillagerStates state)
{
	if (!villager_reactions::IsReacting(villager))
	{
		villager::StorePreviousState(villager);
	}
	villager_reactions::SetTopState(villager, state);
	StateOf(villager).reaction = reaction;
	++Takers()[reaction];
}

/// truncate(float(truncate(m)) / 10 x 65536): the whole metres (truncated) as a whole distance (each step rounded to
/// float, as the original's single-precision FPU mode)
int32_t WholeDistanceOfWholeMetres(float metres)
{
	const auto whole = static_cast<int32_t>(metres);
	return static_cast<int32_t>(static_cast<float>(whole) / 10.0f * 65536.0f);
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, line);
	}
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

int32_t PointTurns(float roll20)
{
	// + 2, x (1000 / msPerTurn), then the one truncation; each step rounded to float (single-precision FPU mode)
	return static_cast<int32_t>((roll20 + 2.0f) * static_cast<float>(static_cast<int32_t>(TurnsPerSecond())));
}

int32_t MournTurns(float roll3)
{
	// + 4, x (1000 / msPerTurn), then the truncation: float steps (single-precision FPU mode)
	return static_cast<int32_t>((roll3 + 4.0f) * static_cast<float>(static_cast<int32_t>(TurnsPerSecond())));
}

// ---- the reaction ------------------------------------------------------------------------------------------------

uint8_t ReactToDeathPriority(entt::entity villager, uint32_t reaction)
{
	const auto* found = effects::reactions::Find(reaction);
	if (found == nullptr)
	{
		return 0;
	}
	const auto priority = static_cast<uint8_t>(DeathReactionInfo().priority & 0xFFu); // a byte
	// the initiator is a creature -> the priority
	if (fire::traits::IsCreature(found->initiator))
	{
		return priority;
	}
	// my town with a functional graveyard -> 0
	const auto* v = Entities().TryGet<const Villager>(villager);
	if (v != nullptr && v->town != entt::null && villager::HasFunctionalGraveyard(v->town))
	{
		return 0;
	}
	// I am the initiator -> 0; 10 or more takers (unsigned) -> 0
	if (villager == found->initiator)
	{
		return 0;
	}
	const auto takers = Takers().find(reaction);
	if (takers != Takers().end() && takers->second >= 10)
	{
		return 0;
	}
	return priority;
}

void SetupReactToDeath(entt::entity villager, entt::entity dead, uint32_t reaction)
{
	if (ActionOf(villager) == nullptr)
	{
		return;
	}
	// a creature initiator -> AddReaction(r, 7 LOOKING_AT_OBJECT_REACTION)
	if (dead != entt::null && fire::traits::IsCreature(dead))
	{
		AddReaction(villager, reaction, VillagerStates::LookingAtObjectReaction);
	}
	else
	{
		// GameRand(2): 0 -> the state counter = 0, AddReaction(r, 205); else AddReaction(r, 206)
		if (villager::GameRand(2) == 0)
		{
			ActionOf(villager)->turnsUntilStateChange = 0;
			AddReaction(villager, reaction, VillagerStates::PointAtDeadPerson);
		}
		else
		{
			AddReaction(villager, reaction, VillagerStates::GoTowardsDeadPerson);
		}
	}
	// keep the dead villager
	StateOf(villager).dead = dead;
	villager::TraceFormatted(villager, "mourn: {} of {}", static_cast<uint32_t>(villager::GetState(villager, Index::Top)),
	                         static_cast<uint32_t>(dead));
}

void ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	auto& registry = Entities();
	if (!registry.AllOf<Villager, LivingAction>(villager) || !villager::IsAvailableForReaction(villager))
	{
		return;
	}
	// it already follows a reaction. Replacing it by a higher scoring one (reactions::MaySwitch) is not ported for the
	// villagers (as the fire's, teleport's and shield's)
	if (villager_reactions::IsReacting(villager))
	{
		return;
	}
	const auto* me = registry.TryGet<const Transform>(villager);
	const auto* dead = registry.TryGet<const Transform>(reaction.initiator);
	if (me == nullptr || dead == nullptr)
	{
		return;
	}
	// the cell walk's distance: (|dz| + |dx|) / 2 from the initiator
	const auto& info = DeathReactionInfo();
	const float distance = 0.5f * (std::abs(me->position.x - dead->position.x) + std::abs(me->position.z - dead->position.z));
	if (distance > info.maxReactionDistance)
	{
		return;
	}
	// reactions::Score with GLivingInfo.isReacting[REACT_TO_DEATH] and ReactToDeathPriority
	const bool reacts = villager::InfoOf(villager).isReacting.isReactingToDeath != 0;
	const auto score = effects::reactions::Score(static_cast<uint8_t>(openblack::Reaction::ReactToDeath), reacts,
	                                             ReactToDeathPriority(villager, reaction.id), distance);
	if (score == 0 ||
	    !effects::reactions::Records(villager, static_cast<uint8_t>(openblack::Reaction::ReactToDeath),
	                                 info.numGameTurnsForNormalThingsBeforeReactingAgain, effects::reactions::Turn()))
	{
		return;
	}
	effects::reactions::MarkStarted(reaction.id, effects::reactions::Turn()); // the start turn, if still 0
	SetupReactToDeath(villager, reaction.initiator, reaction.id);
}

bool IsReacting(entt::entity villager)
{
	const auto* state = FindState(villager);
	return state != nullptr && state->reaction != 0;
}

entt::entity ReactionObject(entt::entity villager)
{
	const auto* state = FindState(villager);
	return state != nullptr ? state->dead : entt::entity(entt::null);
}

void StopReacting(entt::entity villager)
{
	const auto* state = FindState(villager);
	if (state == nullptr)
	{
		return;
	}
	// with a reaction: unlinked from its list with one taker fewer (the cap of ReactToDeathPriority counts the current
	// mourners), then its record gets the turn; the reaction and the dead villager go
	if (state->reaction != 0)
	{
		if (const auto takers = Takers().find(state->reaction); takers != Takers().end() && takers->second > 0)
		{
			--takers->second;
		}
		effects::reactions::RefreshRecord(villager, static_cast<uint8_t>(openblack::Reaction::ReactToDeath),
		                                  effects::reactions::Turn());
	}
	Entities().RemoveState<VillagerMourningState>(villager);
}

// ---- the states --------------------------------------------------------------------------------------------------

uint32_t PointAtDeadPerson(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto dead = ReactionObject(villager);
	// facing the dead villager (LookAtPos of its position, step 0x80) ->
	// (guard) the original tests only that there is one; ReactionValidate asks IsAvailable
	if (dead != entt::null && ecs::IsAvailable(dead) && villager::LookAtPos(villager, tq::PosOf(dead), 1) != 0)
	{
		// ++counter; GameFloatRand(20), drawn every facing turn
		++action.turnsUntilStateChange;
		const auto turns = PointTurns(villager::GameFloatRand(20.0f));
		// (i16) counter > t -> SetTopState(206)
		if (static_cast<int16_t>(action.turnsUntilStateChange) > turns)
		{
			villager_reactions::SetTopState(villager, VillagerStates::GoTowardsDeadPerson);
		}
	}
	return 1;
}

uint32_t GoTowardsDeadPerson(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto dead = ReactionObject(villager);
	// no dead villager -> 0
	// (guard) the original tests only that there is one; ReactionValidate asks IsAvailable
	if (dead == entt::null || !ecs::IsAvailable(dead))
	{
		return 0;
	}
	// R = the reaction's maxDistanceToRunAwayFromObject (4.0 for row 23)
	const float r = DeathReactionInfo().maxDistanceToRunAwayFromObject;
	// d = the distance in metres from me to the dead one
	const auto me = tq::PosOf(villager);
	const auto at = tq::PosOf(dead);
	const float d = gutils::GetDistanceInMetres(me, at);
	// 1.2 R < d and d - R > 1
	if (r * 1.2f < d && d - r > 1.0f)
	{
		// me + the step of the whole metres of d - R towards the dead one
		const uint16_t angle = tq::GetAngleFromXZ(me, at);
		const auto step = gutils::StepFromAngleCoarse(angle, WholeDistanceOfWholeMetres(d - r));
		// SetupMoveToWithHug(that point, 206)
		villager::SetupMoveToWithHug(villager, tq::ToMetres(me + step), VillagerStates::GoTowardsDeadPerson);
		return 1;
	}
	// SetTopState(207)
	villager_reactions::SetTopState(villager, VillagerStates::LookAtDeadPerson);
	return 1;
}

uint32_t LookAtDeadPerson(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto dead = ReactionObject(villager);
	// facing the dead villager -> the state counter = 0, SetTopState(208)
	// (guard) the original tests only that there is one; ReactionValidate asks IsAvailable
	if (dead != entt::null && ecs::IsAvailable(dead) && villager::LookAtPos(villager, tq::PosOf(dead), 1) == 1)
	{
		action.turnsUntilStateChange = 0;
		villager_reactions::SetTopState(villager, VillagerStates::MournDeadPerson);
	}
	return 1;
}

uint32_t MournDeadPerson(LivingAction& action)
{
	const auto villager = EntityOf(action);
	// ++counter; GameFloatRand(3), drawn every turn
	++action.turnsUntilStateChange;
	const auto turns = MournTurns(villager::GameFloatRand(3.0f));
	// (i16) counter > t -> StopReactingAndSetState
	if (static_cast<int16_t>(action.turnsUntilStateChange) > turns)
	{
		villager_reactions::StopReactingAndSetState(villager);
	}
	return 1;
}

uint32_t ExitReaction(LivingAction& action, VillagerStates next)
{
	// not a reactive state -> StopReacting, which knows this reaction
	return villager_reactions::ExitReaction(action, next);
}

bool ReactionValidate(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto dead = ReactionObject(villager);
	// no object, or not available -> PopFromPrevious; also REACT_TO_DEATH's whetherReactionFinishesIfInitiatorInHand
	// (1) and the object in the hand
	bool pop = dead == entt::null || !villager::IsAvailable(dead);
	if (!pop)
	{
		pop = DeathReactionInfo().whetherReactionFinishesIfInitiatorInHand != 0 && fire::traits::InHand(dead);
	}
	if (pop)
	{
		TraceIf(villager, "ReactionValidate: the dead villager went -> PopFromPrevious");
		villager_reactions::PopFromPrevious(villager);
	}
	return !pop;
}

// ---- orphans -----------------------------------------------------------------------------------------------------

void FindChildrenAndOrphanThem(entt::entity mother)
{
	auto& registry = Entities();
	// the mother's town; none -> nothing
	const auto* v = registry.TryGet<const Villager>(mother);
	// (guard) the original only reads the link: this stands for the missing town unlinking
	if (v == nullptr || v->town == entt::null || !ecs::IsAvailable(v->town))
	{
		return;
	}
	const auto town = v->town;
	// the structures, each one's inhabitants; the next is read after MakeChildOrphaned (which unlinks nothing)
	for (const auto abode : town_stats::AbodesOf(town))
	{
		const auto inhabitants = abode_villagers::VillagersOf(abode);
		for (const auto child : inhabitants)
		{
			MakeChildOrphaned(child, mother);
		}
	}
	// the homeless list
	const auto homeless = town_villagers::Homeless(town);
	for (const auto child : homeless)
	{
		MakeChildOrphaned(child, mother);
	}
}

uint32_t MakeChildOrphaned(entt::entity child, entt::entity mother)
{
	auto& registry = Entities();
	auto* v = ecs::IsAvailable(child) ? registry.TryGet<Villager>(child) : nullptr;
	// the mother is not her -> 0
	if (v == nullptr || v->mother != mother)
	{
		return 0;
	}
	// IsVillagerAvailable -> SetTopState(131 MORN_DEATH). No age test: grown-up children mourn too (literal)
	if (villager::IsVillagerAvailable(child))
	{
		villager::SetTopState(child, VillagerStates::MornDeath);
		villager::TraceFormatted(child, "orphan: -> 131 (mother {})", static_cast<uint32_t>(mother));
	}
	// mother = 0; 1
	if (auto* again = registry.TryGet<Villager>(child))
	{
		again->mother = entt::null;
	}
	return 1;
}

void Clear()
{
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Entities();
		std::vector<entt::entity> owners;
		registry.Each<const VillagerMourningState>(
		    [&owners](entt::entity villager, const VillagerMourningState&) { owners.push_back(villager); });
		for (const auto villager : owners)
		{
			registry.RemoveState<VillagerMourningState>(villager);
		}
	}
	Takers().clear();
}
} // namespace openblack::ecs::villager_mourning
