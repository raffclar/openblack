/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerTeleport.h"

#include <cmath>

#include <algorithm>
#include <vector>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerTeleportState.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Objects/MagicTeleport.h"
#include "VillagerMove.h"
#include "VillagerReactions.h"
#include "Worship/TownMagic.h"
#include "Worship/WorshipPercentage.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
auto& Reg()
{
	return Locator::entitiesRegistry::value();
}

/// The villager's teleport state if it has one (any id, alive or not)
VillagerTeleportState* FindState(entt::entity villager)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Reg();
	return registry.Valid(villager) ? registry.TryGet<VillagerTeleportState>(villager) : nullptr;
}

/// The villager's teleport state, added with its defaults if it has none. The villager must exist.
VillagerTeleportState& StateOf(entt::entity villager)
{
	if (auto* state = Reg().TryGet<VillagerTeleportState>(villager))
	{
		return *state;
	}
	return Reg().AssignState<VillagerTeleportState>(villager);
}

LivingAction* ActionOf(entt::entity villager)
{
	return Reg().TryGet<LivingAction>(villager);
}

VillagerStates Get(const LivingAction& action, LivingAction::Index index)
{
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
}

const GVillagerStateTableInfo* TableOf(VillagerStates state)
{
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	const auto i = static_cast<size_t>(state);
	return i < table.size() ? &table[i] : nullptr;
}

const ReactionInfo& TeleportReactionInfo()
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(openblack::Reaction::ReactToTeleport));
}

glm::vec3 PositionOf(entt::entity object)
{
	const auto* transform = Reg().TryGet<const Transform>(object);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

/// The wall hug's arrival test with no extra: d^2 < (the wall hug's step)^2, strictly; the step is openblack's
/// WallHug::speed
bool AreWeThere(entt::entity villager, const glm::vec3& goal)
{
	const auto* wallHug = Reg().TryGet<const WallHug>(villager);
	const float step = wallHug != nullptr ? wallHug->speed : 0.0f;
	const auto at = PositionOf(villager);
	const glm::vec2 d(at.x - goal.x, at.z - goal.z);
	return glm::dot(d, d) < step * step;
}

/// The wall hug's speed as a whole MapCoords distance a turn. openblack keeps it in metres a turn (WallHug::speed =
/// the u16 / 6553.6, ECS/VillagerSpeed.cpp), so the u16 comes back by the same factor, rounded: a plain truncation of
/// u16 / 6553.6 x 6553.6 in float can give u16 - 1 and turn a word one above the threshold into "not faster"
int32_t SpeedUnits(entt::entity villager)
{
	const auto* wallHug = Reg().TryGet<const WallHug>(villager);
	return wallHug != nullptr ? static_cast<int32_t>(std::lround(wallHug->speed * ecs::MapInterface::k_PositionToGridFactor))
	                          : 0;
}

/// The info's speed threshold, in the same units: openblack's speedGroup.speed2 (the third dword of the group, as the
/// flee-from-predator priority reads it for the same comparison)
int32_t SpeedThresholdUnits(entt::entity villager)
{
	const auto* info = VillagerInfoOf(villager);
	return info != nullptr ? static_cast<int32_t>(info->speedGroup.speed2) : 0;
}

void RemoveMoveTags(entt::entity villager)
{
	Reg()
	    .Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag, MoveStateFinalStepTag,
	            MoveStateArrivedTag>(villager);
}

/// The final state: the top state if it is a final one, else the destination state
VillagerStates FinalState(const LivingAction& action)
{
	const auto top = Get(action, LivingAction::Index::Top);
	const auto* info = TableOf(top);
	return info != nullptr && info->isFinalState != 0 ? top : Get(action, LivingAction::Index::Final);
}

/// The villager core's SetTopState (ECS/Villager/VillagerCore.h), which runs the exit
/// functions of TOP and of the final state and the entry of the new state from the rows of k_VillagerStateTable (the
/// fire's, the teleport's and the worship ones among them), once each, with the pause roll (only into a state whose
/// row may pause: not 163, 201, 202) and the codes 1 / 0x2E / 0x2F; the walk's end as villager_reactions::SetTopState
uint32_t SetTopState(entt::entity villager, VillagerStates state)
{
	return villager_reactions::SetTopState(villager, state);
}

/// Stores the previous state (when it had no reaction): the final state goes to index 2, unless it is a passing one
void StorePreviousState(LivingAction& action)
{
	const auto final = FinalState(action);
	const auto* info = TableOf(final);
	auto stored = final;
	if (info != nullptr && (info->field0x10 != 0 || info->field0xb8 != 0))
	{
		stored = Get(action, LivingAction::Index::Previous);
	}
	action.states.at(static_cast<size_t>(LivingAction::Index::Previous)) = static_cast<uint8_t>(stored);
}

/// The villagers' shared walk with a hug (VillagerMove.cpp: TOP, then FINAL)
void SetupMoveToWithHug(entt::entity villager, const glm::vec2& goal, VillagerStates final)
{
	villager::SetupMoveToWithHug(villager, goal, final);
}

/// On the villager's reaction records (ECS/Effects/Reactions): may it react to
/// this type again (more than `again` turns since the last time)? Remembers it.
bool MayReactAgain(entt::entity villager, openblack::Reaction type, uint32_t again)
{
	return effects::reactions::Records(villager, static_cast<uint8_t>(type), again, effects::reactions::Turn());
}

/// The REACT_TO_TELEPORT part of applying a reaction to the living things of a cell, for one villager of the cell (as
/// the fire's: with no reaction of its own, a score above 0 and not reacted to a teleport lately)
void ApplyTeleportReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	auto& registry = Reg();
	// the common availability check for every reaction type: villager::IsAvailableForReaction
	if (!registry.AllOf<Villager, LivingAction>(villager) || villager == reaction.initiator ||
	    !villager::IsAvailableForReaction(villager))
	{
		return;
	}
	if (effects::reactions::Find(StateOf(villager).reaction) != nullptr)
	{
		return;
	}
	const auto& info = TeleportReactionInfo();
	const auto at = PositionOf(villager);
	const auto from = PositionOf(reaction.initiator);
	const float distance = 0.5f * (std::abs(at.x - from.x) + std::abs(at.z - from.z));
	if (distance > info.maxReactionDistance)
	{
		return;
	}
	// the reaction score (ECS/Effects/Reactions: Score)
	const auto priority = villager_teleport::ReactToTeleportPriority(villager, reaction.id);
	const auto score =
	    effects::reactions::Score(static_cast<uint8_t>(openblack::Reaction::ReactToTeleport), true, priority, distance);
	if (score == 0 ||
	    !MayReactAgain(villager, openblack::Reaction::ReactToTeleport, info.numGameTurnsForNormalThingsBeforeReactingAgain))
	{
		return;
	}
	villager_teleport::SetupReactToTeleport(villager, reaction.initiator, reaction.id);
}

} // namespace

bool villager_teleport::IsReacting(entt::entity villager)
{
	const auto* state = FindState(villager);
	return state != nullptr && state->reaction != 0;
}

entt::entity villager_teleport::ReactionObject(entt::entity villager)
{
	const auto* state = FindState(villager);
	return state != nullptr ? state->stone : entt::entity(entt::null);
}

void villager_teleport::StopReacting(entt::entity villager)
{
	const auto* state = FindState(villager);
	if (state == nullptr)
	{
		return;
	}
	// with a reaction its record gets the turn and the reaction is cleared; the stone is cleared on both paths: nothing
	// of the teleport stays
	if (state->reaction != 0)
	{
		effects::reactions::RefreshRecord(villager, static_cast<uint8_t>(openblack::Reaction::ReactToTeleport),
		                                  effects::reactions::Turn());
	}
	Reg().RemoveState<VillagerTeleportState>(villager);
}

uint32_t villager_teleport::ExitReactToTeleport(LivingAction& action, VillagerStates next)
{
	auto& registry = Reg();
	const auto villager = registry.ToEntity(action);
	const bool same = villager::IsStateExitFunctionSameAs(villager, next);
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("ExitReactToTeleport({}) same {}", static_cast<int>(next), same ? 1 : 0));
	}
	if (!same)
	{
		auto* worshipper = registry.TryGet<WorshipVillager>(villager);
		// off the town's list of villagers on the way to the worship site
		// (guard) the original only reads the town link: stands for the missing town unlinking
		if (const auto* v = registry.TryGet<const Villager>(villager); v != nullptr && ecs::IsAvailable(v->town))
		{
			worship::percentage::RemoveVillagerOnWay(v->town, villager);
			if (worshipper != nullptr)
			{
				worshipper->onWayInTown = false;
			}
		}
		// the on-way flag cleared, with or without a town
		if (worshipper != nullptr)
		{
			worshipper->onWay = false;
		}
	}
	// ExitReaction, its result (1)
	return villager_reactions::ExitReaction(action, next);
}

bool villager_teleport::IsMoving(entt::entity living)
{
	auto& registry = Reg();
	if (!registry.AllOf<WallHug>(living))
	{
		return false;
	}
	// the thing's position is not the one of the turn before, so it moved during the last turn.
	// (approximate) openblack keeps no previous-turn position: a Living with a move state that is not ARRIVED and a step
	// to take stands for it. That covers any walking state, not only MOVE_TO_POS (the walk to the worship site, a
	// reaction's walk...), which is what the original's test does.
	if (registry.AllOf<MoveStateArrivedTag>(living))
	{
		return false;
	}
	if (!registry.AnyOf<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                    MoveStateFinalStepTag>(living))
	{
		return false;
	}
	return registry.Get<const WallHug>(living).speed > 0.0f;
}

glm::vec3 villager_teleport::FinalDestination(entt::entity living)
{
	// with a footpath and a node on it, the last non-hidden node of the path (in the walk's direction), else the wall
	// hug's goal, whether it is moving or not. (pending) the footpath branch: no openblack Living walks on a
	// components::Footpath
	auto& registry = Reg();
	if (const auto* wallHug = registry.TryGet<const WallHug>(living); wallHug != nullptr)
	{
		return {wallHug->goal.x, 0.0f, wallHug->goal.y};
	}
	const auto at = PositionOf(living);
	return {at.x, 0.0f, at.z};
}

uint32_t villager_teleport::CurrentReaction(entt::entity living)
{
	const auto* state = FindState(living);
	return state != nullptr ? state->reaction : 0;
}

std::optional<PlayerNames> villager_teleport::PlayerOf(entt::entity villager)
{
	const auto* component = Reg().TryGet<const Villager>(villager);
	if (component == nullptr)
	{
		return std::nullopt;
	}
	return worship::town::OwnerOf(component->town);
}

uint8_t villager_teleport::ReactToTeleportPriority(entt::entity villager, uint32_t reaction)
{
	const auto* found = effects::reactions::Find(reaction);
	if (found == nullptr || !ecs::IsAvailable(found->initiator) || !Reg().AllOf<MagicTeleport>(found->initiator))
	{
		return 0; // not a MagicTeleport
	}
	const bool react = magic::teleport::ShouldLivingThingReact(found->initiator, villager);
	return static_cast<uint8_t>((react ? 0xFFu : 0u) & (TeleportReactionInfo().priority & 0xFFu));
}

void villager_teleport::SetupReactToTeleport(entt::entity villager, entt::entity stone, uint32_t reaction)
{
	auto* action = ActionOf(villager);
	if (action == nullptr || !Reg().AllOf<MagicTeleport>(stone))
	{
		return;
	}
	// the stone keeps where it is going, and the stone is the reaction's object
	magic::teleport::RegisterDestination(stone, villager, FinalDestination(villager));
	auto& state = StateOf(villager);
	state.stone = stone;
	state.reaction = reaction;
	// AddReaction(reaction, 201 + 50 x (speed > threshold)). The number compared is the villager's own speed (the u16
	// wall hug speed) against its info's speed2 (the same field the animal flee reads): on the zero-extended word, so
	// 251 GO_TOWARDS_TELEPORT_REACTION_QUICKLY only when it is strictly faster, else 201. Both
	// rows run the same state function (251 jumps to 201's): only the state table row changes (animation, speed index)
	const bool quick = SpeedUnits(villager) > SpeedThresholdUnits(villager);
	StorePreviousState(*action);
	SetTopState(villager, quick ? VillagerStates::GoTowardsTeleportReactionQuickly : VillagerStates::GoTowardsTeleportReaction);
	if (magic::teleport::TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Teleport: villager {} reacts to stone {} (reaction {}), state {}",
		                   static_cast<uint32_t>(villager), static_cast<uint32_t>(stone), reaction, quick ? 251 : 201);
	}
}

uint32_t villager_teleport::GoToTeleportReaction(LivingAction& action)
{
	auto& registry = Reg();
	const auto villager = registry.ToEntity(action);
	const auto* state = FindState(villager);
	// this state checks nothing: the validate slot of 201/202/251, villager_reactions::ReactionValidate, pops the state
	// once the stone goes, before the state runs. The reaction's object position is then read, which the original reads
	// with no null test; here, with
	// no stone kept, the state only returns 0 (a guard against reading nothing; ReactionValidate has popped by then)
	if (state == nullptr || !ecs::IsAvailable(state->stone))
	{
		return 0;
	}
	const auto stone = PositionOf(state->stone);
	// at the reaction's object position -> TELEPORT_REACTION
	if (AreWeThere(villager, stone))
	{
		SetTopState(villager, VillagerStates::TeleportReaction);
		return 1;
	}
	// SetupMoveToWithHug(stone, GetFinalState()): back to this state (201 or 251) once there
	const auto final = FinalState(action);
	SetupMoveToWithHug(villager, glm::vec2(stone.x, stone.z),
	                   final == VillagerStates::GoTowardsTeleportReactionQuickly ? final
	                                                                             : VillagerStates::GoTowardsTeleportReaction);
	return 1;
}

uint32_t villager_teleport::TeleportReaction(LivingAction& action)
{
	auto& registry = Reg();
	const auto villager = registry.ToEntity(action);
	// no reaction, or its initiator not a MagicTeleport: nothing (the state stays, as in the
	// original)
	const auto* state = FindState(villager);
	if (state == nullptr)
	{
		return 0;
	}
	const auto stone = state->stone;
	if (!ecs::IsAvailable(stone) || !registry.AllOf<MagicTeleport>(stone))
	{
		return 0;
	}
	magic::teleport::DoTeleport(stone, villager, false);
	// StopReactingAndSetState: the state reset after reacting (popped back, then 163 if the final state is a reactive
	// one), then StopReacting if still reacting
	villager_reactions::StopReactingAndSetState(villager);
	return 1;
}

void villager_teleport::ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	if (magic::teleport::TraceEnabled())
	{
		const auto& info = TeleportReactionInfo();
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Teleport: REACT_TO_TELEPORT {} of stone {} to villager {}: priority {} radius {:.1f} distance "
		                   "weight {:.2f} again {}",
		                   reaction.id, static_cast<uint32_t>(reaction.initiator), static_cast<uint32_t>(villager),
		                   info.priority, reaction.radius, info.howImportantIsDistance,
		                   info.numGameTurnsForNormalThingsBeforeReactingAgain);
	}
	ApplyTeleportReaction(villager, reaction);
}

void villager_teleport::LandAt(entt::entity villager, const glm::vec3& mapPosition)
{
	if (!Reg().AllOf<Transform>(villager))
	{
		return;
	}
	// in the original's order: SetTopState(FLYING 10), the interface puts the villager down at the stone (the hand took
	// it out first, HandApplyToObject.cpp), SetTopState(LANDED 11) and DecideWhatToDo. (approximate) LANDED's own state
	// function (the landing animation) only runs
	// until DecideWhatToDo replaces it in the same turn, as in the original
	SetTopState(villager, VillagerStates::Flying);
	// the Transform is taken again: a state change may have moved the registry's storage
	if (auto* transform = Reg().TryGet<Transform>(villager); transform != nullptr)
	{
		const auto world = magic::ToWorld(glm::vec3(mapPosition.x, 0.0f, mapPosition.z));
		// the position, then into the map (the hand took it out of the map, HandSystem::PickUp). (openblack) one that is
		// still in the map moves there, so its lists stay right
		if (map_cells::IsObjectInMap(villager))
		{
			map_cells::MoveMapObject(villager, world);
		}
		else
		{
			transform->position = world;
			map_cells::InsertMapObject(villager);
		}
	}
	SetTopState(villager, VillagerStates::Landed);
	DecideWhatToDo(villager);
	Reg().SetDirty();
}

void villager_teleport::DecideWhatToDo(entt::entity villager)
{
	SetTopState(villager, VillagerStates::DecideWhatToDo);
}

void villager_teleport::OnMoved(entt::entity living)
{
	// MoveMapObject leaves the walk as it was: the step is made again from the new position
	auto& registry = Reg();
	if (auto* wallHug = registry.TryGet<WallHug>(living); wallHug != nullptr && IsMoving(living))
	{
		wallHug->step = glm::vec2(0.0f);
		RemoveMoveTags(living);
		registry.Remove<WallHugObjectReference>(living);
		registry.Assign<MoveStateLinearTag>(living);
	}
}

void villager_teleport::Clear()
{
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Reg();
		std::vector<entt::entity> owners;
		registry.Each<const VillagerTeleportState>(
		    [&owners](entt::entity villager, const VillagerTeleportState&) { owners.push_back(villager); });
		for (const auto villager : owners)
		{
			registry.RemoveState<VillagerTeleportState>(villager);
		}
	}
	villager_reactions::Register(); // the Villager handler of ECS/Effects/Reactions
}
