/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerFire.h"

#include <cmath>

#include <algorithm>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <spdlog/spdlog.h>

#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerFireState.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Life.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerAnimations.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "VillagerMove.h"
#include "VillagerReactions.h"
#include "VillagerWorship.h"
#include "Worship/WorshipPercentage.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// The villager's fire state, added with its defaults the first time it is needed. The villager must exist.
VillagerFireState& StateOf(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* state = registry.TryGet<VillagerFireState>(villager))
	{
		return *state;
	}
	return registry.AssignState<VillagerFireState>(villager);
}

/// The villager's fire state if it has one (any id, alive or not)
VillagerFireState* FindState(entt::entity villager)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(villager) ? registry.TryGet<VillagerFireState>(villager) : nullptr;
}

LivingAction* ActionOf(entt::entity villager)
{
	return Locator::entitiesRegistry::value().TryGet<LivingAction>(villager);
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

/// The top state if it is a final one, else the destination state
VillagerStates FinalState(const LivingAction& action)
{
	const auto top = Get(action, LivingAction::Index::Top);
	const auto* info = TableOf(top);
	return info != nullptr && info->isFinalState != 0 ? top : Get(action, LivingAction::Index::Final);
}

glm::vec3 PositionOf(entt::entity object)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(object);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

/// The distance in metres, x and z only, through the table hypotenuse
float Distance2D(const glm::vec3& a, const glm::vec3& b)
{
	return gutils::GetDistanceInMetres(a, b);
}

/// The game's float random (0 for 0, signed like max)
using game_random::GameFloatRand;

/// The villager core's (ECS/Villager/VillagerCore.h), which runs the exit functions of TOP and of the final state and
/// the entry of the new state from the rows of k_VillagerStateTable, once each, with the pause roll (only into a state
/// whose row may pause: none of 163, 215..220) and the codes 1 / 0x2E / 0x2F; the walk's end as
/// villager_reactions::SetTopState
uint32_t SetTopState(entt::entity villager, VillagerStates state)
{
	return villager_reactions::SetTopState(villager, state);
}

/// The stored state (index 2, PREVIOUS) through the core's SetState, which skips a state whose row has field0x10
/// set and adjusts the town modifiers
void SetStoredState(entt::entity villager, VillagerStates state)
{
	villager::SetState(villager, LivingAction::Index::Previous, state);
}

/// The final state is kept in index 2, unless it is a passing one (field0x10 or field0xb8), which keeps what was
/// stored. A raw store, no town modifiers
void StorePreviousState(LivingAction& action)
{
	const auto final = FinalState(action);
	const auto* info = TableOf(final);
	auto stored = final;
	if (info != nullptr && (info->keepsPreviousState != 0 || info->isReactionState != 0))
	{
		stored = Get(action, LivingAction::Index::Previous);
	}
	action.states.at(static_cast<size_t>(LivingAction::Index::Previous)) = static_cast<uint8_t>(stored);
}

/// villager_reactions::PopFromPrevious: the stored state's resume state (field0x20; nothing stored is row 0, resume
/// 0: INVALID_STATE, as the original), 0x2E -> raw TOP 163, then raw PREVIOUS 0
void PopFromPrevious(entt::entity villager)
{
	villager_reactions::PopFromPrevious(villager);
}

/// The villager core's (MOVE_TO_POS now and `final` as the destination, with their exits and entries, then the walk)
void SetupMoveToWithHug(entt::entity villager, const glm::vec2& goal, VillagerStates final)
{
	villager::SetupMoveToWithHug(villager, goal, final);
}

/// `distance` from the object, on its side away from the villager
glm::vec2 FleeingPosition(entt::entity villager, entt::entity object, float distance)
{
	const auto from = PositionOf(object);
	const auto at = PositionOf(villager);
	glm::vec2 d(at.x - from.x, at.z - from.z);
	if (d.x != 0.0f || d.y != 0.0f)
	{
		d *= distance / glm::length(d);
	}
	return glm::vec2(from.x, from.z) + d;
}

/// from + GetPosFromAngle(angle, radius) added as MapCoords, on the (x, z) in metres (each a MapCoords, as the original
/// holds them)
glm::vec2 PosFromAngle(const glm::vec3& from, float angle, float radius)
{
	const auto at = map_coords::FromMetres(glm::vec2(from.x, from.z)) + gutils::GetPosFromAngle(angle, radius);
	return map_coords::ToMetres(at);
}

float Radius2D(entt::entity object)
{
	return fire::traits::Radius(object);
}

/// The wall hug's arrival test with no extra: d^2 < (the wall hug's step + extra)^2, strictly; the step is openblack's
/// WallHug::speed (as PathfindingSystem's)
bool AreWeThere(entt::entity villager, const glm::vec2& at, const glm::vec2& goal)
{
	const auto* wallHug = Locator::entitiesRegistry::value().TryGet<const WallHug>(villager);
	const float step = wallHug != nullptr ? wallHug->speed : 0.0f;
	return glm::distance2(at, goal) < step * step;
}

/// The firemen list of a fire's group root
bool IsFireman(const fire::FireEffect& fire, entt::entity villager)
{
	const auto& list = fire.root->firemen;
	return std::find(list.begin(), list.end(), villager) != list.end();
}

/// The group root's firemen list, newest first
void AddFireman(fire::FireEffect& fire, entt::entity villager)
{
	auto& list = fire.root->firemen;
	list.insert(list.begin(), villager);
}
void RemoveFireman(fire::FireEffect& fire, entt::entity villager)
{
	auto& list = fire.root->firemen;
	std::erase(list, villager);
}

/// On the line from the fire to the villager, just outside the fire
bool FireFightingPosition(entt::entity villager, const fire::FireEffect& fire, glm::vec2& out)
{
	if (fire.object == entt::null)
	{
		return false;
	}
	const auto centre = fire::traits::FireCentre(fire.object);
	const auto at = PositionOf(villager);
	// the angle from the fire's centre to the villager
	const float angle = gutils::Get3DAngleFromXZ(glm::vec2(centre.x, centre.z), glm::vec2(at.x, at.z));
	// safe < the object's radius -> the radius, else safe, so the larger of the two; + the villager's radius;
	// + GameFloatRand(1)
	const float radius = Radius2D(fire.object);
	const float safe = fire.SafeFireRadius();
	const float keep = safe < radius ? radius : safe;
	const float distance = keep + Radius2D(villager) + GameFloatRand(1.0f);
	// the object's position + GetPosFromAngle(angle, distance)
	out = PosFromAngle(PositionOf(fire.object), angle, distance);
	return true;
}

/// MOVE_AROUND_FIRE towards `destination`, then `after`; true if SetTopState(220) gave 1
bool SetupMoveAroundFire(entt::entity villager, const glm::vec2& destination, VillagerStates after)
{
	// SetTopState(220); anything but 1 -> false, nothing more
	if (SetTopState(villager, VillagerStates::MoveAroundFire) != villager::k_Done)
	{
		return false;
	}
	StateOf(villager).savedDestination = destination;
	SetStoredState(villager, after);
	// going on to ARRIVES_AT_WORSHIP_SITE_FOR_WORSHIP (59), it is on its town's way list again, with its on-the-way flag
	auto& registry = Locator::entitiesRegistry::value();
	const auto* v = registry.TryGet<const Villager>(villager);
	// (guard) the original reads the town as it is: this stands for the missing town unlinking
	if (after == VillagerStates::ArrivesAtWorshipSiteForWorship && v != nullptr && ecs::IsAvailable(v->town))
	{
		worship::percentage::AddVillagerOnWay(v->town, villager);
		auto* worshipper = registry.TryGet<WorshipVillager>(villager);
		if (worshipper == nullptr)
		{
			worshipper = &registry.Assign<WorshipVillager>(villager);
		}
		worshipper->onWayInTown = true;
		worshipper->onWay = true;
	}
	return true;
}

/// The villager stands in the band just outside the fire (its radius plus the fire's) and 2 m further. The fire's part
/// is max(safe radius, the object's radius), as in FireFightingPosition
bool IsBesideFire(entt::entity villager, const fire::FireEffect& fire, float band)
{
	if (fire.object == entt::null)
	{
		return false;
	}
	const float distance = Distance2D(PositionOf(villager), PositionOf(fire.object));
	const float radius = Radius2D(fire.object);
	const float safe = fire.SafeFireRadius();
	const float keep = safe < radius ? radius : safe;
	const float reach = Radius2D(villager) + keep;
	return distance > reach && reach + band > distance;
}

/// To the nearest burning member of the group, to beat it
bool DecideHowToPutOutFire(entt::entity villager, fire::FireEffect& fire)
{
	auto* target = fire.NearestFireToFight(PositionOf(villager));
	StateOf(villager).fire = target != nullptr ? target->id : 0;
	if (target == nullptr)
	{
		return false;
	}
	glm::vec2 position;
	if (!FireFightingPosition(villager, *target, position))
	{
		return false;
	}
	SetupMoveAroundFire(villager, position, VillagerStates::PutOutFireByBeating);
	return true;
}

/// The saved destination back, and the stored state
void FinishBeingOnFire(entt::entity villager)
{
	if (auto* wallHug = Locator::entitiesRegistry::value().TryGet<WallHug>(villager))
	{
		wallHug->goal = StateOf(villager).savedDestination;
	}
	PopFromPrevious(villager);
}

const ReactionInfo& FireReactionInfo()
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(openblack::Reaction::ReactToFire));
}

/// The villager's reaction records (common to the Living: ECS/Effects/Reactions): may it react to this type again
/// (more than `again` turns since the last time)? Remembers it.
bool MayReactAgain(entt::entity villager, openblack::Reaction type, uint32_t again)
{
	return effects::reactions::Records(villager, static_cast<uint8_t>(type), again, effects::reactions::Turn());
}

/// The REACT_TO_FIRE part of applying a reaction to the living of a cell, for one villager of the cell: with no
/// reaction of its own, the reaction score (the priority x (1 + 0.5 howImportantIsDistance (R - d) / R), 0 beyond
/// maxReactionDistance) above 0 and not reacted to a fire lately, it starts reacting (SetupReactToFire).
/// Replacing a current reaction by a higher one is not ported.
void ApplyFireReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the check made for every reaction type: the villager's common IsAvailableForReaction
	if (!registry.AllOf<Villager, LivingAction>(villager) || villager == reaction.initiator ||
	    !villager::IsAvailableForReaction(villager))
	{
		return;
	}
	const auto at = PositionOf(villager);
	const auto from = PositionOf(reaction.initiator);
	// the shield test is done by reactions::SpreadReaction for every Living class
	const float distance = 0.5f * (std::abs(at.x - from.x) + std::abs(at.z - from.z));
	auto& state = StateOf(villager);
	if (effects::reactions::Find(state.reaction) != nullptr)
	{
		return;
	}
	const auto& info = FireReactionInfo();
	if (distance > info.maxReactionDistance)
	{
		return;
	}
	// ECS/Effects/Reactions: Score
	const auto score = effects::reactions::Score(static_cast<uint8_t>(openblack::Reaction::ReactToFire), true,
	                                             villager_fire::ReactToFirePriority(villager, reaction.id, 0), distance);
	if (score == 0 ||
	    !MayReactAgain(villager, openblack::Reaction::ReactToFire, info.numGameTurnsForNormalThingsBeforeReactingAgain))
	{
		return;
	}
	villager_fire::SetupReactToFire(villager, reaction.initiator, reaction.id);
}
/// OPENBLACK_VILLAGER_TRACE: one line per call of the fire's entry / exit functions, with what they return
void TraceCall(entt::entity villager, const char* function, VillagerStates a, VillagerStates b, uint32_t result)
{
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("{}({}, {}) = {}", function, static_cast<int>(a), static_cast<int>(b), result));
	}
}
} // namespace

uint32_t villager_fire::FireOf(entt::entity villager)
{
	const auto* state = FindState(villager);
	return state != nullptr ? state->fire : 0;
}

bool villager_fire::IsFireMan(entt::entity object)
{
	const auto* action = ActionOf(object);
	if (action == nullptr || !Locator::entitiesRegistry::value().AllOf<Villager>(object))
	{
		return false; // not a villager: never a fireman
	}
	const auto final = FinalState(*action);
	switch (final)
	{
	case VillagerStates::PutOutFireByBeating: // exit function ExitPutOutFire
	case VillagerStates::PutOutFireWithWater:
	case VillagerStates::GetWaterToPutOutFire:
	case VillagerStates::MoveAroundFire:
	case VillagerStates::ReactToFire:
		return true;
	default:
		return false;
	}
}

bool villager_fire::IsInOnFireState(entt::entity villager)
{
	const auto* action = ActionOf(villager);
	return action != nullptr && FinalState(*action) == VillagerStates::OnFire;
}

void villager_fire::SetupOnFire(entt::entity villager, uint32_t fire)
{
	auto* action = ActionOf(villager);
	if (action == nullptr || fire::traits::InHand(villager) || !villager::IsAvailable(villager))
	{
		// in the hand or thrown, or not available; one more flag of the original is not ported
		return;
	}
	const auto top = Get(*action, LivingAction::Index::Top);
	if (top == VillagerStates::Flying || top == VillagerStates::InHand)
	{
		return;
	}
	StorePreviousState(*action);
	auto& state = StateOf(villager);
	if (const auto* wallHug = Locator::entitiesRegistry::value().TryGet<const WallHug>(villager))
	{
		state.savedDestination = wallHug->goal;
	}
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	state.fire = fire;
	SetTopState(villager, VillagerStates::OnFire);
	if (fire::TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fire: villager {} on fire (fleeing fire {})", static_cast<int>(villager),
		                   fire);
	}
}

void villager_fire::StopFireFighting(entt::entity villager)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	auto& state = StateOf(villager);
	if (state.fire == 0)
	{
		return;
	}
	auto* fire = fire::Get(state.fire);
	if (FinalState(*action) == VillagerStates::MoveAroundFire)
	{
		// moving around a fire towards a fire-fighting state (the stored one): it resumes, or decides
		auto next = Get(*action, LivingAction::Index::Previous);
		const auto* info = TableOf(next);
		next = info != nullptr ? static_cast<VillagerStates>(info->resumeState) : VillagerStates::DecideWhatToDo;
		const auto* nextInfo = TableOf(next);
		if (next == VillagerStates::PutOutFireByBeating || next == VillagerStates::PutOutFireWithWater ||
		    next == VillagerStates::GetWaterToPutOutFire || next == VillagerStates::MoveAroundFire || nextInfo == nullptr)
		{
			next = VillagerStates::DecideWhatToDo;
		}
		if (fire != nullptr)
		{
			RemoveFireman(*fire, villager);
		}
		state.fire = 0;
		SetTopState(villager, next);
		action->states.at(static_cast<size_t>(LivingAction::Index::Previous)) = 0;
		return;
	}
	if (fire != nullptr)
	{
		RemoveFireman(*fire, villager);
	}
	state.fire = 0;
	SetTopState(villager, VillagerStates::DecideWhatToDo);
}

uint8_t villager_fire::ReactToFirePriority(entt::entity villager, uint32_t reaction, uint32_t currentReaction)
{
	const auto* found = effects::reactions::Find(reaction);
	// (guard) the original only tests for null here, not availability
	if (found == nullptr || !ecs::IsAvailable(found->initiator))
	{
		return 0;
	}
	auto* fire = fire::Find(found->initiator);
	if (fire == nullptr || IsInOnFireState(villager))
	{
		return 0;
	}
	const auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return 0;
	}
	const auto& info = FireReactionInfo();
	const float distance = Distance2D(PositionOf(villager), PositionOf(found->initiator));
	// ReactionInfo[10].priority (210) x (1 + 0.5 x fire radius / max radius), at most 255
	const float ratio = fire->FireRadius() / fire->MaxFireRadius();
	const float value = (ratio * 0.5f + 1.0f) * static_cast<float>(info.priority);
	const auto priority = static_cast<uint8_t>(value < 255.0f ? value : 255.0f);
	// recent (under 25 turns) and too near, or inside the safe radius: at that priority (it flees)
	if ((effects::reactions::Turn() - found->turnCreated < 25 && distance < info.minDistanceToRunAwayFromObject) ||
	    fire->SafeFireRadius() > distance)
	{
		return priority;
	}
	// reacting to something else than a fire: the priority
	if (const auto* current = effects::reactions::Find(currentReaction);
	    current != nullptr && current->type != openblack::Reaction::ReactToFire)
	{
		return priority;
	}
	// not fighting a fire (the final state's exit is not ExitPutOutFire, and not MOVE_AROUND_FIRE): the priority
	const auto final = FinalState(*action);
	const bool fighting = final == VillagerStates::PutOutFireByBeating || final == VillagerStates::PutOutFireWithWater ||
	                      final == VillagerStates::GetWaterToPutOutFire || final == VillagerStates::MoveAroundFire;
	if (!fighting)
	{
		return priority;
	}
	auto* mine = fire::Get(StateOf(villager).fire);
	if (mine == nullptr || mine->object == entt::null)
	{
		return priority;
	}
	// the fire it fights is of the same group: nothing new
	for (const auto* member = fire->root; member != nullptr; member = member->next)
	{
		if (member == mine)
		{
			return 0;
		}
	}
	// within 2 x (10 + maxReactionDistance) of the fire it fights (the distance between the two fires' objects): that
	// fire's group takes this one in
	if (!(2.0f * (10.0f + info.maxReactionDistance) < Distance2D(PositionOf(mine->object), PositionOf(fire->object))))
	{
		fire::AddToFireGroup(*mine, *fire);
		return 0;
	}
	return priority;
}

void villager_fire::SetupReactToFire(entt::entity villager, entt::entity object, uint32_t reaction)
{
	auto* action = ActionOf(villager);
	// (guard) the original only tests for null here, not availability
	if (action == nullptr || !ecs::IsAvailable(object))
	{
		return;
	}
	auto& state = StateOf(villager);
	state.object = object;
	state.reaction = reaction;
	// adding the reaction: the reaction is kept and the state stored, then REACT_TO_FIRE
	StorePreviousState(*action);
	SetTopState(villager, VillagerStates::ReactToFire);
	if (fire::TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fire: villager {} reacts to the fire of object {}", static_cast<int>(villager),
		                   static_cast<int>(object));
	}
}

uint32_t villager_fire::ReactToFire(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	auto& state = StateOf(villager);
	// no object -> return 0; no fire on it -> return 0, both with no state change. What takes the villager out of 215
	// is elsewhere: the object going is the validate slot's (villager_reactions::ReactionValidate, run before the
	// state: no object or not available -> PopFromPrevious); the fire going out (below its reaction temperature,
	// deleted, or moved) removes its REACT_TO_FIRE, whose shut-down runs StopReactingAndSetState on every follower
	// (ShutDownReaction, called by ECS/Fire/FireEffect.cpp before it removes the reaction)
	if (state.reaction != 0 && effects::reactions::Find(state.reaction) == nullptr)
	{
		// (approximate) a REACT_TO_FIRE removed by a path that does not go through the fire (a pot's removal takes all
		// of an object's reactions; openblack's reactions keep no list of followers): the same shut-down, one turn later
		villager_reactions::StopReactingAndSetState(villager);
		return 1;
	}
	// (guard) the original only tests for null here, not availability
	if (!ecs::IsAvailable(state.object))
	{
		return 0;
	}
	auto* fire = fire::Find(state.object);
	if (fire == nullptr)
	{
		return 0;
	}
	state.fire = fire->id;
	// looks at the object (inferred: faces it at once)
	const auto& info = FireReactionInfo();
	const float distance = Distance2D(PositionOf(villager), PositionOf(state.object));
	const auto* reaction = effects::reactions::Find(state.reaction);
	const bool recent = reaction != nullptr && effects::reactions::Turn() - reaction->turnCreated < 25;
	if ((recent && distance < info.minDistanceToRunAwayFromObject) || !(fire->SafeFireRadius() <= distance))
	{
		// too near: away from it, and then look again (in the run-away band if recent, else the safe radius plus
		// GameFloatRand(2.0))
		const float away = recent ? GameFloatRand(info.maxDistanceToRunAwayFromObject - info.minDistanceToRunAwayFromObject) +
		                                info.minDistanceToRunAwayFromObject
		                          : fire->SafeFireRadius() + GameFloatRand(2.0f);
		SetupMoveToWithHug(villager, FleeingPosition(villager, state.object, away), VillagerStates::ReactToFire);
		return 1;
	}
	// its final state already fights a fire (exit ExitPutOutFire): StopReactingAndSetState (the state reset after
	// reacting, then StopReacting)
	if (const auto final = FinalState(action);
	    final == VillagerStates::PutOutFireByBeating || final == VillagerStates::PutOutFireWithWater ||
	    final == VillagerStates::GetWaterToPutOutFire || final == VillagerStates::MoveAroundFire)
	{
		villager_reactions::StopReactingAndSetState(villager);
		return 1;
	}
	// a villager with a town that is not on its way to worship may fight it. The score is the group's burning priority
	// x the room left around it x the town distance term (GetDistanceModifier, 400 m); above 0.1 it goes to beat the
	// fire. The room is the group's burning radius x 2pi / (the villager's radius x 4), 0 when 0, else (room - the
	// firemen count) / room. No random term (the original asks whether it is on fire and ignores the answer).
	// (approximate): the radius, priority and count are taken as GroupBurningRadius, GroupBurningPriority and the size
	// of the firemen list without tracing them; the decision is unverified.
	const auto* worshipper = registry.TryGet<const WorshipVillager>(villager);
	if (const auto* v = registry.TryGet<const Villager>(villager);
	    // (guard) the original reads the town as it is: this stands for the missing town unlinking
	    v != nullptr && ecs::IsAvailable(v->town) && (worshipper == nullptr || !worshipper->onWay))
	{
		// the distance in metres, then GetDistanceModifier = a sigmoid threshold (0.5, 1 - min(d, 400) / 400) over the
		// 41-step table (Common/GUtilsDistance): openblack used to approximate it with a smoothstep, which is 0.156 at
		// 250 m where the original gives 0.0444, and saturates to 1 / 0 at the ends instead of 0.99996 / 3.6e-5
		const float townModifier = gutils::GetDistanceModifier(Distance2D(PositionOf(v->town), PositionOf(villager)), 400.0f);
		float room = fire->GroupBurningRadius() * glm::two_pi<float>() / (4.0f * Radius2D(villager));
		if (room != 0.0f)
		{
			room = (room - static_cast<float>(fire->root->firemen.size())) / room;
		}
		const float score = fire->GroupBurningPriority() * room * townModifier;
		if (score > 0.1f && DecideHowToPutOutFire(villager, *fire))
		{
			return 1;
		}
	}
	// else around the fire towards where it was going
	glm::vec2 destination(PositionOf(villager).x, PositionOf(villager).z);
	if (const auto* wallHug = registry.TryGet<const WallHug>(villager))
	{
		destination = wallHug->goal;
	}
	SetupMoveAroundFire(villager, destination, Get(action, LivingAction::Index::Previous));
	return 1;
}

uint32_t villager_fire::PutOutFireByBeating(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	auto* fire = fire::Get(StateOf(villager).fire);
	if (fire != nullptr && IsBesideFire(villager, *fire, 2.0f))
	{
		// looks at the fire (inferred: at once); ready for a new animation once the clip played once, then every turn
		if (!VillagerAnimationDone(villager, action.turnsSinceStateChange))
		{
			return 1;
		}
		// (guard) the original tests only the fire here
		if (fire->IsAboveReactionTemperature() && ecs::IsAvailable(fire->object))
		{
			// a burn of -8: it cools the fire
			effects::EffectValues values;
			values.numbers[static_cast<size_t>(effects::EffectValues::Number::Burn)] = -8.0f;
			fire::ApplyEffectToFireEffectIfNecessary(fire->object, values);
			return 1;
		}
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	glm::vec2 position;
	if (fire != nullptr && FireFightingPosition(villager, *fire, position))
	{
		SetupMoveAroundFire(villager, position, VillagerStates::PutOutFireByBeating);
		return 1;
	}
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_fire::PutOutFireWithWater(LivingAction& action)
{
	SetTopState(Locator::entitiesRegistry::value().ToEntity(action), VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_fire::OnFire(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	auto* own = fire::Find(villager);
	if (own == nullptr)
	{
		FinishBeingOnFire(villager);
		return 0;
	}
	auto& state = StateOf(villager);
	glm::vec2 target;
	if (state.fire != 0)
	{
		auto* other = fire::Get(state.fire);
		if (other == nullptr || other->object == entt::null)
		{
			return 0; // the fire's object is gone: nothing this turn (no FinishBeingOnFire)
		}
		float distance = 0.0f;
		if (own->IsOnFire())
		{
			distance = other->MaxFireRadius() + GameFloatRand(10.0f);
		}
		else
		{
			// the distance between the villager and the fire's object
			if (other->MaxFireRadius() < Distance2D(PositionOf(villager), PositionOf(other->object)))
			{
				FinishBeingOnFire(villager);
				return 0;
			}
			distance = other->FireRadius() + GameFloatRand(other->MaxFireRadius() - other->FireRadius() + 1.0f);
		}
		target = FleeingPosition(villager, other->object, distance);
	}
	else
	{
		if (!own->IsOnFire())
		{
			FinishBeingOnFire(villager);
			return 0;
		}
		const auto at = PositionOf(villager);
		// GameFloatRand(2 pi) first, then GameFloatRand(6) + 4, then me + GetPosFromAngle(angle, distance)
		const float angle = GameFloatRand(glm::two_pi<float>());
		const float distance = GameFloatRand(6.0f) + 4.0f;
		target = PosFromAngle(at, angle, distance);
	}
	SetupMoveToWithHug(villager, target, VillagerStates::OnFire);
	if (Get(action, LivingAction::Index::Previous) == VillagerStates::InvalidState)
	{
		SetStoredState(villager, VillagerStates::DecideWhatToDo);
	}
	return 1;
}

uint32_t villager_fire::MoveAroundFire(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto destination = StateOf(villager).savedDestination;
	const auto at = PositionOf(villager);
	// AreWeThere(destination): arrived -> the stored state, and DECIDE_WHAT_TO_DO after it
	if (AreWeThere(villager, glm::vec2(at.x, at.z), destination))
	{
		// PopFromPrevious, then a raw store of 163 in index 2 (not the villager's SetState, so no field0x10 skip and
		// no town modifiers)
		PopFromPrevious(villager);
		if (auto* again = ActionOf(villager); again != nullptr)
		{
			again->states.at(static_cast<size_t>(LivingAction::Index::Previous)) =
			    static_cast<uint8_t>(VillagerStates::DecideWhatToDo);
		}
		return 1;
	}
	auto* fire = fire::Get(StateOf(villager).fire);
	if (fire == nullptr || fire->object == entt::null)
	{
		return 0;
	}
	// (approximate) the via point (a point around each burning member of the group, up to 1000 tries) is not ported:
	// it walks straight on to the destination
	SetupMoveToWithHug(villager, destination, VillagerStates::MoveAroundFire);
	return 1;
}

uint32_t villager_fire::EnterPutOutFire(LivingAction& action, VillagerStates final, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto result = [&]() -> uint32_t {
		// the two rows' entry functions are the same (216, 217, 218 and 220 all have EnterPutOutFire) -> 1
		const auto same = [](VillagerStates s) {
			return s == VillagerStates::PutOutFireByBeating || s == VillagerStates::PutOutFireWithWater ||
			       s == VillagerStates::GetWaterToPutOutFire || s == VillagerStates::MoveAroundFire;
		};
		if (same(final) && same(next))
		{
			return 1;
		}
		auto& state = StateOf(villager);
		// no fire -> the refusal
		if (state.fire != 0)
		{
			auto* fire = fire::Get(state.fire);
			if (fire == nullptr)
			{
				// not in the game's fire list -> no fire
				state.fire = 0;
			}
			// the fire is live (approximate: it still has its object), and a reaction that is not shut down
			// (approximate: openblack still has it)
			else if (fire->object != entt::null && effects::reactions::Find(state.reaction) != nullptr)
			{
				// already in the group root's fireman list -> 0; else AddFireman and 1
				if (IsFireman(*fire, villager))
				{
					return 0;
				}
				AddFireman(*fire, villager);
				return 1;
			}
		}
		// the final state a reactive one (field0xb8) -> StopReacting; 0 (refused: 0x2F, and SetTopState enters
		// DECIDE_WHAT_TO_DO)
		if (const auto* info = TableOf(final); info != nullptr && info->isReactionState != 0)
		{
			villager_reactions::StopReacting(villager);
		}
		return 0;
	}();
	TraceCall(villager, "EnterPutOutFire", final, next, result);
	return result;
}

uint32_t villager_fire::ExitPutOutFire(LivingAction& action, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	TraceCall(villager, "ExitPutOutFire", FinalState(action), next, 1);
	auto& state = StateOf(villager);
	// IsStateExitFunctionSameAs: into another state with ExitPutOutFire (216..218, 220) or into a state that is not a
	// final one it stays a fireman
	if (!villager::IsStateExitFunctionSameAs(villager, next))
	{
		if (auto* fire = fire::Get(state.fire); fire != nullptr)
		{
			if (!IsFireman(*fire, villager))
			{
				state.fire = 0; // not in the list: nothing more (no ExitReaction), 1
				return 1;
			}
			RemoveFireman(*fire, villager);
		}
		state.fire = 0;
		// off its town's way list
		// (guard) the original reads the town as it is: this stands for the missing town unlinking
		if (const auto* v = registry.TryGet<const Villager>(villager); v != nullptr && ecs::IsAvailable(v->town))
		{
			worship::percentage::RemoveVillagerOnWay(v->town, villager);
			if (auto* worshipper = registry.TryGet<WorshipVillager>(villager))
			{
				worshipper->onWayInTown = false;
			}
		}
	}
	// ExitReaction: the circle hug reset, and the reaction ends (StopReacting) unless the next state is a reactive one
	// (field0xb8)
	villager_reactions::ExitReaction(action, next);
	return 1; // always 1, it may leave
}

uint32_t villager_fire::EnterOnFire(LivingAction& action, VillagerStates final, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto result = [&]() -> uint32_t {
		// no fire or not a live one -> 1. (approximate) a fire openblack no longer has counts as not live
		auto* fire = fire::Get(StateOf(villager).fire);
		if (fire == nullptr)
		{
			return 1;
		}
		// already in the group root's fireman list -> 0; else AddFireman, 1
		if (IsFireman(*fire, villager))
		{
			return 0;
		}
		AddFireman(*fire, villager);
		return 1;
	}();
	TraceCall(villager, "EnterOnFire", final, next, result);
	return result;
}

uint32_t villager_fire::ExitOnFire(LivingAction& action, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	TraceCall(villager, "ExitOnFire", FinalState(action), next, 1);
	// out of the group root's fireman list if in it; the fire is cleared on every path; 1 (it may leave, whatever the
	// next state: no IsStateExitFunctionSameAs)
	auto& state = StateOf(villager);
	if (auto* fire = fire::Get(state.fire); fire != nullptr && IsFireman(*fire, villager))
	{
		RemoveFireman(*fire, villager);
	}
	state.fire = 0;
	return 1;
}

void villager_fire::ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	ApplyFireReaction(villager, reaction);
}

void villager_fire::ShutDownReaction(uint32_t reaction)
{
	if (reaction == 0)
	{
		return;
	}
	// the reaction is marked shut down, then while it has followers, the first follower's StopReactingAndSetState,
	// whose StopReacting takes it off the list; then the reaction is deleted. (inferred) the list's order is not kept
	// here: the followers go by entity
	std::vector<entt::entity> followers;
	Locator::entitiesRegistry::value().Each<const VillagerFireState>(
	    [&followers, reaction](entt::entity villager, const VillagerFireState& state) {
		    if (state.reaction == reaction)
		    {
			    followers.push_back(villager);
		    }
	    });
	std::sort(followers.begin(), followers.end());
	for (const auto villager : followers)
	{
		const auto* state = FindState(villager);
		if (state == nullptr || state->reaction != reaction || !ecs::IsAvailable(villager))
		{
			continue;
		}
		if (fire::TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fire: reaction {} shut down: villager {} stops reacting", reaction,
			                   static_cast<int>(villager));
		}
		villager_reactions::StopReactingAndSetState(villager);
	}
}

void villager_fire::Clear()
{
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		std::vector<entt::entity> owners;
		registry.Each<const VillagerFireState>(
		    [&owners](entt::entity villager, const VillagerFireState&) { owners.push_back(villager); });
		for (const auto villager : owners)
		{
			registry.RemoveState<VillagerFireState>(villager);
		}
	}
	villager_reactions::Register(); // the Villager handler of ECS/Effects/Reactions
}

bool villager_fire::IsReacting(entt::entity villager)
{
	const auto* state = FindState(villager);
	return state != nullptr && state->reaction != 0;
}

entt::entity villager_fire::ReactionObject(entt::entity villager)
{
	const auto* state = FindState(villager);
	return state != nullptr ? state->object : entt::entity(entt::null);
}

void villager_fire::StopReacting(entt::entity villager)
{
	auto* state = FindState(villager);
	if (state == nullptr)
	{
		return;
	}
	// with a reaction: off the reaction's list (openblack's reactions keep no list of followers), its record of the
	// reaction's type gets the turn, and the reaction is cleared
	if (state->reaction != 0)
	{
		effects::reactions::RefreshRecord(villager, static_cast<uint8_t>(openblack::Reaction::ReactToFire),
		                                  effects::reactions::Turn());
		state->reaction = 0;
	}
	// the object is cleared on both paths
	state->object = entt::null;
}
