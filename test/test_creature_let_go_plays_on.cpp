/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureObjectActions.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Registry.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_object_actions::Kind;
using creature_object_actions::Status;

namespace
{
/// A creature with an action of its hands, and the system that looks at the action
class CreatureLetGoTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		creature = Locator::entitiesRegistry::value().Create();
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	void Give(Kind kind, CreatureObjectAction::Phase phase, Status status)
	{
		Locator::entitiesRegistry::value().AssignOrReplace<CreatureObjectAction>(
		    creature, CreatureObjectAction {.kind = kind, .phase = phase, .status = status});
	}

	CreatureObjectActionSystem hands;
	entt::entity creature {entt::null};
};
} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureLetGoRules, EachActionThatTheBodyStartedPlaysOnWhenThePlanStops)
{
	using creature_object_actions::OnPlanStop;
	using creature_object_actions::PlanStop;
	for (const auto kind : {Kind::PickUp, Kind::PutDown, Kind::Discard, Kind::Eat, Kind::Keep, Kind::Throw, Kind::Destroy,
	                        Kind::Catch, Kind::TakeFromHand, Kind::Kick})
	{
		EXPECT_EQ(OnPlanStop(kind, true, false), PlanStop::PlaysOn) << creature_object_actions::Name(kind);
		EXPECT_EQ(OnPlanStop(kind, false, false), PlanStop::Stops) << creature_object_actions::Name(kind);
	}
	// The point plays its end
	EXPECT_EQ(OnPlanStop(Kind::Point, true, false), PlanStop::EndsPoint);
	EXPECT_EQ(OnPlanStop(Kind::Point, false, false), PlanStop::Stops);
}

// A turn on the spot to reach, to throw or to point is a part of the body action. Other actions have no such turn.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureLetGoRules, ATurnForTheActionIsAPartOfTheBodyAction)
{
	using creature_object_actions::OnPlanStop;
	using creature_object_actions::PlanStop;
	EXPECT_EQ(OnPlanStop(Kind::PickUp, false, true), PlanStop::PlaysOn);
	EXPECT_EQ(OnPlanStop(Kind::Destroy, false, true), PlanStop::PlaysOn);
	EXPECT_EQ(OnPlanStop(Kind::Throw, false, true), PlanStop::PlaysOn);
	EXPECT_EQ(OnPlanStop(Kind::Point, false, true), PlanStop::EndsPoint);
	EXPECT_EQ(OnPlanStop(Kind::Kick, false, true), PlanStop::Stops);
	EXPECT_EQ(OnPlanStop(Kind::Eat, false, true), PlanStop::Stops);
}

// After the bite the creature swallows at once. Before the bite, while the eat animation plays, the thing goes.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureLetGoRules, AStoppedEatSwallowsAfterTheBiteAndLosesTheThingBefore)
{
	using creature_object_actions::EatStop;
	using creature_object_actions::OnEatStop;
	EXPECT_EQ(OnEatStop(true, true, false), EatStop::Swallows);
	EXPECT_EQ(OnEatStop(true, false, true), EatStop::Deletes);
	EXPECT_EQ(OnEatStop(false, false, true), EatStop::Keeps);
	EXPECT_EQ(OnEatStop(true, false, false), EatStop::Keeps);
}

// A put-down that plays its animation continues to its moment, before and after the hand lets go
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureLetGoTest, APutDownThatPlaysContinuesWhenThePlanStops)
{
	Give(Kind::PutDown, CreatureObjectAction::Phase::Playing, Status::Running);
	EXPECT_TRUE(hands.PlaysOnAlone(creature));
	Give(Kind::Discard, CreatureObjectAction::Phase::Playing, Status::Contact);
	EXPECT_TRUE(hands.PlaysOnAlone(creature));
}

// An action that ended, and an action that the body did not start, stop with the plan
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureLetGoTest, OtherActionsStopWithThePlan)
{
	EXPECT_FALSE(hands.PlaysOnAlone(creature));
	Give(Kind::PutDown, CreatureObjectAction::Phase::Playing, Status::Done);
	EXPECT_FALSE(hands.PlaysOnAlone(creature));
	Give(Kind::PickUp, CreatureObjectAction::Phase::Approach, Status::Running);
	EXPECT_FALSE(hands.PlaysOnAlone(creature));
	Give(Kind::Point, CreatureObjectAction::Phase::Playing, Status::Running);
	EXPECT_FALSE(hands.PlaysOnAlone(creature));
}

// A pick-up, an eat and a stomp that play their animations play on when the plan stops
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureLetGoTest, AStartedActionPlaysOnAndKnowsThatItsPlanStopped)
{
	for (const auto kind : {Kind::PickUp, Kind::Destroy, Kind::Keep, Kind::Throw, Kind::Kick})
	{
		Give(kind, CreatureObjectAction::Phase::Playing, Status::Running);
		EXPECT_TRUE(hands.PlaysOnAlone(creature)) << creature_object_actions::Name(kind);
		hands.StopForPlan(creature, true);
		const auto* action = Locator::entitiesRegistry::value().TryGet<const CreatureObjectAction>(creature);
		ASSERT_NE(action, nullptr) << creature_object_actions::Name(kind);
		EXPECT_EQ(action->status, Status::Running);
		EXPECT_TRUE(action->planStopped);
	}
}

// A pick-up that turns to get the thing in reach plays on. A walk up to the thing stops.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureLetGoTest, ATurnToReachPlaysOnButAWalkStops)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.AssignOrReplace<CreatureObjectAction>(
	    creature,
	    CreatureObjectAction {.kind = Kind::PickUp, .phase = CreatureObjectAction::Phase::Approach, .turnedToReach = true});
	EXPECT_TRUE(hands.PlaysOnAlone(creature));
	hands.StopForPlan(creature, true);
	EXPECT_TRUE(registry.AllOf<CreatureObjectAction>(creature));
	Give(Kind::PickUp, CreatureObjectAction::Phase::Approach, Status::Running);
	hands.StopForPlan(creature, true);
	EXPECT_FALSE(registry.AllOf<CreatureObjectAction>(creature));
}

// A point that holds its arm plays its end with no hold. A point that turns first ends after the turn, with no hold.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureLetGoTest, APointPlaysItsEndWhenThePlanStops)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.AssignOrReplace<CreatureObjectAction>(
	    creature, CreatureObjectAction {.kind = Kind::Point, .phase = CreatureObjectAction::Phase::Playing, .holdMs = 5000.0f});
	hands.StopForPlan(creature, true);
	const auto& playing = registry.Get<const CreatureObjectAction>(creature);
	EXPECT_TRUE(playing.pointEnding);
	EXPECT_EQ(playing.holdMs, 0.0f);
	EXPECT_EQ(playing.status, Status::Running);
	EXPECT_TRUE(hands.PlaysOnAlone(creature));

	registry.AssignOrReplace<CreatureObjectAction>(
	    creature,
	    CreatureObjectAction {.kind = Kind::Point, .phase = CreatureObjectAction::Phase::Approach, .pointTurned = true});
	hands.StopForPlan(creature, true);
	const auto& turning = registry.Get<const CreatureObjectAction>(creature);
	EXPECT_TRUE(turning.pointEnding);
	EXPECT_EQ(turning.status, Status::Running);

	// A stop that leaves the body alone leaves the point to hold on alone, and the body stays busy
	registry.AssignOrReplace<CreatureObjectAction>(
	    creature, CreatureObjectAction {.kind = Kind::Point, .phase = CreatureObjectAction::Phase::Playing, .holdMs = 5000.0f});
	hands.StopForPlan(creature, false);
	const auto& holding = registry.Get<const CreatureObjectAction>(creature);
	EXPECT_FALSE(holding.pointEnding);
	EXPECT_EQ(holding.holdMs, 5000.0f);
	EXPECT_TRUE(holding.planStopped);
	EXPECT_TRUE(hands.PlaysOnAlone(creature));

	// A point that did not start stops at once
	Give(Kind::Point, CreatureObjectAction::Phase::Approach, Status::Running);
	hands.StopForPlan(creature, true);
	EXPECT_FALSE(registry.AllOf<CreatureObjectAction>(creature));
}

// An eat that waits to start keeps the thing in the hand when the plan stops
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureLetGoTest, AnEatThatDidNotStartStops)
{
	Give(Kind::Eat, CreatureObjectAction::Phase::Approach, Status::Running);
	EXPECT_FALSE(hands.PlaysOnAlone(creature));
	hands.StopForPlan(creature, true);
	EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<CreatureObjectAction>(creature));
}
