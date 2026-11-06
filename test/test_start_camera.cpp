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

#include "Camera/StartCamera.h"

using openblack::land_change::Kind;
using openblack::map_coords::MapCoords;
using openblack::start_camera::LandStart;
using openblack::start_camera::StartsAtTemple;

TEST(StartCamera, ANewGameWithNoPlaceFromTheScriptStartsAtTheTemple)
{
	EXPECT_TRUE(StartsAtTemple({.change = Kind::NewGame, .networkGame = false, .scriptPlace = std::nullopt}));
	EXPECT_TRUE(StartsAtTemple({.change = Kind::Skirmish, .networkGame = false, .scriptPlace = std::nullopt}));
}

TEST(StartCamera, APlaceAtTheOriginOfTheMapStartsAtTheTemple)
{
	EXPECT_TRUE(StartsAtTemple({.change = Kind::Skirmish, .networkGame = false, .scriptPlace = MapCoords {.x = 0, .z = 0}}));
}

TEST(StartCamera, APlaceAtTheOriginOfTheMapWithAHeightKeepsTheCameraThere)
{
	EXPECT_FALSE(StartsAtTemple({.change = Kind::NewGame, .networkGame = false, .scriptPlace = MapCoords {.altitude = 5.0f}}));
}

TEST(StartCamera, APlaceAwayFromTheOriginKeepsTheCameraThere)
{
	EXPECT_FALSE(StartsAtTemple({.change = Kind::NewGame, .networkGame = false, .scriptPlace = MapCoords {.x = 1, .z = 0}}));
	EXPECT_FALSE(StartsAtTemple({.change = Kind::Skirmish, .networkGame = false, .scriptPlace = MapCoords {.x = 0, .z = -1}}));
	EXPECT_FALSE(
	    StartsAtTemple({.change = Kind::NewGame, .networkGame = false, .scriptPlace = MapCoords {.x = 1432, .z = 1517}}));
}

TEST(StartCamera, ANetworkGameAlwaysStartsAtTheTemple)
{
	EXPECT_TRUE(
	    StartsAtTemple({.change = Kind::NewGame, .networkGame = true, .scriptPlace = MapCoords {.x = 1432, .z = 1517}}));
	EXPECT_TRUE(StartsAtTemple({.change = Kind::NewGame, .networkGame = true, .scriptPlace = std::nullopt}));
}

TEST(StartCamera, OnlyAGameThatStartsOnTheLandMovesTheCamera)
{
	for (const auto kind : {Kind::Story, Kind::FreshScripts, Kind::Testbed, Kind::SavedGame})
	{
		EXPECT_FALSE(StartsAtTemple({.change = kind, .networkGame = false, .scriptPlace = std::nullopt}));
		EXPECT_FALSE(StartsAtTemple({.change = kind, .networkGame = true, .scriptPlace = std::nullopt}));
	}
}

TEST(StartCamera, TheCameraHasTheHeadingAndPitchOfTheGame)
{
	// The heading is about 157.5 degrees and the pitch is about 12.9 degrees, 120 from the point
	const auto origin = openblack::start_camera::OriginOver(glm::vec3(0.0f));
	EXPECT_NEAR(origin.x, 44.75914f, 1e-4f);
	EXPECT_NEAR(origin.y, 26.702513f, 1e-4f);
	EXPECT_NEAR(origin.z, -108.090675f, 1e-4f);
}

TEST(StartCamera, TheCameraLooksAtThePointFromTheStartDistance)
{
	const glm::vec3 focus {100.0f, 20.0f, 300.0f};
	const auto origin = openblack::start_camera::OriginOver(focus);
	EXPECT_NEAR(glm::length(origin - focus), openblack::start_camera::k_OriginDistance, 1e-3f);
}

TEST(StartCamera, TheScriptPlaceLooksAtTheLandUnderIt)
{
	const auto focus = openblack::start_camera::FocusOnLand({3237.21f, 3157.85f}, 42.5f, 0.0f);
	EXPECT_EQ(focus, glm::vec3(3237.21f, 42.5f, 3157.85f));
}

TEST(StartCamera, TheScriptPlaceWithAHeightLooksAtThePointAboveTheLand)
{
	const auto focus = openblack::start_camera::FocusOnLand({3237.21f, 3157.85f}, 42.5f, 7.25f);
	EXPECT_EQ(focus, glm::vec3(3237.21f, 49.75f, 3157.85f));
}
