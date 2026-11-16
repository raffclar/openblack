/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Resources/MissingGameData.h"

using openblack::resources::IsFileNotInGameData;
using openblack::resources::NormaliseGameDataPath;

TEST(MissingGameData, ThePathsOfTheTablesGetOneForm)
{
	EXPECT_EQ(NormaliseGameDataPath("scripts\\Dance\\DancePrayInwards"), "scripts/dance/danceprayinwards");
	EXPECT_EQ(NormaliseGameDataPath("Audio/Music//Script/A.sad"), "audio/music/script/a.sad");
}

TEST(MissingGameData, TheWelcomeDanceMusicIsNotInTheGameData)
{
	EXPECT_TRUE(IsFileNotInGameData("audio/music/script/FollowUsWelcome.sad"));
}

TEST(MissingGameData, TheDanceOfTheDanceTableIsNotInTheGameData)
{
	// The dance table writes two backslashes between the folders
	EXPECT_TRUE(IsFileNotInGameData("scripts\\Dance\\DancePrayInwards"));
}

TEST(MissingGameData, TheOtherFilesStayExpected)
{
	EXPECT_FALSE(IsFileNotInGameData("audio/music/script/Epic03.sad"));
	EXPECT_FALSE(IsFileNotInGameData("scripts\\Dance\\DanceAroundPerson"));
	EXPECT_FALSE(IsFileNotInGameData("scripts\\Dance\\DancePrayingInwards.dat"));
	EXPECT_FALSE(IsFileNotInGameData("audio/music/script/FollowUsWelcome.sad.bak"));
}
