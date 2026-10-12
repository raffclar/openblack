/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>

#include <gtest/gtest.h>

#include "Level.h"

using openblack::Level;

// A land started from the command line names its script the way the player typed it, while the level list holds the
// path the file system found: once both are resolved, the same file must match whatever its spelling.

TEST(LevelScriptMatch, TheSameScriptMatchesWhateverItsLetterCase)
{
	EXPECT_TRUE(
	    Level::IsSameScript("/games/black/Scripts/Playgrounds/TwoGods.txt", "/GAMES/Black/scripts/playgrounds/twogods.txt"));
}

TEST(LevelScriptMatch, DotStepsDontCount)
{
	EXPECT_TRUE(Level::IsSameScript("/games/black/Scripts/Playgrounds/TwoGods.txt",
	                                "/games/black/Scripts/./Quests/../Playgrounds/TwoGods.txt"));
}

TEST(LevelScriptMatch, AnotherPlaygroundDoesntMatch)
{
	EXPECT_FALSE(
	    Level::IsSameScript("/games/black/Scripts/Playgrounds/TwoGods.txt", "/games/black/Scripts/Playgrounds/ThreeGods.txt"));
}

TEST(LevelScriptMatch, AStoryLandDoesntMatchAPlaygroundOfTheSameName)
{
	EXPECT_FALSE(Level::IsSameScript("/games/black/Scripts/Land2.txt", "/games/black/Scripts/Playgrounds/Land2.txt"));
}
