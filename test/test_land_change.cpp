/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "LandChange.h"

using namespace openblack::land_change;

TEST(LandChange, TheStoryKeepsEverythingThatIsNotTheLands)
{
	constexpr auto rules = RulesFor(Kind::Story);
	EXPECT_EQ(rules.advisors, Advisors::CarryOn);
	EXPECT_TRUE(rules.scriptScreenKept);
	EXPECT_TRUE(rules.heartbeatsCarried);
}

TEST(LandChange, TheLandMenuStartsTheScriptsAgainButKeepsThePlayers)
{
	constexpr auto rules = RulesFor(Kind::FreshScripts);
	EXPECT_EQ(rules.advisors, Advisors::SentHome);
	EXPECT_FALSE(rules.scriptScreenKept);
	EXPECT_TRUE(rules.heartbeatsCarried);
}

TEST(LandChange, ANewGameAndTheTestbedStartEverythingAfresh)
{
	for (const auto kind : {Kind::NewGame, Kind::Testbed})
	{
		const auto rules = RulesFor(kind);
		EXPECT_EQ(rules.advisors, Advisors::Afresh);
		EXPECT_FALSE(rules.scriptScreenKept);
		EXPECT_FALSE(rules.heartbeatsCarried);
	}
}
