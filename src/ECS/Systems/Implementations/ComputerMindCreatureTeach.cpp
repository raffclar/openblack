/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "ComputerMindCommon.h"
#include "ComputerMindCreature.h"
#include "ComputerMindMiracles.h"
#include "ComputerMindRuleTable.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerHandSteps.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindCreatureRules.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFind.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;
using computer_hand::Argument;
using computer_hand::StepKind;
using computer_mind::MindDomain;
using computer_mind::NodeKind;
using computer_mind_rules::LessonFacts;
using computer_mind_rules::LessonKind;

namespace
{

[[nodiscard]] bool IsCreature(entt::entity thing)
{
	const auto& registry = Entities();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<Creature>(thing);
}

/// The thing when it is the icon of a miracle at a worship site, else no thing
[[nodiscard]] entt::entity IconOrNone(entt::entity thing)
{
	const auto& registry = Entities();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<WorshipIcon>(thing) ? thing : entt::null;
}

[[nodiscard]] bool Knows(entt::entity creature, CreatureActionLearningType type, size_t action)
{
	return Locator::creatureMindSystem::has_value() &&
	       Locator::creatureMindSystem::value().KnowsAction(creature, type, static_cast<uint32_t>(action));
}

/// What the god knows of each ordinary skill of the game's table for the creature
[[nodiscard]] std::vector<LessonFacts> SkillFacts(entt::entity creature)
{
	std::vector<LessonFacts> facts;
	if (!Locator::infoConstants::has_value())
	{
		return facts;
	}
	const auto& skills = Locator::infoConstants::value().creatireActionKnownAboutEntry;
	for (size_t skill = 0; skill < skills.size(); ++skill)
	{
		facts.push_back({.teachable = skills.at(skill).field0x54 != 0,
		                 .known = Knows(creature, CreatureActionLearningType::Normal, skill)});
	}
	return facts;
}

/// What the god knows of each miracle of the game's table for the creature, in the order of the magic types
[[nodiscard]] std::vector<LessonFacts> MiracleFacts(entt::entity creature)
{
	std::vector<LessonFacts> facts;
	if (!Locator::infoConstants::has_value())
	{
		return facts;
	}
	const auto& info = Locator::infoConstants::value();
	const auto& miracles = info.creatureMagicActionKnownAboutEntry;
	for (size_t miracle = 0; miracle < miracles.size(); ++miracle)
	{
		const auto type = static_cast<MagicType>(miracle);
		const auto seed = magic::GetMagicInfo(info, type).spellSeedType;
		// A miracle with no seed reads an empty record before the table of seeds, so its seed is not a real one
		const bool seedInTable = seed != SpellSeedType::None && static_cast<size_t>(seed) < info.spellSeed.size();
		facts.push_back({.teachable = miracles.at(miracle).field0x54 != 0,
		                 .known = Knows(creature, CreatureActionLearningType::Magic, miracle),
		                 .perceived = magic::GetMagicEffectInfo(info, type).perceivedPlayerDesire.at(0),
		                 .seedExists = seedInTable && magic::GetSpellSeedInfo(info, seed).exists != 0});
	}
	return facts;
}

/// The god teaches the creature at once, and the player sees the creature's lesson
void Learn(entt::entity creature, CreatureActionLearningType type, size_t action)
{
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().LearnAction(creature, type, static_cast<uint32_t>(action));
	}
}

/// The nearest totem of a town round the creature, as the game finds a thing of a type near a place
[[nodiscard]] entt::entity NearestTotem(const map_coords::MapCoords& from)
{
	auto& registry = Entities();
	std::vector<ecs::script_find::Candidate> totems;
	registry.Each<const VillageTotem, const Transform>(
	    [&totems](entt::entity totem, const VillageTotem& /*unused*/, const Transform& transform) {
		    totems.push_back(
		        {.entity = totem, .at = map_coords::FromMetres(glm::vec2(transform.position.x, transform.position.z))});
	    });
	return ecs::script_find::FindNearest(from, computer_mind_rules::k_LessonTotemReach, [&totems](glm::ivec2 cell) {
		std::vector<ecs::script_find::Candidate> found;
		for (const auto& totem : totems)
		{
			if (map_coords::SignedCellOf(totem.at.x) == cell.x && map_coords::SignedCellOf(totem.at.z) == cell.y)
			{
				found.push_back(totem);
			}
		}
		return found;
	});
}

/// The god wants to teach its creature an ordinary skill while the creature does not know a skill that the god teaches.
/// The creature becomes the object of the node.
float SkillLessonWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto creature = CreatureOf(context.player);
	if (creature == entt::null || !computer_mind_rules::FirstSkillToTeach(SkillFacts(creature)).has_value())
	{
		return 0.0f;
	}
	mind.nodes[node].object = creature;
	return computer_mind_rules::k_SkillLessonWish;
}

/// The god wants to teach its creature a kind of miracle as much as the times that it sent the creature while the
/// creature did not know that kind. The creature becomes the object of the node.
float MiracleLessonWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node, CreatureDesires kind)
{
	const auto creature = CreatureOf(context.player);
	if (creature == entt::null)
	{
		return 0.0f;
	}
	const float wish = computer_mind_rules::MiracleLessonWish(mind.untaughtSends.at(static_cast<size_t>(kind)));
	mind.nodes[node].object = creature;
	return wish;
}

float AggressiveLessonWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return MiracleLessonWish(context, mind, node, CreatureDesires::Anger);
}

float CompassionateLessonWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return MiracleLessonWish(context, mind, node, CreatureDesires::Compassion);
}

float ImpressiveLessonWish(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return MiracleLessonWish(context, mind, node, CreatureDesires::ToImpress);
}

/// The hand goes over the creature
void HandOverCreature(RuleContext& context, entt::entity creature)
{
	const auto at = computer_hand::steps::PlaceOf(context.hand, *CoordsOfThing(creature));
	computer_hand::Add(context.state.hand, StepKind::MoveHandToPos, {Argument {.place = at}});
}

/// The creature looks at the camera of its god
void LookAtGod(RuleContext& context, entt::entity creature)
{
	computer_hand::Add(context.state.hand, StepKind::ForceCreatureToPerformAction,
	                   {Argument {.object = creature},
	                    Argument {.number = static_cast<int32_t>(computer_mind_rules::k_LessonLookAtGodAction)},
	                    Argument {.object = creature}});
}

/// The lesson of the totem: the creature looks at the nearest totem, and the god raises the totem. False when no totem
/// is near.
bool TotemLesson(RuleContext& context, entt::entity creature)
{
	const auto totem = NearestTotem(*CoordsOfThing(creature));
	if (totem == entt::null)
	{
		return false;
	}
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::ForceCreatureToPerformAction,
	                   {Argument {.object = creature},
	                    Argument {.number = static_cast<int32_t>(computer_mind_rules::k_LessonLookAtTotemAction)},
	                    Argument {.object = totem}});
	computer_hand::Add(hand, StepKind::RaiseTotem,
	                   {Argument {.object = totem}, Argument {.value = computer_mind_rules::k_LessonTotemShare}});
	return true;
}

/// The god teaches the creature the first ordinary skill that it does not know. The hand goes over the creature, and
/// the creature looks at the god. The creature then knows the skill. Only the skill of the totem has a lesson after that.
/// When it has no lesson, or no totem is near, the action cannot start, but the creature still knows the skill.
bool SkillLesson(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto creature = ObjectOf(mind, node);
	if (!IsCreature(creature) || !CoordsOfThing(creature).has_value())
	{
		return false;
	}
	const auto skill = computer_mind_rules::FirstSkillToTeach(SkillFacts(creature));
	if (!skill.has_value())
	{
		return false;
	}
	HandOverCreature(context, creature);
	LookAtGod(context, creature);
	Learn(creature, CreatureActionLearningType::Normal, *skill);
	return *skill == computer_mind_rules::k_TotemSkill && TotemLesson(context, creature);
}

/// The god teaches the creature the first kind miracle that it does not know. The god does not check that its object is
/// an icon. The hand goes over the creature, and the creature knows the miracle at once. Then the hand goes to the icon,
/// the creature looks at the god, and the hand goes back over the creature. The worshippers chant at the icon for the
/// miracle that the creature learnt, and the hand casts that miracle on the creature.
bool CompassionateLesson(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto creature = ObjectAboveOf(mind, node);
	const auto icon = IconOrNone(ObjectOf(mind, node));
	if (!IsCreature(creature) || !CoordsOfThing(creature).has_value())
	{
		return false;
	}
	const auto miracle = computer_mind_rules::FirstMiracleToTeach(MiracleFacts(creature), LessonKind::Compassionate);
	if (!miracle.has_value())
	{
		return false;
	}
	mind.untaughtSends.at(static_cast<size_t>(CreatureDesires::Compassion)) = 0;
	HandOverCreature(context, creature);
	Learn(creature, CreatureActionLearningType::Magic, *miracle);
	const auto type = static_cast<int32_t>(*miracle);
	auto& hand = context.state.hand;
	computer_hand::Add(hand, StepKind::MoveHandToObject, {Argument {.object = icon}});
	LookAtGod(context, creature);
	computer_hand::Add(hand, StepKind::MoveHandToObject, {Argument {.object = creature}});
	computer_hand::Add(hand, StepKind::MakeWorshippersChantForSpell, {Argument {.number = type}, Argument {.object = icon}});
	computer_hand::Add(hand, StepKind::CastSpellAtObject,
	                   {Argument {.object = creature}, Argument {.number = type}, Argument {.object = icon}});
	return true;
}

} // namespace

void computer_world::RegisterCreatureTeachRules(RuleTable& table)
{
	constexpr auto k_Play = MindDomain::CreaturePlay;
	AddRule(table, k_Play, NodeKind::ActionTeachCreatureNormalAction, {.intensity = SkillLessonWish, .perform = SkillLesson});
	AddRule(table, k_Play, NodeKind::ActionTeachCreatureAggressiveMagic, {.intensity = AggressiveLessonWish});
	AddRule(table, k_Play, NodeKind::ActionTeachCreatureCompassionateMagic, {.intensity = CompassionateLessonWish});
	AddRule(table, k_Play, NodeKind::ActionTeachCreatureImpressiveMagic, {.intensity = ImpressiveLessonWish});
	// Under the lesson, the angry miracle aims at the god's own creature, so it is always too near. The town above the
	// impressive miracle is never a town. Neither object starts an action, so they have no perform rule.
	AddRule(table, k_Play, NodeKind::ObjectTeachCreatureAggressiveMagic, {.intensity = AggressiveSpell});
	AddRule(table, k_Play, NodeKind::ObjectTeachCreatureCompassionateMagic,
	        {.intensity = CompassionateSpell, .perform = CompassionateLesson});
	AddRule(table, k_Play, NodeKind::ObjectTeachCreatureImpressiveMagic, {.intensity = ImpressiveSpell});
}
