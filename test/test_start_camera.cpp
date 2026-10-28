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
using openblack::start_camera::LandStart;
using openblack::start_camera::StartsAtTemple;

TEST(StartCamera, ANewGameWithNoPlaceFromTheScriptStartsAtTheTemple)
{
	EXPECT_TRUE(StartsAtTemple({.change = Kind::NewGame, .networkGame = false, .scriptPlace = std::nullopt}));
	EXPECT_TRUE(StartsAtTemple({.change = Kind::Skirmish, .networkGame = false, .scriptPlace = std::nullopt}));
}

TEST(StartCamera, APlaceAtTheOriginOfTheMapStartsAtTheTemple)
{
	EXPECT_TRUE(StartsAtTemple({.change = Kind::Skirmish, .networkGame = false, .scriptPlace = glm::ivec2(0, 0)}));
}

TEST(StartCamera, APlaceAwayFromTheOriginKeepsTheCameraThere)
{
	EXPECT_FALSE(StartsAtTemple({.change = Kind::NewGame, .networkGame = false, .scriptPlace = glm::ivec2(1, 0)}));
	EXPECT_FALSE(StartsAtTemple({.change = Kind::Skirmish, .networkGame = false, .scriptPlace = glm::ivec2(0, -1)}));
	EXPECT_FALSE(StartsAtTemple({.change = Kind::NewGame, .networkGame = false, .scriptPlace = glm::ivec2(1432, 1517)}));
}

TEST(StartCamera, ANetworkGameAlwaysStartsAtTheTemple)
{
	EXPECT_TRUE(StartsAtTemple({.change = Kind::NewGame, .networkGame = true, .scriptPlace = glm::ivec2(1432, 1517)}));
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

TEST(StartCamera, TheCameraLooksAtThePointFromTheStartDistance)
{
	const glm::vec3 focus {100.0f, 20.0f, 300.0f};
	const auto origin = openblack::start_camera::OriginOver(focus);
	EXPECT_NEAR(glm::length(origin - focus), openblack::start_camera::k_OriginDistance, 1e-3f);
}
