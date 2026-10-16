/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureSteal.h"
#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace steal = openblack::creature_steal;
namespace plan_actions = openblack::creature_plan_actions;

TEST(CreatureSteal, OnlyAToyAScaffoldOrAOneShotMiracleCanBeStolen)
{
	EXPECT_TRUE(steal::IsStealable({.toy = true, .canPickUp = true}));
	EXPECT_TRUE(steal::IsStealable({.scaffold = true, .canPickUp = true}));
	EXPECT_TRUE(steal::IsStealable({.oneShot = true, .canPickUp = true}));
	EXPECT_FALSE(steal::IsStealable({.canPickUp = true}));
}

TEST(CreatureSteal, TheCreatureMustBeAbleToPickTheThingUp)
{
	EXPECT_FALSE(steal::IsStealable({.toy = true, .canPickUp = false}));
}

TEST(CreatureSteal, AThingInTheInfluenceOfItsOwnPlayerIsNotStolen)
{
	EXPECT_FALSE(steal::IsStealable({.toy = true, .canPickUp = true, .ownInfluence = 0.01f}));
	EXPECT_TRUE(steal::IsStealable({.toy = true, .canPickUp = true, .ownInfluence = 0.0f}));
	EXPECT_TRUE(steal::IsStealable({.toy = true, .canPickUp = true, .ownInfluence = -1.0f}));
}

TEST(CreatureSteal, ASightingStirsTheSourceOfTheDesireToStealByThreeTenths)
{
	EXPECT_EQ(steal::k_StealSource, 60u);
	EXPECT_FLOAT_EQ(steal::k_StealableSightingAmount, 0.3f);
}

TEST(CreatureSteal, WithAnEmptyHandItPicksUpTheThingWalksHomeAndPutsItDown)
{
	const auto agenda = steal::StealAndPutByTemple(7u, {10.0f, 20.0f}, 3.0f);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda.at(0).kind, Step::Kind::Object);
	EXPECT_EQ(agenda.at(0).order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda.at(0).order.object, std::optional(7u));
	EXPECT_FALSE(agenda.at(0).main);
	EXPECT_EQ(agenda.at(1).kind, Step::Kind::Move);
	EXPECT_EQ(agenda.at(1).movement.kind, Movement::Kind::ToPoint);
	EXPECT_FLOAT_EQ(agenda.at(1).movement.point.x, 10.0f);
	EXPECT_FLOAT_EQ(agenda.at(1).movement.point.y, 20.0f);
	EXPECT_FLOAT_EQ(agenda.at(1).movement.maxDistance, 6.0f);
	EXPECT_TRUE(agenda.at(1).main);
	EXPECT_EQ(agenda.at(2).order.kind, ObjectOrder::Kind::Discard);
	EXPECT_EQ(agenda.at(2).order.animation, creature_throw::k_PutDown);
}

TEST(CreatureSteal, WithAFullHandItTakesWhatItHolds)
{
	const auto agenda = steal::StealAndPutByTemple(std::nullopt, {0.0f, 0.0f}, 2.0f);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda.at(0).kind, Step::Kind::Move);
	EXPECT_FLOAT_EQ(agenda.at(0).movement.maxDistance, 4.0f);
}

TEST(CreatureSteal, TheActionNeedsATempleAndAThingToSteal)
{
	const auto* action = plan_actions::For("StealObjectAndTakeToCitadel");
	ASSERT_NE(action, nullptr);
	EXPECT_EQ(action->target, plan_actions::Target::Stealable);
	plan_actions::Situation situation;
	situation.hasTemple = false;
	EXPECT_FALSE(plan_actions::Possible(*action, situation));
	situation.hasTemple = true;
	EXPECT_TRUE(plan_actions::Possible(*action, situation));
}
