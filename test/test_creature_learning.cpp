/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureWatching.h"
#include "Creature/PerceivedDesires.h"

using namespace openblack;
using creature_desires::Desire;
using creature_tree::Attribute;

namespace
{
creature_tree::Belief Thing(uint32_t type, uint32_t animate)
{
	creature_tree::Belief belief {.type = type};
	belief.Set(Attribute::Type, type);
	belief.Set(Attribute::Animate, animate);
	belief.Set(Attribute::Allegiance, 1);
	return belief;
}

constexpr uint32_t k_Villager = creature_tree::belief_types::k_Villager;
constexpr uint32_t k_Food = creature_tree::belief_types::k_Other;
const std::array k_HungerAttributes {Attribute::Allegiance, Attribute::Animate, Attribute::Type};

creature_desires::Desires TwoDesires()
{
	creature_desires::Desires desires;
	for (auto& desire : desires.desires)
	{
		desire.activated = false;
	}
	auto& hunger = desires[Desire::Hunger];
	hunger.activated = true;
	hunger.value = 0.5f;
	hunger.max = 2.0f;
	hunger.decay = 0.9f;
	hunger.increaseSeconds = 20.0f;
	hunger.sources = {{.type = 14, .value = 0.8f, .threshold = 0.5f}, {.type = 15, .value = 0.1f, .threshold = 0.5f}};
	auto& anger = desires[Desire::Anger];
	anger.activated = true;
	anger.value = 0.2f;
	anger.max = 0.8f;
	anger.increaseSeconds = 5.0f;
	return desires;
}
} // namespace

// Decision trees

TEST(CreatureDecisionTree, BucketsFeedbackByTheFirstStepWithinAQuarter)
{
	EXPECT_EQ(creature_tree::BucketOf(-1.0f), 0u);
	EXPECT_EQ(creature_tree::BucketOf(1.0f), 9u);
	EXPECT_EQ(creature_tree::BucketOf(0.0f), 4u);
	EXPECT_EQ(creature_tree::BucketOf(0.5f), 7u);
}

TEST(CreatureDecisionTree, EntropyOfAgreeingExamplesIsZero)
{
	const std::vector<creature_tree::Episode> same {{.belief = Thing(k_Food, 0), .feedback = 1.0f},
	                                                {.belief = Thing(k_Food, 0), .feedback = 1.0f}};
	EXPECT_FLOAT_EQ(creature_tree::Entropy(same), 0.0f);
	const std::vector<creature_tree::Episode> mixed {{.belief = Thing(k_Food, 0), .feedback = 1.0f},
	                                                 {.belief = Thing(k_Villager, 1), .feedback = -1.0f}};
	// One bit for the signs, one for the steps
	EXPECT_NEAR(creature_tree::Entropy(mixed), 1.0f, 1e-5f);
}

TEST(CreatureDecisionTree, GainPicksTheAttributeThatSeparatesFeedback)
{
	std::vector<creature_tree::Episode> episodes;
	for (int i = 0; i < 4; ++i)
	{
		episodes.push_back({.belief = Thing(k_Food, 0), .feedback = -1.0f});
		episodes.push_back({.belief = Thing(k_Villager, 1), .feedback = 1.0f});
	}
	EXPECT_GT(creature_tree::Gain(episodes, Attribute::Type), 0.0f);
	EXPECT_FLOAT_EQ(creature_tree::Gain(episodes, Attribute::Allegiance), 0.0f);
	// An example without the attribute gives no gain
	episodes.push_back({.belief = {.type = k_Food}, .feedback = 1.0f});
	EXPECT_FLOAT_EQ(creature_tree::Gain(episodes, Attribute::Type), 0.0f);
}

TEST(CreatureDecisionTree, BuildsAndEvaluates)
{
	std::vector<creature_tree::Episode> episodes {{.belief = Thing(k_Food, 0), .feedback = -1.0f},
	                                              {.belief = Thing(k_Villager, 1), .feedback = 1.0f},
	                                              {.belief = Thing(k_Food, 0), .feedback = -1.0f}};
	const auto tree = creature_tree::Build(episodes, k_HungerAttributes);
	ASSERT_FALSE(tree.nodes.empty());
	EXPECT_TRUE(tree.nodes[0].test.has_value());
	EXPECT_FLOAT_EQ(creature_tree::Evaluate(tree, Thing(k_Food, 0)), -1.0f);
	EXPECT_FLOAT_EQ(creature_tree::Evaluate(tree, Thing(k_Villager, 1)), 0.8f);
	// A thing whose values no example had is neither good nor bad
	EXPECT_FLOAT_EQ(creature_tree::Evaluate(tree, Thing(creature_tree::belief_types::k_Tree, 2)), 0.0f);
	EXPECT_FALSE(creature_tree::Describe(tree).empty());
}

TEST(CreatureDecisionTree, TheLeafAThingReachesIsBelowTheRootOnceTheTreeSplits)
{
	std::vector<creature_tree::Episode> agreeing {{.belief = Thing(k_Food, 0), .feedback = 1.0f},
	                                              {.belief = Thing(k_Villager, 1), .feedback = 1.0f}};
	// Examples that agree leave the tree a single leaf: the root
	EXPECT_EQ(creature_tree::LeafOf(creature_tree::Build(agreeing, k_HungerAttributes), Thing(k_Food, 0)), 0u);
	std::vector<creature_tree::Episode> split {{.belief = Thing(k_Food, 0), .feedback = -1.0f},
	                                           {.belief = Thing(k_Villager, 1), .feedback = 1.0f}};
	const auto tree = creature_tree::Build(split, k_HungerAttributes);
	const auto leaf = creature_tree::LeafOf(tree, Thing(k_Villager, 1));
	ASSERT_TRUE(leaf.has_value());
	EXPECT_NE(*leaf, 0u);
	// No branch for it, or no tree at all: no leaf
	EXPECT_FALSE(creature_tree::LeafOf(tree, Thing(creature_tree::belief_types::k_Tree, 2)).has_value());
	EXPECT_FALSE(creature_tree::LeafOf(creature_tree::Build({}, k_HungerAttributes), Thing(k_Food, 0)).has_value());
}

TEST(CreatureDecisionTree, EmptyTreeKnowsNothing)
{
	const auto tree = creature_tree::Build({}, k_HungerAttributes);
	EXPECT_FLOAT_EQ(creature_tree::Evaluate(tree, Thing(k_Food, 0)), 0.0f);
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(0.0f), 0.1f);
}

TEST(CreatureDecisionTree, UsefulnessFromUtility)
{
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(-1.0f), 0.0f);
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(-0.5f), 0.05f);
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(0.02f), 0.118f);
	// Capped at an eighth: anything a little better than unknown is as useful as the best
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(0.5f), 0.125f);
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(1.0f), 0.125f);
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(-2.0f), 0.0f);
}

TEST(CreatureDecisionTree, KeepsTheOldestFifteenExamplesAndTheLatest)
{
	std::vector<creature_tree::Episode> episodes;
	for (int i = 0; i < 20; ++i)
	{
		creature_tree::AddEpisode(episodes, {.belief = Thing(k_Food, 0), .feedback = static_cast<float>(i) / 20.0f});
	}
	ASSERT_EQ(episodes.size(), creature_tree::k_MaxEpisodes);
	// Once full, each new example takes the place of the one before it
	EXPECT_FLOAT_EQ(episodes.back().feedback, 19.0f / 20.0f);
	EXPECT_FLOAT_EQ(episodes.front().feedback, 0.0f);
	EXPECT_FLOAT_EQ(episodes.at(creature_tree::k_MaxEpisodes - 2).feedback, 14.0f / 20.0f);
}

TEST(CreatureDecisionTree, SlotsFollowTheKindOfThing)
{
	EXPECT_EQ(creature_tree::SlotsFor(creature_tree::belief_types::k_Villager).size(), 11u);
	EXPECT_EQ(creature_tree::SlotsFor(creature_tree::belief_types::k_Forest).size(), 8u);
	EXPECT_EQ(creature_tree::SlotsFor(creature_tree::belief_types::k_Animal).size(), 7u);
	const std::vector<uint32_t> slots {1, 0, 1, 7, 0, 0, 6, 1, 16, 1, 0};
	const auto belief = creature_tree::FromSlots(k_Villager, slots);
	EXPECT_EQ(belief.Value(Attribute::Sex), 1u);
	EXPECT_EQ(belief.Value(Attribute::VillagerJob), 16u);
	EXPECT_EQ(creature_tree::ToSlots(belief), slots);
}

// A belief keeps the kind, origin and player number of the first sight, and takes the allegiance and life of now
TEST(CreatureDecisionTree, ABeliefKeepsWhatDoesNotChangeFromTheFirstSight)
{
	creature_tree::Belief first {.type = creature_tree::belief_types::k_Villager};
	first.Set(Attribute::PlayerNumber, 1);
	first.Set(Attribute::Origin, 0);
	first.Set(Attribute::Allegiance, 1);
	first.Set(Attribute::Life, 1);
	first.Set(Attribute::VillagerJob, 3);
	creature_tree::Belief now {.type = creature_tree::belief_types::k_Abode};
	now.Set(Attribute::PlayerNumber, 2);
	now.Set(Attribute::Origin, 1);
	now.Set(Attribute::Allegiance, 0);
	now.Set(Attribute::Life, 0);
	now.Set(Attribute::AbodeType, 4);
	const auto kept = creature_tree::WithFirstSight(first, now);
	EXPECT_EQ(kept.type, creature_tree::belief_types::k_Villager);
	EXPECT_EQ(kept.Value(Attribute::PlayerNumber), 1u);
	EXPECT_EQ(kept.Value(Attribute::Origin), 0u);
	EXPECT_EQ(kept.Value(Attribute::VillagerJob), 3u);
	EXPECT_EQ(kept.Value(Attribute::Allegiance), 0u);
	EXPECT_EQ(kept.Value(Attribute::Life), 0u);
	// Only the attributes of the first sight are in the belief
	EXPECT_FALSE(kept.Value(Attribute::AbodeType).has_value());
	EXPECT_TRUE(creature_tree::KeptFromFirstSight(Attribute::Type));
	EXPECT_FALSE(creature_tree::KeptFromFirstSight(Attribute::HarderThanMe));
}

TEST(CreatureDecisionTree, EveryThingIsFriendlyOnlyWhenItHasTheCreaturesPlayer)
{
	namespace types = creature_tree::belief_types;
	// A rock of the neutral player, seen by the first player's creature
	const auto rock = creature_tree::CommonBelief({.type = types::k_Rock, .player = 7, .creaturesPlayer = 0});
	EXPECT_EQ(rock.type, types::k_Rock);
	EXPECT_EQ(rock.Value(Attribute::Type), types::k_Rock);
	EXPECT_EQ(rock.Value(Attribute::Allegiance), creature_tree::allegiance::k_Enemy);
	EXPECT_EQ(rock.Value(Attribute::PlayerNumber), 7u);
	EXPECT_EQ(rock.Value(Attribute::Origin), creature_tree::origin::k_Natural);
	EXPECT_EQ(rock.Value(Attribute::Animate), 0u);
	// A villager of the creature's own town, and one of no town
	const auto own =
	    creature_tree::CommonBelief({.type = types::k_Villager, .animate = true, .player = 0, .creaturesPlayer = 0});
	EXPECT_EQ(own.Value(Attribute::Allegiance), creature_tree::allegiance::k_Friendly);
	EXPECT_EQ(own.Value(Attribute::PlayerNumber), 0u);
	EXPECT_EQ(own.Value(Attribute::Animate), 1u);
	const auto homeless = creature_tree::CommonBelief({.type = types::k_Villager, .animate = true, .creaturesPlayer = 0});
	EXPECT_EQ(homeless.Value(Attribute::Allegiance), creature_tree::allegiance::k_Enemy);
	EXPECT_EQ(homeless.Value(Attribute::PlayerNumber), creature_tree::k_NoPlayerNumber);
	// Another player's creature, taller than the player's own
	const auto other = creature_tree::CommonBelief({.type = types::k_Creature,
	                                                .animate = true,
	                                                .player = 1,
	                                                .creaturesPlayer = 0,
	                                                .harderThanMe = true,
	                                                .creatureType = 4});
	EXPECT_EQ(other.Value(Attribute::Allegiance), creature_tree::allegiance::k_Enemy);
	EXPECT_EQ(other.Value(Attribute::HarderThanMe), 1u);
	EXPECT_EQ(other.Value(Attribute::CreatureType), 4u);
	EXPECT_EQ(creature_tree::ToSlots(other).size(), 11u);
}

TEST(CreatureDecisionTree, ForestsAndCreaturesAreSortedBySize)
{
	EXPECT_EQ(creature_tree::ForestSize(0), 0u);
	EXPECT_EQ(creature_tree::ForestSize(9), 0u);
	EXPECT_EQ(creature_tree::ForestSize(10), 1u);
	EXPECT_EQ(creature_tree::ForestSize(19), 1u);
	EXPECT_EQ(creature_tree::ForestSize(20), 2u);
	EXPECT_EQ(creature_tree::CreatureHeight(0.2f), 0u);
	EXPECT_EQ(creature_tree::CreatureHeight(0.25f), 1u);
	EXPECT_EQ(creature_tree::CreatureHeight(0.74f), 1u);
	EXPECT_EQ(creature_tree::CreatureHeight(0.75f), 2u);
	EXPECT_EQ(creature_tree::CreatureHeight(1.25f), 3u);
	EXPECT_EQ(creature_tree::CreatureHeight(2.0f), 3u);
}

TEST(CreatureDecisionTree, BuildingsAndVillagersShowTheirLifeAndWhetherTheyAreBuilt)
{
	EXPECT_EQ(creature_tree::LifeValue(0.0f), 0u);
	EXPECT_EQ(creature_tree::LifeValue(0.2f), 0u);
	EXPECT_EQ(creature_tree::LifeValue(0.21f), 1u);
	EXPECT_EQ(creature_tree::LifeValue(1.0f), 1u);
	EXPECT_EQ(creature_tree::BeingBuiltValue(false), 0u);
	EXPECT_EQ(creature_tree::BeingBuiltValue(true), 1u);
}

TEST(CreatureDecisionTree, AnotherCreatureShowsItsDesireItsMiraclesAndWhatItCarries)
{
	EXPECT_EQ(creature_tree::DominantDesireValue(16), 16u);
	EXPECT_EQ(creature_tree::DominantDesireValue(40), 40u);
	EXPECT_EQ(creature_tree::DominantDesireValue(41), 40u);
	EXPECT_EQ(creature_tree::SpellKnowledgeValue(0), 0u);
	EXPECT_EQ(creature_tree::SpellKnowledgeValue(5), 5u);
	EXPECT_EQ(creature_tree::SpellKnowledgeValue(50), 42u);
	EXPECT_EQ(creature_tree::CarryingValue(std::nullopt), creature_tree::k_CarriesNothing);
	EXPECT_EQ(creature_tree::CarryingValue(2), 2u);
	EXPECT_EQ(creature_tree::CarryingValue(45), 44u);
}

// The planner

TEST(CreaturePlanner, DistancePriority)
{
	EXPECT_FLOAT_EQ(creature_planner::DistancePriority(true, 500.0f, 0.4f), 1.6f);
	EXPECT_FLOAT_EQ(creature_planner::DistancePriority(false, 0.0f, 0.4f), 1.0f);
	EXPECT_FLOAT_EQ(creature_planner::DistancePriority(false, 100.0f, 0.4f), 0.8f);
	EXPECT_FLOAT_EQ(creature_planner::DistancePriority(false, 900.0f, 0.4f), 0.6f);
}

TEST(CreaturePlanner, ActionPriorityAndNovelty)
{
	EXPECT_FLOAT_EQ(creature_planner::ActionPriority(-1.0f), 0.5f);
	EXPECT_FLOAT_EQ(creature_planner::ActionPriority(1.0f), 0.505f);
	EXPECT_FLOAT_EQ(creature_planner::ActionPriority(1.0f, true), 0.0f);
	EXPECT_FLOAT_EQ(creature_planner::Novelty(0, false), 0.0f);
	EXPECT_FLOAT_EQ(creature_planner::Novelty(36000, false), 0.01f);
	EXPECT_FLOAT_EQ(creature_planner::Novelty(900, false), 0.01f);
	EXPECT_NEAR(creature_planner::Novelty(36000, true), 0.01f, 1e-6f);
	EXPECT_NEAR(creature_planner::Novelty(900, true), 4.0f * 0.025f * 0.025f, 1e-6f);
}

TEST(CreaturePlanner, PriorityAndSwitching)
{
	// An action's priority counts for at most 0.01, so every action that can be taken counts the same
	EXPECT_FLOAT_EQ(creature_planner::Priority(1.0f, std::nullopt, 0.5f, 0.1f, std::nullopt), 1.0f);
	EXPECT_FLOAT_EQ(creature_planner::Priority(1.0f, std::nullopt, 0.505f, 0.1f, std::nullopt), 1.0f);
	EXPECT_FLOAT_EQ(creature_planner::Priority(1.0f, 1.0f, 0.5f, 0.1f, std::nullopt), 10.0f);
	EXPECT_FLOAT_EQ(creature_planner::Priority(0.5f, 0.8f, 0.5f, 0.1f, std::nullopt), 4.0f);
	EXPECT_FLOAT_EQ(creature_planner::Priority(1.0f, std::nullopt, 0.005f, 0.1f, std::nullopt), 0.5f);
	EXPECT_FLOAT_EQ(creature_planner::Priority(1.0f, std::nullopt, 0.0f, 0.1f, std::nullopt), 0.0f);
	EXPECT_TRUE(creature_planner::ShouldSwitch(21.0f, 10.0f));
	EXPECT_FALSE(creature_planner::ShouldSwitch(20.0f, 10.0f));
	EXPECT_TRUE(creature_planner::ShouldSwitch(1.0f, 0.0f));
}

TEST(CreaturePlanner, AnAgendaThatNoPlanGaveRunsToItsEndWhateverItsKind)
{
	// A plan that runs can change for a plan that is pressing enough
	EXPECT_TRUE(creature_planner::MayChoose(true, true));
	EXPECT_TRUE(creature_planner::MayChoose(true, false));
	// With nothing left to do, it chooses
	EXPECT_TRUE(creature_planner::MayChoose(false, false));
	// An agenda that it was made to do, with steps left, stops the choice
	EXPECT_FALSE(creature_planner::MayChoose(false, true));
}

TEST(CreaturePlanner, PlansTheActionFirstAndThenTheThing)
{
	// The action is the most wanted, the last of equals; the thing is then found for that action only
	const std::vector<creature_planner::ActionCandidate> actions {
	    {.action = 12, .needsObject = true, .turnsSinceDone = 0},
	    {.action = 13, .needsObject = true, .turnsSinceDone = 0},
	};
	std::vector<uint32_t> asked;
	const auto plan = creature_planner::PlanDesire({
	    .desire = Desire::Hunger,
	    .strength = 1.0f,
	    .actions = actions,
	    .findObject = [&](const creature_planner::ActionCandidate& action) -> std::optional<creature_planner::ObjectCandidate> {
		    asked.push_back(action.action);
		    return creature_planner::ObjectCandidate {.id = 2, .usefulness = 0.08f, .actedOnUsefulness = 0.08f};
	    },
	    .findUsed = [](const creature_planner::ActionCandidate&,
	                   std::optional<uint32_t>) { return std::optional<creature_planner::UsedChoice> {}; },
	});
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->action, 13u);
	EXPECT_EQ(plan->object, 2u);
	EXPECT_EQ(asked, std::vector<uint32_t> {13});
	EXPECT_FLOAT_EQ(plan->goalUsefulness, 0.08f);
	EXPECT_FLOAT_EQ(plan->priority, creature_planner::Priority(1.0f, std::nullopt, plan->actionPriority, 0.08f, std::nullopt));
}

TEST(CreaturePlanner, TriesTheNextActionWhenNoThingIsFound)
{
	// The second action is the most wanted, but has nothing to use. The last ties with the first and wins the tie,
	// but has nothing to act on. The first is the plan.
	const std::vector<creature_planner::ActionCandidate> actions {
	    {.action = 1},
	    {.action = 2, .usesThing = true, .opinion = 1.0f},
	    {.action = 3, .needsObject = true},
	};
	const auto plan = creature_planner::PlanDesire({
	    .desire = Desire::Poo,
	    .strength = 1.0f,
	    .actions = actions,
	    .findObject =
	        [](const creature_planner::ActionCandidate&) { return std::optional<creature_planner::ObjectCandidate> {}; },
	    .findUsed = [](const creature_planner::ActionCandidate&,
	                   std::optional<uint32_t>) { return std::optional<creature_planner::UsedChoice> {}; },
	});
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->action, 1u);
	EXPECT_FALSE(plan->object.has_value());
	// An action that acts on nothing counts the default usefulness
	EXPECT_FLOAT_EQ(plan->priority, creature_planner::Priority(1.0f, std::nullopt, plan->actionPriority,
	                                                           creature_planner::k_DefaultUsefulness, std::nullopt));
	// No action that can be done: no plan
	const std::vector<creature_planner::ActionCandidate> none {{.action = 3, .needsObject = true},
	                                                           {.action = 4, .disabled = true}};
	EXPECT_FALSE(creature_planner::PlanDesire({.desire = Desire::Poo,
	                                           .strength = 1.0f,
	                                           .actions = none,
	                                           .findObject =
	                                               [](const creature_planner::ActionCandidate&) {
		                                               return std::optional<creature_planner::ObjectCandidate> {};
	                                               },
	                                           .findUsed = {}})
	                 .has_value());
}

TEST(CreaturePlanner, AnActionNotOnAThingActsOnTheCreatureItself)
{
	// With the action first, an action that does not act on a thing gets the creature itself, with the default
	// usefulness, and no thing is looked for
	const std::vector<creature_planner::ActionCandidate> actions {{.action = 7}};
	int asked = 0;
	const auto plan = creature_planner::PlanDesire({
	    .desire = Desire::Poo,
	    .strength = 1.0f,
	    .actions = actions,
	    .findObject = [&](const creature_planner::ActionCandidate&) -> std::optional<creature_planner::ObjectCandidate> {
		    ++asked;
		    return std::nullopt;
	    },
	    .findUsed = {},
	    .itself = 42,
	});
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->action, 7u);
	EXPECT_EQ(plan->object, 42u);
	EXPECT_EQ(asked, 0);
	EXPECT_FLOAT_EQ(plan->goalUsefulness, creature_planner::k_DefaultUsefulness);
	EXPECT_FLOAT_EQ(plan->priority, creature_planner::Priority(1.0f, std::nullopt, plan->actionPriority,
	                                                           creature_planner::k_DefaultUsefulness, std::nullopt));
	// An action that acts on a thing and finds none is not planned, even when the creature is given
	const std::vector<creature_planner::ActionCandidate> onThing {{.action = 8, .needsObject = true}};
	EXPECT_FALSE(creature_planner::PlanDesire({.desire = Desire::Poo,
	                                           .strength = 1.0f,
	                                           .actions = onThing,
	                                           .findObject =
	                                               [](const creature_planner::ActionCandidate&) {
		                                               return std::optional<creature_planner::ObjectCandidate> {};
	                                               },
	                                           .findUsed = {},
	                                           .itself = 42})
	                 .has_value());
}

TEST(CreaturePlanner, AnActionThatFailsIsNotTriedAgainInAnotherPlace)
{
	// An action can be in the list of a desire twice. The later entry wins the tie, and when it fails, the earlier entry
	// is not tried.
	const std::vector<creature_planner::ActionCandidate> actions {
	    {.action = 5, .needsObject = true},
	    {.action = 6},
	    {.action = 5, .needsObject = true},
	};
	int asked = 0;
	const auto plan = creature_planner::PlanDesire({
	    .desire = Desire::Impress,
	    .strength = 1.0f,
	    .actions = actions,
	    .findObject =
	        [&](const creature_planner::ActionCandidate&) {
		        ++asked;
		        return std::optional<creature_planner::ObjectCandidate> {};
	        },
	    .findUsed = {},
	});
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->action, 6u);
	EXPECT_EQ(asked, 1);
}

TEST(CreaturePlanner, AnActionThatUsesAThingCountsItsUsefulness)
{
	const std::vector<creature_planner::ActionCandidate> actions {{.action = 15, .needsObject = true, .usesThing = true}};
	const auto plan = creature_planner::PlanDesire({
	    .desire = Desire::Hunger,
	    .strength = 1.0f,
	    .actions = actions,
	    .findObject =
	        [](const creature_planner::ActionCandidate&) {
		        return std::optional(
		            creature_planner::ObjectCandidate {.id = 4, .usefulness = 0.05f, .actedOnUsefulness = 0.05f});
	        },
	    .findUsed =
	        [](const creature_planner::ActionCandidate&, std::optional<uint32_t> object) {
		        // The thing used is never the thing acted on
		        EXPECT_EQ(object, 4u);
		        return std::optional(creature_planner::UsedChoice {.id = 9, .usefulness = 0.07f});
	        },
	});
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->instrument, 9u);
	EXPECT_FLOAT_EQ(plan->priority, creature_planner::Priority(1.0f, std::nullopt, plan->actionPriority, 0.05f, 0.07f));
}

TEST(CreaturePlanner, ADesireAboutAWholeThingFindsThePartAfterTheAction)
{
	// The action that uses the more useful thing wins; the part then counts the default usefulness
	const std::vector<creature_planner::ActionCandidate> actions {
	    {.action = 7, .needsObject = true, .usesThing = true},
	    {.action = 8, .needsObject = true, .usesThing = true},
	};
	const auto plan = creature_planner::PlanDesire({
	    .desire = Desire::Impress,
	    .strength = 0.5f,
	    .order = creature_planner::PlanOrder::ActionThenPart,
	    .actions = actions,
	    .activityUsefulness = 0.09f,
	    .findObject =
	        [](const creature_planner::ActionCandidate& action) {
		        return action.action == 7 ? std::optional(creature_planner::ObjectCandidate {.id = 4, .usefulness = 0.06f})
		                                  : std::nullopt;
	        },
	    .findUsed =
	        [](const creature_planner::ActionCandidate& action, std::optional<uint32_t>) {
		        return std::optional(creature_planner::UsedChoice {.id = 9, .usefulness = action.action == 8 ? 0.08f : 0.05f});
	        },
	});
	// Action 8 wins first, but the whole thing has no part for it: action 7 is tried next
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->action, 7u);
	EXPECT_EQ(plan->object, 4u);
	EXPECT_FLOAT_EQ(plan->priority, creature_planner::Priority(0.5f, 0.09f, plan->actionPriority,
	                                                           creature_planner::k_DefaultUsefulness, 0.05f));
	// Only anger, of the desires about something, counts what it acts on as well
	EXPECT_TRUE(creature_planner::CountsActedOn(Desire::Anger, true));
	EXPECT_FALSE(creature_planner::CountsActedOn(Desire::Impress, true));
	EXPECT_FALSE(creature_planner::CountsActedOn(Desire::BeFriends, true));
	EXPECT_TRUE(creature_planner::CountsActedOn(Desire::Hunger, false));
}

TEST(CreaturePlanner, AnActionThatNeedsNoThingActsOnThePartOfTheWholeThing)
{
	// Under a desire about a whole thing, an action with no thing of its own still gets the part as its object
	const std::vector<creature_planner::ActionCandidate> actions {{.action = 39}};
	const auto planning = [&](creature_planner::PlanOrder order) {
		return creature_planner::PlanDesire({
		    .desire = Desire::Impress,
		    .strength = 0.5f,
		    .order = order,
		    .actions = actions,
		    .activityUsefulness = 0.2f,
		    .findObject =
		        [](const creature_planner::ActionCandidate&) {
			        return std::optional(creature_planner::ObjectCandidate {.id = 12, .usefulness = 0.3f});
		        },
		    .findUsed = [](const creature_planner::ActionCandidate&,
		                   std::optional<uint32_t>) { return std::optional<creature_planner::UsedChoice> {}; },
		});
	};
	const auto part = planning(creature_planner::PlanOrder::ActionThenPart);
	ASSERT_TRUE(part.has_value());
	EXPECT_EQ(part->object, 12u);
	// Not about a whole thing, such an action acts on nothing
	const auto alone = planning(creature_planner::PlanOrder::ActionFirst);
	ASSERT_TRUE(alone.has_value());
	EXPECT_FALSE(alone->object.has_value());
}

TEST(CreaturePlanner, AngerFindsThePartFirst)
{
	// Only the actions that can act on the part count, and the part's usefulness counts in the priority
	const std::vector<creature_planner::ActionCandidate> actions {
	    {.action = 20, .needsObject = true},
	    {.action = 21, .needsObject = true, .opinion = 1.0f},
	    {.action = 22, .needsObject = true, .opinion = 1.0f},
	};
	const auto plan = creature_planner::PlanDesire({
	    .desire = Desire::Anger,
	    .strength = 1.0f,
	    .order = creature_planner::PlanOrder::PartThenAction,
	    .actions = actions,
	    .activityUsefulness = 0.09f,
	    .findObject =
	        [](const creature_planner::ActionCandidate& action) {
		        return action.action == 22 ? std::nullopt
		                                   : std::optional(creature_planner::ObjectCandidate {
		                                         .id = 4, .usefulness = 0.06f, .actedOnUsefulness = 0.06f});
	        },
	    .findUsed = {},
	});
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->action, 21u);
	EXPECT_EQ(plan->object, 4u);
	EXPECT_FLOAT_EQ(plan->priority, creature_planner::Priority(1.0f, 0.09f, plan->actionPriority, 0.06f, std::nullopt));
}

TEST(CreaturePlanner, BestDesireOnOneThing)
{
	// The score of a desire is compared with the usefulness of the best desire before it, without its strength
	const std::vector<creature_planner::DesireOnThing> desires {
	    {.desire = Desire::Compassion, .strength = 0.5f, .usefulness = 0.08f},
	    {.desire = Desire::Anger, .strength = 1.0f, .usefulness = 0.09f},
	    {.desire = Desire::BeFriends, .strength = 0.9f, .usefulness = 0.095f},
	};
	EXPECT_EQ(creature_planner::BestDesireOnThing(desires), Desire::Anger);
	// No score more than zero: no desire
	const std::vector<creature_planner::DesireOnThing> none {{.desire = Desire::Anger, .strength = 0.0f, .usefulness = 0.1f}};
	EXPECT_FALSE(creature_planner::BestDesireOnThing(none).has_value());
}

TEST(CreaturePlanner, DesireForAThingShownOnTheLeash)
{
	const std::vector<creature_planner::DesireOnThing> aboutWhole {
	    {.desire = Desire::Compassion, .strength = 0.5f, .usefulness = 0.08f},
	};
	// A desire not about a whole thing scores a tenth of its strength, against the usefulness of the best desire before
	const std::vector<creature_planner::DesireOnThing> weak {{.desire = Desire::Curiosity, .strength = 0.7f}};
	EXPECT_EQ(creature_planner::DesireForShownThing(aboutWhole, weak), Desire::Compassion);
	const std::vector<creature_planner::DesireOnThing> strong {{.desire = Desire::Curiosity, .strength = 0.9f},
	                                                           {.desire = Desire::Poo, .strength = 0.95f}};
	EXPECT_EQ(creature_planner::DesireForShownThing(aboutWhole, strong), Desire::Poo);
	// With no whole thing, the strongest of the others; with nothing, none
	EXPECT_EQ(creature_planner::DesireForShownThing({}, strong), Desire::Poo);
	EXPECT_FALSE(creature_planner::DesireForShownThing({}, {}).has_value());
	// Only the desires that test the thing they act on
	EXPECT_TRUE(creature_planner::TestsThingActedOn(Desire::Curiosity));
	EXPECT_FALSE(creature_planner::TestsThingActedOn(Desire::Hunger));
}

TEST(CreaturePlanner, OpinionBreaksTies)
{
	const std::vector<creature_planner::ActionCandidate> actions {{.action = 1, .opinion = 1.0f, .turnsSinceDone = 0},
	                                                              {.action = 2, .opinion = -1.0f, .turnsSinceDone = 0}};
	EXPECT_EQ(creature_planner::PlanDesire({.desire = Desire::Rest, .strength = 0.5f, .actions = actions})->action, 1u);
}

TEST(CreaturePlanner, GoesRoundTheEligibleDesires)
{
	creature_planner::PlannerState state;
	std::array<bool, creature_desires::k_DesireCount> eligible {};
	eligible.at(static_cast<size_t>(Desire::Hunger)) = true;
	eligible.at(static_cast<size_t>(Desire::Tiredness)) = true;
	eligible.at(static_cast<size_t>(Desire::Rest)) = true;
	const auto first = creature_planner::NextGoals(state, eligible);
	ASSERT_EQ(first.size(), 2u);
	EXPECT_EQ(first[0], Desire::Hunger);
	EXPECT_EQ(first[1], Desire::Tiredness);
	const auto second = creature_planner::NextGoals(state, eligible);
	EXPECT_EQ(second[0], Desire::Rest);
	EXPECT_EQ(second[1], Desire::Hunger);
	// With three or more creatures in the world each plans one desire a turn
	const auto third = creature_planner::NextGoals(state, eligible, creature_planner::GoalsPerTurn(3));
	ASSERT_EQ(third.size(), 1u);
	EXPECT_EQ(third[0], Desire::Tiredness);
}

TEST(CreaturePlanner, FewerGoalsWithMoreCreatures)
{
	EXPECT_EQ(creature_planner::GoalsPerTurn(0), 2u);
	EXPECT_EQ(creature_planner::GoalsPerTurn(1), 2u);
	EXPECT_EQ(creature_planner::GoalsPerTurn(2), 2u);
	EXPECT_EQ(creature_planner::GoalsPerTurn(3), 1u);
	EXPECT_EQ(creature_planner::GoalsPerTurn(7), 1u);
}

TEST(CreaturePlanner, AnyPlanWorthMoreThanNothingIsChosen)
{
	creature_planner::PlannerState state;
	state.best.at(static_cast<size_t>(Desire::Hunger)) =
	    creature_planner::Plan {.desire = Desire::Hunger, .action = 1, .priority = 0.001f};
	const auto chosen = creature_planner::Choose(state);
	ASSERT_TRUE(chosen.has_value());
	EXPECT_EQ(chosen->desire, Desire::Hunger);
	state.best.at(static_cast<size_t>(Desire::Hunger))->priority = 0.0f;
	EXPECT_FALSE(creature_planner::Choose(state).has_value());
}

// A plan that a script stopped has no steps and priority 0: the same plan can start again from its first step
TEST(CreaturePlanner, APlanWithNoStepsCanStartAgain)
{
	creature_planner::PlannerState state;
	const auto idle = creature_planner::Plan {.desire = Desire::IdleWithPlayer, .action = 3, .priority = 0.5f};
	state.best.at(static_cast<size_t>(Desire::IdleWithPlayer)) = idle;
	state.current = idle;
	EXPECT_FALSE(creature_planner::Choose(state).has_value());
	state.current->priority = 0.0f;
	EXPECT_FALSE(creature_planner::Choose(state).has_value());
	state.currentHasNoSteps = true;
	const auto chosen = creature_planner::Choose(state);
	ASSERT_TRUE(chosen.has_value());
	EXPECT_EQ(chosen->action, 3u);
	EXPECT_FLOAT_EQ(chosen->priority, 0.5f);
}

TEST(CreaturePlanner, NothingReplacesAForcedPlan)
{
	creature_planner::PlannerState state;
	state.best.at(0) = creature_planner::Plan {.desire = Desire::Impress, .action = 1, .priority = 1.0e30f};
	state.current = creature_planner::Plan {.desire = Desire::Compassion,
	                                        .action = 2,
	                                        .goalUsefulness = creature_planner::k_ForcedScore,
	                                        .actionPriority = creature_planner::k_ForcedScore,
	                                        .priority = creature_planner::k_ForcedScore};
	EXPECT_FALSE(creature_planner::Choose(state).has_value());
}

TEST(CreaturePlanner, APlanOfItsOwnReplacesACatchOnlyWhenTwiceAsPressing)
{
	// A catch or a gift from the hand has a score of 1, so a plan of the creature with a score above 2 replaces it
	creature_planner::PlannerState state;
	state.current = creature_planner::Plan {.desire = Desire::Play,
	                                        .action = 2,
	                                        .goalUsefulness = creature_planner::k_CatchOrGiftScore,
	                                        .actionPriority = creature_planner::k_CatchOrGiftScore,
	                                        .priority = creature_planner::k_CatchOrGiftScore};
	state.best.at(0) = creature_planner::Plan {.desire = Desire::Impress, .action = 1, .priority = 2.0f};
	EXPECT_FALSE(creature_planner::Choose(state).has_value());
	state.best.at(0)->priority = 2.5f;
	EXPECT_TRUE(creature_planner::Choose(state).has_value());
}

TEST(CreaturePlanner, ChoosesOnlyWhatIsPressingEnough)
{
	creature_planner::PlannerState state;
	state.best.at(0) = creature_planner::Plan {.desire = Desire::Impress, .action = 1, .priority = 30.0f};
	EXPECT_TRUE(creature_planner::Choose(state).has_value());
	state.current = creature_planner::Plan {.desire = Desire::Hunger, .action = 2, .priority = 20.0f};
	EXPECT_FALSE(creature_planner::Choose(state).has_value());
	state.best.at(0)->priority = 41.0f;
	EXPECT_TRUE(creature_planner::Choose(state).has_value());
}

// Learning from feedback

TEST(CreatureLearning, WhatADesireWasAbout)
{
	// What the plan says it was about, else the thing acted on for a desire about something in itself, else nothing
	EXPECT_EQ(creature_learning::ActivityOf(true, 5u, 9u), 9u);
	EXPECT_EQ(creature_learning::ActivityOf(false, 5u, 9u), 9u);
	EXPECT_EQ(creature_learning::ActivityOf(true, 5u, std::nullopt), 5u);
	EXPECT_FALSE(creature_learning::ActivityOf(false, 5u, std::nullopt).has_value());
}

TEST(CreatureLearning, LessonsAboutThings)
{
	const creature_tree::Belief cow {.type = creature_tree::belief_types::k_Animal};
	const creature_tree::Belief town {.type = creature_tree::belief_types::k_Town};
	constexpr uint32_t k_Self = 1;
	// Eating a cow for hunger: what it acted on only
	const creature_learning::Context eat {.object = 7u, .belief = cow};
	EXPECT_FALSE(creature_learning::ThingLessonsFor(eat, k_Self).activity);
	EXPECT_TRUE(creature_learning::ThingLessonsFor(eat, k_Self).actedOn);
	// Helping a town by acting on one of its things: both
	const creature_learning::Context help {.object = 7u, .belief = cow, .activity = 3u, .activityBelief = town};
	EXPECT_TRUE(creature_learning::ThingLessonsFor(help, k_Self).activity);
	EXPECT_TRUE(creature_learning::ThingLessonsFor(help, k_Self).actedOn);
	// Acting on the very thing the desire is about: only that
	const creature_learning::Context befriend {.object = 3u, .belief = town, .activity = 3u, .activityBelief = town};
	EXPECT_TRUE(creature_learning::ThingLessonsFor(befriend, k_Self).activity);
	EXPECT_FALSE(creature_learning::ThingLessonsFor(befriend, k_Self).actedOn);
	// Acting on itself teaches nothing about acting on such things
	const creature_learning::Context itself {.object = k_Self, .belief = cow};
	EXPECT_FALSE(creature_learning::ThingLessonsFor(itself, k_Self).actedOn);
}

TEST(CreatureLearning, LessonFactor)
{
	EXPECT_FLOAT_EQ(creature_learning::LessonFactor(2.0f, 1.0f), 0.5f);
	EXPECT_FLOAT_EQ(creature_learning::LessonFactor(2.0f, -1.0f), 1.5f);
	EXPECT_FLOAT_EQ(creature_learning::LessonFactor(200.0f, 0.0f), 1.0f);
}

TEST(CreatureLearning, DesireLessonAndDependencies)
{
	auto desires = TwoDesires();
	creature_learning::AllDesireRules rules {};
	auto& hunger = rules.at(static_cast<size_t>(Desire::Hunger));
	hunger = {.initialMax = 2.0f, .decayMin = 0.9f, .decayMax = 0.95f, .lessonDivisor = 2.0f, .learnable = true};
	creature_learning::Dependencies dependencies {};
	dependencies.at(static_cast<size_t>(Desire::Hunger)).at(static_cast<size_t>(Desire::Anger)) = -0.5f;
	creature_learning::LearnDesireLesson(desires, Desire::Hunger, 0, 1.0f, rules, dependencies);
	const auto& state = desires[Desire::Hunger];
	// Rewarded, it grows twice as fast, its source drives it sooner, it fades more slowly
	EXPECT_FLOAT_EQ(state.increaseSeconds, 10.0f);
	EXPECT_FLOAT_EQ(state.sources[0].threshold, 0.38f);
	EXPECT_FLOAT_EQ(state.sources[1].threshold, 0.5f);
	EXPECT_FLOAT_EQ(state.max, 2.0f);
	EXPECT_FLOAT_EQ(state.decay, 0.901f);
	// The opposed desire grows slower
	EXPECT_FLOAT_EQ(desires[Desire::Anger].increaseSeconds, 10.0f);
	creature_learning::LearnDesireLesson(desires, Desire::Hunger, 0, -1.0f, rules, dependencies);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].max, 1.9f);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].increaseSeconds, 15.0f);
	// The maximum goes at most 0.2 below where it started
	creature_learning::LearnDesireLesson(desires, Desire::Hunger, 0, -1.0f, rules, dependencies);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].max, 1.8f);
	creature_learning::LearnDesireLesson(desires, Desire::Hunger, 0, -1.0f, rules, dependencies);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].max, 1.8f);
}

TEST(CreatureLearning, DesireMaximumReachesTwoTenthsEitherWay)
{
	auto desires = TwoDesires();
	desires[Desire::Anger].max = 0.8f;
	creature_learning::AllDesireRules rules {};
	rules.at(static_cast<size_t>(Desire::Anger)) = {.initialMax = 0.8f, .lessonDivisor = 200.0f, .learnable = true};
	const creature_learning::Dependencies dependencies {};
	for (int i = 0; i < 5; ++i)
	{
		creature_learning::LearnDesireLesson(desires, Desire::Anger, std::nullopt, 1.0f, rules, dependencies);
	}
	EXPECT_FLOAT_EQ(desires[Desire::Anger].max, 1.0f);
	for (int i = 0; i < 10; ++i)
	{
		creature_learning::LearnDesireLesson(desires, Desire::Anger, std::nullopt, -1.0f, rules, dependencies);
	}
	EXPECT_FLOAT_EQ(desires[Desire::Anger].max, 0.6f);
}

TEST(CreatureLearning, OpinionsAndFeedbackStrength)
{
	EXPECT_FLOAT_EQ(creature_learning::OpinionAfter(0.0f, 1.0f), 0.8f);
	EXPECT_FLOAT_EQ(creature_learning::OpinionAfter(0.8f, -1.0f), -0.64f);
	EXPECT_FLOAT_EQ(creature_learning::Strengthened(0.2f), 0.5f);
	EXPECT_FLOAT_EQ(creature_learning::Strengthened(-0.9f), -0.9f);
}

TEST(CreatureLearning, SpreadsLessonsByDependency)
{
	creature_learning::Dependencies dependencies {};
	dependencies.at(static_cast<size_t>(Desire::Compassion)).at(static_cast<size_t>(Desire::Anger)) = -0.86f;
	dependencies.at(static_cast<size_t>(Desire::Compassion)).at(static_cast<size_t>(Desire::BeFriends)) = 0.5f;
	const auto spread = creature_learning::Spread(Desire::Compassion, 1.0f, dependencies);
	ASSERT_EQ(spread.size(), 3u);
	EXPECT_EQ(spread[0].first, Desire::Compassion);
	EXPECT_FLOAT_EQ(spread[1].second, -0.86f);
	EXPECT_FLOAT_EQ(spread[2].second, 0.5f);
}

TEST(CreatureLearning, CreditGoesToRecentActions)
{
	// An action that did not end well counts fully, whether the player saw it or not
	creature_learning::Context underway {.action = 1, .windowSeconds = 20.0f};
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(underway), 1.0f);
	underway.running = false;
	underway.secondsSince = 100.0f;
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(underway), 1.0f);
	// An action that ended well counts only when the player saw it, less and less until its window closes
	creature_learning::Context finished {
	    .action = 2, .running = false, .secondsSince = 5.0f, .windowSeconds = 20.0f, .done = true};
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(finished), 0.0f);
	finished.seen = true;
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(finished), 0.75f);
	finished.secondsSince = 25.0f;
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(finished), 0.0f);
	creature_learning::Context unlearnable {.action = 3, .learnable = false};
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(unlearnable), 0.0f);

	std::vector<creature_learning::Context> contexts;
	static_cast<void>(creature_learning::Remember(contexts, {.action = 1, .windowSeconds = 20.0f}));
	creature_learning::Age(contexts, 1.0f);
	creature_learning::MarkNewestDone(contexts, true);
	static_cast<void>(creature_learning::Remember(contexts, {.action = 2, .windowSeconds = 10.0f}));
	creature_learning::Age(contexts, 1.0f);
	EXPECT_FALSE(contexts[0].running);
	EXPECT_EQ(creature_learning::BestContext(contexts), 1u);
	creature_learning::MarkNewestDone(contexts, true);
	creature_learning::Age(contexts, 9.0f);
	// The older action's longer window now gives it the bigger share
	EXPECT_EQ(creature_learning::BestContext(contexts), 0u);
	for (uint32_t i = 0; i < 20; ++i)
	{
		contexts.back().done = true;
		static_cast<void>(creature_learning::Remember(contexts, {.action = i}));
	}
	EXPECT_EQ(contexts.size(), 5u);
	EXPECT_EQ(contexts.back().action, 19u);
	EXPECT_EQ(contexts.front().action, 15u);
	// Of two that count the same, the newer takes the credit
	EXPECT_EQ(creature_learning::BestContext(contexts), 4u);
	contexts.back().done = true;
	contexts.back().seen = true;
	contexts.back().windowSeconds = 10.0f;
	contexts.front().done = true;
	contexts.front().seen = true;
	contexts.front().windowSeconds = 10.0f;
	EXPECT_EQ(creature_learning::BestContext(contexts), 4u);
}

TEST(CreatureLearning, ThePlayerMustSeeAnActionThatEndsWell)
{
	std::vector<creature_learning::Context> contexts;
	static_cast<void>(creature_learning::Remember(contexts, {.action = 1, .windowSeconds = 10.0f}));
	creature_learning::Age(contexts, 3.0f);
	// Not seen as it ends: feedback does not go to it
	creature_learning::MarkNewestDone(contexts, false);
	EXPECT_TRUE(contexts.back().done);
	EXPECT_FALSE(contexts.back().seen);
	EXPECT_FLOAT_EQ(contexts.back().secondsSince, 0.0f);
	EXPECT_EQ(creature_learning::BestContext(contexts), std::nullopt);

	// Seen before it ends, at the main step: the window starts when it ends
	static_cast<void>(creature_learning::Remember(contexts, {.action = 2, .windowSeconds = 10.0f}));
	creature_learning::MarkNewestSeen(contexts);
	creature_learning::Age(contexts, 4.0f);
	creature_learning::MarkNewestDone(contexts, false);
	EXPECT_TRUE(contexts.back().seen);
	creature_learning::Age(contexts, 2.0f);
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(contexts.back()), 0.8f);
	// A later sighting starts the window again
	creature_learning::MarkNewestSeen(contexts);
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(contexts.back()), 1.0f);
	creature_learning::Age(contexts, 10.0f);
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(contexts.back()), 0.0f);
}

TEST(CreatureLearning, DecidingSuppressesOpposedDesires)
{
	auto desires = TwoDesires();
	creature_learning::Dependencies dependencies {};
	dependencies.at(static_cast<size_t>(Desire::Hunger)).at(static_cast<size_t>(Desire::Anger)) = -0.5f;
	creature_learning::SuppressOpposed(desires, Desire::Hunger, dependencies, 10.0f);
	EXPECT_EQ(desires[Desire::Anger].suppressedTurns, 1200u);
	EXPECT_EQ(desires[Desire::Hunger].suppressedTurns, 0u);
	// A desire that opposes itself is held back by deciding on it as well
	dependencies.at(static_cast<size_t>(Desire::Hunger)).at(static_cast<size_t>(Desire::Hunger)) = -0.25f;
	creature_learning::SuppressOpposed(desires, Desire::Hunger, dependencies, 10.0f);
	EXPECT_EQ(desires[Desire::Hunger].suppressedTurns, 600u);
}

TEST(CreatureLearning, DominanceAndActions)
{
	auto desires = TwoDesires();
	creature_learning::MakeLeastDominant(desires, Desire::Hunger, 1.3f, 0.0f);
	EXPECT_NEAR(desires[Desire::Hunger].value, 0.2f / 1.3f, 1e-6f);
	creature_learning::MakeFullyDominant(desires, Desire::Hunger, 0.05f);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 2.0f);
	// Every other desire goes down to the floor, active or not
	EXPECT_FLOAT_EQ(desires[Desire::Anger].value, 0.05f);
	EXPECT_FLOAT_EQ(desires[Desire::Fear].value, 0.05f);
	creature_learning::AfterAction(desires, Desire::Hunger, 0.3f);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 0.6f);
}

TEST(CreatureLearning, FullyDominantChangesNothingForAnInactiveDesire)
{
	auto desires = TwoDesires();
	desires[Desire::Hunger].activated = false;
	desires[Desire::Hunger].suppressedTurns = 40;
	creature_learning::MakeFullyDominant(desires, Desire::Hunger, 0.05f);
	EXPECT_FALSE(desires[Desire::Hunger].activated);
	EXPECT_EQ(desires[Desire::Hunger].suppressedTurns, 40u);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 0.5f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].value, 0.2f);
}

TEST(CreatureLearning, FullyDominantKeepsTheDesireHeldBack)
{
	auto desires = TwoDesires();
	desires[Desire::Hunger].suppressedTurns = 40;
	creature_learning::MakeFullyDominant(desires, Desire::Hunger, 0.0f);
	EXPECT_EQ(desires[Desire::Hunger].suppressedTurns, 40u);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 2.0f);
}

TEST(CreatureLearning, LeastDominantCountsItselfAndHoldsBetweenFloorAndMaximum)
{
	auto desires = TwoDesires();
	// The weakest active desire is the desire itself
	desires[Desire::Hunger].value = 0.1f;
	creature_learning::MakeLeastDominant(desires, Desire::Hunger, 2.0f, 0.0f);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 0.05f);
	// The species' floor holds it up
	desires[Desire::Hunger].value = 0.1f;
	creature_learning::MakeLeastDominant(desires, Desire::Hunger, 2.0f, 0.08f);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 0.08f);
	// Its maximum holds it down
	desires[Desire::Hunger].value = 1.8f;
	desires[Desire::Anger].value = 1.5f;
	desires[Desire::Hunger].max = 0.4f;
	creature_learning::MakeLeastDominant(desires, Desire::Hunger, 1.0f, 0.0f);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 0.4f);
	// The floor wins over a maximum below it
	creature_learning::MakeLeastDominant(desires, Desire::Hunger, 1.0f, 0.6f);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 0.6f);
}

TEST(CreatureLearning, LeastDominantWithNoneBelowTwoReadsTheFirstDesire)
{
	creature_desires::Desires desires;
	for (auto& desire : desires.desires)
	{
		desire.activated = false;
		desire.max = 4.0f;
	}
	desires[Desire::Impress].value = 0.9f;
	auto& hunger = desires[Desire::Hunger];
	hunger.activated = true;
	hunger.value = 3.0f;
	creature_learning::MakeLeastDominant(desires, Desire::Hunger, 1.5f, 0.0f);
	EXPECT_FLOAT_EQ(hunger.value, 0.6f);
}

TEST(CreatureLearning, ACarriedOutPlanStopsSomeDesiresAndMakesTirednessLeast)
{
	auto desires = TwoDesires();
	auto& tiredness = desires[Desire::Tiredness];
	tiredness.activated = true;
	tiredness.value = 0.9f;
	tiredness.max = 1.0f;
	creature_learning::AfterPlanCarriedOut(desires, Desire::Tiredness, 0.0f, 10.0f);
	EXPECT_NEAR(tiredness.value, 0.2f / creature_learning::k_LeastDominantFactor, 1e-6f);
	creature_learning::AfterPlanCarriedOut(desires, Desire::AttractAttention, 0.0f, 10.0f);
	EXPECT_EQ(desires[Desire::AttractAttention].suppressedTurns, 600u);
	creature_learning::AfterPlanCarriedOut(desires, Desire::GetColder, 0.0f, 10.0f);
	EXPECT_EQ(desires[Desire::GetColder].suppressedTurns, 1200u);
	creature_learning::AfterPlanCarriedOut(desires, Desire::Hunger, 0.0f, 10.0f);
	EXPECT_EQ(desires[Desire::Hunger].suppressedTurns, 0u);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 0.5f);
}

TEST(CreatureLearning, DominantSourceFollowsDrive)
{
	auto desires = TwoDesires();
	creature_desires::UpdateDesires(desires, 10.0f);
	EXPECT_EQ(creature_learning::DominantSource(desires[Desire::Hunger]), 0u);
	creature_learning::ResetDrives(desires[Desire::Hunger]);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].sources[0].drive, 0.0f);
}

TEST(CreatureLearning, AttitudesToCreatures)
{
	std::vector<creature_learning::CreatureAttitude> attitudes;
	auto& attitude = creature_learning::AttitudeTo(attitudes, 7);
	const auto lessons = creature_learning::ChangeHowNice(attitude, 1.5f);
	EXPECT_FLOAT_EQ(attitude.howNice, 1.0f);
	EXPECT_EQ(lessons[0].first, Desire::BeFriends);
	EXPECT_FLOAT_EQ(lessons[0].second, 0.5f);
	EXPECT_FLOAT_EQ(lessons[1].second, -0.5f);
	EXPECT_EQ(&creature_learning::AttitudeTo(attitudes, 7), &attitudes[0]);
}

TEST(CreatureLearning, ThoughtsAndLikes)
{
	EXPECT_EQ(creature_learning::DesireLessonText(1.0f, "desire to eat"), "I've learnt to increase my desire to eat");
	EXPECT_EQ(creature_learning::ObjectLessonText(-1.0f, "villagers", "am trying to eat"),
	          "I've learnt to avoid villagers when I am trying to eat");
	creature_desires::DesireState state {.sources = {{.threshold = 0.3f}}};
	const std::array<float, 1> start {0.5f};
	EXPECT_LT(creature_learning::LikesLevel(Desire::Hunger, state, start), 5u);
	state.sources[0].threshold = 0.5f;
	EXPECT_EQ(creature_learning::LikesLevel(Desire::Hunger, state, start), 5u);
	state.value = 1.0f;
	state.max = 1.0f;
	EXPECT_EQ(creature_learning::LikesLevel(Desire::Fear, state, start), 0u);
}

// Learning by watching

TEST(CreatureWatching, LearnsASkillOnceWatchedLongEnough)
{
	const std::vector<creature_watching::SkillRule> skills {{.name = "Build", .watchSeconds = 6.0f, .minPhase = 4}};
	auto knowledge = creature_watching::StartKnowledge(skills, {});
	EXPECT_TRUE(creature_watching::SeeSkill(knowledge, 0, skills, 3, 0, 10.0f).ignored);
	EXPECT_FALSE(creature_watching::SeeSkill(knowledge, 0, skills, 5, 100, 10.0f).learnt);
	EXPECT_FALSE(creature_watching::SeeSkill(knowledge, 0, skills, 5, 150, 10.0f).learnt);
	EXPECT_TRUE(creature_watching::SeeSkill(knowledge, 0, skills, 5, 160, 10.0f).learnt);
	EXPECT_TRUE(knowledge.skillsKnown[0]);
}

TEST(CreatureWatching, TooYoungForASkillIsToldSoEvenWhenItKnowsIt)
{
	const std::vector<creature_watching::SkillRule> skills {{.name = "Dance", .watchSeconds = 6.0f, .minPhase = 4}};
	auto knowledge = creature_watching::StartKnowledge(skills, {});
	knowledge.skillsKnown[0] = true;
	const auto young = creature_watching::SeeSkill(knowledge, 0, skills, 3, 50, 10.0f);
	EXPECT_EQ(young.event, creature_watching::LearningEvent::TooYoung);
	// Too young, nothing is counted
	EXPECT_EQ(knowledge.skillsSeen[0].count, 0u);
}

TEST(CreatureWatching, ASkillIsLearntByWholeSecondsFromTheFirstSightingEver)
{
	const std::vector<creature_watching::SkillRule> skills {{.name = "Dance", .watchSeconds = 6.0f, .minPhase = 4}};
	auto knowledge = creature_watching::StartKnowledge(skills, {});
	// The first sighting starts the clock: still learning, the meter at a half
	const auto first = creature_watching::SeeSkill(knowledge, 0, skills, 4, 1000, 10.0f);
	EXPECT_EQ(first.event, creature_watching::LearningEvent::NearlyLearnt);
	EXPECT_EQ(first.meter, 0.5f);
	// 59 turns are five whole seconds: not yet
	EXPECT_FALSE(creature_watching::SeeSkill(knowledge, 0, skills, 4, 1059, 10.0f).learnt);
	// The clock is never reset: a sighting much later teaches it at once
	const auto later = creature_watching::SeeSkill(knowledge, 0, skills, 4, 5000, 10.0f);
	EXPECT_TRUE(later.learnt);
	EXPECT_EQ(later.event, creature_watching::LearningEvent::Learnt);
	EXPECT_EQ(later.meter, 1.0f);
	EXPECT_EQ(knowledge.skillsSeen[0].count, 3u);
	// Known, a sighting tells nothing and moves no meter
	const auto again = creature_watching::SeeSkill(knowledge, 0, skills, 4, 5001, 10.0f);
	EXPECT_FALSE(again.event.has_value());
	EXPECT_FALSE(again.meter.has_value());
}

TEST(CreatureWatching, AFirstSightingOnTurnZeroStartsTheClockAgain)
{
	const std::vector<creature_watching::SkillRule> skills {{.name = "Build", .watchSeconds = 7.0f, .minPhase = 0}};
	auto knowledge = creature_watching::StartKnowledge(skills, {});
	EXPECT_FALSE(creature_watching::SeeSkill(knowledge, 0, skills, 13, 0, 10.0f).learnt);
	EXPECT_FALSE(creature_watching::SeeSkill(knowledge, 0, skills, 13, 100, 10.0f).learnt);
	EXPECT_TRUE(creature_watching::SeeSkill(knowledge, 0, skills, 13, 170, 10.0f).learnt);
}

TEST(CreatureWatching, TheMeterShowsAtMostEveryHundredTurns)
{
	EXPECT_FALSE(creature_watching::MeterShows(0, 100));
	EXPECT_TRUE(creature_watching::MeterShows(0, 101));
	EXPECT_FALSE(creature_watching::MeterShows(500, 600));
	EXPECT_TRUE(creature_watching::MeterShows(500, 601));
}

TEST(CreatureWatching, AVillagerShowsTheStateItGoesBackTo)
{
	using creature_watching::StateToLearnFrom;
	// Building teaches building
	EXPECT_EQ(StateToLearnFrom({.state = 41, .finalState = 7}), 41u);
	// Waiting for an animation, the state it goes back to
	EXPECT_EQ(StateToLearnFrom({.state = creature_watching::k_WaitForAnimationState, .finalState = 60}), 60u);
	// On its way to the pit for food, only near the pit
	EXPECT_EQ(StateToLearnFrom({.state = 33, .distanceToGoal = 40.0f}), 33u);
	EXPECT_EQ(StateToLearnFrom({.state = 33, .distanceToGoal = 40.5f}), creature_watching::k_DecideWhatToDoState);
}

TEST(CreatureWatching, ACreatureWatchesVillagersWhenFreeToAndGrownUpEnough)
{
	using creature_watching::WatchesVillagers;
	EXPECT_FALSE(WatchesVillagers({.phase = 2}));
	EXPECT_TRUE(WatchesVillagers({.phase = 3}));
	EXPECT_FALSE(WatchesVillagers({.phase = 13, .scriptControlled = true}));
	// Plans for its needs and feelings keep it busy
	for (const auto desire : {Desire::Impress,   Desire::Compassion, Desire::Anger,         Desire::Play,
	                          Desire::Hunger,    Desire::Fear,       Desire::Poo,           Desire::Tiredness,
	                          Desire::Puke,      Desire::Water,      Desire::RestoreHealth, Desire::BeFriends,
	                          Desire::GetWarmer, Desire::GetColder,  Desire::Scratch,       Desire::RunAwayFromPlayer,
	                          Desire::Rest,      Desire::Illness,    Desire::ObeyCreature,  Desire::Steal})
	{
		EXPECT_FALSE(WatchesVillagers({.phase = 13, .planDesire = desire})) << creature_desires::Name(desire);
	}
	for (const auto desire :
	     {Desire::Curiosity, Desire::IdleWithPlayer, Desire::Wanderlust, Desire::BuildHome, Desire::BringStuffHome,
	      Desire::AttractAttention, Desire::ManifestState, Desire::ObeyPlayer, Desire::Sadness, Desire::LookAround})
	{
		EXPECT_TRUE(WatchesVillagers({.phase = 13, .planDesire = desire})) << creature_desires::Name(desire);
	}
}

TEST(CreatureWatching, LearnsAMiracleBySightingsAndSpecies)
{
	const std::vector<creature_watching::MiracleRule> miracles {{.name = "Heal", .timesToSee = 10, .minPhase = 8},
	                                                            {.name = "Food", .knownAtStart = true}};
	auto knowledge = creature_watching::StartKnowledge({}, miracles);
	// Nothing is known from the start
	EXPECT_FALSE(knowledge.miraclesKnown[1]);
	EXPECT_FLOAT_EQ(creature_watching::TimesNeeded(10, 1.5f), 15.0f);
	// Not rounded: a cow needs 1.7 times as many sightings
	EXPECT_FLOAT_EQ(creature_watching::TimesNeeded(9, 1.7f), 15.3f);
	uint32_t turn = 100;
	// Too young, it is told so and nothing counts
	const auto young = creature_watching::SeeMiracle(knowledge, 0, miracles, 7, turn, 3, 1.5f);
	EXPECT_TRUE(young.ignored);
	EXPECT_EQ(young.event, creature_watching::LearningEvent::TooYoung);
	EXPECT_FALSE(knowledge.miraclesKnown[0]);
	// The first sighting counts, and it knows about the miracle from then on
	EXPECT_NEAR(creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f).share, 0.2f, 1e-6f);
	EXPECT_TRUE(knowledge.miraclesKnown[0]);
	// Seen again within 50 turns it doesn't count, and the wait starts again from then
	turn += 10;
	EXPECT_NEAR(creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f).share, 0.2f, 1e-6f);
	turn += creature_watching::k_MiracleSightingTurns;
	EXPECT_NEAR(creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f).share, 0.2f, 1e-6f);
	for (int i = 0; i < 3; ++i)
	{
		turn += creature_watching::k_MiracleSightingTurns + 1;
		EXPECT_FALSE(creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f).learnt);
	}
	// Three quarters of the way, it is nearly learnt
	EXPECT_EQ(creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f).event,
	          creature_watching::LearningEvent::NearlyLearnt);
	turn += creature_watching::k_MiracleSightingTurns + 1;
	const auto learnt = creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f);
	EXPECT_TRUE(learnt.learnt);
	EXPECT_EQ(learnt.event, creature_watching::LearningEvent::Learnt);
	EXPECT_EQ(learnt.meter, 1.0f);
	// It goes on counting, and is told again
	turn += creature_watching::k_MiracleSightingTurns + 1;
	const auto again = creature_watching::SeeMiracle(knowledge, 0, miracles, 13, turn, 3, 1.5f);
	EXPECT_EQ(again.event, creature_watching::LearningEvent::Learnt);
	EXPECT_EQ(knowledge.miraclesSeen[0].count, 18u);
	// 18 is three beyond the 15 needed: the meter no longer shows it
	EXPECT_FALSE(again.meter.has_value());
}

TEST(CreatureWatching, APowerUpTeachesNothingBeforeItsMiracle)
{
	std::vector<creature_watching::MiracleRule> miracles(42, {.timesToSee = 10, .minPhase = 0});
	auto knowledge = creature_watching::StartKnowledge({}, miracles);
	// The fireball's first power-up before the fireball
	EXPECT_TRUE(creature_watching::SeeMiracle(knowledge, 2, miracles, 13, 100, 1, 1.0f).ignored);
	EXPECT_EQ(knowledge.miraclesSeen[2].count, 0u);
	(void)creature_watching::SeeMiracle(knowledge, 1, miracles, 13, 100, 1, 1.0f);
	EXPECT_FALSE(creature_watching::SeeMiracle(knowledge, 2, miracles, 13, 100, 1, 1.0f).ignored);
	// The thirst spell needs the skill of building
	EXPECT_TRUE(creature_watching::SeeMiracle(knowledge, 40, miracles, 13, 100, 1, 1.0f).ignored);
	ASSERT_EQ(creature_watching::MiraclePrerequisite(40)->kind, creature_watching::Prerequisite::Kind::Skill);
	EXPECT_FALSE(creature_watching::MiraclePrerequisite(19).has_value());
}

TEST(CreatureWatching, MimicryStages)
{
	const std::vector<creature_watching::MimicRule> rules {
	    {.name = "Play with toy", .chance = 0.9f, .needsLearningLeash = false, .stageSteps = 1, .copiesDesire = true},
	    {.name = "Steal", .chance = 0.9f, .needsLearningLeash = true, .stageSteps = 1},
	};
	std::optional<creature_watching::Mimicry> mimicry;
	const auto noExtra = [](uint32_t) { return 0u; };
	creature_watching::MimicConditions grown {.phase = 13};
	EXPECT_FALSE(creature_watching::StartMimicry(mimicry, 1, rules, grown, std::nullopt, 0, noExtra));
	creature_watching::MimicConditions young {.phase = 2};
	EXPECT_FALSE(creature_watching::StartMimicry(mimicry, 0, rules, young, std::nullopt, 0, noExtra));
	ASSERT_TRUE(creature_watching::StartMimicry(mimicry, 0, rules, grown, 5u, 100, noExtra));
	EXPECT_EQ(mimicry->stage, creature_watching::MimicStage::Notice);
	EXPECT_EQ(mimicry->limit, 1u);
	EXPECT_EQ(mimicry->stageStart, 100u);
	// The notice stage goes on until one action is done
	EXPECT_TRUE(creature_watching::StageContinues(*mimicry, 150, 10.0f));
	creature_watching::MimicActionDone(mimicry);
	EXPECT_FALSE(creature_watching::StageContinues(*mimicry, 150, 10.0f));
	creature_watching::NextMimicStage(mimicry, rules, 150, noExtra);
	EXPECT_EQ(mimicry->stage, creature_watching::MimicStage::CopyAction);
	EXPECT_EQ(mimicry->done, 0u);
	// A stage ends after 180 seconds at the latest
	EXPECT_TRUE(creature_watching::StageContinues(*mimicry, 150 + 1800, 10.0f));
	EXPECT_FALSE(creature_watching::StageContinues(*mimicry, 150 + 1801, 10.0f));
	creature_watching::NextMimicStage(mimicry, rules, 2000, noExtra);
	EXPECT_EQ(mimicry->stage, creature_watching::MimicStage::CopyDesire);
	creature_watching::NextMimicStage(mimicry, rules, 2100, noExtra);
	EXPECT_FALSE(mimicry.has_value());
	// A rule with no desire stage ends after it copies the action
	ASSERT_TRUE(creature_watching::StartMimicry(mimicry, 1, rules, {.phase = 13, .learningLeashInHand = true}, 5u, 0, noExtra));
	creature_watching::NextMimicStage(mimicry, rules, 10, noExtra);
	creature_watching::NextMimicStage(mimicry, rules, 20, noExtra);
	EXPECT_FALSE(mimicry.has_value());
}

// Stroked while it copies, it goes back to the copy of the action, in the time of the stage that it was in
TEST(CreatureWatching, AStrokeWhileCopyingStartsTheActionCopyAgain)
{
	const std::vector<creature_watching::MimicRule> rules {
	    {.name = "Play with toy", .chance = 0.9f, .needsLearningLeash = false, .stageSteps = 2, .copiesDesire = true},
	};
	const auto noExtra = [](uint32_t) { return 0u; };
	std::optional<creature_watching::Mimicry> mimicry = creature_watching::Mimicry {
	    .rule = 0, .stage = creature_watching::MimicStage::CopyDesire, .done = 1, .limit = 2, .stageStart = 30};
	creature_watching::StrokedWhileMimicking(mimicry, rules, noExtra);
	EXPECT_EQ(mimicry->stage, creature_watching::MimicStage::CopyAction);
	EXPECT_EQ(mimicry->done, 0u);
	EXPECT_EQ(mimicry->limit, 2u);
	EXPECT_EQ(mimicry->stageStart, 30u);
}

TEST(CreatureWatching, MimicChanceIsARankNotARoll)
{
	const std::vector<creature_watching::MimicRule> rules {
	    {.name = "Low", .chance = 0.1f, .needsLearningLeash = false, .stageSteps = 1, .copiesDesire = true},
	    {.name = "High", .chance = 0.5f, .needsLearningLeash = false, .stageSteps = 1},
	    {.name = "Never", .chance = 0.0f, .needsLearningLeash = false, .stageSteps = 1},
	    {.name = "AlsoLow", .chance = 0.1f, .needsLearningLeash = false, .stageSteps = 1},
	};
	const creature_watching::MimicConditions grown {.phase = 13};
	std::optional<creature_watching::Mimicry> mimicry;
	const auto noExtra = [](uint32_t) { return 0u; };
	// Copying nothing, even the lowest rank above 0 is always copied, and nothing at 0
	EXPECT_FALSE(creature_watching::StartMimicry(mimicry, 2, rules, grown, std::nullopt, 0, noExtra));
	ASSERT_TRUE(creature_watching::StartMimicry(mimicry, 0, rules, grown, std::nullopt, 0, noExtra));
	// The same deed again doesn't restart it; a deed of the same rank or higher replaces it, a lower one doesn't
	EXPECT_FALSE(creature_watching::StartMimicry(mimicry, 0, rules, grown, std::nullopt, 0, noExtra));
	ASSERT_TRUE(creature_watching::StartMimicry(mimicry, 3, rules, grown, std::nullopt, 0, noExtra));
	EXPECT_EQ(mimicry->rule, 3u);
	ASSERT_TRUE(creature_watching::StartMimicry(mimicry, 1, rules, grown, std::nullopt, 0, noExtra));
	EXPECT_FALSE(creature_watching::StartMimicry(mimicry, 0, rules, grown, std::nullopt, 0, noExtra));
	EXPECT_EQ(mimicry->rule, 1u);
	// Once it copies a desire, any deed starts it copying anew
	mimicry = creature_watching::Mimicry {.rule = 0, .stage = creature_watching::MimicStage::CopyDesire};
	ASSERT_TRUE(creature_watching::StartMimicry(mimicry, 2, rules, grown, std::nullopt, 0, noExtra));
	EXPECT_EQ(mimicry->rule, 2u);
	EXPECT_EQ(mimicry->stage, creature_watching::MimicStage::Notice);
}

TEST(CreatureWatching, CopyingNeedsAFreeMindAndADeedWithinReach)
{
	const std::vector<creature_watching::MimicRule> rules {
	    {.name = "Deed", .chance = 0.5f, .needsLearningLeash = false, .stageSteps = 1},
	};
	std::optional<creature_watching::Mimicry> mimicry;
	const auto noExtra = [](uint32_t) { return 0u; };
	// A reaction of priority 150 or more keeps it from copying
	EXPECT_FALSE(
	    creature_watching::StartMimicry(mimicry, 0, rules, {.phase = 3, .reactionPriority = 150.0f}, std::nullopt, 0, noExtra));
	EXPECT_TRUE(
	    creature_watching::StartMimicry(mimicry, 0, rules, {.phase = 3, .reactionPriority = 149.0f}, std::nullopt, 0, noExtra));
	// The reach is the count of cells the eyes sweep, taken as metres: 225 cells for a creature of size 1
	EXPECT_EQ(creature_object_beliefs::SweepCells(1.0f), 225u);
	EXPECT_TRUE(creature_watching::WithinCopyReach(224.9f, 225u));
	EXPECT_FALSE(creature_watching::WithinCopyReach(225.0f, 225u));
}

TEST(CreatureWatching, ThingsAreCopiedOnByTheirKind)
{
	using creature_watching::MimicKind;
	using Class = creature_watching::MimicFacts::Class;
	EXPECT_EQ(creature_watching::KindToCopy({.kind = Class::Villager}), MimicKind::Man);
	EXPECT_EQ(creature_watching::KindToCopy({.kind = Class::Villager, .female = true}), MimicKind::Woman);
	EXPECT_EQ(creature_watching::KindToCopy({.kind = Class::Villager, .child = true, .female = true}), MimicKind::Child);
	EXPECT_EQ(creature_watching::KindToCopy({.kind = Class::Animal, .nasty = true}), MimicKind::NastyAnimal);
	EXPECT_EQ(creature_watching::KindToCopy({.kind = Class::Animal}), MimicKind::NiceAnimal);
	EXPECT_EQ(creature_watching::KindToCopy({.kind = Class::Rock}), MimicKind::Rock);
	EXPECT_EQ(creature_watching::KindToCopy({.kind = Class::LooseObject}), MimicKind::LooseObject);
	EXPECT_EQ(creature_watching::KindToCopy({.kind = Class::Other}), MimicKind::Other);
	EXPECT_EQ(static_cast<int>(MimicKind::Creature), 9);
}

TEST(CreatureWatching, TheNearestThingOfTheSameKindIsCopiedOn)
{
	using creature_watching::MimicKind;
	const std::vector<creature_watching::MimicCandidate> candidates {
	    {.kind = MimicKind::Tree, .nearness = 0.9f},  {.kind = MimicKind::Rock, .nearness = 0.2f},
	    {.kind = MimicKind::Rock, .nearness = 0.6f},  {.kind = MimicKind::Rock, .nearness = 0.6f},
	    {.kind = MimicKind::Woman, .nearness = 0.0f},
	};
	// The first of the nearest equals of the kind
	EXPECT_EQ(creature_watching::NearestOfKind(candidates, MimicKind::Rock), 2u);
	EXPECT_EQ(creature_watching::NearestOfKind(candidates, MimicKind::Tree), 0u);
	// None of the kind, or none nearer than nothing
	EXPECT_FALSE(creature_watching::NearestOfKind(candidates, MimicKind::Man).has_value());
	EXPECT_FALSE(creature_watching::NearestOfKind(candidates, MimicKind::Woman).has_value());
}

// Carrying out plans

TEST(CreaturePlanActions, BuildsAgendas)
{
	const auto random = [](uint32_t) { return 0u; };
	const auto* eat = creature_plan_actions::For("EatAfterExamining");
	ASSERT_NE(eat, nullptr);
	EXPECT_FALSE(creature_plan_actions::Agenda(*eat, std::nullopt, {}, {}, random).has_value());
	EXPECT_EQ(creature_plan_actions::Agenda(*eat, 3u, {}, {}, random)->size(), 3u);
	const auto* drink = creature_plan_actions::For("DrinkFromTheSea");
	ASSERT_NE(drink, nullptr);
	// It can always try to drink, but with no place to drink it has no agenda
	EXPECT_TRUE(creature_plan_actions::Possible(*drink, {}));
	EXPECT_FALSE(creature_plan_actions::Agenda(*drink, std::nullopt, {}, {}, random).has_value());
	const auto* wave = creature_plan_actions::For("WaveAtPlayer");
	ASSERT_NE(wave, nullptr);
	creature_plan_actions::Situation situation {.camera = glm::vec2(0.0f, -100.0f)};
	EXPECT_EQ(creature_plan_actions::Agenda(*wave, std::nullopt, {}, situation, random)->size(), 2u);
	EXPECT_EQ(creature_plan_actions::For("CastWarmingSpellOnCreature"), nullptr);
}

TEST(CreaturePlanActions, FishingWalksToTheShoalBringsFoodOutOfTheSeaAndEatsIt)
{
	const auto random = [](uint32_t) { return 0u; };
	const auto* fish = creature_plan_actions::For("FishAndEat");
	ASSERT_NE(fish, nullptr);
	// Only with a fish farm near
	EXPECT_FALSE(creature_plan_actions::Possible(*fish, {}));
	creature_plan_actions::Situation situation;
	situation.fishing = creature_plan_actions::Situation::Fishing {.shoal = {12.0f, 22.0f}, .arriveWithin = 15.0f};
	// Not with nowhere to stand near the shoal
	EXPECT_FALSE(creature_plan_actions::Possible(*fish, situation));
	situation.fishing->standAt = glm::vec2(10.0f, 20.0f);
	const auto agenda = creature_plan_actions::Agenda(*fish, std::nullopt, {}, situation, random);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 3u);
	EXPECT_EQ(agenda->at(0).kind, creature_mind::Step::Kind::Move);
	EXPECT_EQ(agenda->at(0).movement.point, glm::vec2(10.0f, 20.0f));
	EXPECT_FLOAT_EQ(agenda->at(0).movement.maxDistance, 15.0f);
	EXPECT_EQ(agenda->at(1).order.kind, creature_mind::ObjectOrder::Kind::FishFromSea);
	EXPECT_EQ(agenda->at(2).order.kind, creature_mind::ObjectOrder::Kind::Eat);
	EXPECT_EQ(agenda->at(2).effect, creature_mind::Effect::Eat);
}

TEST(CreaturePlanActions, FishingForSomethingOtherThanEatingIsAllowedWithFoodInHand)
{
	const auto* fish = creature_plan_actions::For("FishAndEat");
	ASSERT_NE(fish, nullptr);
	creature_plan_actions::Situation situation;
	situation.fishing = creature_plan_actions::Situation::Fishing {.standAt = glm::vec2(0.0f), .holdingFood = true};
	// It doesn't fish to eat with food already in its hand
	EXPECT_FALSE(creature_plan_actions::Possible(*fish, situation));
}

TEST(CreaturePlanActions, GivingFishToAStorePitFishesTurnsToThePitAndThrowsItIn)
{
	const auto random = [](uint32_t) { return 0u; };
	const auto* give = creature_plan_actions::For("GiveFishToStoragePit");
	ASSERT_NE(give, nullptr);
	EXPECT_EQ(give->target, creature_plan_actions::Target::BuiltStoragePit);
	creature_plan_actions::Situation situation;
	EXPECT_FALSE(creature_plan_actions::Possible(*give, situation));
	// It walks straight to the shoal, even where it couldn't stand to fish for itself
	situation.fishing =
	    creature_plan_actions::Situation::Fishing {.shoal = {10.0f, 20.0f}, .arriveWithin = 15.0f, .height = 12.0f};
	constexpr uint32_t k_Pit = 7;
	const auto agenda = creature_plan_actions::Agenda(*give, k_Pit, {}, situation, random);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 5u);
	EXPECT_EQ(agenda->at(0).movement.point, glm::vec2(10.0f, 20.0f));
	// No step of its own empties the hand: the step that brings food out of the sea needs empty hands
	EXPECT_EQ(agenda->at(1).order.kind, creature_mind::ObjectOrder::Kind::FishFromSea);
	EXPECT_EQ(agenda->at(2).movement.kind, creature_mind::Movement::Kind::ToThrowPosition);
	EXPECT_EQ(agenda->at(2).movement.object, std::optional<uint32_t>(k_Pit));
	EXPECT_FLOAT_EQ(agenda->at(2).movement.maxDistance, 12.0f);
	EXPECT_EQ(agenda->at(2).face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda->at(3).movement.kind, creature_mind::Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda->at(3).seconds, 0.1f);
	// Counted as done as it turns, before it throws
	EXPECT_EQ(agenda->at(3).effect, creature_mind::Effect::Completed);
	EXPECT_EQ(agenda->at(4).order.kind, creature_mind::ObjectOrder::Kind::ThrowInStore);
	EXPECT_EQ(agenda->at(4).order.object, std::optional<uint32_t>(k_Pit));
	// With food already in its hand it goes straight to the pit
	situation.fishing->holdingFood = true;
	const auto holding = creature_plan_actions::Agenda(*give, k_Pit, {}, situation, random);
	ASSERT_TRUE(holding.has_value());
	ASSERT_EQ(holding->size(), 3u);
	EXPECT_EQ(holding->at(0).movement.kind, creature_mind::Movement::Kind::ToThrowPosition);
}

TEST(CreaturePlanActions, TakingFishHomePutsItDownWithinItsHeightOfHome)
{
	const auto random = [](uint32_t) { return 0u; };
	const auto* take = creature_plan_actions::For("TakeFishHome");
	ASSERT_NE(take, nullptr);
	creature_plan_actions::Situation situation;
	situation.fishing =
	    creature_plan_actions::Situation::Fishing {.shoal = {10.0f, 20.0f}, .arriveWithin = 15.0f, .height = 12.0f};
	// Not without a home
	EXPECT_FALSE(creature_plan_actions::Possible(*take, situation));
	situation.home = glm::vec2(100.0f, 200.0f);
	const auto agenda = creature_plan_actions::Agenda(*take, std::nullopt, {}, situation, random);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 4u);
	EXPECT_EQ(agenda->at(1).order.kind, creature_mind::ObjectOrder::Kind::FishFromSea);
	EXPECT_EQ(agenda->at(2).movement.point, glm::vec2(100.0f, 200.0f));
	EXPECT_FLOAT_EQ(agenda->at(2).movement.maxDistance, 12.0f);
	EXPECT_EQ(agenda->at(3).order.kind, creature_mind::ObjectOrder::Kind::PutDown);
}

// The model of what is learnt

TEST(CreatureMindModel, FreshOpinionsComeFromTheActionTable)
{
	creature_desires::Desires desires;
	const std::array k_Opinions {1.0f, 0.0f, 0.04f, -0.5f};
	const auto learnt = creature_mind_model::Fresh(desires, k_Opinions, {});
	EXPECT_TRUE(std::ranges::equal(learnt.opinions, k_Opinions));
	EXPECT_EQ(learnt.turnsSinceDone.size(), k_Opinions.size());
}

namespace
{
/// The places of the two trees of a desire
constexpr auto k_ActivityTree = static_cast<size_t>(creature_mind_model::TreeKind::Activity);
constexpr auto k_ActOnTree = static_cast<size_t>(creature_mind_model::TreeKind::ActOn);
} // namespace

TEST(CreatureMindModel, StartingLessonIsAnEnemyVillagerGoodToEat)
{
	const auto lesson = creature_mind_model::StartingLesson();
	EXPECT_EQ(lesson.belief.type, creature_tree::belief_types::k_Villager);
	EXPECT_EQ(lesson.belief.Value(Attribute::Allegiance), creature_tree::allegiance::k_Enemy);
	EXPECT_EQ(lesson.belief.Value(Attribute::Life), 1u);
	// A thing that does not burn has the second value of the attribute
	EXPECT_EQ(lesson.belief.Value(Attribute::OnFire), 1u);
	EXPECT_EQ(creature_tree::OnFireValue(false), 1u);
	EXPECT_EQ(creature_tree::OnFireValue(true), 0u);
	EXPECT_EQ(lesson.belief.Value(Attribute::Sex), 0u);
	EXPECT_EQ(lesson.belief.Value(Attribute::VillagerJob), 0u);
	EXPECT_FALSE(lesson.belief.Value(Attribute::Tribe).has_value());
	EXPECT_FLOAT_EQ(lesson.feedback, 0.8f);
	EXPECT_EQ(lesson.lessonKind, 2);
	EXPECT_EQ(lesson.action, 0);
	EXPECT_EQ(lesson.belief.x, 0);
	EXPECT_EQ(lesson.belief.z, 0);
}

TEST(CreatureMindModel, StartingLessonMakesEverythingGoodToEat)
{
	creature_desires::Desires desires;
	auto learnt = creature_mind_model::Fresh(desires, std::vector<float>(3, 0.0f), {});
	creature_mind_model::TeachStartingLesson(learnt, k_HungerAttributes);
	const auto& episodes = learnt.episodes[k_ActOnTree][static_cast<size_t>(Desire::Hunger)];
	ASSERT_EQ(episodes.size(), 1u);
	// One example makes the tree one leaf, so each thing scores as the example does. The feedback 0.8 is within a
	// quarter of the 0.6 step, which comes first.
	const auto& tree = learnt.trees[k_ActOnTree][static_cast<size_t>(Desire::Hunger)];
	EXPECT_FLOAT_EQ(creature_tree::Evaluate(tree, Thing(k_Food, 0)), 0.6f);
	EXPECT_FLOAT_EQ(creature_tree::Evaluate(tree, Thing(k_Villager, 1)), 0.6f);
	// The other trees stay empty
	EXPECT_TRUE(learnt.episodes[k_ActivityTree][static_cast<size_t>(Desire::Hunger)].empty());
	EXPECT_TRUE(learnt.episodes[k_ActOnTree][static_cast<size_t>(Desire::Anger)].empty());
}

TEST(CreatureMindModel, StartingLessonOnlyWhenNothingIsKnownAboutWhatToEat)
{
	creature_desires::Desires desires;
	auto learnt = creature_mind_model::Fresh(desires, std::vector<float>(3, 0.0f), {});
	creature_mind_model::Learn(learnt, creature_mind_model::TreeKind::ActOn, Desire::Hunger,
	                           {.belief = Thing(k_Food, 0), .feedback = -1.0f}, k_HungerAttributes);
	creature_mind_model::TeachStartingLesson(learnt, k_HungerAttributes);
	const auto& episodes = learnt.episodes[k_ActOnTree][static_cast<size_t>(Desire::Hunger)];
	ASSERT_EQ(episodes.size(), 1u);
	EXPECT_FLOAT_EQ(episodes[0].feedback, -1.0f);
	// A second time adds nothing either
	auto fresh = creature_mind_model::Fresh(desires, std::vector<float>(3, 0.0f), {});
	creature_mind_model::TeachStartingLesson(fresh, k_HungerAttributes);
	creature_mind_model::TeachStartingLesson(fresh, k_HungerAttributes);
	EXPECT_EQ(fresh.episodes[k_ActOnTree][static_cast<size_t>(Desire::Hunger)].size(), 1u);
}

TEST(CreatureMindModel, LearningRebuildsTrees)
{
	creature_desires::Desires desires;
	auto learnt = creature_mind_model::Fresh(desires, std::vector<float>(10, 0.0f), {});
	creature_mind_model::Learn(learnt, creature_mind_model::TreeKind::Activity, Desire::Hunger,
	                           {.belief = Thing(k_Food, 0), .feedback = -1.0f}, k_HungerAttributes);
	creature_mind_model::Learn(learnt, creature_mind_model::TreeKind::Activity, Desire::Hunger,
	                           {.belief = Thing(k_Villager, 1), .feedback = 1.0f}, k_HungerAttributes);
	const auto& tree = learnt.trees[0][static_cast<size_t>(Desire::Hunger)];
	EXPECT_LT(creature_tree::Evaluate(tree, Thing(k_Food, 0)), 0.0f);
	EXPECT_GT(creature_tree::Evaluate(tree, Thing(k_Villager, 1)), 0.0f);
	for (int i = 0; i < 12; ++i)
	{
		creature_mind_model::Think(learnt, "thought");
	}
	EXPECT_EQ(learnt.thoughts.size(), creature_mind_model::k_MaxThoughts);
}

TEST(CreaturePlanActions, ACreatureRunsFromWhatFrightensIt)
{
	// Other creatures, bats, vultures, lions and miracles, not villagers
	const auto* run = creature_plan_actions::For("RunAwayFromObject");
	ASSERT_NE(run, nullptr);
	EXPECT_EQ(run->target, creature_plan_actions::Target::Frightening);
}

// What a creature thinks its player wants

TEST(PerceivedDesires, SeenDesiresAddUpHeldToOneAndFade)
{
	using namespace openblack::creature_perceived_desires;
	PerceivedDesires desires;
	Increase(desires, 2, 1.0f);
	Increase(desires, 2, 1.0f);
	EXPECT_FLOAT_EQ(desires.player[2], 1.0f);
	Increase(desires, 40, 1.0f);
	IncreaseTown(desires, 16, 0.5f);
	IncreaseTown(desires, 17, 0.5f);
	EXPECT_FLOAT_EQ(desires.town[16], 0.5f);
	Fade(desires);
	EXPECT_FLOAT_EQ(desires.player[2], k_TurnFade);
	// The last activated one wanted at all is the dominant, and those found are forgotten
	Increase(desires, 1, 0.2f);
	Increase(desires, 5, 0.2f);
	const auto dominant = TakeDominant(desires, [](size_t desire) { return desire != 5; });
	EXPECT_EQ(dominant, 2u);
	EXPECT_FLOAT_EQ(desires.player[1], 0.0f);
	EXPECT_FLOAT_EQ(desires.player[5], 0.2f);
	EXPECT_FALSE(TakeDominant(desires, [](size_t desire) { return desire != 5; }).has_value());
}

TEST(PerceivedDesires, ACreatureSeesTwoThirdsOfAHalfTurnEitherWayOrInItsCell)
{
	using namespace openblack::creature_perceived_desires;
	EXPECT_TRUE(CanSeePos(0, 0x2AA, false));
	EXPECT_FALSE(CanSeePos(0, 0x2AB, false));
	// Across the turn's end
	EXPECT_TRUE(CanSeePos(0x7FF, 0x2A9, false));
	EXPECT_TRUE(CanSeePos(0, 0x400, true));
}

namespace
{
namespace town_compassion = creature_town_compassion;

/// A town that wants food (0), wood (1) and protection (3), in that order, and relaxation (15) last; its other desires
/// have no actions or aren't felt
struct FakeTown
{
	std::array<float, town_compassion::k_TownDesireCount> felt {};
	std::vector<uint32_t> order {0, 1, 3, 15, 2, 4};
	town_compassion::DesireActions actions {};

	FakeTown()
	{
		felt.at(0) = 0.8f;
		felt.at(1) = 0.5f;
		felt.at(3) = 0.3f;
		felt.at(15) = 0.2f;
		actions.at(0) = {29, 28};
		actions.at(1) = {32};
		actions.at(3) = {286};
		actions.at(4) = {38, 101};
		actions.at(15) = {288};
	}
};

std::function<uint32_t(uint32_t)> Always(uint32_t value)
{
	return [value](uint32_t) { return value; };
}
} // namespace

TEST(CreatureTownCompassion, ATownsDesiresToHelpWithRunInItsOrderToTheFirstItDoesntFeel)
{
	FakeTown town;
	EXPECT_EQ(town_compassion::DesiresToHelp(town.order, town.felt, town.actions), (std::vector<uint32_t> {0, 1, 3, 15}));
	// Mercy has actions but isn't felt: the list stops there, even with desires after it
	town.order = {0, 4, 1};
	EXPECT_EQ(town_compassion::DesiresToHelp(town.order, town.felt, town.actions), (std::vector<uint32_t> {0}));
	// Desires without actions are passed over, felt or not
	town.felt.at(2) = 0.9f;
	town.order = {2, 1};
	EXPECT_EQ(town_compassion::DesiresToHelp(town.order, town.felt, town.actions), (std::vector<uint32_t> {1}));
}

TEST(CreatureTownCompassion, ItTakesUpTheFirstDesireAndRemembersMostOfHowMuchTheTownFeelsIt)
{
	const FakeTown town;
	const auto desires = town_compassion::DesiresToHelp(town.order, town.felt, town.actions);
	town_compassion::State state;
	town_compassion::Settle(state, desires, town.felt);
	ASSERT_EQ(state.desire, 0u);
	EXPECT_FLOAT_EQ(state.remembered, 0.8f * 0.85f);
	EXPECT_EQ(town_compassion::Actions(state, town.actions, std::nullopt), (std::vector<uint32_t> {29, 28}));
	// One it is already helping with stays
	state.desire = 3;
	town_compassion::Settle(state, desires, town.felt);
	EXPECT_EQ(state.desire, 3u);
	// With nothing to help with, nothing changes
	state.desire = 7;
	town_compassion::Settle(state, {}, town.felt);
	EXPECT_EQ(state.desire, 7u);
}

TEST(CreatureTownCompassion, ItMovesOnAsItPlansOnlyWhenCompassionLeadsAndHalfAMinuteHasGone)
{
	town_compassion::State state {.lastTurn = 100};
	EXPECT_FALSE(town_compassion::MovesOnWhilePlanning(state, true, false, 130));
	EXPECT_TRUE(town_compassion::MovesOnWhilePlanning(state, true, false, 131));
	EXPECT_FALSE(town_compassion::MovesOnWhilePlanning(state, false, false, 131));
	EXPECT_FALSE(town_compassion::MovesOnWhilePlanning(state, true, true, 131));
	state.choosesFreely = false;
	EXPECT_FALSE(town_compassion::MovesOnWhilePlanning(state, true, false, 131));

	const FakeTown town;
	const auto desires = town_compassion::DesiresToHelp(town.order, town.felt, town.actions);
	town_compassion::State going {.desire = 15, .index = 3, .timesKept = 2};
	town_compassion::MoveOnWhilePlanning(going, desires, town.felt, 500);
	EXPECT_EQ(going.desire, 0u);
	EXPECT_EQ(going.index, 0u);
	EXPECT_EQ(going.lastTurn, 500u);
	EXPECT_EQ(going.timesKept, 0u);
	town_compassion::MoveOn(going, {}, town.felt);
	EXPECT_FALSE(going.desire.has_value());
}

TEST(CreatureTownCompassion, HavingHelpedItStaysWithADesireTheTownStillFeelsMoreAFewTimes)
{
	FakeTown town;
	const auto desires = town_compassion::DesiresToHelp(town.order, town.felt, town.actions);
	town_compassion::State state;
	town_compassion::Settle(state, desires, town.felt);
	// The town still wants food more than it remembered: with the low toss it stays three times more
	for (uint32_t times = 1; times <= 3; ++times)
	{
		town_compassion::FinishedHelping(state, desires, town.felt, 10 * times, Always(0));
		EXPECT_EQ(state.desire, 0u);
		EXPECT_EQ(state.timesKept, times);
		EXPECT_EQ(state.lastTurn, 10 * times);
	}
	town_compassion::FinishedHelping(state, desires, town.felt, 40, Always(0));
	EXPECT_EQ(state.desire, 1u);
	EXPECT_EQ(state.timesKept, 0u);
	EXPECT_FLOAT_EQ(state.remembered, 0.5f * 0.85f);
	// Once the town feels it no more than remembered, it moves on at once
	town.felt.at(1) = 0.4f;
	town_compassion::FinishedHelping(state, desires, town.felt, 50, Always(1));
	EXPECT_EQ(state.desire, 3u);
	// It never stays with relaxation
	state = {.desire = 15, .index = 3, .remembered = 0.0f};
	town_compassion::FinishedHelping(state, desires, town.felt, 60, Always(1));
	EXPECT_EQ(state.desire, 0u);
	// Held to a desire by the leash, it doesn't move on
	state = {.desire = 1, .index = 1, .choosesFreely = false};
	town_compassion::FinishedHelping(state, desires, town.felt, 70, Always(1));
	EXPECT_EQ(state.desire, 1u);
	EXPECT_EQ(state.lastTurn, 0u);
}

TEST(CreatureTownCompassion, HurtPeopleMakeItThinkOfHealingFirstOnceItHasSeenHealingEnough)
{
	EXPECT_TRUE(town_compassion::HealFirst(1, false, 7.0f, 10.0f, Always(0)));
	EXPECT_FALSE(town_compassion::HealFirst(1, false, 7.0f, 10.0f, Always(1)));
	EXPECT_FALSE(town_compassion::HealFirst(1, false, 6.0f, 10.0f, Always(0)));
	EXPECT_FALSE(town_compassion::HealFirst(0, false, 10.0f, 10.0f, Always(0)));
	EXPECT_FALSE(town_compassion::HealFirst(1, true, 10.0f, 10.0f, Always(0)));

	const FakeTown town;
	const town_compassion::State state {.desire = 0};
	EXPECT_EQ(town_compassion::Actions(state, town.actions, 101u), (std::vector<uint32_t> {101, 28}));
	// Healing comes first even with no desire to help with
	EXPECT_EQ(town_compassion::Actions({}, town.actions, 101u), (std::vector<uint32_t> {101}));
}

TEST(CreatureTownCompassion, WhatACreatureMakesOfATown)
{
	EXPECT_EQ(town_compassion::ReligiousBelief(0.19f), 0u);
	EXPECT_EQ(town_compassion::ReligiousBelief(0.2f), 1u);
	EXPECT_EQ(town_compassion::ReligiousBelief(0.5f), 2u);
	EXPECT_EQ(town_compassion::ReligiousBelief(0.6f), 3u);
	EXPECT_EQ(town_compassion::ReligiousBelief(9.0f), 3u);
	EXPECT_EQ(town_compassion::NeedsMost(4), 4u);
	EXPECT_EQ(town_compassion::NeedsMost(-1), 16u);
	EXPECT_EQ(town_compassion::TownSize(19), 0u);
	EXPECT_EQ(town_compassion::TownSize(20), 1u);
	EXPECT_EQ(town_compassion::TownSize(40), 2u);
}

// What a script teaches

TEST(CreatureWatching, AScriptTeachesAndTakesAwaySkillsAndMiracles)
{
	const std::vector<creature_watching::SkillRule> skills(6);
	const std::vector<creature_watching::MiracleRule> miracles(42);
	auto knowledge = creature_watching::StartKnowledge(skills, miracles);
	using creature_watching::KnownList;
	// Newly known the first time only
	EXPECT_TRUE(creature_watching::SetKnown(knowledge, KnownList::Skill, 4, true, 0));
	EXPECT_FALSE(creature_watching::SetKnown(knowledge, KnownList::Skill, 4, true, 0));
	EXPECT_TRUE(knowledge.skillsKnown[4]);
	EXPECT_EQ(knowledge.skillsSeen[4].count, 0u);
	// A miracle taught counts as seen the times given, even when it was known already
	knowledge.miraclesSeen[10].count = 3;
	EXPECT_TRUE(creature_watching::SetKnown(knowledge, KnownList::Miracle, 10, true, 15));
	EXPECT_EQ(knowledge.miraclesSeen[10].count, 15u);
	EXPECT_FALSE(creature_watching::SetKnown(knowledge, KnownList::Miracle, 10, true, 12));
	EXPECT_EQ(knowledge.miraclesSeen[10].count, 12u);
	// Forgetting leaves the sightings
	EXPECT_FALSE(creature_watching::SetKnown(knowledge, KnownList::Miracle, 10, false, 0));
	EXPECT_FALSE(knowledge.miraclesKnown[10]);
	EXPECT_EQ(knowledge.miraclesSeen[10].count, 12u);
	EXPECT_FALSE(creature_watching::SetKnown(knowledge, KnownList::Skill, 4, false, 0));
	EXPECT_FALSE(knowledge.skillsKnown[4]);
	// Past the lists nothing happens
	EXPECT_FALSE(creature_watching::SetKnown(knowledge, KnownList::Skill, 6, true, 0));
	EXPECT_FALSE(creature_watching::SetKnown(knowledge, KnownList::Miracle, 42, true, 9));
}

TEST(CreatureWatching, TaughtSightingsAreTheTimesNeededRoundedDown)
{
	EXPECT_EQ(creature_watching::TaughtSightings(10, 1.5f), 15u);
	// 9 * 1.7 is 15.3
	EXPECT_EQ(creature_watching::TaughtSightings(9, 1.7f), 15u);
	// 10 * 0.7 is just short of 7, but the float product rounds to 7
	EXPECT_EQ(creature_watching::TaughtSightings(10, 0.7f), 7u);
	EXPECT_EQ(creature_watching::TaughtSightings(0, 4.0f), 0u);
}

TEST(CreatureWatching, AnActionNeedsItsSkillsAndMiracleKnown)
{
	const std::vector<creature_watching::SkillRule> skills(6);
	const std::vector<creature_watching::MiracleRule> miracles(42);
	auto knowledge = creature_watching::StartKnowledge(skills, miracles);
	EXPECT_TRUE(creature_watching::KnowsWhatItNeeds(knowledge, {}));
	const creature_watching::ActionNeeds fishing {.skills = {4, std::nullopt}};
	const creature_watching::ActionNeeds casting {.miracle = 21};
	const creature_watching::ActionNeeds both {.skills = {4, 5}, .miracle = 21};
	EXPECT_FALSE(creature_watching::KnowsWhatItNeeds(knowledge, fishing));
	EXPECT_FALSE(creature_watching::KnowsWhatItNeeds(knowledge, casting));
	knowledge.skillsKnown[4] = true;
	EXPECT_TRUE(creature_watching::KnowsWhatItNeeds(knowledge, fishing));
	knowledge.miraclesKnown[21] = true;
	EXPECT_TRUE(creature_watching::KnowsWhatItNeeds(knowledge, casting));
	EXPECT_FALSE(creature_watching::KnowsWhatItNeeds(knowledge, both));
	knowledge.skillsKnown[5] = true;
	EXPECT_TRUE(creature_watching::KnowsWhatItNeeds(knowledge, both));
	// A need past the lists is never known
	EXPECT_FALSE(creature_watching::KnowsWhatItNeeds(knowledge, {.skills = {9, std::nullopt}}));
}

TEST(CreatureLearning, SeeingTheHandMakesADesireGrowFaster)
{
	creature_desires::DesireState state;
	state.increaseSeconds = 100.0f;
	creature_learning::SpeedUpAfterSeeingPlayer(state, 2.5f, 1.0f, 1200.0f);
	EXPECT_FLOAT_EQ(state.increaseSeconds, 40.0f);
	// Each time it sees the hand, the desire grows faster again, but never faster than the least time
	creature_learning::SpeedUpAfterSeeingPlayer(state, 2.5f, 1.0f, 1200.0f);
	EXPECT_FLOAT_EQ(state.increaseSeconds, 16.0f);
	state.increaseSeconds = 2.0f;
	creature_learning::SpeedUpAfterSeeingPlayer(state, 2.5f, 1.0f, 1200.0f);
	EXPECT_FLOAT_EQ(state.increaseSeconds, 1.0f);
	// A speed-up below 1 makes it slower, but never slower than the most time
	state.increaseSeconds = 1000.0f;
	creature_learning::SpeedUpAfterSeeingPlayer(state, 0.5f, 1.0f, 1200.0f);
	EXPECT_FLOAT_EQ(state.increaseSeconds, 1200.0f);
}

TEST(CreatureLearning, AnActionNotCarriedOutGivesItsPlaceToTheNext)
{
	std::vector<creature_learning::Context> contexts;
	EXPECT_FALSE(creature_learning::Remember(contexts, {.action = 1}).has_value());
	// The first action was cut short: the second takes its place
	EXPECT_FALSE(creature_learning::Remember(contexts, {.action = 2}).has_value());
	ASSERT_EQ(contexts.size(), 1u);
	EXPECT_EQ(contexts.back().action, 2u);
	// Carried out to its end, it stays and the next one comes after it
	contexts.back().done = true;
	EXPECT_FALSE(creature_learning::Remember(contexts, {.action = 3}).has_value());
	ASSERT_EQ(contexts.size(), 2u);
	EXPECT_EQ(contexts.front().action, 2u);
	EXPECT_FALSE(contexts.front().running);
}

TEST(CreatureLearning, TheSixthActionMakesItForgetTheOldest)
{
	std::vector<creature_learning::Context> contexts;
	for (uint32_t i = 0; i < creature_learning::k_MaxContexts; ++i)
	{
		EXPECT_FALSE(creature_learning::Remember(contexts, {.action = i, .ordered = i == 0}).has_value());
		contexts.back().done = true;
	}
	const auto forgotten = creature_learning::Remember(contexts, {.action = 9});
	ASSERT_TRUE(forgotten.has_value());
	EXPECT_EQ(forgotten->action, 0u);
	EXPECT_TRUE(forgotten->ordered);
	EXPECT_EQ(contexts.size(), creature_learning::k_MaxContexts);
	EXPECT_EQ(contexts.front().action, 1u);
	EXPECT_EQ(contexts.back().action, 9u);
}

TEST(CreatureLearning, AForgottenOrderTeachesApprovalWhenItsDesireLearnsFromOrders)
{
	constexpr float k_Approval = 0.3f;
	const creature_learning::DesireRules approves {.approvesOrders = true};
	const creature_learning::DesireRules ignores {.approvesOrders = false};
	const creature_learning::Context ordered {.action = 4, .ordered = true, .done = true};
	const creature_learning::Context ownChoice {.action = 4, .done = true};
	EXPECT_EQ(creature_learning::ForgottenOrderApproval(ordered, approves, k_Approval), k_Approval);
	EXPECT_FALSE(creature_learning::ForgottenOrderApproval(ordered, ignores, k_Approval).has_value());
	EXPECT_FALSE(creature_learning::ForgottenOrderApproval(ownChoice, approves, k_Approval).has_value());
	// A stroke or a slap for the order does not stop the approval
	auto slapped = ordered;
	slapped.credited = -1.0f;
	EXPECT_EQ(creature_learning::ForgottenOrderApproval(slapped, approves, k_Approval), k_Approval);
}
