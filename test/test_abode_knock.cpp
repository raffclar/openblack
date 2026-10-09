/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/AbodeKnock.h"

using namespace openblack;
using namespace openblack::ecs::abode_knock;

TEST(AbodeKnock, AnyBegunBuildingButTheVillageCentreTakesAKnock)
{
	EXPECT_TRUE(IsTappable(AbodeType::LivingQuarters, 0.1f));
	EXPECT_TRUE(IsTappable(AbodeType::StoragePit, 1.0f));
	EXPECT_FALSE(IsTappable(AbodeType::LivingQuarters, 0.0f));
	EXPECT_FALSE(IsTappable(AbodeType::TownCentre, 1.0f));
}

TEST(AbodeKnock, OnlyHousesAndWindmillsKnockBack)
{
	EXPECT_TRUE(KnocksBack(AbodeType::LivingQuarters));
	EXPECT_TRUE(KnocksBack(AbodeType::Windmill));
	EXPECT_FALSE(KnocksBack(AbodeType::StoragePit));
	EXPECT_FALSE(KnocksBack(AbodeType::Wonder));
	EXPECT_FALSE(KnocksBack(AbodeType::Field));
}

TEST(AbodeKnock, TheNineSoundsComeInTurn)
{
	EXPECT_EQ(NextKnockSound(0), 1u);
	EXPECT_EQ(NextKnockSound(8), 0u);
}

TEST(AbodeKnock, AKnockStartsOrHoldsTheReadout)
{
	// Nothing showing: it starts, growing in
	EXPECT_EQ(Knock(0), 8000u);
	// Still growing in: unchanged
	EXPECT_EQ(Knock(7500), 7500u);
	// Showing: held for its full six seconds again
	EXPECT_EQ(Knock(7000), 7000u);
	EXPECT_EQ(Knock(3000), 7000u);
	EXPECT_EQ(Knock(1000), 7000u);
	// Shrinking: it grows back from the size it had shrunk to
	EXPECT_EQ(Knock(400), 7600u);
	EXPECT_NEAR(ReadoutScale(400), ReadoutScale(7600), 1.0f / 255.0f);
}

TEST(AbodeKnock, TheReadoutRunsDownByTheFrame)
{
	EXPECT_EQ(Tick(8000, 16), 7984u);
	EXPECT_EQ(Tick(10, 16), 0u);
}

TEST(AbodeKnock, TheMarkersGrowInHoldAndShrinkAway)
{
	EXPECT_FLOAT_EQ(ReadoutScale(8000), 0.0f);
	EXPECT_FLOAT_EQ(ReadoutScale(7000), 1.0f);
	EXPECT_FLOAT_EQ(ReadoutScale(4000), 1.0f);
	// The game's step per millisecond is a little under 0.255, so the last second starts a step down
	EXPECT_FLOAT_EQ(ReadoutScale(1000), 254.0f / 255.0f);
	EXPECT_FLOAT_EQ(ReadoutScale(0), 0.0f);
	// Half way through growing or shrinking: 127 of 255 steps, the fraction cut off
	EXPECT_FLOAT_EQ(ReadoutScale(500), 127.0f / 255.0f);
	EXPECT_FLOAT_EQ(ReadoutScale(7500), 127.0f / 255.0f);
}

TEST(AbodeKnock, AHouseShowsGoldForItsAdultsAndGreenForItsFreePlaces)
{
	const auto markers = Readout(3, 1, 1.0f);
	ASSERT_EQ(markers.size(), 3u);
	EXPECT_EQ(markers[0].colour, k_LivedInColour);
	EXPECT_EQ(markers[1].colour, k_FreePlaceColour);
	EXPECT_EQ(markers[2].colour, k_FreePlaceColour);
	// The row starts half a place to the left for each place, and its markers stand a 1.3th apart
	EXPECT_FLOAT_EQ(markers[0].offset, -1.5f);
	EXPECT_FLOAT_EQ(markers[2].offset, -1.5f + 2.0f / 1.3f);
	EXPECT_FLOAT_EQ(markers[0].size, 1.0f / 1.3f);
}

TEST(AbodeKnock, ShrunkMarkersKeepASmallestSize)
{
	EXPECT_FLOAT_EQ(Readout(1, 0, 0.0f)[0].size, k_SmallestMarker);
	EXPECT_TRUE(Readout(0, 0, 1.0f).empty());
}
