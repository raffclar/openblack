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

#include "Creature/CreatureDance.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureTempleDance.h"

using namespace openblack;
using creature_mind::DanceOrder;
using creature_mind::Movement;
using creature_mind::Step;
namespace plan_actions = openblack::creature_plan_actions;
namespace temple_dance = openblack::creature_temple_dance;

namespace
{
const plan_actions::Executor& ActionNamed(std::string_view name)
{
	const auto* action = plan_actions::For(name);
	EXPECT_NE(action, nullptr) << name;
	static const plan_actions::Executor k_None {};
	return action != nullptr ? *action : k_None;
}

uint32_t First(uint32_t /*range*/)
{
	return 0;
}
} // namespace

// The dance outside a worship site acts on a worship site; the dance around a thing acts on any thing
TEST(CreatureTempleDance, TheActionsActOnTheirThings)
{
	EXPECT_EQ(ActionNamed("DanceOutsideWorshipSite").target, plan_actions::Target::WorshipSite);
	EXPECT_EQ(ActionNamed("DanceAroundArtefact").target, plan_actions::Target::Anything);
}

// The creature goes near to six or four times its height, never further than 50 metres
TEST(CreatureTempleDance, TheApproachIsHeldAtFiftyMetres)
{
	EXPECT_FLOAT_EQ(temple_dance::ApproachDistance(5.0f, temple_dance::k_TempleApproachHeights), 30.0f);
	EXPECT_FLOAT_EQ(temple_dance::ApproachDistance(10.0f, temple_dance::k_TempleApproachHeights), 50.0f);
	EXPECT_FLOAT_EQ(temple_dance::ApproachDistance(10.0f, temple_dance::k_ThingApproachHeights), 40.0f);
	EXPECT_FLOAT_EQ(temple_dance::ApproachDistance(20.0f, temple_dance::k_ThingApproachHeights), 50.0f);
}

// Outside the temple: go near the heart, start the last dance, face the heart for two seconds, dance for sixty seconds.
// No step makes a dance.
TEST(CreatureTempleDance, OutsideTheTempleItWalksStartsFacesAndDances)
{
	const auto steps = temple_dance::DanceOutsideTemple(7, 5.0f, 10.0f);
	ASSERT_EQ(steps.size(), 4u);
	EXPECT_EQ(steps[0].kind, Step::Kind::Move);
	EXPECT_EQ(steps[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(steps[0].movement.object, 7u);
	EXPECT_FLOAT_EQ(steps[0].movement.maxDistance, 30.0f);
	EXPECT_EQ(steps[1].kind, Step::Kind::Dance);
	EXPECT_EQ(steps[1].dance.kind, DanceOrder::Kind::Start);
	EXPECT_EQ(steps[2].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(steps[2].movement.object, 7u);
	EXPECT_FLOAT_EQ(steps[2].seconds, 2.0f);
	EXPECT_EQ(steps[3].dance.kind, DanceOrder::Kind::Dance);
	EXPECT_EQ(steps[3].dance.count, 600u);
	for (const auto& step : steps)
	{
		EXPECT_FALSE(step.kind == Step::Kind::Dance && step.dance.kind == DanceOrder::Kind::Set);
	}
}

// Around a thing: go near it, start the last dance, dance for sixty seconds
TEST(CreatureTempleDance, AroundAThingItWalksStartsAndDances)
{
	const auto steps = temple_dance::DanceAroundThing(9, 5.0f, 10.0f);
	ASSERT_EQ(steps.size(), 3u);
	EXPECT_EQ(steps[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(steps[0].movement.object, 9u);
	EXPECT_FLOAT_EQ(steps[0].movement.maxDistance, 20.0f);
	EXPECT_EQ(steps[1].dance.kind, DanceOrder::Kind::Start);
	EXPECT_EQ(steps[2].dance.kind, DanceOrder::Kind::Dance);
	EXPECT_EQ(steps[2].dance.count, 600u);
}

// The dance outside the temple goes to the heart of the creature's own temple, whatever worship site it was chosen
// for, and fails without a temple
TEST(CreatureTempleDance, TheTempleDanceGoesToItsOwnTemple)
{
	const auto& action = ActionNamed("DanceOutsideWorshipSite");
	plan_actions::Situation situation;
	situation.height = 5.0f;
	EXPECT_FALSE(plan_actions::Agenda(action, 3u, {}, situation, First).has_value());
	situation.templeHeart = plan_actions::Situation::TempleHeart {.temple = 11};
	const auto steps = plan_actions::Agenda(action, 3u, {}, situation, First);
	ASSERT_TRUE(steps.has_value());
	EXPECT_EQ(steps->front().movement.object, 11u);
}

// The dance that the agenda started continues through the turn to the temple between the start and the dance
TEST(CreatureTempleDance, TheDanceContinuesThroughTheStepsOfItsAgenda)
{
	constexpr uint32_t k_Agenda = 7;
	EXPECT_TRUE(creature_dance::DanceGoesOn(k_Agenda, k_Agenda, true));
}

// The dance stops when its agenda ends, when a new agenda replaces it, or when no agenda started it
TEST(CreatureTempleDance, TheDanceStopsWithItsAgenda)
{
	constexpr uint32_t k_Agenda = 7;
	EXPECT_FALSE(creature_dance::DanceGoesOn(k_Agenda, k_Agenda, false));
	EXPECT_FALSE(creature_dance::DanceGoesOn(k_Agenda, k_Agenda + 1, true));
	EXPECT_FALSE(creature_dance::DanceGoesOn(std::nullopt, k_Agenda, true));
}
