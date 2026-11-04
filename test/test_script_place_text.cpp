/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <gtest/gtest.h>

#include "LHScriptX/ScriptPlaceText.h"

using openblack::lhscriptx::ReadScriptPlace;
using openblack::map_coords::MapCoords;

TEST(ScriptPlaceText, APlaceWithTwoNumbersIsOnTheLand)
{
	const auto place = ReadScriptPlace("10.5,20.25");
	ASSERT_TRUE(place.has_value());
	EXPECT_EQ(place->metres, glm::vec2(10.5f, 20.25f));
	EXPECT_EQ(place->onMap, openblack::map_coords::ScriptPlace(10.5, 20.25));
	EXPECT_EQ(place->onMap.altitude, 0.0f);
}

TEST(ScriptPlaceText, TheThirdNumberIsTheHeightAboveTheLand)
{
	const auto place = ReadScriptPlace("1280.04,1280.04,7.5");
	ASSERT_TRUE(place.has_value());
	EXPECT_EQ(place->metres, glm::vec2(1280.04f, 1280.04f));
	EXPECT_EQ(place->onMap, (MapCoords {.x = 8388870, .z = 8388870, .altitude = 7.5f}));
}

TEST(ScriptPlaceText, TheHeightIsReadAsADoubleAndRoundedToAFloat)
{
	const auto place = ReadScriptPlace("1,2,0.1");
	ASSERT_TRUE(place.has_value());
	EXPECT_EQ(place->onMap.altitude, static_cast<float>(0.1));
	const auto below = ReadScriptPlace("1,2,-3.25");
	ASSERT_TRUE(below.has_value());
	EXPECT_EQ(below->onMap.altitude, -3.25f);
}

TEST(ScriptPlaceText, AnEmptyNumberIsZero)
{
	const auto noHeight = ReadScriptPlace("1,2,");
	ASSERT_TRUE(noHeight.has_value());
	EXPECT_EQ(noHeight->onMap, openblack::map_coords::ScriptPlace(1.0, 2.0));
	const auto noZ = ReadScriptPlace("1,");
	ASSERT_TRUE(noZ.has_value());
	EXPECT_EQ(noZ->metres, glm::vec2(1.0f, 0.0f));
}

TEST(ScriptPlaceText, ATextInADifferentFormIsNotAPlace)
{
	EXPECT_FALSE(ReadScriptPlace("CELTIC").has_value());
	EXPECT_FALSE(ReadScriptPlace("1").has_value());
	EXPECT_FALSE(ReadScriptPlace("1,2x").has_value());
	EXPECT_FALSE(ReadScriptPlace("1,2,3x").has_value());
	EXPECT_FALSE(ReadScriptPlace("1,2,3,4").has_value());
}
