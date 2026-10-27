/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <tuple>

#include <gtest/gtest.h>

#include "Hand/OtherHeldDraw.h"

using namespace openblack;
using ecs::components::HeldDrawTrack;

TEST(OtherHeldDraw, BeforeTheSecondReportTheThingIsAtTheReportedPlace)
{
	HeldDrawTrack track;
	EXPECT_EQ(hand_grab::other_held::Step(track, {5.0f, 1.0f, 7.0f}, 20), glm::vec3(5.0f, 1.0f, 7.0f));
	EXPECT_EQ(track.elapsedMs, 20);
	// A report with no frames between the reports keeps it at the reported place
	hand_grab::other_held::Reported(track, {5.0f, 1.0f, 7.0f});
	hand_grab::other_held::Reported(track, {5.0f, 1.0f, 7.0f});
	EXPECT_EQ(track.intervalMs, 0);
	EXPECT_EQ(hand_grab::other_held::Step(track, {9.0f, 1.0f, 7.0f}, 20), glm::vec3(9.0f, 1.0f, 7.0f));
}

TEST(OtherHeldDraw, TheThingMovesInAStraightLineOverTheTimeBetweenTheLastTwoReports)
{
	HeldDrawTrack track;
	// 100 ms of frames go by between two reports
	for (int frame = 0; frame < 5; ++frame)
	{
		std::ignore = hand_grab::other_held::Step(track, {0.0f, 0.0f, 0.0f}, 20);
	}
	hand_grab::other_held::Reported(track, {0.0f, 0.0f, 0.0f});
	EXPECT_EQ(track.intervalMs, 100);
	EXPECT_EQ(track.elapsedMs, 0);
	const glm::vec3 reported {10.0f, 4.0f, -20.0f};
	EXPECT_EQ(hand_grab::other_held::Step(track, reported, 25), glm::vec3(2.5f, 1.0f, -5.0f));
	EXPECT_EQ(hand_grab::other_held::Step(track, reported, 25), glm::vec3(5.0f, 2.0f, -10.0f));
	// It stops at the reported place, however long the next report takes
	EXPECT_EQ(hand_grab::other_held::Step(track, reported, 60), reported);
	EXPECT_EQ(hand_grab::other_held::Step(track, reported, 60), reported);
	// The next movement starts from the place before the report, over the time of the frames since the last report
	hand_grab::other_held::Reported(track, reported);
	EXPECT_EQ(track.intervalMs, 170);
	EXPECT_EQ(track.from, reported);
}
