/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <limits>
#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFireAgenda.h"
#include "Creature/CreatureFireReaction.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreaturePlanActions.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_mind;

TEST(CreatureFire, PriorityWithinBothReachesAndBigEnough)
{
	// Within the burning thing's reach and its own, and more than a fifth of the thing's height
	EXPECT_EQ(creature_fire::Priority(9.0f, 5.0f, 5.0f, 3.0f, 10.0f, 120), 120);
	// Both tests are strict
	EXPECT_EQ(creature_fire::Priority(10.0f, 5.0f, 5.0f, 3.0f, 10.0f, 120), 0);
	EXPECT_EQ(creature_fire::Priority(9.0f, 5.0f, 5.0f, 2.0f, 10.0f, 120), 0);
	EXPECT_EQ(creature_fire::Priority(9.0f, 5.0f, 5.0f, 2.01f, 10.0f, 120), 120);
	// Its own fire: right there, and as tall as itself
	EXPECT_EQ(creature_fire::Priority(0.0f, 4.0f, 4.0f, 30.0f, 30.0f, 120), 120);
}

TEST(CreatureFire, ChoosesToPutOutOnlyWhenItCaresAndKnowsWater)
{
	using creature_fire::Response;
	// Usefulness times compassion must be above a twentieth
	EXPECT_EQ(creature_fire::Choose(0.125f, 0.5f, 3.0f, 3.0f), Response::PutOut);
	EXPECT_EQ(creature_fire::Choose(0.1f, 0.5f, 3.0f, 3.0f), Response::RunAway);
	EXPECT_EQ(creature_fire::Choose(0.125f, 0.4f, 3.0f, 3.0f), Response::RunAway);
	// Nothing the fire belongs to: it runs
	EXPECT_EQ(creature_fire::Choose(std::nullopt, 1.0f, 3.0f, 3.0f), Response::RunAway);
	// Seen the water miracle often enough: exactly enough is enough, more too, less isn't
	EXPECT_EQ(creature_fire::Choose(0.125f, 1.0f, 4.0f, 3.0f), Response::PutOut);
	EXPECT_EQ(creature_fire::Choose(0.125f, 1.0f, 2.0f, 3.0f), Response::RunAway);
	// Never seen and never needed to: the share is no number, which counts as enough
	EXPECT_EQ(creature_fire::Choose(0.125f, 1.0f, 0.0f, 0.0f), Response::PutOut);
	// No number for how much it cares: it runs
	EXPECT_EQ(creature_fire::Choose(std::numeric_limits<float>::quiet_NaN(), 1.0f, 3.0f, 3.0f), Response::RunAway);
}

TEST(CreatureFire, WhatABurningThingBelongsTo)
{
	using creature_fire::BelongsTo;
	using creature_fire::Burning;
	EXPECT_EQ(creature_fire::Owner(Burning::Building), BelongsTo::Town);
	EXPECT_EQ(creature_fire::Owner(Burning::Villager), BelongsTo::Town);
	EXPECT_EQ(creature_fire::Owner(Burning::Field), BelongsTo::Town);
	EXPECT_EQ(creature_fire::Owner(Burning::TotemStatue), BelongsTo::Town);
	EXPECT_EQ(creature_fire::Owner(Burning::Tree), BelongsTo::Forest);
	EXPECT_EQ(creature_fire::Owner(Burning::Animal), BelongsTo::Flock);
	EXPECT_EQ(creature_fire::Owner(Burning::Creature), BelongsTo::Itself);
	EXPECT_EQ(creature_fire::Owner(Burning::TemplePart), BelongsTo::Temple);
	EXPECT_EQ(creature_fire::Owner(Burning::Other), BelongsTo::Nothing);
}

TEST(CreatureFire, UsefulnessIsCappedAtAnEighth)
{
	// Combined with compassion it takes more than 0.4 compassion to put a fire out
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(1.0f), 0.125f);
	EXPECT_FALSE(0.125f * 0.4f > creature_fire::k_LeastCompassionateUse);
	EXPECT_TRUE(0.125f * 0.41f > creature_fire::k_LeastCompassionateUse);
}

TEST(CreatureFireAgenda, PutsOutWithTheWaterMiracleThenDouses)
{
	const auto agenda = PutOutFireWithWater(7, 15.0f);
	ASSERT_EQ(agenda.size(), 5u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 0.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 30.0f);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[2].seconds, 0.1f);
	EXPECT_EQ(agenda[3].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[3].cast.magicType, static_cast<uint32_t>(MagicType::Water));
	EXPECT_EQ(agenda[3].sequence, k_CastPose);
	EXPECT_FLOAT_EQ(agenda[3].seconds, 3.0f);
	EXPECT_EQ(agenda[4].kind, Step::Kind::Douse);
	EXPECT_EQ(agenda[4].object, 7u);
	for (const auto& step : agenda)
	{
		if (step.kind == Step::Kind::Move)
		{
			EXPECT_EQ(step.movement.object, 7u);
		}
	}
}

TEST(CreatureFireAgenda, SetsAlightWithABurningThing)
{
	EXPECT_FALSE(SetFireTo(7, std::nullopt, false).has_value());
	const auto agenda = SetFireTo(7, 9u, false);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 6u);
	EXPECT_EQ((*agenda)[0].kind, Step::Kind::Object);
	EXPECT_EQ((*agenda)[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ((*agenda)[0].order.object, 9u);
	EXPECT_EQ((*agenda)[1].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ((*agenda)[1].movement.maxDistance, 1.0f);
	EXPECT_EQ((*agenda)[2].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ((*agenda)[3].order.kind, ObjectOrder::Kind::Discard);
	EXPECT_EQ((*agenda)[3].order.animation, 97u);
	EXPECT_EQ((*agenda)[4].kind, Step::Kind::WaitInMap);
	EXPECT_EQ((*agenda)[4].object, 9u);
	EXPECT_EQ((*agenda)[5].kind, Step::Kind::Wait);
	// One second: ten turns of a tenth of a second
	EXPECT_FLOAT_EQ((*agenda)[5].seconds, 1.0f);
	// With its hand full it doesn't pick the thing up
	const auto full = SetFireTo(7, 9u, true);
	ASSERT_TRUE(full.has_value());
	EXPECT_EQ(full->size(), 5u);
	EXPECT_EQ(full->front().kind, Step::Kind::Move);
}

TEST(CreatureFireAgenda, ExecutorsForTheFireActions)
{
	const auto* water = creature_plan_actions::For("PutOutFireWithMagicWater");
	ASSERT_NE(water, nullptr);
	EXPECT_TRUE(creature_plan_actions::IsCast(*water));
	const auto* setFire = creature_plan_actions::For("SetFireToObject");
	ASSERT_NE(setFire, nullptr);
	const Random random = [](uint32_t) { return 0u; };
	// Without a burning thing to use, setting something alight can't be planned
	EXPECT_FALSE(creature_plan_actions::Agenda(*setFire, 7u, {}, {}, random, std::nullopt).has_value());
	EXPECT_TRUE(creature_plan_actions::Agenda(*setFire, 7u, {}, {.instrument = 9u}, random, std::nullopt).has_value());
	// Starting a fire never can
	const auto* start = creature_plan_actions::For("StartFire");
	ASSERT_NE(start, nullptr);
	EXPECT_FALSE(creature_plan_actions::Agenda(*start, std::nullopt, {}, {}, random, std::nullopt).has_value());
}

TEST(CreatureFireAgenda, DousesAtOnceAndWaitsForTheThingToLand)
{
	const Random random = [](uint32_t) { return 0u; };
	IdleMind mind;
	Step douse {.kind = Step::Kind::Douse};
	douse.object = 7u;
	Step land {.kind = Step::Kind::WaitInMap};
	land.object = 9u;
	Plan(mind, Activity::Planned, {douse, land, {.kind = Step::Kind::Wait, .seconds = 1.0f}});
	Senses senses {.seconds = 0.1f};
	auto commands = Think(mind, senses, random);
	EXPECT_EQ(commands.douse, 7u);
	EXPECT_EQ(mind.step, 1u);
	// The thing still flying: it waits
	commands = Think(mind, senses, random);
	senses.objectInMap = false;
	commands = Think(mind, senses, random);
	EXPECT_EQ(mind.step, 1u);
	senses.objectInMap = true;
	commands = Think(mind, senses, random);
	EXPECT_EQ(mind.step, 2u);
	// The thing gone: the rest is given up
	Plan(mind, Activity::Planned, {land, {.kind = Step::Kind::Wait, .seconds = 1.0f}});
	senses.objectInMap.reset();
	commands = Think(mind, senses, random);
	commands = Think(mind, senses, random);
	EXPECT_TRUE(mind.gaveUp);
	EXPECT_GE(mind.step, mind.agenda.size());
}

TEST(CreatureFireLearning, ForcedPlanIsRememberedUnlessNothingSaysWhy)
{
	using creature_learning::k_LastTellingSource;
	creature_desires::DesireState desire;
	// No sources at all: nothing says why
	EXPECT_FALSE(creature_learning::RemembersPlan(desire));
	desire.sources.push_back({.type = k_LastTellingSource + 5, .drive = 0.0f});
	desire.sources.push_back({.type = 3, .drive = 2.0f});
	// The source that drove it most says why
	EXPECT_TRUE(creature_learning::RemembersPlan(desire));
	desire.sources[1].type = k_LastTellingSource + 2;
	// Neither the most driving nor the first says why
	EXPECT_FALSE(creature_learning::RemembersPlan(desire));
	// The first source says why
	desire.sources[0].type = k_LastTellingSource;
	EXPECT_TRUE(creature_learning::RemembersPlan(desire));
	// Nothing has driven it: the first source has its say
	desire.sources[1].drive = 0.0f;
	desire.sources[0].type = 4;
	desire.sources[1].type = 2;
	EXPECT_TRUE(creature_learning::RemembersPlan(desire));
	desire.sources[0].type = k_LastTellingSource + 1;
	EXPECT_FALSE(creature_learning::RemembersPlan(desire));
}

TEST(CreatureFireLearning, AChosenPlanIsRememberedByTheDrivesBeforeTheyAreCountedAfresh)
{
	using creature_learning::k_LastTellingSource;
	creature_desires::DesireState desire;
	desire.sources.push_back({.type = k_LastTellingSource + 5, .drive = 0.0f});
	desire.sources.push_back({.type = 3, .drive = 2.0f});
	// The source that drove it most says why, and is asked before its drive goes
	EXPECT_TRUE(creature_learning::TakeUpChosenPlan(desire));
	EXPECT_EQ(desire.sources[1].drive, 0.0f);
	// With every drive gone, only the first source has its say, which says nothing
	EXPECT_FALSE(creature_learning::TakeUpChosenPlan(desire));
}

TEST(CreatureFire, EveryExecutorsTargetIsCountedAmongTheKinds)
{
	// The planner weighs actions in a group for each kind of target, the fire's kinds included
	for (const auto& executor : creature_plan_actions::All())
	{
		EXPECT_LT(static_cast<size_t>(executor.target), creature_plan_actions::k_TargetCount) << executor.action;
	}
}
