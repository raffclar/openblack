/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Help/HelpProfile.h"

using namespace openblack::help::profile;

TEST(HelpProfile, AnEventCountsOnceATurn)
{
	HelpProfile profile;
	profile.Trigger(k_TapWithText);
	profile.Trigger(k_TapWithText);
	EXPECT_EQ(profile.Count(k_TapWithText).Total(), 1u);
	profile.EndTurn();
	profile.Trigger(k_TapWithText);
	EXPECT_EQ(profile.Count(k_TapWithText).Total(), 2u);
	EXPECT_EQ(profile.Count(k_TapWithoutText).Total(), 0u);
}

TEST(HelpProfile, EventsAlsoCountAsTheirGroups)
{
	HelpProfile profile;
	// A tap is one of events 1 to 42, and every event counts as 48
	profile.Trigger(k_TapWithoutText);
	EXPECT_EQ(profile.Count(43).Total(), 1u);
	EXPECT_EQ(profile.Count(k_AnyEvent).Total(), 1u);
	EXPECT_EQ(profile.Count(24).Total(), 0u);
	EXPECT_EQ(profile.Count(11).Total(), 0u);
	profile.EndTurn();
	// 14 to 23 count as 24, 9 to 11 as 11 (11 itself only once)
	profile.Trigger(14);
	profile.Trigger(11);
	EXPECT_EQ(profile.Count(24).Total(), 1u);
	EXPECT_EQ(profile.Count(11).Total(), 1u);
	// Two events in a turn are one of their group's
	EXPECT_EQ(profile.Count(43).Total(), 2u);
	profile.EndTurn();
	// 43 and later aren't in the 1 to 42 group; 0 isn't either
	profile.Trigger(44);
	profile.Trigger(0);
	EXPECT_EQ(profile.Count(43).Total(), 2u);
	EXPECT_EQ(profile.Count(k_AnyEvent).Total(), 3u);
	// Past the last: nothing
	profile.Trigger(k_EventCount);
	EXPECT_EQ(profile.Count(k_AnyEvent).Total(), 3u);
}

TEST(HelpProfile, TheClockRunsATenthOfASecondATurn)
{
	HelpProfile profile;
	EXPECT_EQ(profile.Clock(), 0u);
	profile.EndTurn();
	profile.EndTurn();
	EXPECT_EQ(profile.Clock(), 200u);
}

TEST(HelpProfile, SecondsSinceTheLastTime)
{
	HelpProfile profile;
	EXPECT_FLOAT_EQ(profile.Count(k_TapWithText).SecondsSince(profile.Clock()), 0.0f);
	profile.Trigger(k_TapWithText);
	for (int turn = 0; turn < 25; ++turn)
	{
		profile.EndTurn();
	}
	EXPECT_FLOAT_EQ(profile.Count(k_TapWithText).SecondsSince(profile.Clock()), 2.5f);
}

TEST(HelpProfile, PerSecondIsTheGapsOverTheTimeSinceTheOldest)
{
	HelpProfile profile;
	auto& taps = profile.Count(k_TapWithText);
	profile.Trigger(k_TapWithText);
	EXPECT_FLOAT_EQ(taps.PerSecond(profile.Clock()), 0.0f);
	// Taps at 0, 0.5 and 1 second: two gaps over one second
	for (int tap = 0; tap < 2; ++tap)
	{
		for (int turn = 0; turn < 5; ++turn)
		{
			profile.EndTurn();
		}
		profile.Trigger(k_TapWithText);
	}
	EXPECT_FLOAT_EQ(taps.PerSecond(profile.Clock()), 2.0f);
	// Measured to now, not to the last tap
	for (int turn = 0; turn < 10; ++turn)
	{
		profile.EndTurn();
	}
	EXPECT_FLOAT_EQ(taps.PerSecond(profile.Clock()), 1.0f);
}

TEST(HelpProfile, PerSecondKeepsTheLatestSixtyFourTimes)
{
	EventCount count;
	for (uint32_t turn = 0; turn < 100; ++turn)
	{
		count.Trigger(turn * 100);
		count.EndTurn();
	}
	// The oldest kept is turn 36: 63 gaps over 6.3 seconds
	EXPECT_FLOAT_EQ(count.PerSecond(9900), 10.0f);
	EXPECT_EQ(count.Total(), 100u);
}

TEST(HelpProfile, AClockBehindTheTimesReadsNothing)
{
	EventCount count;
	count.Trigger(1000);
	count.EndTurn();
	count.Trigger(2000);
	EXPECT_FLOAT_EQ(count.SecondsSince(500), 0.0f);
	// The times were pulled back to the clock: both at 500
	EXPECT_FLOAT_EQ(count.PerSecond(500), 0.0f);
	EXPECT_FLOAT_EQ(count.SecondsSince(1500), 1.0f);
	EXPECT_FLOAT_EQ(count.PerSecond(1500), 1.0f);
}

TEST(HelpProfile, TheSmoothedRateFollowsTheTurnsItHappensOn)
{
	EventCount count;
	count.Trigger(0);
	count.EndTurn();
	EXPECT_FLOAT_EQ(count.SmoothedRate(), 0.005f);
	count.EndTurn();
	EXPECT_FLOAT_EQ(count.SmoothedRate(), 0.005f - 0.005f * 0.005f);
}

TEST(HelpProfile, ScriptsAskAboutOneToFortyEight)
{
	EXPECT_FALSE(HelpProfile::ScriptMayAsk(0));
	EXPECT_TRUE(HelpProfile::ScriptMayAsk(1));
	EXPECT_TRUE(HelpProfile::ScriptMayAsk(48));
	EXPECT_FALSE(HelpProfile::ScriptMayAsk(49));
	EXPECT_FALSE(HelpProfile::ScriptMayAsk(-3));
}
