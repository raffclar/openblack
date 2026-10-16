/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <deque>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureFace.h"
#include "Creature/CreatureMusicReaction.h"

using namespace openblack;
using namespace openblack::creature_music;
using creature_desires::Desire;
using creature_mind::Step;

namespace
{
/// Gives the rolls in turn, then the last roll again and again, and records the ranges asked for
struct FakeRolls
{
	std::deque<uint32_t> rolls;
	uint32_t after {0};
	std::vector<uint32_t> ranges;

	[[nodiscard]] creature_mind::Random Function()
	{
		return [this](uint32_t range) {
			ranges.push_back(range);
			if (rolls.empty())
			{
				return after;
			}
			const auto roll = rolls.front();
			rolls.pop_front();
			return roll;
		};
	}
};

constexpr Camera k_Camera {.position = {10.0f, 20.0f, 30.0f}, .known = true};
} // namespace

TEST(CreatureMusicReaction, TheMoodPicksTheReaction)
{
	EXPECT_EQ(ReactionTo(0.0f), Reaction::New);
	EXPECT_EQ(ReactionTo(0.1f), Reaction::Liked);
	EXPECT_EQ(ReactionTo(-0.1f), Reaction::Disliked);
}

TEST(CreatureMusicReaction, ThePlanLooksOrPointsAtTheCameraOrIsSad)
{
	EXPECT_EQ(PlanFor(0.0f, 0).action, "LookAtCamera");
	EXPECT_EQ(PlanFor(0.5f, 2).action, "PointAtCamera");
	EXPECT_EQ(PlanFor(0.5f, 2).desire, Desire::IdleWithPlayer);
	EXPECT_EQ(PlanFor(-0.5f, 1).action, "BeSad");
	EXPECT_EQ(PlanFor(-0.5f, 1).desire, Desire::Sadness);
}

TEST(CreatureMusicReaction, ANewTrackMakesAGoodCreaturePullFacesAtTheCamera)
{
	// Eight rounds; each round turns to the camera and pulls the main face, then the static action, the point at the
	// camera, and no dance
	FakeRolls fake {.rolls = {0}};
	const auto agenda = NewTrack(fake.Function(), 0.5f, k_Camera);
	ASSERT_EQ(agenda.size(), 18u);
	EXPECT_EQ(agenda.at(0).kind, Step::Kind::Move);
	EXPECT_EQ(agenda.at(0).face, creature_face::Cue::Curiosity);
	EXPECT_EQ(agenda.at(1).kind, Step::Kind::Face);
	EXPECT_EQ(agenda.at(1).animation, 16u);
	EXPECT_TRUE(agenda.at(1).main);
	const auto& still = agenda.at(16);
	EXPECT_EQ(still.kind, Step::Kind::Static);
	EXPECT_FLOAT_EQ(still.seconds, 20.0f);
	EXPECT_TRUE(still.sleepyEyes);
	EXPECT_EQ(still.sequence.at(0), 37u);
	EXPECT_EQ(still.sequence.at(2), 39u);
	EXPECT_EQ(agenda.at(17).kind, Step::Kind::Object);
	EXPECT_FLOAT_EQ(agenda.at(17).order.seconds.value_or(0.0f), 1.0f);
	// The first round asks no roll for the turn: 8 rounds, then 5 choices, 7 turns, the static's time and the dance
	EXPECT_EQ(fake.ranges.front(), 8u);
	EXPECT_EQ(fake.ranges.at(1), 5u);
	EXPECT_EQ(fake.ranges.at(2), 2u);
}

TEST(CreatureMusicReaction, ANewTrackMakesAnEvilCreatureAmazedAndMayDanceASecond)
{
	FakeRolls fake {.rolls = {0}, .after = 99};
	const auto agenda = NewTrack(fake.Function(), -0.5f, k_Camera);
	// Round 0: turn and the choice 99 (a sleepy action, 99 > 50: the yawn); the turn roll 99 skips the turn afterwards
	EXPECT_EQ(agenda.at(1).kind, Step::Kind::Action);
	EXPECT_EQ(agenda.at(1).animation, 63u);
	EXPECT_TRUE(agenda.at(1).sleepyEyes);
	EXPECT_EQ(agenda.back().kind, Step::Kind::PracticeDance);
	EXPECT_FLOAT_EQ(agenda.back().seconds, 1.0f);
}

TEST(CreatureMusicReaction, ALikedTrackEndsWithAStaticActionAndADance)
{
	FakeRolls fake {.rolls = {0}};
	const auto agenda = LikedTrack(fake.Function(), 0.3f, k_Camera);
	ASSERT_GE(agenda.size(), 2u);
	const auto& still = agenda.at(agenda.size() - 2);
	EXPECT_EQ(still.kind, Step::Kind::Static);
	EXPECT_FLOAT_EQ(still.seconds, 0.0f);
	EXPECT_EQ(still.face, creature_face::Cue::Smile);
	const auto& dance = agenda.back();
	EXPECT_EQ(dance.kind, Step::Kind::PracticeDance);
	EXPECT_FLOAT_EQ(dance.seconds, 8.0f);
	ASSERT_TRUE(dance.gaze.has_value());
	EXPECT_TRUE(dance.gaze->camera);
	// The first round: the face roll, the turn, then the happy clip
	EXPECT_EQ(agenda.at(1).animation, 55u);
	EXPECT_EQ(agenda.at(1).face, creature_face::Cue::Smile);
	EXPECT_EQ(fake.ranges.at(1), 3u);
	EXPECT_EQ(fake.ranges.at(2), 12u);
}

TEST(CreatureMusicReaction, ALikedTrackDancesAtMostAMinute)
{
	FakeRolls fake {.rolls = {0}};
	EXPECT_FLOAT_EQ(LikedTrack(fake.Function(), 10.0f, k_Camera).back().seconds, 60.0f);
}

TEST(CreatureMusicReaction, ADislikedTrackMakesAnEvilCreatureRudeOrAngry)
{
	// Eight rounds; round 0: face anger, turn, choice 5 (by alignment), roll 30: the rude gesture
	FakeRolls fake {.rolls = {0, 0, 5, 30, 1, 1, 8, 31}, .after = 1};
	const auto agenda = DislikedTrack(fake.Function(), -0.5f, k_Camera);
	EXPECT_EQ(agenda.at(0).kind, Step::Kind::Move);
	EXPECT_EQ(agenda.at(0).face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda.at(1).animation, 222u);
	EXPECT_FALSE(agenda.at(1).gaze.has_value());
	// Round 1: face fear, no turn, choice 8, roll 31: angry
	EXPECT_EQ(agenda.at(2).animation, 53u);
	EXPECT_EQ(agenda.at(2).face, creature_face::Cue::Fear);
}

TEST(CreatureMusicReaction, ADislikedTrackMakesAGoodCreatureCry)
{
	FakeRolls fake {.rolls = {0, 2, 6}, .after = 1};
	const auto agenda = DislikedTrack(fake.Function(), 0.0f, k_Camera);
	EXPECT_EQ(agenda.at(1).animation, 219u);
	EXPECT_EQ(agenda.at(1).face, creature_face::Cue::Sad);
}

TEST(CreatureMusicReaction, WithoutACameraItNeitherTurnsNorPoints)
{
	FakeRolls fake {.rolls = {0, 1}, .after = 2};
	const auto agenda = NewTrack(fake.Function(), 0.5f, Camera {});
	for (const auto& step : agenda)
	{
		EXPECT_NE(step.kind, Step::Kind::Move);
		EXPECT_NE(step.kind, Step::Kind::Object);
	}
}

TEST(CreatureMusicReaction, WhatComesNext)
{
	EXPECT_EQ(FollowUpValue(0.95f), 9);
	EXPECT_EQ(FollowUpValue(-0.25f), -2);
	EXPECT_TRUE(ShowsHappyAfterLikedTrack(20));
	EXPECT_FALSE(ShowsHappyAfterLikedTrack(21));
	EXPECT_TRUE(MayDanceWithVillagers(9));
	EXPECT_FALSE(MayDanceWithVillagers(8));
}

TEST(CreatureMusicReaction, TheDominantDesireAfterTheReaction)
{
	EXPECT_EQ(DominantAfter(Reaction::New, 0).desire, Desire::IdleWithPlayer);
	EXPECT_FLOAT_EQ(DominantAfter(Reaction::New, 0).seconds, 15.0f);
	EXPECT_EQ(DominantAfter(Reaction::Liked, 0).desire, Desire::PlayWithPlayer);
	EXPECT_FLOAT_EQ(DominantAfter(Reaction::Liked, 0).seconds, 180.0f);
	EXPECT_EQ(DominantAfter(Reaction::Disliked, 51).desire, Desire::Sadness);
	EXPECT_EQ(DominantAfter(Reaction::Disliked, 50).desire, Desire::Fear);
}
