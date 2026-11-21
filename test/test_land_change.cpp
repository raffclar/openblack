/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/ProfileAlignment.h"
#include "LandChange.h"

using namespace openblack::land_change;

TEST(LandChange, TheStoryKeepsEverythingThatIsNotTheLands)
{
	constexpr auto k_Rules = RulesFor(Kind::Story);
	EXPECT_EQ(k_Rules.advisors, Advisors::CarryOn);
	EXPECT_TRUE(k_Rules.scriptScreenKept);
	EXPECT_TRUE(k_Rules.heartbeatsCarried);
	EXPECT_TRUE(k_Rules.playersCarried);
	EXPECT_FALSE(k_Rules.alignmentFromProfile);
	EXPECT_TRUE(k_Rules.helpTextsKept);
	EXPECT_TRUE(k_Rules.gameClockRunsOn);
}

TEST(LandChange, TheLandMenuStartsTheScriptsAgainButKeepsThePlayers)
{
	constexpr auto k_Rules = RulesFor(Kind::FreshScripts);
	EXPECT_EQ(k_Rules.advisors, Advisors::SentHome);
	EXPECT_FALSE(k_Rules.scriptScreenKept);
	EXPECT_TRUE(k_Rules.heartbeatsCarried);
	EXPECT_TRUE(k_Rules.playersCarried);
	EXPECT_FALSE(k_Rules.alignmentFromProfile);
	EXPECT_FALSE(k_Rules.helpTextsKept);
	// The land menu's challenges start as a new game does, the clock with them
	EXPECT_FALSE(k_Rules.gameClockRunsOn);
}

TEST(LandChange, ANewGameAndTheTestbedStartEverythingAfresh)
{
	for (const auto kind : {Kind::NewGame, Kind::Skirmish, Kind::Testbed})
	{
		const auto rules = RulesFor(kind);
		EXPECT_EQ(rules.advisors, Advisors::Afresh);
		EXPECT_FALSE(rules.scriptScreenKept);
		EXPECT_FALSE(rules.heartbeatsCarried);
		EXPECT_FALSE(rules.playersCarried);
		EXPECT_FALSE(rules.helpTextsKept);
		EXPECT_FALSE(rules.gameClockRunsOn);
	}
}

TEST(LandChange, ANewGameOrALoadStartsThePlayerAtTheirProfilesAlignment)
{
	EXPECT_TRUE(RulesFor(Kind::NewGame).alignmentFromProfile);
	EXPECT_TRUE(RulesFor(Kind::Skirmish).alignmentFromProfile);
	// Each load starts a game, which takes the alignment of the profile, not the alignment of the save
	EXPECT_TRUE(RulesFor(Kind::SavedGame).alignmentFromProfile);
	EXPECT_FALSE(RulesFor(Kind::Story).alignmentFromProfile);
	EXPECT_FALSE(RulesFor(Kind::Testbed).alignmentFromProfile);
}

TEST(LandChange, ANewGameOrALoadIsPlayedAsTheProfilePickedNow)
{
	EXPECT_TRUE(RulesFor(Kind::NewGame).gameStarts);
	EXPECT_TRUE(RulesFor(Kind::Skirmish).gameStarts);
	EXPECT_TRUE(RulesFor(Kind::SavedGame).gameStarts);
	EXPECT_FALSE(RulesFor(Kind::SavedGame).playersCarried);
	EXPECT_FALSE(RulesFor(Kind::Story).gameStarts);
	EXPECT_FALSE(RulesFor(Kind::FreshScripts).gameStarts);
	EXPECT_FALSE(RulesFor(Kind::Testbed).gameStarts);
}

TEST(LandChange, APlaygroundLandFromTheLandMenuStartsASkirmish)
{
	EXPECT_EQ(FromLandMenu(true), Kind::Skirmish);
	EXPECT_EQ(FromLandMenu(false), Kind::FreshScripts);
}

TEST(ProfileAlignment, ANewGameStartsThePlayerAtTheKeptAlignmentNoFurtherThanGoodOrEvil)
{
	using namespace openblack::profile_alignment;
	EXPECT_FLOAT_EQ(Starting(0.0), 0.0f);
	EXPECT_FLOAT_EQ(Starting(-0.25), -0.25f);
	EXPECT_FLOAT_EQ(Starting(1.5), 1.0f);
	EXPECT_FLOAT_EQ(Starting(-3.0), -1.0f);
}

// Only a game for one player uses the alignment of the profile of this computer. A network game neither starts a god
// from it nor changes it, so that the game server and each player start the same world.
TEST(ProfileAlignment, ANetworkGameDoesNotUseTheProfileOfThisComputer)
{
	using namespace openblack::profile_alignment;
	EXPECT_TRUE(StartsFromOwnProfile(false));
	EXPECT_FALSE(StartsFromOwnProfile(true));
	EXPECT_TRUE(KeptInOwnProfile(false));
	EXPECT_FALSE(KeptInOwnProfile(true));
}

// A player who leaves a network game for the skirmish box stays on the network land under the box. That land never
// changes the profile, because the god of this computer on it can be the god of another human. The next land of this
// computer alone changes the profile again.
TEST(ProfileAlignment, ALandOfANetworkGameNeverChangesTheProfileAfterThePlayerLeaves)
{
	using namespace openblack::profile_alignment;
	NetworkLand land;
	// A single player land
	land.StartLand(false);
	land.See(false);
	EXPECT_TRUE(KeptInOwnProfile(land.Is()));
	// A land of a network game
	land.StartLand(true);
	land.See(true);
	EXPECT_FALSE(KeptInOwnProfile(land.Is()));
	// The player leaves the network game, and the network land stays under the skirmish box
	land.See(false);
	EXPECT_FALSE(KeptInOwnProfile(land.Is()));
	land.See(false);
	EXPECT_FALSE(KeptInOwnProfile(land.Is()));
	// The skirmish land starts after the network game ended
	land.StartLand(false);
	land.See(false);
	EXPECT_TRUE(KeptInOwnProfile(land.Is()));
}

// A land that becomes the land of a network game after it started, as a world state that a player who joins late
// loads, is a land of a network game from that frame.
TEST(ProfileAlignment, ALandBecomesALandOfANetworkGameWhenTheGameBecomesOne)
{
	using namespace openblack::profile_alignment;
	NetworkLand land;
	land.StartLand(false);
	EXPECT_TRUE(KeptInOwnProfile(land.Is()));
	land.See(true);
	EXPECT_FALSE(KeptInOwnProfile(land.Is()));
	land.See(false);
	EXPECT_FALSE(KeptInOwnProfile(land.Is()));
}

TEST(ProfileAlignment, TheAlignmentIsKeptEverySixHundredTurnsAndAtOnceOnANewLand)
{
	using namespace openblack::profile_alignment;
	// A new game's first turns
	EXPECT_FALSE(DueToKeep(0, 0));
	EXPECT_FALSE(DueToKeep(599, 0));
	EXPECT_TRUE(DueToKeep(600, 0));
	EXPECT_FALSE(DueToKeep(1199, 600));
	EXPECT_TRUE(DueToKeep(1200, 600));
	// The turns start again from nothing on the next land
	EXPECT_TRUE(DueToKeep(0, 1200));
}

TEST(LandChange, OnlyASkirmishBringsTheCreatureOfTheProfile)
{
	// The god of the player brings the creature that the profile keeps in the first turn of a skirmish only
	EXPECT_TRUE(RulesFor(Kind::Skirmish).profileCreatureBrought);
	for (const auto kind : {Kind::Story, Kind::FreshScripts, Kind::NewGame, Kind::Testbed, Kind::SavedGame})
	{
		EXPECT_FALSE(RulesFor(kind).profileCreatureBrought) << static_cast<int>(kind);
	}
}

TEST(LandChange, ALoadedSkirmishTriesToBringTheCreatureOfTheProfileAgain)
{
	// A saved skirmish loads as a skirmish, and the god of the player tries to bring the creature from the profile again.
	// The god brings no second creature when the save kept its creature (test_creature_file_load).
	EXPECT_TRUE(LoadBringsProfileCreature(true, false));
	// A saved game that is not a skirmish brings no creature, and a network game saves no game
	EXPECT_FALSE(LoadBringsProfileCreature(false, false));
	EXPECT_FALSE(LoadBringsProfileCreature(true, true));
	EXPECT_FALSE(LoadBringsProfileCreature(false, true));
}

// The load of a saved game gives the player the alignment of the profile, as the game does. A network game does not use
// the profile of one computer, so that each computer loads the same world.
TEST(LandChange, OnlyALoadOutOfANetworkGameTakesTheAlignmentOfTheProfile)
{
	EXPECT_TRUE(LoadTakesProfileAlignment(false));
	EXPECT_FALSE(LoadTakesProfileAlignment(true));
}

TEST(LandChange, OnlyTheOpenBlockOfTheCommandsStaysForTheNewLand)
{
	// The story continues in the same session, so the open block goes after the turns of the frame
	EXPECT_EQ(RulesFor(Kind::Story).commands, Commands::OpenBlockStays);
	// The land menu, a new game and a skirmish start a new session, which sends the open block at once
	EXPECT_EQ(RulesFor(Kind::FreshScripts).commands, Commands::OpenBlockSent);
	EXPECT_EQ(RulesFor(Kind::NewGame).commands, Commands::OpenBlockSent);
	EXPECT_EQ(RulesFor(Kind::Skirmish).commands, Commands::OpenBlockSent);
	// The load of a saved game keeps its commands in its own way. Nothing comes onto the testbed.
	EXPECT_EQ(RulesFor(Kind::SavedGame).commands, Commands::Removed);
	EXPECT_EQ(RulesFor(Kind::Testbed).commands, Commands::Removed);
}
