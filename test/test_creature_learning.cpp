/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindModel.h"
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

TEST(CreatureDecisionTree, KeepsTheNewestSixteenExamples)
{
	std::vector<creature_tree::Episode> episodes;
	for (int i = 0; i < 20; ++i)
	{
		creature_tree::AddEpisode(episodes, {.belief = Thing(k_Food, 0), .feedback = static_cast<float>(i) / 20.0f});
	}
	ASSERT_EQ(episodes.size(), creature_tree::k_MaxEpisodes);
	EXPECT_FLOAT_EQ(episodes.back().feedback, 19.0f / 20.0f);
	EXPECT_FLOAT_EQ(episodes.front().feedback, 4.0f / 20.0f);
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
	EXPECT_FLOAT_EQ(creature_planner::Priority(1.0f, std::nullopt, 0.5f, 0.1f, std::nullopt), 50.0f);
	EXPECT_FLOAT_EQ(creature_planner::Priority(1.0f, 1.0f, 0.5f, 0.1f, std::nullopt), 500.0f);
	EXPECT_FLOAT_EQ(creature_planner::Priority(1.0f, std::nullopt, 0.0f, 0.1f, std::nullopt), 1.0f);
	EXPECT_TRUE(creature_planner::ShouldSwitch(21.0f, 10.0f));
	EXPECT_FALSE(creature_planner::ShouldSwitch(20.0f, 10.0f));
	EXPECT_TRUE(creature_planner::ShouldSwitch(1.0f, 0.0f));
}

TEST(CreaturePlanner, PlansTheMostUsefulGoal)
{
	const std::vector<creature_planner::ObjectCandidate> objects {
	    {.id = 1, .distance = 10.0f, .usefulness = 0.0f},
	    {.id = 2, .distance = 150.0f, .usefulness = 0.82f},
	};
	const std::vector<creature_planner::ActionCandidate> actions {{.action = 12, .needsObject = true}};
	const auto plan = creature_planner::PlanDesire(Desire::Hunger, 1.0f, objects, 0.05f, actions);
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->object, 2u);
	EXPECT_FLOAT_EQ(plan->goalUsefulness, 0.82f);
	// Without anything learnt the nearer wins
	const std::vector<creature_planner::ObjectCandidate> unknown {
	    {.id = 1, .distance = 10.0f},
	    {.id = 2, .distance = 150.0f},
	};
	EXPECT_EQ(creature_planner::PlanDesire(Desire::Hunger, 1.0f, unknown, 0.05f, actions)->object, 1u);
	// An action that needs a goal can't be planned without one
	EXPECT_FALSE(creature_planner::PlanDesire(Desire::Hunger, 1.0f, {}, 0.05f, actions).has_value());
}

TEST(CreaturePlanner, OpinionBreaksTies)
{
	const std::vector<creature_planner::ActionCandidate> actions {{.action = 1, .opinion = 1.0f, .turnsSinceDone = 0},
	                                                              {.action = 2, .opinion = -1.0f, .turnsSinceDone = 0}};
	EXPECT_EQ(creature_planner::PlanDesire(Desire::Rest, 0.5f, {}, 0.02f, actions)->action, 1u);
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
	EXPECT_FALSE(creature_planner::Choose(state, 15.0f).has_value());
}

TEST(CreaturePlanner, ChoosesOnlyWhatIsPressingEnough)
{
	creature_planner::PlannerState state;
	state.best.at(0) = creature_planner::Plan {.desire = Desire::Impress, .action = 1, .priority = 30.0f};
	EXPECT_FALSE(creature_planner::Choose(state, 40.0f).has_value());
	EXPECT_TRUE(creature_planner::Choose(state, 15.0f).has_value());
	state.current = creature_planner::Plan {.desire = Desire::Hunger, .action = 2, .priority = 20.0f};
	EXPECT_FALSE(creature_planner::Choose(state, 15.0f).has_value());
	state.best.at(0)->priority = 41.0f;
	EXPECT_TRUE(creature_planner::Choose(state, 15.0f).has_value());
}

// Learning from feedback

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
	creature_learning::Context running {.action = 1, .running = true, .windowSeconds = 20.0f};
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(running), 1.0f);
	creature_learning::Context finished {.action = 2, .running = false, .secondsSince = 5.0f, .windowSeconds = 20.0f};
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(finished), 0.75f);
	finished.secondsSince = 25.0f;
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(finished), 0.0f);
	creature_learning::Context unlearnable {.action = 3, .learnable = false};
	EXPECT_FLOAT_EQ(creature_learning::LearningPriority(unlearnable), 0.0f);

	std::vector<creature_learning::Context> contexts;
	creature_learning::Remember(contexts, {.action = 1, .windowSeconds = 20.0f});
	creature_learning::Age(contexts, 1.0f);
	creature_learning::Remember(contexts, {.action = 2, .windowSeconds = 10.0f});
	creature_learning::Age(contexts, 1.0f);
	EXPECT_FALSE(contexts[0].running);
	EXPECT_EQ(creature_learning::BestContext(contexts), 1u);
	contexts[1].running = false;
	creature_learning::Age(contexts, 9.0f);
	// The older action's longer window now gives it the bigger share
	EXPECT_EQ(creature_learning::BestContext(contexts), 0u);
	for (uint32_t i = 0; i < 20; ++i)
	{
		creature_learning::Remember(contexts, {.action = i});
	}
	EXPECT_EQ(contexts.size(), creature_learning::k_MaxContexts);
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
	creature_learning::MakeLeastDominant(desires, Desire::Hunger, 1.3f);
	EXPECT_NEAR(desires[Desire::Hunger].value, 0.2f / 1.3f, 1e-6f);
	creature_learning::MakeFullyDominant(desires, Desire::Hunger);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 2.0f);
	creature_learning::AfterAction(desires, Desire::Hunger, 0.3f);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 0.6f);
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
	const auto never = [] { return 0.99f; };
	const auto always = [] { return 0.0f; };
	const auto noExtra = [](uint32_t) { return 0u; };
	creature_watching::MimicConditions grown {.phase = 13};
	EXPECT_FALSE(creature_watching::StartMimicry(mimicry, 0, rules, grown, std::nullopt, never));
	EXPECT_FALSE(creature_watching::StartMimicry(mimicry, 1, rules, grown, std::nullopt, always));
	creature_watching::MimicConditions young {.phase = 2};
	EXPECT_FALSE(creature_watching::StartMimicry(mimicry, 0, rules, young, std::nullopt, always));
	ASSERT_TRUE(creature_watching::StartMimicry(mimicry, 0, rules, grown, 5u, always));
	EXPECT_EQ(mimicry->stage, creature_watching::MimicStage::Notice);
	creature_watching::StepMimicry(mimicry, rules, noExtra);
	creature_watching::StepMimicry(mimicry, rules, noExtra);
	EXPECT_EQ(mimicry->stage, creature_watching::MimicStage::CopyAction);
	creature_watching::StepMimicry(mimicry, rules, noExtra);
	creature_watching::StepMimicry(mimicry, rules, noExtra);
	EXPECT_EQ(mimicry->stage, creature_watching::MimicStage::CopyDesire);
	creature_watching::StepMimicry(mimicry, rules, noExtra);
	creature_watching::StepMimicry(mimicry, rules, noExtra);
	EXPECT_FALSE(mimicry.has_value());
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
	EXPECT_FALSE(creature_plan_actions::Possible(*drink, {}));
	const auto* wave = creature_plan_actions::For("WaveAtPlayer");
	ASSERT_NE(wave, nullptr);
	creature_plan_actions::Situation situation {.camera = glm::vec2(0.0f, -100.0f)};
	EXPECT_EQ(creature_plan_actions::Agenda(*wave, std::nullopt, {}, situation, random)->size(), 2u);
	EXPECT_EQ(creature_plan_actions::For("CastFireball"), nullptr);
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
	// Its hand emptied first when it holds something it can't eat
	situation.fishing->putDownFirst = true;
	const auto emptied = creature_plan_actions::Agenda(*fish, std::nullopt, {}, situation, random);
	ASSERT_TRUE(emptied.has_value());
	ASSERT_EQ(emptied->size(), 4u);
	EXPECT_EQ(emptied->at(1).order.kind, creature_mind::ObjectOrder::Kind::PutDown);
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
	EXPECT_EQ(give->target, creature_plan_actions::Target::StoragePit);
	creature_plan_actions::Situation situation;
	EXPECT_FALSE(creature_plan_actions::Possible(*give, situation));
	// It walks straight to the shoal, even where it couldn't stand to fish for itself
	situation.fishing = creature_plan_actions::Situation::Fishing {
	    .shoal = {10.0f, 20.0f}, .arriveWithin = 15.0f, .putDownFirst = true, .height = 12.0f};
	constexpr uint32_t k_Pit = 7;
	const auto agenda = creature_plan_actions::Agenda(*give, k_Pit, {}, situation, random);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 6u);
	EXPECT_EQ(agenda->at(0).movement.point, glm::vec2(10.0f, 20.0f));
	EXPECT_EQ(agenda->at(1).order.kind, creature_mind::ObjectOrder::Kind::PutDown);
	EXPECT_EQ(agenda->at(2).order.kind, creature_mind::ObjectOrder::Kind::FishFromSea);
	EXPECT_EQ(agenda->at(3).movement.kind, creature_mind::Movement::Kind::ToThrowPosition);
	EXPECT_EQ(agenda->at(3).movement.object, std::optional<uint32_t>(k_Pit));
	EXPECT_FLOAT_EQ(agenda->at(3).movement.maxDistance, 12.0f);
	EXPECT_EQ(agenda->at(3).face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda->at(4).movement.kind, creature_mind::Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda->at(4).seconds, 0.1f);
	// Counted as done as it turns, before it throws
	EXPECT_EQ(agenda->at(4).effect, creature_mind::Effect::Completed);
	EXPECT_EQ(agenda->at(5).order.kind, creature_mind::ObjectOrder::Kind::ThrowInStore);
	EXPECT_EQ(agenda->at(5).order.object, std::optional<uint32_t>(k_Pit));
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

TEST(CreatureMindModel, LearningRebuildsTrees)
{
	creature_desires::Desires desires;
	auto learnt = creature_mind_model::Fresh(desires, 10, {});
	creature_mind_model::Learn(learnt, creature_mind_model::TreeKind::ActOn, Desire::Hunger,
	                           {.belief = Thing(k_Food, 0), .feedback = -1.0f}, k_HungerAttributes);
	creature_mind_model::Learn(learnt, creature_mind_model::TreeKind::ActOn, Desire::Hunger,
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
	// 10 * 0.7 falls just short of 7 at full precision
	EXPECT_EQ(creature_watching::TaughtSightings(10, 0.7f), 6u);
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
