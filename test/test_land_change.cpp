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
	constexpr auto rules = RulesFor(Kind::Story);
	EXPECT_EQ(rules.advisors, Advisors::CarryOn);
	EXPECT_TRUE(rules.scriptScreenKept);
	EXPECT_TRUE(rules.heartbeatsCarried);
	EXPECT_TRUE(rules.playersCarried);
	EXPECT_FALSE(rules.alignmentFromProfile);
}

TEST(LandChange, TheLandMenuStartsTheScriptsAgainButKeepsThePlayers)
{
	constexpr auto rules = RulesFor(Kind::FreshScripts);
	EXPECT_EQ(rules.advisors, Advisors::SentHome);
	EXPECT_FALSE(rules.scriptScreenKept);
	EXPECT_TRUE(rules.heartbeatsCarried);
	EXPECT_TRUE(rules.playersCarried);
	EXPECT_FALSE(rules.alignmentFromProfile);
}

TEST(LandChange, ANewGameAndTheTestbedStartEverythingAfresh)
{
	for (const auto kind : {Kind::NewGame, Kind::Testbed})
	{
		const auto rules = RulesFor(kind);
		EXPECT_EQ(rules.advisors, Advisors::Afresh);
		EXPECT_FALSE(rules.scriptScreenKept);
		EXPECT_FALSE(rules.heartbeatsCarried);
		EXPECT_FALSE(rules.playersCarried);
	}
}

TEST(LandChange, OnlyANewGameStartsThePlayerAtTheirProfilesAlignment)
{
	EXPECT_TRUE(RulesFor(Kind::NewGame).alignmentFromProfile);
	EXPECT_FALSE(RulesFor(Kind::Testbed).alignmentFromProfile);
}

TEST(ProfileAlignment, ANewGameStartsThePlayerAtTheKeptAlignmentNoFurtherThanGoodOrEvil)
{
	using namespace openblack::profile_alignment;
	EXPECT_FLOAT_EQ(Starting(0.0), 0.0f);
	EXPECT_FLOAT_EQ(Starting(-0.25), -0.25f);
	EXPECT_FLOAT_EQ(Starting(1.5), 1.0f);
	EXPECT_FLOAT_EQ(Starting(-3.0), -1.0f);
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
