/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerReactions.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerReaction.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ReactionsSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerMourning.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerFire.h"
#include "VillagerResourceReactions.h"
#include "VillagerShield.h"
#include "VillagerTeleport.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
namespace reactions = effects::reactions;
using villager_reactions::SlotType;

/// The villager type table for the slot types: 7 and 12
const SlotType k_ReactToFood {
    .sameTypeSwitch = false,
    .setup = &villager_resource_reactions::SetupReactToFood,
    .priority = &villager_resource_reactions::ReactToFoodPriority,
    .turnsToReact = &villager_resource_reactions::StandardTurnsToReact,
    .turnsBeforeAgain = &villager_resource_reactions::StandardTurnsBeforeAgain,
};
const SlotType k_ReactToWood {
    .sameTypeSwitch = false,
    .setup = &villager_resource_reactions::SetupReactToWood,
    .priority = &villager_resource_reactions::ReactToWoodPriority,
    .turnsToReact = &villager_resource_reactions::StandardTurnsToReact,
    .turnsBeforeAgain = &villager_resource_reactions::TurnsBeforeReactingToWoodAgain,
};

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

uint8_t TypeByte(openblack::Reaction type)
{
	return static_cast<uint8_t>(static_cast<int>(type));
}

/// The reaction a villager follows, whatever keeps it: the slot, else the miracle maps in ReactionValidate's
/// order (fire, teleport, shield, death). (inferred) the fire's, the shield's and the mourning's reaction is the one of
/// that type their object started (their maps keep no public id; one reaction of the type per object); the
/// teleport's is villager_teleport::CurrentReaction
struct Current
{
	uint32_t id {0};
	openblack::Reaction type {openblack::Reaction::None};
	[[nodiscard]] bool Any() const { return type != openblack::Reaction::None; }
};

Current CurrentOf(entt::entity villager)
{
	if (const auto* slot = Entities().TryGet<const VillagerReactionSlot>(villager); slot != nullptr && slot->reaction != 0)
	{
		return {slot->reaction, slot->type};
	}
	if (villager_fire::IsReacting(villager))
	{
		return {
		    reactions::GetReactionOfTypeInitiatedBy(villager_fire::ReactionObject(villager), openblack::Reaction::ReactToFire),
		    openblack::Reaction::ReactToFire};
	}
	if (villager_teleport::IsReacting(villager))
	{
		return {villager_teleport::CurrentReaction(villager), openblack::Reaction::ReactToTeleport};
	}
	if (villager_shield::IsReacting(villager))
	{
		return {reactions::GetReactionOfTypeInitiatedBy(villager_shield::ReactionObject(villager),
		                                                openblack::Reaction::ReactToMagicShield),
		        openblack::Reaction::ReactToMagicShield};
	}
	if (villager_mourning::IsReacting(villager))
	{
		return {reactions::GetReactionOfTypeInitiatedBy(villager_mourning::ReactionObject(villager),
		                                                openblack::Reaction::ReactToDeath),
		        openblack::Reaction::ReactToDeath};
	}
	return {};
}

/// The miracle maps: one of them is held
bool IsReactingToMiracle(entt::entity villager)
{
	return villager_fire::IsReacting(villager) || villager_teleport::IsReacting(villager) ||
	       villager_shield::IsReacting(villager) || villager_mourning::IsReacting(villager);
}

/// The priority function of a type for a villager: the slot table's, the miracle functions' (the original's
/// arguments: ReactToFirePriority is the only one that reads `other`), 0 for the others
uint8_t PriorityOf(entt::entity villager, uint32_t reaction, openblack::Reaction type, uint32_t other)
{
	if (const auto* row = villager_reactions::SlotTypeOf(type); row != nullptr)
	{
		return row->priority(villager, reaction, other);
	}
	switch (type)
	{
	case openblack::Reaction::ReactToFire:
		return villager_fire::ReactToFirePriority(villager, reaction, other);
	case openblack::Reaction::ReactToTeleport:
		return villager_teleport::ReactToTeleportPriority(villager, reaction);
	case openblack::Reaction::ReactToMagicShield:
		return villager_shield::ReactToMagicShieldPriority(villager, reaction);
	case openblack::Reaction::ReactToDeath:
		return villager_mourning::ReactToDeathPriority(villager, reaction);
	default:
		return 0;
	}
}

/// The switch from the reaction a villager follows (`cur`) to `reaction` at distance d. (approximate) a current miracle
/// reaction no longer in the list scores 0 (the original's shut-down would have stopped the villager first)
bool SwitchAllowed(entt::entity villager, const Current& cur, const reactions::Reaction& reaction, float d)
{
	const auto* current = reactions::Find(cur.id);
	// the current one's distance to its cell; its score with the new one as `other`
	const float dc = current != nullptr ? reactions::DistanceToReactionCell(villager, *current) : 0.0f;
	const float curScore = current != nullptr
	                           ? static_cast<float>(villager_reactions::ScoreOf(villager, cur.id, cur.type, dc, reaction.id))
	                           : 0.0f;
	// the new one's
	const auto newScore = static_cast<float>(villager_reactions::ScoreOf(villager, reaction.id, reaction.type, d, 0));
	// the same type: only with the table's sameTypeSwitch, and never to the reaction it follows
	if (reaction.type == cur.type)
	{
		if (!reactions::SameTypeSwitch(TypeByte(reaction.type)) || cur.id == reaction.id)
		{
			return false;
		}
	}
	// (now - RecordTurn(current type)) / (1000 / ms per turn), both unsigned divisions. (openblack, guard) never a
	// division by 0
	const uint32_t perSecond = std::max<uint32_t>(1000u / std::max<uint32_t>(game_clock::MsPerTurn(), 1u), 1u);
	const uint32_t seconds = (villager::CurrentTurn() - reactions::RecordTurn(villager, TypeByte(cur.type))) / perSecond;
	// the seconds as a float
	return reactions::MaySwitch(curScore, newScore, static_cast<float>(seconds), TypeByte(cur.type));
}

/// The body that applies a reaction to one villager, for a slot type
void ApplySlotReaction(entt::entity villager, const reactions::Reaction& reaction, float d)
{
	auto& registry = Entities();
	const auto* row = villager_reactions::SlotTypeOf(reaction.type);
	const auto t = TypeByte(reaction.type);
	// a Living, available for the reaction. The initiator itself is skipped by SpreadReaction already
	if (row == nullptr || !registry.AllOf<Villager, LivingAction>(villager) || villager == reaction.initiator ||
	    !villager::IsAvailableForReaction(villager, reaction.type))
	{
		return;
	}
	// again = the table's turnsBeforeAgain (initiator, t, d)
	const auto again = row->turnsBeforeAgain(villager, reaction.initiator, t, d);
	// available for belief but not for the reaction: (pending)
	const auto now = villager::CurrentTurn();
	const auto cur = CurrentOf(villager);
	if (!cur.Any())
	{
		// Score(r)(me, d, 0) > 0 and Records(t, again)
		const auto score = villager_reactions::ScoreOf(villager, reaction.id, reaction.type, d, 0);
		if (score == 0 || !reactions::Records(villager, t, again, now))
		{
			if (villager::TraceOn(villager))
			{
				villager::Trace(villager,
				                fmt::format("react {}: {} score {} -> skip ({})", t, static_cast<uint32_t>(reaction.initiator),
				                            score, score == 0 ? "score" : "again"));
			}
			return;
		}
		// the reaction's start turn = now if not set; then starting to react is the table's setup
		reactions::MarkStarted(reaction.id, now);
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager,
			                fmt::format("react {}: {} score {} -> setup", t, static_cast<uint32_t>(reaction.initiator), score));
		}
		row->setup(villager, reaction.initiator, reaction.id);
		return;
	}
	if (!SwitchAllowed(villager, cur, reaction, d))
	{
		return;
	}
	// SetReactionDoneWhen(t); out of the current reaction's followers (AddReaction drops the slot or the miracle map);
	// then the setup
	reactions::SetReactionDoneWhen(villager, t, now);
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("react {}: {} switch from {} -> setup", t,
		                                      static_cast<uint32_t>(reaction.initiator), static_cast<int>(cur.type)));
	}
	row->setup(villager, reaction.initiator, reaction.id);
}

/// The dispatch of the miracle types, as before the slot
void DispatchMiracleReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	switch (reaction.type)
	{
	case openblack::Reaction::ReactToFire:
		villager_fire::ApplyReaction(villager, reaction);
		break;
	case openblack::Reaction::ReactToTeleport:
		villager_teleport::ApplyReaction(villager, reaction);
		break;
	case openblack::Reaction::ReactToMagicShield:
		villager_shield::ApplyReaction(villager, reaction);
		break;
	case openblack::Reaction::ReactToDeath:
		// ECS/Villager/VillagerMourning.h
		villager_mourning::ApplyReaction(villager, reaction);
		break;
	default:
		break;
	}
}

void VillagerReaction(entt::entity villager, const effects::reactions::Reaction& reaction, float distance)
{
	// a slot type: the original's per-villager body
	if (villager_reactions::SlotTypeOf(reaction.type) != nullptr)
	{
		ApplySlotReaction(villager, reaction, distance);
		return;
	}
	// the only path before the slot: no slot reaction held -> the miracle dispatch, unchanged
	if (!villager_reactions::SlotHeld(villager))
	{
		DispatchMiracleReaction(villager, reaction);
		return;
	}
	// a miracle type reaches a villager that follows a slot reaction: first IsAvailableForReaction(type) (whole: the
	// life test lets only type 7 through), as for every type in the original, then the switch rule with the slot as
	// the current one (the miracle types are not in the villager type table yet: their score and setup are theirs as
	// today)
	if (!villager::IsAvailableForReaction(villager, reaction.type))
	{
		return;
	}
	const auto cur = CurrentOf(villager);
	if (!SwitchAllowed(villager, cur, reaction, distance))
	{
		return;
	}
	// the slot is unlinked (no record refresh, as the original's switch) and the miracle ApplyReaction runs with its own
	// gates and setup; it took the villager -> SetReactionDoneWhen (after their Records: idempotent), else the slot is
	// put back
	auto& registry = Entities();
	const auto saved = registry.Get<const VillagerReactionSlot>(villager);
	registry.Remove<VillagerReactionSlot>(villager);
	DispatchMiracleReaction(villager, reaction);
	if (IsReactingToMiracle(villager))
	{
		reactions::SetReactionDoneWhen(villager, TypeByte(reaction.type), villager::CurrentTurn());
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, fmt::format("react {}: switch from slot {} -> {}", static_cast<int>(saved.type),
			                                      saved.reaction, static_cast<int>(reaction.type)));
		}
	}
	else if (registry.Valid(villager))
	{
		registry.AssignOrReplace<VillagerReactionSlot>(villager, saved);
	}
}

/// The reaction's shut-down for the villagers: while the reaction has followers, the head one's (the newest)
/// StopReactingAndSetState. Only the slot holders keep the follower link (the miracle maps: unchanged)
void ShutDownSlotReaction(uint32_t reaction)
{
	auto& registry = Entities();
	std::vector<std::pair<uint32_t, entt::entity>> followers;
	registry.Each<const VillagerReactionSlot>([&followers, reaction](entt::entity villager, const VillagerReactionSlot& slot) {
		if (slot.reaction == reaction)
		{
			followers.emplace_back(slot.joinOrder, villager);
		}
	});
	std::sort(followers.begin(), followers.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	for (const auto& [order, villager] : followers)
	{
		// one StopReactingAndSetState may move another follower off it: only the ones still following
		const auto* slot = registry.Valid(villager) ? registry.TryGet<const VillagerReactionSlot>(villager) : nullptr;
		if (slot == nullptr || slot->reaction != reaction)
		{
			continue;
		}
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager,
			                fmt::format("react {}: shut down {} (follower {})", static_cast<int>(slot->type), reaction, order));
		}
		villager_reactions::StopReactingAndSetState(villager);
		// (openblack, guard) the original loops until the list is empty: StopReacting always unlinks
		if (registry.Valid(villager))
		{
			if (const auto* still = registry.TryGet<const VillagerReactionSlot>(villager);
			    still != nullptr && still->reaction == reaction)
			{
				registry.Remove<VillagerReactionSlot>(villager);
			}
		}
	}
}
} // namespace

void villager_reactions::RegisterHandlers()
{
	effects::reactions::SetLivingReactionHandler(effects::reactions::LivingClass::Villager, &VillagerReaction);
	effects::reactions::SetLivingShutDownHandler(effects::reactions::LivingClass::Villager, &ShutDownSlotReaction);
}

void villager_reactions::Register()
{
	RegisterHandlers();
	Locator::reactionsSystem::value().ResetJoinOrder();
}

void villager_reactions::HandleReaction(entt::entity villager, const effects::reactions::Reaction& reaction, float distance)
{
	VillagerReaction(villager, reaction, distance);
}

const villager_reactions::SlotType* villager_reactions::SlotTypeOf(openblack::Reaction type)
{
	switch (type)
	{
	case openblack::Reaction::ReactToFood:
		return &k_ReactToFood;
	case openblack::Reaction::ReactToWood:
		return &k_ReactToWood;
	default:
		return nullptr;
	}
}

bool villager_reactions::SlotHeld(entt::entity villager)
{
	return SlotReaction(villager) != 0;
}

uint32_t villager_reactions::SlotReaction(entt::entity villager)
{
	const auto* slot = Entities().TryGet<const VillagerReactionSlot>(villager);
	return slot != nullptr ? slot->reaction : 0;
}

entt::entity villager_reactions::SlotObject(entt::entity villager)
{
	const auto* slot = Entities().TryGet<const VillagerReactionSlot>(villager);
	return slot != nullptr ? slot->object : entt::entity(entt::null);
}

void villager_reactions::SetSlotObject(entt::entity villager, entt::entity object)
{
	if (auto* slot = Entities().TryGet<VillagerReactionSlot>(villager); slot != nullptr)
	{
		slot->object = object;
	}
}

uint32_t villager_reactions::ScoreOf(entt::entity villager, uint32_t reaction, openblack::Reaction type, float distance,
                                     uint32_t other)
{
	const auto t = TypeByte(type);
	const auto& infos = Locator::infoConstants::value();
	if (t >= infos.reaction.size())
	{
		return 0;
	}
	// isReacting[t] of the villager's info and d <= maxReactionDistance: only then the priority function
	const auto& info = villager::InfoOf(villager);
	const bool reacts = reinterpret_cast<const uint32_t*>(&info.isReacting)[t] != 0;
	if (!reacts || distance > infos.reaction.at(t).maxReactionDistance)
	{
		return 0;
	}
	return reactions::Score(t, true, PriorityOf(villager, reaction, type, other), distance);
}

void villager_reactions::AddReaction(entt::entity villager, uint32_t reaction, VillagerStates state)
{
	auto& registry = Entities();
	const auto* found = reactions::Find(reaction);
	const auto type = found != nullptr ? found->type : openblack::Reaction::None;
	// how impressed it is by r. TODO(reactions): the town belief it feeds is not ported
	// no reaction yet -> StorePreviousState
	if (!IsReacting(villager))
	{
		villager::StorePreviousState(villager);
	}
	// SetTopState(state): the exits run while the old reaction is still held
	SetTopState(villager, state);
	// dancing -> out of the dance. TODO(dance): no villager dances in openblack
	if (!registry.Valid(villager))
	{
		return;
	}
	// a switch from a miracle map: their StopReacting refreshes its record, the original's switch only unlinks: the
	// record's turn is put back
	if (IsReactingToMiracle(villager))
	{
		const auto old = CurrentOf(villager);
		const auto turn = reactions::RecordTurn(villager, TypeByte(old.type));
		villager_fire::StopReacting(villager);
		villager_teleport::StopReacting(villager);
		villager_shield::StopReacting(villager);
		villager_mourning::StopReacting(villager);
		reactions::RefreshRecord(villager, TypeByte(old.type), turn);
	}
	// the reaction is r; at the head of r's followers, one more follower
	auto& slot = registry.AssignOrReplace<VillagerReactionSlot>(villager);
	slot.reaction = reaction;
	slot.type = type;
	slot.object = entt::null;
	slot.joinOrder = Locator::reactionsSystem::value().NextJoinOrder();
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager,
		                fmt::format("AddReaction {} type {} -> {}", reaction, static_cast<int>(type), static_cast<int>(state)));
	}
}

void villager_reactions::ProcessSlotReaction(entt::entity villager)
{
	auto& registry = Entities();
	const auto* slot = registry.TryGet<const VillagerReactionSlot>(villager);
	// no reaction -> nothing
	if (slot == nullptr || slot->reaction == 0)
	{
		return;
	}
	// the reaction not available -> StopReacting. (inferred) the reaction's own availability is
	// effects::reactions::IsAvailable (in the list, not shut down, initiator alive).
	// (approximate) the shut-down runs at BeginTurn's Prune, not when the initiator is deleted: an initiator destroyed
	// mid-turn reaches this test (StopReacting, no state reset) before its shut-down
	if (!reactions::IsAvailable(reactions::Find(slot->reaction)))
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, fmt::format("ProcessReaction: reaction {} gone -> StopReacting", slot->reaction));
		}
		StopReacting(villager);
		return;
	}
	// the object none or not available -> StopReactingAndSetState
	const auto object = slot->object;
	if (object == entt::null || !fire::traits::IsAvailable(object))
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, "ProcessReaction: object gone -> StopReactingAndSetState");
		}
		StopReactingAndSetState(villager);
		return;
	}
	// elapsed = now - the record's turn of the type; the distance in metres from the villager to the object
	const auto t = TypeByte(slot->type);
	const auto* row = SlotTypeOf(slot->type);
	if (row == nullptr)
	{
		return;
	}
	const auto elapsed = static_cast<int32_t>(villager::CurrentTurn() - reactions::RecordTurn(villager, t));
	const float d = town_queries::GetDistanceInMetres(town_queries::PosOf(villager), town_queries::PosOf(object));
	// the table's turnsToReact (object, type, d); a signed elapsed > turns -> stop
	const auto turns = static_cast<int32_t>(row->turnsToReact(villager, object, t, d));
	if (elapsed > turns)
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, fmt::format("ProcessReaction: {} turns > {} -> StopReactingAndSetState", elapsed, turns));
		}
		StopReactingAndSetState(villager);
	}
}

uint32_t villager_reactions::SetTopState(entt::entity villager, VillagerStates state)
{
	const auto result = villager::SetTopState(villager, state);
	if (result != villager::k_ExitRefused)
	{
		Locator::entitiesRegistry::value()
		    .Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
		            MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	}
	return result;
}

void villager_reactions::PopFromPrevious(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.TryGet<LivingAction>(villager);
	if (action == nullptr)
	{
		return;
	}
	// the stored state's resume state (field0x20). Nothing stored is row 0, whose resume is 0 in info.dat:
	// SetTopState(0) = INVALID_STATE (which then returns 0 every turn), as the original
	const auto stored = static_cast<VillagerStates>(action->states.at(static_cast<size_t>(LivingAction::Index::Previous)));
	const auto next = villager::state_info::ResumeState(villager::state_info::StateInfo(stored));
	// SetTopState; 0x2E (an exit refused) -> a raw store of 163 in index 0 (which also resets the turns in the state)
	const auto result = SetTopState(villager, next);
	action = registry.TryGet<LivingAction>(villager);
	if (action == nullptr)
	{
		return;
	}
	if (result == villager::k_ExitRefused)
	{
		action->states.at(static_cast<size_t>(LivingAction::Index::Top)) = static_cast<uint8_t>(VillagerStates::DecideWhatToDo);
		action->turnsSinceStateChange = 0;
	}
	// a raw store of 0 in index 2
	action->states.at(static_cast<size_t>(LivingAction::Index::Previous)) = 0;
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("PopFromPrevious stored {} -> resume {} = {:#x}", static_cast<int>(stored),
		                                      static_cast<int>(next), result));
	}
}

void villager_reactions::ResetStateAfterReacting(entt::entity villager)
{
	PopFromPrevious(villager);
	// the final state reactive (field0xb8) -> SetTopState(163)
	if (villager::state_info::IsReactive(villager::state_info::StateInfo(villager::GetFinalState(villager))))
	{
		SetTopState(villager, VillagerStates::DecideWhatToDo);
	}
}

void villager_reactions::StopReactingAndSetState(entt::entity villager)
{
	// ResetStateAfterReacting; still reacting -> StopReacting
	ResetStateAfterReacting(villager);
	if (IsReacting(villager))
	{
		StopReacting(villager);
	}
}

bool villager_reactions::ReactionValidate(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	// the object and the type of the reaction it follows, kept by VillagerFire.cpp or VillagerTeleport.cpp
	auto object = entt::entity(entt::null);
	auto type = openblack::Reaction::None;
	if (villager_fire::ReactionObject(villager) != entt::null)
	{
		object = villager_fire::ReactionObject(villager);
		type = openblack::Reaction::ReactToFire;
	}
	else if (villager_teleport::ReactionObject(villager) != entt::null)
	{
		object = villager_teleport::ReactionObject(villager);
		type = openblack::Reaction::ReactToTeleport;
	}
	else if (villager_shield::ReactionObject(villager) != entt::null)
	{
		object = villager_shield::ReactionObject(villager);
		type = openblack::Reaction::ReactToMagicShield;
	}
	else if (SlotHeld(villager))
	{
		// the slot types (7 / 12), the last branch: with no slot the object stays none, as before
		object = SlotObject(villager);
		type = registry.Get<const VillagerReactionSlot>(villager).type;
	}
	// no object, or not available -> PopFromPrevious. The shield's object is
	// the spell itself, which is no fire object: its own availability test (and nothing holds a spell, so the in-hand
	// branch below cannot fire for it)
	if (type == openblack::Reaction::ReactToMagicShield)
	{
		const bool gone = !villager_shield::IsReactionObjectAvailable(villager);
		if (gone)
		{
			if (villager::TraceOn(villager))
			{
				villager::Trace(villager, "ReactionValidate: the shield spell went -> PopFromPrevious");
			}
			PopFromPrevious(villager);
		}
		return !gone;
	}
	bool pop = object == entt::null || !fire::traits::IsAvailable(object);
	// ReactionInfo[type].whetherReactionFinishesIfInitiatorInHand and the object in the hand
	if (!pop)
	{
		const auto& info = Locator::infoConstants::value().reaction.at(static_cast<size_t>(type));
		pop = info.whetherReactionFinishesIfInitiatorInHand != 0 && fire::traits::InHand(object);
	}
	if (pop)
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, "ReactionValidate: the reaction's object went -> PopFromPrevious");
		}
		PopFromPrevious(villager);
	}
	return !pop;
}

bool villager_reactions::IsReacting(entt::entity villager)
{
	// the slot types' (components::VillagerReactionSlot; never held together with a miracle map)
	if (SlotHeld(villager))
	{
		return true;
	}
	// one reaction at a time: the fire's, the teleport's, the shield's or the mourning's (VillagerMourning.h)
	return villager_fire::IsReacting(villager) || villager_teleport::IsReacting(villager) ||
	       villager_shield::IsReacting(villager) || villager_mourning::IsReacting(villager);
}

void villager_reactions::StopReacting(entt::entity villager)
{
	// TOP 203 DANCE_WHILE_REACTING and dancing -> out of the dance. TODO(dance): 203 has no state function in
	// openblack, so no villager is there. Then the Living's StopReacting for the reaction it follows: the fire's, the
	// teleport's, the shield's or the mourning's (which also takes one off the reaction's follower count)
	if (villager::TraceOn(villager) && IsReacting(villager))
	{
		villager::Trace(villager, "StopReacting");
	}
	villager_fire::StopReacting(villager);
	villager_teleport::StopReacting(villager);
	villager_shield::StopReacting(villager);
	villager_mourning::StopReacting(villager);
	// the slot: out of the reaction's followers, its record of the type = now, the reaction cleared; the object is
	// cleared always
	auto& registry = Entities();
	if (const auto* slot = registry.TryGet<const VillagerReactionSlot>(villager); slot != nullptr)
	{
		if (slot->reaction != 0)
		{
			reactions::RefreshRecord(villager, TypeByte(slot->type), villager::CurrentTurn());
		}
		registry.Remove<VillagerReactionSlot>(villager);
	}
}

uint32_t villager_reactions::ExitReaction(LivingAction& action, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	// the circle hug reset: no hugged object, turns to the object 0xFF. (approximate) openblack's
	// WallHugObjectReference
	registry.Remove<WallHugObjectReference>(villager);
	// whether next is a reactive state (field0xb8); not -> StopReacting
	const bool reactive = villager::state_info::IsReactive(villager::state_info::StateInfo(next));
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("ExitReaction({}) reactive {}", static_cast<int>(next), reactive ? 1 : 0));
	}
	if (!reactive)
	{
		StopReacting(villager);
	}
	return 1;
}
