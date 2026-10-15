/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <gtest/gtest.h>

#include "Creature/CreatureLayers.h"
#include "Creature/CreatureOddAgendas.h"
#include "Creature/CreaturePlanActions.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace odd = openblack::creature_odd;
namespace plan_actions = openblack::creature_plan_actions;
namespace animations = openblack::creature_layers::animations;

namespace
{
uint32_t First(uint32_t /*range*/)
{
	return 0;
}
uint32_t Last(uint32_t range)
{
	return range - 1;
}
} // namespace

// The creature scratches its head once, with a smile
TEST(CreatureOddAgendas, ShowLearntLessonScratchesWithASmile)
{
	const auto agenda = odd::ShowLearntLesson();
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[0].animation, animations::k_Scratch);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Smile);
	EXPECT_FALSE(agenda[0].gaze.has_value());
}

// It turns to the camera, then scratches its head and looks confused, puzzled, with its eyes on the camera
TEST(CreatureOddAgendas, LookConfusedFacesTheCamera)
{
	const auto agenda = odd::LookConfused(glm::vec2 {3.0f, 4.0f});
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Move);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(3.0f, 4.0f));
	EXPECT_EQ(agenda[1].animation, animations::k_Scratch);
	EXPECT_EQ(agenda[2].animation, animations::k_Confused);
	for (const auto& step : agenda)
	{
		ASSERT_TRUE(step.gaze.has_value());
		EXPECT_TRUE(step.gaze->camera);
	}
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Puzzled);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Puzzled);
}

// Without a camera it does not turn first
TEST(CreatureOddAgendas, LookConfusedWithoutACameraDoesNotTurn)
{
	const auto agenda = odd::LookConfused(std::nullopt);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].animation, animations::k_Scratch);
	EXPECT_EQ(agenda[1].animation, animations::k_Confused);
}

// One time in two it faints, lies still for five seconds and more, and gets up with four tenths of its life
TEST(CreatureOddAgendas, BehaveStrangelyCanFaint)
{
	const auto agenda = odd::BehaveStrangely(10.0f, First);
	ASSERT_EQ(agenda.size(), 5u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::WatchCamera);
	EXPECT_FLOAT_EQ(agenda[0].seconds, odd::k_StrangeStareSeconds);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Curiosity);
	EXPECT_EQ(agenda[0].heldEyes, Eyes::Stoned);
	EXPECT_EQ(agenda[1].animation, animations::k_Happy);
	const auto& faint = agenda[2];
	EXPECT_EQ(faint.kind, Step::Kind::Static);
	EXPECT_TRUE(faint.faint);
	EXPECT_TRUE(faint.holdLoop);
	EXPECT_EQ(faint.sequence[0], animations::k_Faint);
	EXPECT_EQ(faint.sequence[2], animations::k_GetUp);
	EXPECT_EQ(faint.effect, Effect::Revived);
	// The random fraction of a draw of 0 is 0
	EXPECT_FLOAT_EQ(faint.seconds, odd::k_StrangeFaintSeconds);
	EXPECT_EQ(agenda[3].animation, animations::k_Confused);
	EXPECT_TRUE(agenda[3].sleepyEyes);
	EXPECT_EQ(agenda[4].animation, animations::k_Confused);
	EXPECT_EQ(agenda[4].heldEyes, Eyes::Stoned);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Grimace);
}

// The time that it lies is in whole turns, up to ten seconds
TEST(CreatureOddAgendas, BehaveStrangelyLiesInWholeTurns)
{
	uint32_t draws = 0;
	const Random random = [&draws](uint32_t range) { return draws++ == 0 ? 0 : range - 1; };
	const auto agenda = odd::BehaveStrangely(10.0f, random);
	ASSERT_EQ(agenda.size(), 5u);
	const float seconds = agenda[2].seconds;
	EXPECT_GT(seconds, odd::k_StrangeFaintSeconds);
	EXPECT_LE(seconds, odd::k_StrangeFaintSeconds + odd::k_StrangeFaintExtraSeconds);
	EXPECT_FLOAT_EQ(seconds * 10.0f, std::trunc(seconds * 10.0f + 1e-3f));
}

// The other time it does not faint
TEST(CreatureOddAgendas, BehaveStrangelyMayNotFaint)
{
	const auto agenda = odd::BehaveStrangely(10.0f, Last);
	ASSERT_EQ(agenda.size(), 4u);
	for (const auto& step : agenda)
	{
		EXPECT_FALSE(step.faint);
	}
}

// The three actions have agendas a plan can carry out
TEST(CreatureOddAgendas, PlansCanCarryOutTheOddActions)
{
	for (const auto* name : {"ShowLearntLesson", "LookConfused", "BehaveStrangely"})
	{
		const auto* action = plan_actions::For(name);
		ASSERT_NE(action, nullptr) << name;
		const auto agenda =
		    plan_actions::Agenda(*action, std::nullopt, {0.0f, 0.0f}, {.camera = glm::vec2 {1.0f, 1.0f}}, First);
		ASSERT_TRUE(agenda.has_value()) << name;
		EXPECT_FALSE(agenda->empty()) << name;
	}
}
