/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <numbers>

#include <gtest/gtest.h>

#include "Help/GuidanceRules.h"

using namespace openblack::guidance;

TEST(GuidanceRules, FirstLandKindsAreTheGamesOwn)
{
	// On the first land only these of the advisors' remarks may be heard
	for (size_t kind = static_cast<size_t>(Kind::OurTownAttacked); kind <= static_cast<size_t>(Kind::DestroyBuilding); ++kind)
	{
		const auto k = static_cast<Kind>(kind);
		const bool expected = k == Kind::AttackingTown || k == Kind::CreatureFight || k == Kind::GoodBeingEvil ||
		                      k == Kind::EvilBeingGood || k == Kind::DestroyBuilding;
		EXPECT_EQ(Rule(k).onFirstLand, expected) << kind;
	}
	EXPECT_EQ(Rule(Kind::AnyRemark).leastGap, 100u);
	EXPECT_EQ(Rule(Kind::AttackingTown).helpLevel, 3u);
	EXPECT_EQ(Rule(Kind::DestroyBuilding).leastGap, 1000u);
	EXPECT_EQ(Rule(Kind::GoodBeingEvil).leastGap, 5000u);
	EXPECT_EQ(Rule(Kind::CreatureFight).leastGap, 200u);
}

TEST(GuidanceRules, GapSpreadShrinksByTheCube)
{
	EXPECT_EQ(GapSpread(100, 0.0f), 500u);
	EXPECT_EQ(GapSpread(100, 1.0f), 0u);
	// 1 - 0.125 = 0.875 of 500
	EXPECT_EQ(GapSpread(100, 0.5f), 437u);
	EXPECT_EQ(GapSpread(0, 0.3f), 0u);
}

TEST(GuidanceRules, MayPlayNeedsLevelLandAndWait)
{
	const KindRule rule {.leastGap = 1000, .helpLevel = 3, .onFirstLand = false};
	EXPECT_TRUE(MayPlay(rule, 3, false, 1000, 1001));
	EXPECT_FALSE(MayPlay(rule, 3, false, 1000, 1000));
	EXPECT_FALSE(MayPlay(rule, 2, false, 10, 1001));
	EXPECT_FALSE(MayPlay(rule, 3, true, 10, 1001));
	const KindRule firstLand {.leastGap = 0, .helpLevel = 1, .onFirstLand = true};
	EXPECT_TRUE(MayPlay(firstLand, 1, true, 0, 1));
	// Not on the very turn it was last heard
	EXPECT_FALSE(MayPlay(firstLand, 1, true, 0, 0));
	// The help system off counts as help level 0
	EXPECT_FALSE(MayPlay(firstLand, 0, true, 0, 5));
}

TEST(GuidanceRules, FirstStampNeverBeforeTheFirstTurn)
{
	EXPECT_EQ(FirstStamp(500, 20), 480u);
	EXPECT_EQ(FirstStamp(20, 20), 0u);
	EXPECT_EQ(FirstStamp(0, 7), 0u);
}

TEST(GuidanceRules, RemarkRowsFollowTheKinds)
{
	EXPECT_EQ(RemarkRow(Kind::OurTownAttacked), 0u);
	EXPECT_EQ(RemarkRow(Kind::AttackingTown), 4u);
	EXPECT_EQ(RemarkRow(Kind::CreatureFight), 12u);
	EXPECT_EQ(RemarkRow(Kind::GoodBeingEvil), 16u);
	EXPECT_EQ(RemarkRow(Kind::EvilBeingGood), 17u);
	EXPECT_EQ(RemarkRow(Kind::DestroyBuilding), 21u);
	EXPECT_FALSE(RemarkRow(Kind::AnyRemark).has_value());
	EXPECT_FALSE(RemarkRow(Kind::OneOff).has_value());
}

TEST(GuidanceRules, RemarkChoicesStopAtTheEndOrOneShortOfAFullRow)
{
	RemarkTexts texts {};
	EXPECT_EQ(RemarkChoices(texts), 0u);
	texts.at(0) = 2361;
	texts.at(1) = 2362;
	texts.at(2) = 2365;
	texts.at(3) = 2017;
	EXPECT_EQ(RemarkChoices(texts), 4u);
	texts.fill(2168);
	EXPECT_EQ(RemarkChoices(texts), 33u);
}

TEST(GuidanceRules, DriftKeepsMostOfThePast)
{
	EXPECT_FLOAT_EQ(Drift(0.0f, 0.01f), 0.01f);
	EXPECT_FLOAT_EQ(Drift(1.0f, 0.0f), 0.95f);
	EXPECT_FLOAT_EQ(Drift(0.2f, -0.1f), 0.09f);
}

TEST(GuidanceRules, AlignmentRemarks)
{
	constexpr float k_Max = 0.01f;
	// Drift no more than twice a turn's most: nothing
	EXPECT_FALSE(AlignmentRemark(-0.02f, 0.9f, k_Max).has_value());
	// A good player drifting evil
	EXPECT_EQ(AlignmentRemark(-0.03f, 0.5f, k_Max), Kind::GoodBeingEvil);
	// Not good enough yet to be told off
	EXPECT_FALSE(AlignmentRemark(-0.03f, 0.4f, k_Max).has_value());
	// An evil player drifting good
	EXPECT_EQ(AlignmentRemark(0.03f, -0.5f, k_Max), Kind::EvilBeingGood);
	// Going further the same way needs more than 0.75, and picks the remarks the other way round
	EXPECT_FALSE(AlignmentRemark(0.03f, 0.7f, k_Max).has_value());
	EXPECT_EQ(AlignmentRemark(0.03f, 0.8f, k_Max), Kind::VeryEvil);
	EXPECT_EQ(AlignmentRemark(-0.03f, -0.8f, k_Max), Kind::VeryGood);
	// No alignment at all counts as drifting the other way
	EXPECT_FALSE(AlignmentRemark(0.03f, 0.0f, k_Max).has_value());
}

TEST(GuidanceRules, OneOffs)
{
	// Each check says its own remark, with that remark's chance
	EXPECT_EQ(Rule(OneOff::FullMoon).text, 3325u);
	EXPECT_FLOAT_EQ(Rule(OneOff::FullMoon).chance, 0.05f);
	EXPECT_EQ(Rule(OneOff::HandNearby).text, 3326u);
	EXPECT_FLOAT_EQ(Rule(OneOff::HandNearby).chance, 0.1f);
	EXPECT_TRUE(OneOffSaid(0.1f, 0.1f));
	EXPECT_FALSE(OneOffSaid(0.11f, 0.1f));
}

TEST(GuidanceRules, ClockNight)
{
	EXPECT_TRUE(ClockSaysNight(0, 0));
	EXPECT_TRUE(ClockSaysNight(5, 59));
	EXPECT_FALSE(ClockSaysNight(6, 0));
	EXPECT_FALSE(ClockSaysNight(20, 44));
	EXPECT_TRUE(ClockSaysNight(20, 45));
	EXPECT_TRUE(ClockSaysNight(20, 59));
	EXPECT_FALSE(ClockSaysNight(21, 0));
	EXPECT_FALSE(ClockSaysNight(22, 59));
	EXPECT_TRUE(ClockSaysNight(23, 0));
}

TEST(GuidanceRules, MoonLook)
{
	const float full = std::numbers::pi_v<float>;
	const auto atFull = LookAtMoon(full + 0.1f, true);
	EXPECT_TRUE(atFull.remark);
	EXPECT_EQ(atFull.wait, k_MoonRemarkWait);
	// Not at night by the computer's clock: looked at again later, by the cube of the distance
	const auto day = LookAtMoon(full + 0.1f, false);
	EXPECT_FALSE(day.remark);
	EXPECT_NEAR(day.wait, 12, 1);
	const auto later = LookAtMoon(full + 1.0f, true);
	EXPECT_FALSE(later.remark);
	EXPECT_NEAR(later.wait, 12000, 1);
	// A waxing moon is looked at again at once
	EXPECT_LT(LookAtMoon(full - 1.0f, true).wait, 1);
}
