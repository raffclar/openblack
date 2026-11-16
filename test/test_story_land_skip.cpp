/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Story/StoryLandSkip.h"

using namespace openblack::story_land_skip;

TEST(StoryLandSkip, TheCheatOffersTheFiveLandsOfTheStory)
{
	EXPECT_FALSE(Offers(0));
	for (int32_t land = 1; land <= 5; ++land)
	{
		EXPECT_TRUE(Offers(land)) << land;
	}
	EXPECT_FALSE(Offers(6));
	EXPECT_FALSE(Offers(-1));
}

TEST(StoryLandSkip, TheFirstLandStartsANewGameAndTheLaterLandsStartTheTopScriptAgain)
{
	EXPECT_EQ(PathTo(1), Path::NewGame);
	EXPECT_FALSE(GoesAsCommand(1));
	for (int32_t land = 2; land <= 5; ++land)
	{
		EXPECT_EQ(PathTo(land), Path::TopScript) << land;
		EXPECT_TRUE(GoesAsCommand(land)) << land;
	}
	EXPECT_FALSE(GoesAsCommand(0));
	EXPECT_FALSE(GoesAsCommand(6));
}

TEST(StoryLandSkip, TheChangeOfLandComesAfterTheControlScriptOfTheLandBefore)
{
	EXPECT_EQ(ControlScriptBefore(1), std::nullopt);
	EXPECT_EQ(ControlScriptBefore(2), "LandControl1");
	EXPECT_EQ(ControlScriptBefore(3), "LandControl2");
	EXPECT_EQ(ControlScriptBefore(5), "LandControl4");
	EXPECT_EQ(ControlScriptBefore(6), std::nullopt);
	// The top script starts again at the instruction after the run
	EXPECT_EQ(StartAfterRun(155485), 155486u);
}

TEST(StoryLandSkip, OnlyAGameOfTheStoryOnThisComputerAloneShowsTheCheat)
{
	EXPECT_TRUE(Shown({.networkGame = false, .skirmish = false, .landNumber = 1}));
	EXPECT_TRUE(Shown({.networkGame = false, .skirmish = false, .landNumber = 5}));
	// A network game has no lands of the story
	EXPECT_FALSE(Shown({.networkGame = true, .skirmish = false, .landNumber = 2}));
	// Nor a skirmish on a land of the Gods' Playground
	EXPECT_FALSE(Shown({.networkGame = false, .skirmish = true, .landNumber = 1}));
	// Nor a land that is not of the story, such as the testbed
	EXPECT_FALSE(Shown({.networkGame = false, .skirmish = false, .landNumber = 0}));
	EXPECT_FALSE(Shown({.networkGame = false, .skirmish = false, .landNumber = 6}));
}

TEST(StoryLandSkip, TheWorldDoesTheSkipOfACommandOnlyInAGameOfTheStory)
{
	const GameFacts story {.networkGame = false, .skirmish = false, .landNumber = 1};
	EXPECT_TRUE(WorldDoes(story, 3));
	EXPECT_TRUE(WorldDoes(story, 5));
	// The first land does not go as a command
	EXPECT_FALSE(WorldDoes(story, 1));
	EXPECT_FALSE(WorldDoes(story, 6));
	EXPECT_FALSE(WorldDoes({.networkGame = true, .skirmish = false, .landNumber = 1}, 3));
	EXPECT_FALSE(WorldDoes({.networkGame = false, .skirmish = true, .landNumber = 1}, 3));
}
