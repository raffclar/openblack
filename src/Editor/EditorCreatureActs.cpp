/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EditorCreatureActs.h"

#include <cmath>

#include <glm/gtx/vec_swizzle.hpp>
#include <glm/vec2.hpp>

#include "Common/DeveloperToolsGate.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/LeashRules.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/CreatureReactionNotice.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "Locator.h"

namespace openblack::editor
{

namespace
{
using creature_edit_acts::Act;
using ecs::components::Creature;
using ecs::components::CreatureMindState;
using ecs::components::CreatureMorph;
using ecs::components::CreatureNeeds;
namespace fight = creature_fight;
namespace leash = creature_leash;
namespace animations = creature_layers::animations;

/// The kinds of thing that the trainers reward, after the first trainer, which is none
constexpr std::array<uint32_t, creature_edit_acts::k_TrainerCount - 1> k_TrainerKinds {
    creature_tree::belief_types::k_Villager, creature_tree::belief_types::k_MobileObject, creature_tree::belief_types::k_Tree,
    creature_tree::belief_types::k_Creature, creature_tree::belief_types::k_Abode,
};

/// The space between the three posts that the leash tab puts down
constexpr float k_PostSpacing = 6.0f;
/// The pot of food that the body tab drops
constexpr auto k_FoodPot = PotInfo::FoodPot;
constexpr int32_t k_FoodAmount = 800;
/// The highest skin and the highest kind and column of a mark that the looks tab gives
constexpr uint8_t k_MaxSkin = 3;
constexpr uint8_t k_MaxMarkKind = 7;
constexpr uint8_t k_MaxMarkColumn = 7;
/// The length of the trail of blood that the looks tab gives, in texels
constexpr int k_BloodTrailLength = 12;

[[nodiscard]] bool IsSwitch(uint8_t arg) noexcept
{
	return arg <= 1;
}

[[nodiscard]] bool IsLeash(uint16_t index) noexcept
{
	return std::ranges::any_of(leash::k_Types, [index](LeashType type) { return static_cast<uint16_t>(type) == index; });
}

[[nodiscard]] bool UsesSwitch(Act act) noexcept
{
	switch (act)
	{
	case Act::PauseMind:
	case Act::TakesNotice:
	case Act::Fainting:
	case Act::GoTo:
	case Act::Leashable:
	case Act::KnowsLeash:
	case Act::LeashWorks:
	case Act::LeashDrawn:
	case Act::StartFight:
	case Act::FightsByItself:
	case Act::Heal:
		return true;
	default:
		return false;
	}
}

/// The limit of the index of each act that has one, from the tables of the game that need no world
[[nodiscard]] std::optional<size_t> IndexLimit(Act act) noexcept
{
	switch (act)
	{
	case Act::PlayAction:
		return animations::k_ActionCount;
	case Act::PlayGesture:
		return animations::k_GestureCount;
	case Act::PullFace:
		return animations::k_FaceCount;
	case Act::ShowFeeling:
		return creature_face::k_CueCount;
	case Act::Trainer:
		return creature_edit_acts::k_TrainerCount;
	case Act::BodyTime:
		return creature_edit_acts::k_TimeScales.size();
	case Act::PutThing:
		return k_PutThings.size();
	case Act::LookOver:
		return creature_object_actions::k_KeepAnimationCount;
	case Act::Owner:
		return static_cast<size_t>(PlayerNames::_COUNT);
	case Act::Blow:
		return 3;
	case Act::FightStep:
		return 4;
	default:
		return std::nullopt;
	}
}

[[nodiscard]] creature_marks::Mark MarkOf(const player_commands::EditorCreatureAct& act, uint8_t type, uint8_t column) noexcept
{
	return {.u = act.args[0], .v = act.args[1], .skin = act.args[2], .age = 0, .type = type, .column = column};
}

/// The acts of the mind and of the body that the mind does
ActResult ApplyMindAct(ecs::Registry& registry, entt::entity creature, const player_commands::EditorCreatureAct& act)
{
	auto& minds = Locator::creatureMindSystem::value();
	auto& mind = registry.Get<CreatureMindState>(creature);
	const auto on = act.args[0] == 1;
	switch (static_cast<Act>(act.act))
	{
	case Act::PauseMind:
		mind.paused = on;
		return {.done = true};
	case Act::TakesNotice:
		ecs::creature_reaction_notice::SetTakesNotice(registry, creature, on);
		return {.done = true};
	case Act::PlayAction:
		return {.done = minds.PlayAction(creature, animations::k_FirstAction + act.index)};
	case Act::PlayGesture:
		return {.done = minds.PlayGesture(creature, animations::k_FirstGesture + act.index)};
	case Act::PullFace:
		minds.PullFace(creature, animations::k_FirstFace + act.index);
		return {.done = true};
	case Act::ShowFeeling:
		return {.done = minds.ShowFeeling(creature, static_cast<creature_face::Cue>(act.index)).has_value()};
	case Act::SitDown:
		return {.done = minds.SitDown(creature)};
	case Act::StandUp:
		minds.StandUp(creature);
		return {.done = true};
	case Act::Feedback:
		minds.ReceiveFeedback(creature, act.amount);
		return {.done = true};
	case Act::ClearLearning:
		minds.ClearLearning(creature);
		return {.done = true};
	case Act::Trainer:
		mind.trainer = TrainerKind(act.index);
		return {.done = true};
	case Act::SeeSkill:
	case Act::SeeMiracle:
	case Act::PlayerDeed:
	{
		const auto* tables = minds.GetTables();
		if (tables == nullptr)
		{
			return {};
		}
		const auto kind = static_cast<Act>(act.act);
		const auto count = kind == Act::SeeSkill     ? tables->skills.size()
		                   : kind == Act::SeeMiracle ? tables->miracles.size()
		                                             : tables->mimics.size();
		if (act.index >= count)
		{
			return {};
		}
		if (kind == Act::SeeSkill)
		{
			minds.SeeSkill(act.position, act.index);
		}
		else if (kind == Act::SeeMiracle)
		{
			minds.SeeMiracle(act.position, act.index);
		}
		else
		{
			minds.PlayerDid(act.index, act.position, std::nullopt, std::nullopt);
		}
		return {.done = true};
	}
	case Act::Sleep:
		return {.done = minds.Sleep(creature)};
	case Act::Wake:
		minds.Wake(creature);
		return {.done = true};
	case Act::Eat:
		return {.done = minds.Eat(creature, std::nullopt)};
	case Act::Drink:
		return {.done = minds.Drink(creature)};
	case Act::Poo:
		return {.done = minds.Poo(creature)};
	case Act::Puke:
		return {.done = minds.Puke(creature)};
	case Act::Faint:
		return {.done = minds.Faint(creature)};
	default:
		return {};
	}
}

/// The acts that set the needs of the body
ActResult ApplyNeedsAct(ecs::Registry& registry, entt::entity creature, Act act)
{
	auto& body = registry.Get<CreatureNeeds>(creature).needs;
	switch (act)
	{
	case Act::FillUp:
		body.energy = 1.0f;
		body.exhaustion = 0.0f;
		body.dehydration = 0.0f;
		body.poo = 0.0f;
		body.life = 1.0f;
		body.warmth = 0.0f;
		break;
	case Act::Starve:
		body.energy = 0.05f;
		break;
	case Act::TireOut:
		body.exhaustion = 0.85f;
		break;
	case Act::Parch:
		body.dehydration = 0.9f;
		break;
	case Act::Bloat:
		body.poo = 0.9f;
		break;
	default:
		return {};
	}
	registry.SetDirty();
	return {.done = true};
}

/// The acts of the movement and of the hands
ActResult ApplyMoveAct(entt::entity creature, const player_commands::EditorCreatureAct& act)
{
	using Pace = ecs::systems::CreatureLocomotionSystemInterface::Pace;
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	auto& hands = Locator::creatureObjectActionSystem::value();
	const auto point = glm::xz(act.position);
	switch (static_cast<Act>(act.act))
	{
	case Act::GoTo:
	{
		const auto result = locomotion.MoveTo(creature, point, act.args[0] == 1 ? Pace::Run : Pace::Walk, 0.0f, 1.0f);
		return {.done = result == ecs::systems::CreatureLocomotionSystemInterface::MoveResult::Started};
	}
	case Act::FleeFrom:
		return {.done = locomotion.FleeFrom(creature, point, Pace::Run) ==
		                ecs::systems::CreatureLocomotionSystemInterface::MoveResult::Started};
	case Act::Face:
		return {.done = locomotion.TurnToFace(creature, point)};
	case Act::StopMoving:
		locomotion.Stop(creature);
		return {.done = true};
	case Act::PickUp:
		return {.done = hands.CanPickUp(creature, act.other) && hands.PickUp(creature, act.other)};
	case Act::KnockDown:
		return {.done = hands.CanDestroy(act.other) && hands.Destroy(creature, act.other)};
	case Act::ThrowAt:
		return {.done = hands.Throw(creature, act.position)};
	case Act::PointAt:
		return {.done = hands.PointAt(creature, act.position)};
	case Act::PutThing:
		ecs::archetypes::MobileObjectArchetype::Create(act.position, k_PutThings.at(act.index), 0.0f, 1.0f);
		return {.done = true};
	case Act::PutDown:
		return {.done = hands.PutDown(creature)};
	case Act::TossAway:
		return {.done = hands.Discard(creature)};
	case Act::EatHeld:
		return {.done = hands.EatHeld(creature)};
	case Act::Drop:
		hands.Drop(creature);
		return {.done = true};
	case Act::LookOver:
		return {.done = hands.Keep(creature, creature_object_actions::k_FirstKeepAnimation + act.index)};
	case Act::StopHands:
		hands.Cancel(creature);
		return {.done = true};
	default:
		return {};
	}
}

/// The acts of the leash
ActResult ApplyLeashAct(ecs::Registry& registry, entt::entity creature, const player_commands::EditorCreatureAct& act)
{
	auto& leashes = Locator::leashSystem::value();
	const auto on = act.args[0] == 1;
	const auto type = static_cast<LeashType>(act.index);
	switch (static_cast<Act>(act.act))
	{
	case Act::Owner:
		leashes.SetOwner(creature, static_cast<PlayerNames>(act.index));
		return {.done = true};
	case Act::Leashable:
		return {.done = leashes.SetLeashable(creature, on)};
	case Act::KnowsLeash:
		leashes.SetKnown(creature, type, on);
		return {.done = true};
	case Act::GrantAllLeashes:
		for (const auto known : leash::k_Types)
		{
			leashes.SetKnown(creature, known, true);
		}
		return {.done = true};
	case Act::PutOnLeash:
		return {.done = leashes.IsLeashed(creature) ? leashes.ChangeType(creature, type) : leashes.PutOn(creature, type)};
	case Act::TakeOffLeash:
		leashes.TakeOff(creature);
		return {.done = true};
	case Act::TieTo:
		return {.done = leashes.HandTie(creature, act.other)};
	case Act::UntieToHand:
		leashes.UntieToHand(creature);
		return {.done = true};
	case Act::LeashWorks:
		leashes.SetWorks(creature, on);
		return {.done = true};
	case Act::KeepAtHome:
		leashes.ConfineToHome(creature, leash::k_HomeConfinement);
		return {.done = true};
	case Act::LetRoam:
		leashes.ClearConfinement(creature);
		return {.done = true};
	case Act::PlacePosts:
	{
		const auto owner = registry.Get<const Creature>(creature).owner;
		const glm::vec3 step {k_PostSpacing, 0.0f, 0.0f};
		leashes.PlacePosts(owner, {act.position - step, act.position, act.position + step});
		return {.done = true};
	}
	default:
		return {};
	}
}

/// The acts of the fight
ActResult ApplyFightAct(entt::entity creature, const player_commands::EditorCreatureAct& act)
{
	using Result = ecs::systems::CreatureFightSystemInterface::StartResult;
	auto& fights = Locator::creatureFightSystem::value();
	const auto on = act.args[0] == 1;
	switch (static_cast<Act>(act.act))
	{
	case Act::BringRound:
		fights.Resurrect(creature);
		return {.done = true};
	case Act::StartFight:
	{
		const auto started = fights.StartFight(creature, act.other) == Result::Started;
		if (started && on)
		{
			fights.SetAutoFighting(creature, true);
		}
		return {.done = started};
	}
	case Act::KnockOut:
		fights.KnockOut(creature);
		return {.done = true};
	case Act::KillForGood:
		fights.KillPermanently(creature);
		return {.done = true};
	case Act::FightsByItself:
		fights.SetAutoFighting(creature, on);
		return {.done = true};
	case Act::AbortFight:
		fights.AbortFight(creature);
		return {.done = true};
	case Act::Blow:
	{
		const auto queued = fights.QueueMove(creature, fight::AttackMove(static_cast<fight::Band>(act.index)), false);
		fights.ReleaseCharge(creature, act.amount);
		return {.done = queued};
	}
	case Act::Block:
		return {.done = fights.QueueMove(creature, fight::BlockMove(), false)};
	case Act::Special:
		return {.done = fights.QueueMove(creature, {.kind = fight::Move::Kind::Special}, false)};
	case Act::FightStep:
		return {.done = fights.QueueMove(creature, fight::StepMove(static_cast<fight::Step>(act.index)), false)};
	default:
		return {};
	}
}

/// The acts of the marks on the skin
ActResult ApplySkinAct(entt::entity creature, const player_commands::EditorCreatureAct& act)
{
	auto& skins = Locator::creatureSkinSystem::value();
	switch (static_cast<Act>(act.act))
	{
	case Act::AddWound:
		skins.AddWound(creature, MarkOf(act, act.args[3], act.args[4]));
		return {.done = true};
	case Act::AddBlood:
	{
		// The trail goes down the skin from the point, to the edge of the skin at most
		auto drop = MarkOf(act, 0, 0);
		for (int i = 0; i < k_BloodTrailLength && drop.v < 255; ++i, ++drop.v)
		{
			skins.AddBlood(creature, drop);
		}
		return {.done = true};
	}
	case Act::Heal:
		skins.Heal(creature, act.args[0] == 1 ? creature_marks::k_HealEffectCounts : creature_marks::k_CountsPerStep);
		return {.done = true};
	default:
		return {};
	}
}

/// The acts on every creature, which change the rules of the game
ActResult ApplyEveryCreatureAct(const player_commands::EditorCreatureAct& act)
{
	const auto on = act.args[0] == 1;
	switch (static_cast<Act>(act.act))
	{
	case Act::BodyTime:
		if (!Locator::creaturePhysiologySystem::has_value())
		{
			return {};
		}
		Locator::creaturePhysiologySystem::value().SetTimeScale(creature_edit_acts::k_TimeScales.at(act.index));
		return {.done = true};
	case Act::Fainting:
		if (!Locator::creaturePhysiologySystem::has_value())
		{
			return {};
		}
		Locator::creaturePhysiologySystem::value().SetFaintingEnabled(on);
		return {.done = true};
	case Act::LeashDrawn:
		if (!Locator::leashSystem::has_value())
		{
			return {};
		}
		Locator::leashSystem::value().SetDrawn(on);
		return {.done = true};
	default:
		return {};
	}
}

/// Whether the systems that the act needs are there
[[nodiscard]] bool HasSystemsFor(Act act) noexcept
{
	const auto index = static_cast<size_t>(act);
	if (index < static_cast<size_t>(Act::BodyTime))
	{
		return Locator::creatureMindSystem::has_value();
	}
	switch (act)
	{
	case Act::Sleep:
	case Act::Wake:
	case Act::Eat:
	case Act::Drink:
	case Act::Poo:
	case Act::Puke:
	case Act::Faint:
		return Locator::creatureMindSystem::has_value();
	case Act::FillUp:
	case Act::Starve:
	case Act::TireOut:
	case Act::Parch:
	case Act::Bloat:
	case Act::ShowFatnessNow:
	case Act::DropFood:
		return true;
	default:
		break;
	}
	if (index <= static_cast<size_t>(Act::StopHands))
	{
		return Locator::creatureLocomotionSystem::has_value() && Locator::creatureObjectActionSystem::has_value();
	}
	if (index <= static_cast<size_t>(Act::PlacePosts))
	{
		return Locator::leashSystem::has_value();
	}
	if (index <= static_cast<size_t>(Act::FightStep))
	{
		return Locator::creatureFightSystem::has_value();
	}
	return Locator::creatureSkinSystem::has_value();
}

} // namespace

std::optional<uint32_t> TrainerKind(size_t trainer) noexcept
{
	if (trainer == 0 || trainer > k_TrainerKinds.size())
	{
		return std::nullopt;
	}
	return k_TrainerKinds.at(trainer - 1);
}

std::optional<size_t> TrainerOf(std::optional<uint32_t> kind) noexcept
{
	if (!kind.has_value())
	{
		return 0;
	}
	const auto found = std::ranges::find(k_TrainerKinds, *kind);
	if (found == k_TrainerKinds.end())
	{
		return std::nullopt;
	}
	return static_cast<size_t>(found - k_TrainerKinds.begin()) + 1;
}

std::optional<std::string_view> ActRefusal(const player_commands::EditorCreatureAct& act) noexcept
{
	const auto known = creature_edit_acts::ActFrom(act.act);
	if (!known.has_value())
	{
		return "the act is not known";
	}
	const auto kind = *known;
	if (!std::isfinite(act.amount) || !std::isfinite(act.position.x) || !std::isfinite(act.position.y) ||
	    !std::isfinite(act.position.z))
	{
		return "a number of the act is not finite";
	}
	if (UsesSwitch(kind) && !IsSwitch(act.args[0]))
	{
		return "the switch is not on or off";
	}
	if (const auto limit = IndexLimit(kind); limit.has_value() && act.index >= *limit)
	{
		return "the index is out of its list";
	}
	if ((kind == Act::KnowsLeash || kind == Act::PutOnLeash) && !IsLeash(act.index))
	{
		return "the leash is not known";
	}
	if (kind == Act::Feedback && (act.amount < -1.0f || act.amount > 1.0f))
	{
		return "the feedback is out of its limits";
	}
	if (kind == Act::Blow && (act.amount < 0.0f || act.amount > fight::k_MaxChargeMs))
	{
		return "the charge is out of its limits";
	}
	if ((kind == Act::AddWound || kind == Act::AddBlood) && act.args[2] > k_MaxSkin)
	{
		return "the skin is not known";
	}
	if (kind == Act::AddWound && (act.args[3] > k_MaxMarkKind || act.args[4] > k_MaxMarkColumn))
	{
		return "the mark is not known";
	}
	return std::nullopt;
}

ActResult ApplyCreatureAct(ecs::Registry& registry, const player_commands::EditorCreatureAct& act)
{
	if (ActRefusal(act).has_value())
	{
		return {};
	}
	const auto kind = static_cast<Act>(act.act);
	if (creature_edit_acts::ActsOnEveryCreature(kind))
	{
		return ApplyEveryCreatureAct(act);
	}
	const auto creature = act.creature;
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureNeeds, CreatureMindState>(creature) ||
	    !HasSystemsFor(kind))
	{
		return {};
	}
	// The other thing of an act must be in the world, and not the creature itself
	const bool needsOther = kind == Act::PickUp || kind == Act::KnockDown || kind == Act::TieTo || kind == Act::StartFight;
	if (needsOther && (act.other == creature || !registry.Valid(act.other)))
	{
		return {};
	}
	const auto index = static_cast<size_t>(kind);
	if (index < static_cast<size_t>(Act::BodyTime))
	{
		return ApplyMindAct(registry, creature, act);
	}
	switch (kind)
	{
	case Act::FillUp:
	case Act::Starve:
	case Act::TireOut:
	case Act::Parch:
	case Act::Bloat:
		return ApplyNeedsAct(registry, creature, kind);
	case Act::ShowFatnessNow:
		if (auto* morph = registry.TryGet<CreatureMorph>(creature))
		{
			morph->shownFatness = registry.Get<const Creature>(creature).fatness;
			return {.done = true};
		}
		return {};
	case Act::Sleep:
	case Act::Wake:
	case Act::Eat:
	case Act::Drink:
	case Act::Poo:
	case Act::Puke:
	case Act::Faint:
		return ApplyMindAct(registry, creature, act);
	case Act::DropFood:
		ecs::archetypes::PotArchetype::Create(act.position, 0.0f, k_FoodPot, k_FoodAmount);
		return {.done = true};
	default:
		break;
	}
	if (index <= static_cast<size_t>(Act::StopHands))
	{
		return ApplyMoveAct(creature, act);
	}
	if (index <= static_cast<size_t>(Act::PlacePosts))
	{
		return ApplyLeashAct(registry, creature, act);
	}
	if (index <= static_cast<size_t>(Act::FightStep))
	{
		return ApplyFightAct(creature, act);
	}
	return ApplySkinAct(creature, act);
}

std::optional<ActResult> DoCreatureAct(const player_commands::EditorCreatureAct& act)
{
	if (developer_tools::NetworkSessionLive())
	{
		if (Locator::playerCommandSystem::has_value())
		{
			Locator::playerCommandSystem::value().Submit(act);
		}
		return std::nullopt;
	}
	if (!Locator::entitiesRegistry::has_value())
	{
		return ActResult {};
	}
	return ApplyCreatureAct(Locator::entitiesRegistry::value(), act);
}

} // namespace openblack::editor
