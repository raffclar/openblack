/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/HandWaterGlow.h"

namespace hand_water_glow = openblack::graphics::hand_water_glow;

TEST(HandWaterGlow, WarmsThePaletteColourTowardsOrange)
{
	// A quarter of the way from black to (255, 128, 64), rounding down; full strength is 190 of alpha
	EXPECT_EQ(hand_water_glow::Colour(0x000000, 1.0f), 0xBE3F2010u);
	EXPECT_EQ(hand_water_glow::Colour(0xFFFFFF, 0.5f), 0x5FFFDFCFu);
	EXPECT_EQ(hand_water_glow::Colour(0x123456, 0.0f), 0x004D4750u);
}

TEST(HandWaterGlow, NeverShowsInsideTheTemple)
{
	// However strong the hand's light, the temple has no sea for the glow to lie on
	EXPECT_FALSE(hand_water_glow::Shows(true, 1.0f));
	EXPECT_FALSE(hand_water_glow::Shows(true, 0.5f));
	// Outside, it shows once the hand's light is strong enough
	EXPECT_TRUE(hand_water_glow::Shows(false, 1.0f));
	EXPECT_FALSE(hand_water_glow::Shows(false, hand_water_glow::k_MinimumStrength));
}

TEST(HandWaterGlow, ShowsOnlyNearWater)
{
	// High land everywhere
	const auto high = [](int /*x*/, int /*z*/) { return std::optional<uint8_t>(40); };
	EXPECT_FALSE(hand_water_glow::NearLowLand({2560.0f, 2560.0f}, high));

	// One low cell within reach, 70 units from the hand
	const auto lowCell = [](int x, int z) { return std::optional<uint8_t>(x == 263 && z == 256 ? 4 : 40); };
	EXPECT_TRUE(hand_water_glow::NearLowLand({2560.0f, 2560.0f}, lowCell));
	EXPECT_FALSE(hand_water_glow::NearLowLand({2400.0f, 2560.0f}, lowCell));

	// Where there is no land at all
	const auto noLand = [](int x, int /*z*/) { return x < 300 ? std::optional<uint8_t>(40) : std::nullopt; };
	EXPECT_TRUE(hand_water_glow::NearLowLand({2950.0f, 2560.0f}, noLand));
}

TEST(HandWaterGlow, GivesTheFirstLowCellAlongXThenZ)
{
	// Two low cells within reach: the scan goes along x in each row of z, so the cell of the lower z comes first
	const auto twoCells = [](int x, int z) {
		return std::optional<uint8_t>((x == 260 && z == 258) || (x == 254 && z == 257) ? 2 : 40);
	};
	const auto first = hand_water_glow::FirstLowCell({2560.0f, 2560.0f}, twoCells);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(first->x, 254);
	EXPECT_EQ(first->y, 257);
	const auto high = [](int /*x*/, int /*z*/) { return std::optional<uint8_t>(40); };
	EXPECT_FALSE(hand_water_glow::FirstLowCell({2560.0f, 2560.0f}, high).has_value());
}

TEST(HandWaterGlow, MeasuresTheLandByALakeFromTheLakeLevel)
{
	// A lake at 120.75 units: the first whole unit over it (121) counts as altitude 4, the first altitude over the sea
	EXPECT_EQ(hand_water_glow::AltitudeOverLake(121, 120.75f), 4);
	// The land is low under altitude 5: up to one unit over the first whole unit over the level
	EXPECT_LT(hand_water_glow::AltitudeOverLake(121, 120.75f), hand_water_glow::k_LowAltitude);
	EXPECT_EQ(hand_water_glow::AltitudeOverLake(122, 120.75f), hand_water_glow::k_LowAltitude);
	// The bed of the lake is low, and the altitude does not go under 0 or over 255
	EXPECT_EQ(hand_water_glow::AltitudeOverLake(100, 120.75f), 0);
	EXPECT_EQ(hand_water_glow::AltitudeOverLake(2000, 120.75f), 255);
	// A level a quarter under altitude 4, as the sea's coast has it, gives the altitude of the land itself
	EXPECT_EQ(hand_water_glow::AltitudeOverLake(4, 3.75f), 4);
	EXPECT_EQ(hand_water_glow::AltitudeOverLake(9, 3.75f), 9);
}
