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
TEST(CreatureLetGoRules, OnlyACatchAndALetGoContinueWhenThePlanStops)
{
	using creature_object_actions::PlaysOnWhenPlanStops;
	EXPECT_TRUE(PlaysOnWhenPlanStops(Kind::Catch));
	EXPECT_TRUE(PlaysOnWhenPlanStops(Kind::PutDown));
	EXPECT_TRUE(PlaysOnWhenPlanStops(Kind::Discard));
	for (const auto kind :
	     {Kind::PickUp, Kind::Eat, Kind::Keep, Kind::Throw, Kind::Destroy, Kind::Point, Kind::TakeFromHand, Kind::Kick})
	{
		EXPECT_FALSE(PlaysOnWhenPlanStops(kind)) << creature_object_actions::Name(kind);
	}
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

// A let-go that ended, and an action that does not let go, stop with the plan
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureLetGoTest, OtherActionsStopWithThePlan)
{
	EXPECT_FALSE(hands.PlaysOnAlone(creature));
	Give(Kind::PutDown, CreatureObjectAction::Phase::Playing, Status::Done);
	EXPECT_FALSE(hands.PlaysOnAlone(creature));
	Give(Kind::PickUp, CreatureObjectAction::Phase::Approach, Status::Running);
	EXPECT_FALSE(hands.PlaysOnAlone(creature));
	Give(Kind::Eat, CreatureObjectAction::Phase::Playing, Status::Running);
	EXPECT_FALSE(hands.PlaysOnAlone(creature));
}
