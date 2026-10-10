/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "3D/LandLightTable.h"

using namespace openblack;

namespace
{
/// A palette whose land rows are all one colour through the day, whatever the alignment
LandLightPalette FakePalette(uint8_t r, uint8_t g, uint8_t b)
{
	std::vector<uint8_t> bytes(LandLightPalette::k_Side * LandLightPalette::k_Side * 4, 0);
	for (size_t row = 0; row < 3; ++row)
	{
		for (size_t column = 0; column < LandLightPalette::k_Side; ++column)
		{
			const auto at = (row * LandLightPalette::k_Side + column) * 4;
			bytes[at] = r;
			bytes[at + 1] = g;
			bytes[at + 2] = b;
			bytes[at + 3] = 0xFF;
		}
	}
	return LandLightPalette(bytes);
}
} // namespace

TEST(LandLightTable, OvercastDarkensTheLand)
{
	const auto palette = FakePalette(200, 220, 240);
	EXPECT_EQ(LandLightTable::GetLandColour(palette, 2.0f, 1.0f, 0.0f), 0xC8DCF0u);
	// No channel brighter than 255 - 96 times the overcast
	EXPECT_EQ(LandLightTable::GetLandColour(palette, 2.0f, 1.0f, 0.5f), 0xC8CFCFu);
	EXPECT_EQ(LandLightTable::GetLandColour(palette, 2.0f, 1.0f, 1.0f), 0x9F9F9Fu);

	LandLightTable table;
	table.Build(palette, 2.0f, 1.0f, 0.5f);
	EXPECT_EQ(table.GetLandColour(), 0xC8CFCFu);
}

TEST(LandLightTable, OvercastClosesTheHazeIn)
{
	const auto palette = FakePalette(200, 220, 240);
	LandLightTable table;
	table.Build(palette, 2.0f, 1.0f, 0.0f);
	EXPECT_EQ(table.GetHaze().colour, glm::vec3(66.0f, 73.0f, 80.0f));
	EXPECT_FLOAT_EQ(table.GetHaze().k, 233.0f);
	EXPECT_NEAR(table.GetHaze().nearDistance, 400.0f, 0.01f);
	EXPECT_NEAR(table.GetHaze().farDistance, 900.0f, 0.01f);

	// Halfway to a storm's haze, from the darkened land's colour
	table.Build(palette, 2.0f, 1.0f, 0.5f);
	EXPECT_FLOAT_EQ(table.GetHaze().colour.r, 61.5f);
	EXPECT_FLOAT_EQ(table.GetHaze().k, 131.0f);

	// A full overcast, or more, is a storm's, though more darkens the land further: 139, an eighth and 32
	table.Build(palette, 2.0f, 1.0f, 1.2f);
	EXPECT_EQ(table.GetHaze().colour, glm::vec3(49.0f));
	EXPECT_FLOAT_EQ(table.GetHaze().k, 48.0f);
	EXPECT_NEAR(table.GetHaze().nearDistance, 15.0f, 0.01f);
	EXPECT_NEAR(table.GetHaze().farDistance, 350.0f, 0.01f);
}

TEST(LandLightTable, TheMoonsColourIsThePalettesMoonRowByAlignmentAlone)
{
	// Every other colour of the palette noise, the moon's row a ramp across its columns
	std::vector<uint8_t> bytes(LandLightPalette::k_Side * LandLightPalette::k_Side * 4);
	for (size_t i = 0; i < bytes.size(); ++i)
	{
		bytes[i] = static_cast<uint8_t>((i * 37u) + (i / 97u));
	}
	constexpr size_t k_MoonRow = 5;
	for (size_t column = 0; column < LandLightPalette::k_Side; ++column)
	{
		const auto at = (k_MoonRow * LandLightPalette::k_Side + column) * 4;
		bytes[at] = static_cast<uint8_t>(column * 8);
		bytes[at + 1] = static_cast<uint8_t>(255 - column * 8);
		bytes[at + 2] = static_cast<uint8_t>(column * 4);
		bytes[at + 3] = 0xFF;
	}
	const LandLightPalette palette(bytes);
	for (const float skyType : {0.0f, 0.6f, 1.0f, 2.0f})
	{
		// Good is the first column, neutral the fifteenth, evil the thirtieth
		EXPECT_EQ(LandLightTable::GetMoonColour(palette, skyType, 1.0f), 0x00FF00u) << skyType;
		EXPECT_EQ(LandLightTable::GetMoonColour(palette, skyType, 0.0f), 0x78873Cu) << skyType;
		EXPECT_EQ(LandLightTable::GetMoonColour(palette, skyType, -1.0f), 0xF00F78u) << skyType;
		// Halfway between the seventh and eighth columns
		EXPECT_EQ(LandLightTable::GetMoonColour(palette, skyType, 0.5f), 0x3CC31Eu) << skyType;
	}
}
