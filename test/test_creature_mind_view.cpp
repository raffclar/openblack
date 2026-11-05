/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMindView.h"
#include "Creature/CreaturePlanner.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/CreatureMindLogSystem.h"

using namespace openblack;
using namespace openblack::creature_mind_view;
using creature_desires::Desire;
using creature_desires::Desires;
using creature_planner::Plan;
using creature_planner::PlannerState;

namespace
{
constexpr float k_TurnsPerSecond = 10.0f;

/// Desires with every desire turned off, so that each test turns on only the desires it looks at
Desires NoDesires()
{
	Desires desires;
	for (auto& desire : desires.desires)
	{
		desire.activated = false;
	}
	return desires;
}

creature_desires::Source SourceOf(uint32_t type, float value, float threshold)
{
	return {.type = type, .value = value, .threshold = threshold, .multiplier = 0.99f, .drive = 2.5f};
}

Plan PlanFor(Desire desire, uint32_t action, float priority, std::optional<uint32_t> object = std::nullopt)
{
	return {.desire = desire, .action = action, .object = object, .priority = priority};
}
} // namespace

TEST(CreatureMindView, ShowsOnlyActivatedDesiresWithAValueOrADrive)
{
	auto desires = NoDesires();
	desires[Desire::Hunger] = {.activated = true, .value = 0.4f};
	// Activated, with no value and no source past its threshold
	desires[Desire::Fear] = {.activated = true, .value = 0.0f};
	// Not activated, with a value
	desires[Desire::Anger] = {.activated = false, .value = 0.9f};
	// Activated, with no value but a source past its threshold
	desires[Desire::Tiredness] = {.activated = true, .value = 0.0f, .sources = {SourceOf(22, 0.9f, 0.2f)}};

	EXPECT_TRUE(IsActive(desires, static_cast<size_t>(Desire::Hunger)));
	EXPECT_FALSE(IsActive(desires, static_cast<size_t>(Desire::Fear)));
	EXPECT_FALSE(IsActive(desires, static_cast<size_t>(Desire::Anger)));
	EXPECT_TRUE(IsActive(desires, static_cast<size_t>(Desire::Tiredness)));
	EXPECT_EQ(ActiveDesires(desires, nullptr, k_TurnsPerSecond).size(), 2u);
}

TEST(CreatureMindView, SortsTheHighestFirstAndMarksTheWinner)
{
	auto desires = NoDesires();
	desires[Desire::Play] = {.activated = true, .value = 0.2f};
	desires[Desire::Hunger] = {.activated = true, .value = 0.7f};
	desires[Desire::Curiosity] = {.activated = true, .value = 0.7f};

	const auto rows = ActiveDesires(desires, nullptr, k_TurnsPerSecond);
	ASSERT_EQ(rows.size(), 3u);
	// Of equals, the first desire comes first and wins, as the mind finds the dominant desire
	EXPECT_EQ(rows[0].desire, Desire::Hunger);
	EXPECT_EQ(rows[1].desire, Desire::Curiosity);
	EXPECT_EQ(rows[2].desire, Desire::Play);
	EXPECT_TRUE(rows[0].winner);
	EXPECT_FALSE(rows[1].winner);
	EXPECT_FALSE(rows[2].winner);
}

TEST(CreatureMindView, GivesEachSourceItsPartOfTheDrive)
{
	auto desires = NoDesires();
	desires[Desire::Hunger] = {.activated = true,
	                           .value = 0.5f,
	                           .increaseSeconds = 20.0f,
	                           .sources = {SourceOf(14, 0.8f, 0.3f), SourceOf(16, 0.1f, 0.5f)}};

	const auto rows = ActiveDesires(desires, nullptr, k_TurnsPerSecond);
	ASSERT_EQ(rows.size(), 1u);
	const auto& row = rows[0];
	ASSERT_EQ(row.sources.size(), 2u);
	EXPECT_EQ(row.sources[0].type, 14u);
	EXPECT_FLOAT_EQ(row.sources[0].part, creature_desires::Sigmoid(0.3f, 0.8f));
	EXPECT_FLOAT_EQ(row.sources[1].part, creature_desires::Sigmoid(0.5f, 0.1f));
	EXPECT_FLOAT_EQ(row.sources[0].drive, 2.5f);
	EXPECT_FLOAT_EQ(row.drive, row.sources[0].part + row.sources[1].part);
	EXPECT_FLOAT_EQ(row.growthPerTurn, row.drive / (k_TurnsPerSecond * 20.0f));
	EXPECT_FALSE(row.change.has_value());
	EXPECT_EQ(SourceName(14), "hunger: low energy");
	EXPECT_EQ(SourceName(creature_desires::k_NoSource), "none");
}

TEST(CreatureMindView, AHeldBackDesireDoesNotGrow)
{
	auto desires = NoDesires();
	desires[Desire::Hunger] = {.activated = true, .value = 0.5f, .suppressedTurns = 4, .sources = {SourceOf(14, 0.9f, 0.1f)}};
	const auto rows = ActiveDesires(desires, nullptr, k_TurnsPerSecond);
	ASSERT_EQ(rows.size(), 1u);
	EXPECT_GT(rows[0].drive, 0.0f);
	EXPECT_EQ(rows[0].growthPerTurn, 0.0f);
	EXPECT_EQ(rows[0].suppressedTurns, 4u);
}

TEST(CreatureMindView, TheTraceGivesTheChangeSinceTheTurnBefore)
{
	auto desires = NoDesires();
	desires[Desire::Hunger] = {.activated = true, .value = 0.25f};
	Trace trace;
	const PlannerState planner;
	trace.Record(10, desires, planner, false);
	EXPECT_EQ(trace.Before(), nullptr);
	desires[Desire::Hunger].value = 0.5f;
	trace.Record(11, desires, planner, false);
	ASSERT_NE(trace.Before(), nullptr);

	const auto rows = ActiveDesires(desires, trace.Before(), k_TurnsPerSecond);
	ASSERT_EQ(rows.size(), 1u);
	ASSERT_TRUE(rows[0].change.has_value());
	EXPECT_FLOAT_EQ(*rows[0].change, 0.25f);
	ASSERT_EQ(trace.Samples().size(), 2u);
	EXPECT_EQ(trace.Samples().back().turn, 11u);
}

TEST(CreatureMindView, TheTraceKeepsOnlyTheLastTurns)
{
	const auto desires = NoDesires();
	const PlannerState planner;
	Trace trace;
	for (uint32_t turn = 0; turn < k_SampleTurns + 20; ++turn)
	{
		trace.Record(turn, desires, planner, false);
	}
	ASSERT_EQ(trace.Samples().size(), k_SampleTurns);
	EXPECT_EQ(trace.Samples().front().turn, 20u);
}

TEST(CreatureMindView, TheFirstTurnRecordsNoPlanStep)
{
	const auto desires = NoDesires();
	PlannerState planner;
	planner.best.at(static_cast<size_t>(Desire::Hunger)) = PlanFor(Desire::Hunger, 3, 5.0f);
	Trace trace;
	trace.Record(1, desires, planner, false);
	EXPECT_TRUE(trace.Steps().empty());
}

TEST(CreatureMindView, APlanThatStartsIsChosen)
{
	const auto desires = NoDesires();
	PlannerState planner;
	planner.current = PlanFor(Desire::Play, 7, 2.0f);
	Trace trace;
	trace.Record(1, desires, planner, false);

	const auto hunger = PlanFor(Desire::Hunger, 3, 5.0f, 42u);
	planner.best.at(static_cast<size_t>(Desire::Hunger)) = hunger;
	planner.current = hunger;
	trace.Record(2, desires, planner, false);

	ASSERT_EQ(trace.Steps().size(), 1u);
	const auto& step = trace.Steps().back();
	EXPECT_EQ(step.turn, 2u);
	EXPECT_EQ(step.desire, Desire::Hunger);
	EXPECT_EQ(step.outcome, Outcome::Chosen);
	ASSERT_TRUE(step.current.has_value());
	EXPECT_FLOAT_EQ(*step.current, 2.0f);
	ASSERT_TRUE(step.plan.has_value());
	EXPECT_EQ(step.plan->object, 42u);
}

TEST(CreatureMindView, AForcedPlanIsForced)
{
	const auto desires = NoDesires();
	PlannerState planner;
	Trace trace;
	trace.Record(1, desires, planner, false);
	// An order gives a plan that the planner did not make
	planner.current = PlanFor(Desire::ObeyPlayer, 9, creature_planner::k_ForcedScore);
	trace.Record(2, desires, planner, true);

	ASSERT_EQ(trace.Steps().size(), 1u);
	EXPECT_EQ(trace.Steps().back().outcome, Outcome::Forced);
	EXPECT_EQ(trace.Steps().back().desire, Desire::ObeyPlayer);
	EXPECT_FALSE(trace.Steps().back().current.has_value());
}

TEST(CreatureMindView, APlanBelowAnotherIsNotTheBest)
{
	const auto desires = NoDesires();
	PlannerState planner;
	planner.best.at(static_cast<size_t>(Desire::Hunger)) = PlanFor(Desire::Hunger, 3, 9.0f);
	planner.current = planner.best.at(static_cast<size_t>(Desire::Hunger));
	Trace trace;
	trace.Record(1, desires, planner, false);

	planner.best.at(static_cast<size_t>(Desire::Play)) = PlanFor(Desire::Play, 5, 4.0f);
	trace.Record(2, desires, planner, false);

	ASSERT_EQ(trace.Steps().size(), 1u);
	const auto& step = trace.Steps().back();
	EXPECT_EQ(step.outcome, Outcome::NotBest);
	ASSERT_TRUE(step.best.has_value());
	EXPECT_EQ(*step.best, Desire::Hunger);
	EXPECT_FLOAT_EQ(step.bestPriority, 9.0f);
}

TEST(CreatureMindView, TheBestPlanBelowTwiceTheCurrentIsNotPressing)
{
	const auto desires = NoDesires();
	PlannerState planner;
	planner.current = PlanFor(Desire::Play, 5, 4.0f);
	Trace trace;
	trace.Record(1, desires, planner, false);

	planner.best.at(static_cast<size_t>(Desire::Hunger)) = PlanFor(Desire::Hunger, 3, 7.0f);
	trace.Record(2, desires, planner, false);

	ASSERT_EQ(trace.Steps().size(), 1u);
	EXPECT_EQ(trace.Steps().back().outcome, Outcome::NotPressing);
	EXPECT_FLOAT_EQ(trace.Steps().back().current.value_or(0.0f), 4.0f);
}

TEST(CreatureMindView, AReplannedCurrentPlanIsKeptAndALostPlanIsNoPlan)
{
	const auto desires = NoDesires();
	PlannerState planner;
	planner.best.at(static_cast<size_t>(Desire::Hunger)) = PlanFor(Desire::Hunger, 3, 6.0f);
	planner.best.at(static_cast<size_t>(Desire::Play)) = PlanFor(Desire::Play, 5, 1.0f);
	planner.current = planner.best.at(static_cast<size_t>(Desire::Hunger));
	Trace trace;
	trace.Record(1, desires, planner, false);

	// The planner plans hunger again with a new priority, and finds no plan for play
	planner.best.at(static_cast<size_t>(Desire::Hunger)) = PlanFor(Desire::Hunger, 3, 6.5f);
	planner.best.at(static_cast<size_t>(Desire::Play)).reset();
	trace.Record(2, desires, planner, false);

	// The steps come in the order of the desires: play before hunger
	ASSERT_EQ(trace.Steps().size(), 2u);
	EXPECT_EQ(trace.Steps()[0].desire, Desire::Play);
	EXPECT_EQ(trace.Steps()[0].outcome, Outcome::NoPlan);
	EXPECT_FALSE(trace.Steps()[0].plan.has_value());
	EXPECT_EQ(trace.Steps()[1].desire, Desire::Hunger);
	EXPECT_EQ(trace.Steps()[1].outcome, Outcome::Kept);
}

TEST(CreatureMindView, APlanThatStopsEnds)
{
	const auto desires = NoDesires();
	PlannerState planner;
	planner.current = PlanFor(Desire::Play, 5, 4.0f);
	Trace trace;
	trace.Record(1, desires, planner, false);
	planner.current.reset();
	trace.Record(2, desires, planner, false);

	ASSERT_EQ(trace.Steps().size(), 1u);
	EXPECT_EQ(trace.Steps().back().outcome, Outcome::Ended);
	EXPECT_EQ(trace.Steps().back().desire, Desire::Play);
}

TEST(CreatureMindView, TheTraceKeepsOnlyTheLastPlanSteps)
{
	const auto desires = NoDesires();
	PlannerState planner;
	Trace trace;
	trace.Record(0, desires, planner, false);
	for (uint32_t turn = 1; turn <= k_PlanSteps + 5; ++turn)
	{
		planner.best.at(0) = PlanFor(Desire::Impress, 1, static_cast<float>(turn));
		trace.Record(turn, desires, planner, false);
	}
	ASSERT_EQ(trace.Steps().size(), k_PlanSteps);
	EXPECT_EQ(trace.Steps().front().turn, 6u);
}

TEST(CreatureMindLogSystem, RecordsOnlyTheWatchedCreatures)
{
	ecs::Registry registry;
	const auto watched = registry.Create();
	const auto other = registry.Create();
	auto& mind = registry.Assign<ecs::components::CreatureMindState>(watched);
	mind.desires = NoDesires();
	registry.Assign<ecs::components::CreatureMindState>(other).desires = NoDesires();

	ecs::systems::CreatureMindLogSystem log;
	log.TurnEnded(registry, 1);
	EXPECT_EQ(log.Find(watched), nullptr);

	log.Watch(watched, true);
	EXPECT_TRUE(log.IsWatched(watched));
	log.TurnEnded(registry, 2);
	log.TurnEnded(registry, 3);
	ASSERT_NE(log.Find(watched), nullptr);
	EXPECT_EQ(log.Find(watched)->Samples().size(), 2u);
	EXPECT_EQ(log.Find(other), nullptr);

	log.Watch(watched, false);
	EXPECT_EQ(log.Find(watched), nullptr);
}

TEST(CreatureMindLogSystem, LeavesTheMindAsItWas)
{
	ecs::Registry registry;
	const auto creature = registry.Create();
	auto& mind = registry.Assign<ecs::components::CreatureMindState>(creature);
	mind.desires = NoDesires();
	(*mind.desires)[Desire::Hunger] = {.activated = true, .value = 0.3f, .sources = {SourceOf(14, 0.6f, 0.2f)}};
	mind.planner.best.at(static_cast<size_t>(Desire::Hunger)) = PlanFor(Desire::Hunger, 3, 2.0f);
	const auto driveBefore = (*mind.desires)[Desire::Hunger].sources.at(0).drive;
	const auto planBefore = mind.planner.best.at(static_cast<size_t>(Desire::Hunger));

	ecs::systems::CreatureMindLogSystem log;
	log.Watch(creature, true);
	log.TurnEnded(registry, 1);
	log.TurnEnded(registry, 2);

	const auto& after = registry.Get<const ecs::components::CreatureMindState>(creature);
	EXPECT_EQ(after.desires->desires.at(static_cast<size_t>(Desire::Hunger)).value, 0.3f);
	EXPECT_EQ(after.desires->desires.at(static_cast<size_t>(Desire::Hunger)).sources.at(0).drive, driveBefore);
	EXPECT_TRUE(SamePlan(after.planner.best.at(static_cast<size_t>(Desire::Hunger)), planBefore));
}

TEST(CreatureMindLogSystem, ForgetsACreatureThatHasNoMindAnyMore)
{
	ecs::Registry registry;
	const auto creature = registry.Create();
	registry.Assign<ecs::components::CreatureMindState>(creature).desires = NoDesires();
	ecs::systems::CreatureMindLogSystem log;
	log.Watch(creature, true);
	registry.Remove<ecs::components::CreatureMindState>(creature);
	log.TurnEnded(registry, 1);
	EXPECT_FALSE(log.IsWatched(creature));
}
