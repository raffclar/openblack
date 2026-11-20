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

#include "Online/ServerSilence.h"

using namespace openblack::online::server_silence;

namespace
{
constexpr int64_t k_Ms = 1'000;
} // namespace

TEST(ServerSilence, TheTextShowsOnlyAfterFourTurnsWithNoMessage)
{
	Watch watch;
	EXPECT_FALSE(watch.Update({.now = 1000 * k_Ms, .waits = true, .heardAt = 1000 * k_Ms}));
	EXPECT_FALSE(watch.Update({.now = 1400 * k_Ms, .waits = true, .heardAt = 1000 * k_Ms}));
	EXPECT_FALSE(watch.Shown().has_value());
	EXPECT_EQ(watch.Silence(), 400 * k_Ms);
	EXPECT_TRUE(watch.Update({.now = 1401 * k_Ms, .waits = true, .heardAt = 1000 * k_Ms}));
	ASSERT_TRUE(watch.Shown().has_value());
	EXPECT_EQ(*watch.Shown(), 401 * k_Ms);
}

TEST(ServerSilence, TheTextCountsUpAndStopsAtOnceWhenAMessageComes)
{
	Watch watch;
	std::ignore = watch.Update({.now = 0, .waits = true, .heardAt = 0});
	std::ignore = watch.Update({.now = 24'300 * k_Ms, .waits = true, .heardAt = 0});
	ASSERT_TRUE(watch.Shown().has_value());
	EXPECT_EQ(Text(*watch.Shown()), u"24.3s");
	EXPECT_TRUE(watch.Update({.now = 24'310 * k_Ms, .waits = true, .heardAt = 24'305 * k_Ms}));
	EXPECT_FALSE(watch.Shown().has_value());
}

TEST(ServerSilence, NoTextWhileTheClientDoesNotWaitForTurns)
{
	Watch watch;
	std::ignore = watch.Update({.now = 0, .waits = true, .heardAt = 0});
	// A pause of the clock of the room, a wait for the loads or the end of the turns: no text, however long
	EXPECT_FALSE(watch.Update({.now = 60'000 * k_Ms, .waits = false, .heardAt = 0}));
	EXPECT_FALSE(watch.Shown().has_value());
	EXPECT_EQ(watch.Silence(), 0);
}

TEST(ServerSilence, TheSilenceCountsFromTheStartOfTheWaitAfterAPause)
{
	Watch watch;
	// The last message (a pong) came 0.9 s before the pause of the room ended
	std::ignore = watch.Update({.now = 10'000 * k_Ms, .waits = false, .heardAt = 9'100 * k_Ms});
	EXPECT_FALSE(watch.Update({.now = 10'000 * k_Ms, .waits = true, .heardAt = 9'100 * k_Ms}));
	EXPECT_FALSE(watch.Shown().has_value());
	EXPECT_EQ(watch.Silence(), 0);
	EXPECT_TRUE(watch.Update({.now = 10'500 * k_Ms, .waits = true, .heardAt = 9'100 * k_Ms}));
	EXPECT_EQ(*watch.Shown(), 500 * k_Ms);
}

TEST(ServerSilence, NoMessageAtAllCountsFromTheStartOfTheWait)
{
	Watch watch;
	std::ignore = watch.Update({.now = 5'000 * k_Ms, .waits = true, .heardAt = std::nullopt});
	std::ignore = watch.Update({.now = 6'000 * k_Ms, .waits = true, .heardAt = std::nullopt});
	ASSERT_TRUE(watch.Shown().has_value());
	EXPECT_EQ(*watch.Shown(), 1'000 * k_Ms);
}

TEST(ServerSilence, ASlowClockOfTheRoomNeedsALongerSilence)
{
	EXPECT_EQ(ShowAfter(1.0f), 400 * k_Ms);
	// A fast clock keeps the time of four normal turns
	EXPECT_EQ(ShowAfter(0.5f), 400 * k_Ms);
	EXPECT_EQ(ShowAfter(4.0f), 1'600 * k_Ms);
	Watch watch;
	std::ignore = watch.Update({.now = 0, .waits = true, .heardAt = 0, .speed = 4.0f});
	std::ignore = watch.Update({.now = 1'000 * k_Ms, .waits = true, .heardAt = 0, .speed = 4.0f});
	EXPECT_FALSE(watch.Shown().has_value());
	std::ignore = watch.Update({.now = 1'601 * k_Ms, .waits = true, .heardAt = 0, .speed = 4.0f});
	EXPECT_TRUE(watch.Shown().has_value());
}

TEST(ServerSilence, TheTextCutsOffTheHundredths)
{
	EXPECT_EQ(Text(0), u"0.0s");
	EXPECT_EQ(Text(401 * k_Ms), u"0.4s");
	EXPECT_EQ(Text(499'999), u"0.4s");
	EXPECT_EQ(Text(24'399 * k_Ms), u"24.3s");
	EXPECT_EQ(Text(125'000 * k_Ms), u"125.0s");
	EXPECT_EQ(Text(-5), u"0.0s");
}

TEST(ServerSilence, TheHandGetsNoPressOrMoveAndItsReleasesWaitDuringASilence)
{
	EXPECT_EQ(Admit(HandEvent::Press, false), Admission::Pass);
	EXPECT_EQ(Admit(HandEvent::Release, false), Admission::Pass);
	EXPECT_EQ(Admit(HandEvent::Move, false), Admission::Pass);
	EXPECT_EQ(Admit(HandEvent::Other, false), Admission::Pass);
	EXPECT_EQ(Admit(HandEvent::Press, true), Admission::Drop);
	EXPECT_EQ(Admit(HandEvent::Move, true), Admission::Drop);
	EXPECT_EQ(Admit(HandEvent::Other, true), Admission::Drop);
	EXPECT_EQ(Admit(HandEvent::Release, true), Admission::Hold);
}
