/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerShield.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <numbers>
#include <vector>

#include <fmt/format.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerShieldState.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerAnimations.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Spells/SpellShield.h"
#include "Resources/ResourcesInterface.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace tq = openblack::ecs::town_queries;

namespace
{
auto& Reg()
{
	return Locator::entitiesRegistry::value();
}

/// The villager's shield state if it has one (any id, alive or not)
VillagerShieldState* FindState(entt::entity villager)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Reg();
	return registry.Valid(villager) ? registry.TryGet<VillagerShieldState>(villager) : nullptr;
}

/// The villager's shield state, added with its defaults if it has none. The villager must exist.
VillagerShieldState& StateOf(entt::entity villager)
{
	if (auto* state = Reg().TryGet<VillagerShieldState>(villager))
	{
		return *state;
	}
	return Reg().AssignState<VillagerShieldState>(villager);
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

/// The top state if it is a final one (state table), else the destination state
VillagerStates FinalState(const LivingAction& action)
{
	const auto top = Get(action, LivingAction::Index::Top);
	const auto* info = TableOf(top);
	return info != nullptr && info->isFinalState != 0 ? top : Get(action, LivingAction::Index::Final);
}

const ReactionInfo& ShieldReactionInfo()
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(openblack::Reaction::ReactToMagicShield));
}

/// The villager's town
entt::entity TownEntityOf(entt::entity villager)
{
	const auto* component = Reg().TryGet<const Villager>(villager);
	// (guard) the original reads this link with no availability test
	if (component == nullptr || !ecs::IsAvailable(component->town))
	{
		return entt::null;
	}
	return component->town;
}

/// A live shield spell: an entity with a Spell component of the shield class, not marked by ToBeDeleted
/// (ecs::IsAvailable; the spells are destroyed at once today)
bool IsAvailableShieldSpell(entt::entity spell)
{
	auto& registry = Reg();
	if (spell == entt::null || !ecs::IsAvailable(spell) || !registry.AllOf<Spell>(spell))
	{
		return false;
	}
	return registry.Get<const Spell>(spell).spellClass == SpellClass::Shield;
}

/// The radius of a shield spell (Get2DRadius): its magnitude
float RadiusOf(entt::entity spell)
{
	return Reg().Get<const Spell>(spell).magnitude;
}

/// The reaction's position is its initiator's: for a shield spell its cast position, as MapCoords
glm::ivec2 ShieldPos(entt::entity spell)
{
	const auto& component = Reg().Get<const Spell>(spell);
	return tq::ToMapCoords(glm::vec2(component.position.x, component.position.z));
}

/// OPENBLACK_TEST_SHIELD_REACTION=1: takes the town as "it wants protection and was just attacked", so the reaction
/// can be seen in game while what feeds those two inputs is not ported (the protection desire's input comes from the
/// player's interaction with the town, which openblack does not write yet; only a script boost moves it; and the
/// aggressor's turn only comes from the physical shield's impacts). Off by default: the original's reads
bool TestForceTownGates()
{
	static const bool forced = [] {
		const char* value = std::getenv("OPENBLACK_TEST_SHIELD_REACTION");
		return value != nullptr && value[0] != '\0' && value[0] != '0';
	}();
	return forced;
}

/// The town's desire for protection (GetDesireSignificanceToVillager, TOWN_DESIRE_FOR_PROTECTION): the sum of its
/// inputs minus the desire's trigger, at least 0 (ecs::town_desire)
float ProtectionDesireSignificance(entt::entity town)
{
	if (TestForceTownGates())
	{
		return 1.0f;
	}
	return town_desire::GetDesireSignificanceToVillager(town, TownDesireInfo::ForProtection);
}

/// The turns since the town's last aggressor against the villager info's
/// numGameTurnsAfterAggressionInterestedInShield, unsigned
bool AttackedRecently(entt::entity town)
{
	if (TestForceTownGates())
	{
		return true;
	}
	const auto* component = Reg().TryGet<const Town>(town);
	if (component == nullptr)
	{
		return false;
	}
	const uint32_t since = effects::reactions::Turn() - component->aggressorTurn;
	// (approximate) the original reads the villager's own info row; openblack's villager code uses row 0
	return since <= Locator::infoConstants::value().villager.at(0).numGameTurnsAfterAggressionInterestedInShield;
}

/// From the villager's reaction records: may it react to this type again?
bool MayReactAgain(entt::entity villager, uint32_t again)
{
	return effects::reactions::Records(villager, static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield), again,
	                                   effects::reactions::Turn());
}

/// Done by AddReaction when the villager had no reaction: the final state goes to index 2, unless it is a passing one
/// (two flags of the state table)
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

/// The same point both times it is set: the villager's own position plus 1 m along the bearing from the shield's
/// centre to it, jittered by GameFloatRand(pi / 4) - pi / 8. It is what LookAtPos turns towards, so the villager ends
/// up facing away from the dome
glm::ivec2 LookAtPoint(entt::entity villager, entt::entity spell)
{
	const auto me = tq::PosOf(villager);
	const float bearing = tq::Get3DAngleFromXZ(ShieldPos(spell), me);
	const float angle = bearing + villager::GameFloatRand(std::numbers::pi_v<float> / 4.0f) - 0.39269909f;
	return me + tq::GetPosFromAngle(angle, 1.0f);
}

/// The state's clip is kept for GameFloatRand(10) + 20 seconds (as many loops as fit: the loop count asked for is that
/// time x 1000 / the clip's duration in ms) and then rolled again (SetAnim with the state's clip, the AmazedByShield
/// set of ECS/VillagerAnimations). The one exception: while the clip is INTO_POINTING (286) it only plays once and is
/// followed by TALKING_AND_POINTING (395)
void UpdateAmazedClip(entt::entity villager, LivingAction& action)
{
	// GameFloatRand(10) + 20 seconds, drawn every turn the state runs (before anything else here, so
	// the random sequence does not depend on openblack's clip guards below)
	const float seconds = villager::GameFloatRand(10.0f) + 20.0f;
	auto* animation = Reg().TryGet<const SkeletalAnimation>(villager);
	if (animation == nullptr || !animation->hasClip || !Locator::resources::has_value())
	{
		return;
	}
	auto& animations = Locator::resources::value().GetAnimations();
	if (!animations.Contains(animation->clip))
	{
		return;
	}
	const int32_t duration = animations.Handle(animation->clip)->GetDurationMs();
	if (duration <= 0)
	{
		return;
	}
	// those seconds x 1000 / the duration of the clip it has
	auto loops = static_cast<int32_t>(seconds * 1000.0f / static_cast<float>(duration));
	constexpr int32_t k_IntoPointing = 286;       ///< AnimPack entry
	constexpr int32_t k_TalkingAndPointing = 395; ///< AnimPack entry
	int32_t clip = -1;                            ///< < 0: roll the state's clip again
	if (animation->clipIndex == k_IntoPointing)
	{
		loops = 1;
		clip = k_TalkingAndPointing;
	}
	// ready for a new clip: turns in the state x the ms per turn (100) >= loops x the duration
	if (static_cast<int32_t>(action.turnsSinceStateChange) * 100 < loops * duration)
	{
		return;
	}
	if (clip < 0)
	{
		VillagerSetStateClip(villager, true); // the state's clip again, from the start
	}
	else
	{
		VillagerSetClip(villager, clip, true); // SetAnim(395, 1)
	}
	action.turnsSinceStateChange = 0;
}

/// The REACT_TO_MAGIC_SHIELD part of applying a reaction to the living objects of a cell, for one villager, as the
/// fire's and the teleport's: with no reaction of its own, a reaction score above 0 and not reacted to a shield lately,
/// it starts reacting (the reaction's turn is stamped, then StartReacting -> SetupReactToMagicShield). Replacing a
/// current reaction by a higher scoring one (reactions::MaySwitch) is not ported for the villagers
void ApplyShieldReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	auto& registry = Reg();
	// the check made for every reaction type: IsAvailableForReaction, the common one
	if (!registry.AllOf<Villager, LivingAction>(villager) || villager == reaction.initiator ||
	    !villager::IsAvailableForReaction(villager))
	{
		return;
	}
	if (villager_reactions::IsReacting(villager))
	{
		return; // it already follows a reaction
	}
	const auto& info = ShieldReactionInfo();
	const auto* transform = registry.TryGet<const Transform>(villager);
	const auto* spell = registry.TryGet<const Spell>(reaction.initiator);
	if (transform == nullptr || spell == nullptr)
	{
		return;
	}
	const float distance =
	    0.5f * (std::abs(transform->position.x - spell->position.x) + std::abs(transform->position.z - spell->position.z));
	if (distance > info.maxReactionDistance)
	{
		return;
	}
	// the reaction's score (ECS/Effects/Reactions)
	const auto score = effects::reactions::Score(static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield), true,
	                                             villager_shield::ReactToMagicShieldPriority(villager, reaction.id), distance);
	if (score == 0 || !MayReactAgain(villager, info.numGameTurnsForNormalThingsBeforeReactingAgain))
	{
		return;
	}
	effects::reactions::MarkStarted(reaction.id, effects::reactions::Turn()); // stamps the turn if not yet set
	villager_shield::SetupReactToMagicShield(villager, reaction.initiator, reaction.id);
}
} // namespace

uint8_t villager_shield::ReactToMagicShieldPriority(entt::entity villager, uint32_t reaction)
{
	const auto* found = effects::reactions::Find(reaction);
	if (found == nullptr || !IsAvailableShieldSpell(found->initiator))
	{
		return 0; // not a SpellShield, or not available
	}
	// the original computes the distance to the reaction here and drops it
	const auto town = TownEntityOf(villager);
	if (town == entt::null)
	{
		return static_cast<uint8_t>(ShieldReactionInfo().priority & 0xFFu); // no town: the priority's low byte
	}
	// the town's desire for protection must not be 0
	if (ProtectionDesireSignificance(town) == 0.0f)
	{
		return 0;
	}
	if (!AttackedRecently(town))
	{
		return 0;
	}
	return static_cast<uint8_t>(ShieldReactionInfo().priority & 0xFFu);
}

void villager_shield::SetupReactToMagicShield(entt::entity villager, entt::entity spell, uint32_t reaction)
{
	auto* action = ActionOf(villager);
	if (action == nullptr || !IsAvailableShieldSpell(spell))
	{
		return; // not a SpellShield, nothing happens
	}
	// AddReaction(reaction, 168): the reaction and the spell are kept and the final state stored
	{
		auto& state = StateOf(villager);
		state.spell = spell;
		state.reaction = reaction;
	}
	StorePreviousState(*action);
	villager_reactions::SetTopState(villager, VillagerStates::AmazedByMagicShieldReaction);
	const float radius = RadiusOf(spell);
	const auto shield = ShieldPos(spell);
	const auto me = tq::PosOf(villager);
	// already under the shield (minus 0.2 R) -> no walk
	const glm::vec3 at(tq::ToMetres(me).x, 0.0f, tq::ToMetres(me).y);
	if (!magic::spell_shield::IsUnder(spell, at, 0.2f * radius))
	{
		// the bearing from the shield to the villager, + GameFloatRand(pi / 4) - pi / 8
		const float bearing = tq::Get3DAngleFromXZ(shield, me);
		const float angle = bearing + villager::GameFloatRand(std::numbers::pi_v<float> / 4.0f) - 0.39269909f;
		// (R - 0.2 R) x GameFloatRand(1)^3, taken off that distance
		const float band = radius - 0.2f * radius;
		const float r = villager::GameFloatRand(1.0f);
		const float distance = band - band * (r * r * r);
		// SetupMoveToWithHug(the reaction's pos + GetPosFromAngle(angle, distance), 168)
		const auto goal = tq::ToMetres(shield + tq::GetPosFromAngle(angle, distance));
		villager::SetupMoveToWithHug(villager, goal, VillagerStates::AmazedByMagicShieldReaction);
	}
	// the look-at point, walking or not
	// the state is looked up again: the calls above may have removed another villager's, which moves the storage
	if (auto* state = FindState(villager))
	{
		state->lookAt = LookAtPoint(villager, spell);
	}
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("SetupReactToMagicShield: spell {} r {:.1f} reaction {}",
		                                      static_cast<uint32_t>(spell), radius, reaction));
	}
}

uint32_t villager_shield::AmazedByMagicShieldReaction(LivingAction& action)
{
	auto& registry = Reg();
	const auto villager = registry.ToEntity(action);
	const auto* state = FindState(villager);
	const auto town = TownEntityOf(villager);
	const auto spell = state != nullptr ? state->spell : entt::entity(entt::null);
	// a town, an available SpellShield, and the town's desire for protection above 0
	if (town == entt::null || !IsAvailableShieldSpell(spell) || !(ProtectionDesireSignificance(town) > 0.0f))
	{
		// wait GameRand(60) + 20 turns, then DECIDE_WHAT_TO_DO
		const auto turns = static_cast<uint16_t>(static_cast<float>(villager::GameRand(60)) + 20.0f);
		villager::SetupWaitForCounter(villager, turns, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// turn towards the look-at point; once it is facing that way, one chance in four of a new point
	if (villager::LookAtPos(villager, state->lookAt, 0) != 0 && villager::GameRand(4) == 0)
	{
		// looked up again, as in SetupReactToMagicShield
		if (auto* again = FindState(villager))
		{
			again->lookAt = LookAtPoint(villager, spell);
		}
	}
	UpdateAmazedClip(villager, action);
	return 1;
}

void villager_shield::ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	ApplyShieldReaction(villager, reaction);
}

bool villager_shield::IsReacting(entt::entity villager)
{
	const auto* state = FindState(villager);
	return state != nullptr && state->reaction != 0;
}

entt::entity villager_shield::ReactionObject(entt::entity villager)
{
	const auto* state = FindState(villager);
	return state != nullptr ? state->spell : entt::entity(entt::null);
}

bool villager_shield::IsReactionObjectAvailable(entt::entity villager)
{
	return IsAvailableShieldSpell(ReactionObject(villager));
}

void villager_shield::StopReacting(entt::entity villager)
{
	const auto* state = FindState(villager);
	if (state == nullptr)
	{
		return;
	}
	// with a reaction, its record gets the turn; the reaction and its spell go
	if (state->reaction != 0)
	{
		effects::reactions::RefreshRecord(villager, static_cast<uint8_t>(openblack::Reaction::ReactToMagicShield),
		                                  effects::reactions::Turn());
	}
	Reg().RemoveState<VillagerShieldState>(villager);
}

void villager_shield::Clear()
{
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Reg();
		std::vector<entt::entity> owners;
		registry.Each<const VillagerShieldState>(
		    [&owners](entt::entity villager, const VillagerShieldState&) { owners.push_back(villager); });
		for (const auto villager : owners)
		{
			registry.RemoveState<VillagerShieldState>(villager);
		}
	}
	villager_reactions::Register(); // the Villager handler of ECS/Effects/Reactions
}
