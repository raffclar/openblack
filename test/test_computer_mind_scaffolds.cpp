/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>

#include <gtest/gtest.h>

#include "ECS/ComputerMindScaffoldRules.h"

namespace rules = openblack::computer_mind_rules;

TEST(ComputerMindScaffolds, TheConstantsAreTheFloatsOfTheGame)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(rules::k_ScaffoldPlaceSpread), 0x3F2AAAABu);
	EXPECT_EQ(std::bit_cast<uint32_t>(rules::k_ScaffoldPlaceStep), 0x3F490FDBu);
	EXPECT_EQ(std::bit_cast<uint32_t>(rules::k_ScaffoldReachMore), 0x42480000u);
}

TEST(ComputerMindScaffolds, TheGodJoinsScaffoldsWhenItsTownsWantCivicBuildingsMore)
{
	EXPECT_TRUE(rules::WantsToCombine(0.2f, 0.3f));
	EXPECT_FALSE(rules::WantsToCombine(0.3f, 0.3f));
	EXPECT_FALSE(rules::WantsToCombine(0.4f, 0.3f));
}

TEST(ComputerMindScaffolds, TheValueOfAScaffoldGoesWithItsKindOfBuilding)
{
	const rules::ScaffoldTownFacts town {.wantsAbodes = 0.5f, .wantsCivic = 0.8f, .adultPlaces = 100};
	// For a house: three quarters of the want of houses and a quarter more, by the nearness
	EXPECT_FLOAT_EQ(rules::ScaffoldUseValue(1, town, 0.5f), ((0.5f * 0.75f) + 0.25f) * 0.5f);
	EXPECT_FLOAT_EQ(rules::ScaffoldUseValue(2, town, 1.0f), (0.5f * 0.75f) + 0.25f);
	// A town with more than 175 adult places wants no house
	EXPECT_EQ(rules::ScaffoldUseValue(2, {.wantsAbodes = 0.5f, .adultPlaces = 176}, 1.0f), 0.0f);
	EXPECT_GT(rules::ScaffoldUseValue(2, {.wantsAbodes = 0.5f, .adultPlaces = 175}, 1.0f), 0.0f);
	// For a civic building: the same with the want of civic buildings
	for (const uint32_t value : {3u, 4u, 5u})
	{
		EXPECT_FLOAT_EQ(rules::ScaffoldUseValue(value, town, 0.5f), ((0.8f * 0.75f) + 0.25f) * 0.5f);
	}
	// A scaffold of 7 is twice as good near the hand, at most 1
	EXPECT_FLOAT_EQ(rules::ScaffoldUseValue(7, town, 0.3f), 0.6f);
	EXPECT_EQ(rules::ScaffoldUseValue(7, town, 0.5f), 1.0f);
	EXPECT_EQ(rules::ScaffoldUseValue(7, town, 0.9f), 1.0f);
	// Any other worth is the nearness
	for (const uint32_t value : {0u, 6u, 8u})
	{
		EXPECT_EQ(rules::ScaffoldUseValue(value, town, 0.4f), 0.4f);
	}
}

TEST(ComputerMindScaffolds, ATownWantsTheBuildingOfAScaffoldByItsWorth)
{
	const rules::ScaffoldTownFacts town {.wantsAbodes = 0.5f, .wantsCivic = 0.8f, .adultPlaces = 100};
	EXPECT_EQ(rules::ScaffoldTownDesire(0, town), 0.5f);
	EXPECT_EQ(rules::ScaffoldTownDesire(2, town), 0.5f);
	EXPECT_EQ(rules::ScaffoldTownDesire(2, {.wantsAbodes = 0.5f, .adultPlaces = 200}), 0.0f);
	EXPECT_EQ(rules::ScaffoldTownDesire(3, town), 0.8f);
	EXPECT_EQ(rules::ScaffoldTownDesire(4, town), 0.8f);
	EXPECT_EQ(rules::ScaffoldTownDesire(5, town), 1.0f);
	EXPECT_EQ(rules::ScaffoldTownDesire(7, town), 1.0f);
}

TEST(ComputerMindScaffolds, TheGodPutsAScaffoldBetweenHalfARadiusAndMore)
{
	EXPECT_EQ(rules::ScaffoldPlaceDistance(0.0f, 60.0f), 30.0f);
	EXPECT_EQ(rules::ScaffoldPlaceDistance(12.5f, 60.0f), 42.5f);
}

TEST(ComputerMindScaffolds, ACellIsTooSteepWhenItsCornersRiseMoreThanTen)
{
	EXPECT_FALSE(rules::TooSteepForScaffold({10, 20, 15, 12}));
	EXPECT_TRUE(rules::TooSteepForScaffold({10, 21, 15, 12}));
	EXPECT_TRUE(rules::TooSteepForScaffold({255, 0, 0, 0}));
	EXPECT_FALSE(rules::TooSteepForScaffold({0, 0, 0, 0}));
}

TEST(ComputerMindScaffolds, AMapValueKeepsItsCellOnTheMap)
{
	EXPECT_EQ(rules::KeepCellOnMap(0x00128000, 512), 0x00128000);
	// A cell below 0 becomes cell 0, and the part of the cell stays
	EXPECT_EQ(rules::KeepCellOnMap(static_cast<int32_t>(0xFFFF8000u), 512), 0x00008000);
	EXPECT_EQ(rules::KeepCellOnMap(0x02004000, 512), 0x01FF4000);
	EXPECT_EQ(rules::KeepCellOnMap(0x01FFC000, 512), 0x01FFC000);
}

TEST(ComputerMindScaffolds, AWorkshopWantsWoodByItsRoom)
{
	EXPECT_EQ(rules::WorkshopFillValue(3, 3.0f), 1.0f);
	EXPECT_FLOAT_EQ(rules::WorkshopFillValue(1, 3.0f), 1.0f / 3.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(rules::WorkshopFillValue(2, 3.0f)), std::bit_cast<uint32_t>(2.0f / 3.0f));
}
