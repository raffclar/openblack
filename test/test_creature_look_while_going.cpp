/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureLook.h"

using namespace openblack::creature_look;

namespace
{
constexpr float k_Tolerance = 1e-3f;
/// The values that the creature tables give every species
constexpr WhileGoingInfo k_Info {
    .sweepHalfAngle = 1.5f,
    .sweepAngleStep = 0.1f,
    .sweepNearest = 15.0f,
    .sweepFurthest = 95.0f,
    .sweepDistanceStep = 10.0f,
    .placeRange = 25.0f,
};
constexpr WhileGoingView k_Far {.position = {0.0f, 0.0f}, .place = {0.0f, 100.0f}, .turnsPerSecond = 10.0f};
} // namespace

TEST(CreatureLookWhileGoing, NearThePlaceItLooksAboutAsUsual)
{
	auto state = StartWhileGoing(k_Info);
	auto view = k_Far;
	view.place = {0.0f, 8.0f};
	EXPECT_FALSE(LookWhileGoing(state, k_Info, view).has_value());
	EXPECT_EQ(state.turns, 0u);
}

TEST(CreatureLookWhileGoing, WithinRangeItLooksAtThePlace)
{
	auto state = StartWhileGoing(k_Info);
	state.look = WhileGoing::Look::Sweep;
	auto view = k_Far;
	view.place = {0.0f, 25.0f};
	const auto point = LookWhileGoing(state, k_Info, view);
	ASSERT_TRUE(point.has_value());
	EXPECT_NEAR(point->at.y, 25.0f, k_Tolerance);
	EXPECT_FALSE(point->height.has_value());
	// The look does not count a turn
	EXPECT_EQ(state.turns, 0u);
}

TEST(CreatureLookWhileGoing, EachLookLastsTwoSecondsOfTurns)
{
	auto state = StartWhileGoing(k_Info);
	for (int turn = 0; turn < 20; ++turn)
	{
		const auto point = LookWhileGoing(state, k_Info, k_Far);
		ASSERT_TRUE(point.has_value());
		EXPECT_NEAR(point->at.y, 100.0f, k_Tolerance);
	}
	EXPECT_EQ(state.look, WhileGoing::Look::Place);
	// The twenty-first turn ends the look
	(void)LookWhileGoing(state, k_Info, k_Far);
	EXPECT_EQ(state.look, WhileGoing::Look::Watched);
	EXPECT_EQ(state.turns, 0u);
}

TEST(CreatureLookWhileGoing, TheSecondLookIsAtWhatItWatches)
{
	auto state = StartWhileGoing(k_Info);
	state.look = WhileGoing::Look::Watched;
	auto view = k_Far;
	view.watched = glm::vec3(40.0f, 12.0f, 30.0f);
	const auto watched = LookWhileGoing(state, k_Info, view);
	ASSERT_TRUE(watched.has_value());
	EXPECT_NEAR(watched->at.x, 40.0f, k_Tolerance);
	ASSERT_TRUE(watched->height.has_value());
	EXPECT_NEAR(*watched->height, 12.0f, k_Tolerance);
	// When it watches nothing, it looks at the place
	const auto place = LookWhileGoing(state, k_Info, k_Far);
	ASSERT_TRUE(place.has_value());
	EXPECT_NEAR(place->at.y, 100.0f, k_Tolerance);
}

TEST(CreatureLookWhileGoing, TheSweepTurnsBackAndLooksFurther)
{
	auto state = StartWhileGoing(k_Info);
	state.look = WhileGoing::Look::Sweep;
	state.sweepAngle = 1.35f;
	(void)LookWhileGoing(state, k_Info, k_Far);
	EXPECT_NEAR(state.sweepAngle, 1.45f, k_Tolerance);
	EXPECT_NEAR(state.sweepDistance, 15.0f, k_Tolerance);
	// At the side it turns back and looks a step further ahead
	(void)LookWhileGoing(state, k_Info, k_Far);
	EXPECT_NEAR(state.sweepAngleStep, -0.1f, k_Tolerance);
	EXPECT_NEAR(state.sweepDistance, 25.0f, k_Tolerance);
	// At the furthest it starts again at the nearest
	state.sweepAngle = -1.45f;
	state.sweepDistance = 85.0f;
	(void)LookWhileGoing(state, k_Info, k_Far);
	EXPECT_NEAR(state.sweepDistance, 15.0f, k_Tolerance);
}

TEST(CreatureLookWhileGoing, TheSweepLooksAtTheGroundAhead)
{
	auto state = StartWhileGoing(k_Info);
	state.look = WhileGoing::Look::Sweep;
	state.sweepAngleStep = 0.0f;
	const auto point = LookWhileGoing(state, k_Info, k_Far);
	ASSERT_TRUE(point.has_value());
	EXPECT_NEAR(glm::length(point->at), 15.0f, 0.05f);
	EXPECT_FALSE(point->height.has_value());
}

TEST(CreatureLookWhileGoing, WithNothingWatchedTheSecondLookIsAtARecentMover)
{
	auto state = StartWhileGoing(k_Info);
	state.look = WhileGoing::Look::Watched;
	auto view = k_Far;
	view.mover = glm::vec3(-20.0f, 7.0f, 50.0f);
	view.moverTurn = 100;
	view.turn = 119;
	const auto mover = LookWhileGoing(state, k_Info, view);
	ASSERT_TRUE(mover.has_value());
	EXPECT_NEAR(mover->at.x, -20.0f, k_Tolerance);
	EXPECT_NEAR(mover->at.y, 50.0f, k_Tolerance);
	ASSERT_TRUE(mover->height.has_value());
	EXPECT_NEAR(*mover->height, 7.0f, k_Tolerance);
	// What it watches comes first
	view.watched = glm::vec3(40.0f, 12.0f, 30.0f);
	const auto watched = LookWhileGoing(state, k_Info, view);
	ASSERT_TRUE(watched.has_value());
	EXPECT_NEAR(watched->at.x, 40.0f, k_Tolerance);
}

TEST(CreatureLookWhileGoing, AMoverNoticedTwoSecondsAgoIsTooOld)
{
	auto state = StartWhileGoing(k_Info);
	state.look = WhileGoing::Look::Watched;
	auto view = k_Far;
	view.mover = glm::vec3(-20.0f, 7.0f, 50.0f);
	view.moverTurn = 100;
	view.turn = 120;
	const auto point = LookWhileGoing(state, k_Info, view);
	ASSERT_TRUE(point.has_value());
	EXPECT_NEAR(point->at.y, 100.0f, k_Tolerance);
	EXPECT_FALSE(point->height.has_value());
}

TEST(CreatureLookWhileGoing, ARecentMoverIsInTheLastTwoSecondsOfTurns)
{
	EXPECT_TRUE(IsRecent(100, 100, 10.0f));
	EXPECT_TRUE(IsRecent(100, 119, 10.0f));
	EXPECT_FALSE(IsRecent(100, 120, 10.0f));
	// The count of turns can go round to zero
	EXPECT_TRUE(IsRecent(0xFFFFFFFFu, 5, 10.0f));
}

TEST(CreatureLookWhileGoing, ItNoticesOnlyAMovingThingThatIsNotItself)
{
	EXPECT_TRUE(Notices(false, true));
	EXPECT_FALSE(Notices(true, true));
	EXPECT_FALSE(Notices(false, false));
}
