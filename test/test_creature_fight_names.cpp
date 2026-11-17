/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <set>
#include <string_view>

#include <gtest/gtest.h>

#include "Debug/CreatureFightNames.h"

using namespace openblack::debug::creature_fight_names;
using openblack::ecs::components::CreatureFighting;
using openblack::ecs::components::CreatureKnockedOut;

TEST(CreatureFightNames, EachStageOfAKnockOutHasItsOwnName)
{
	// The window showed the creature that gets up with a list of names that was one name short, which stopped the game
	using Stage = CreatureKnockedOut::Stage;
	std::set<std::string_view> names;
	for (const auto stage : {Stage::Lying, Stage::FadingOut, Stage::FadingIn, Stage::Waiting, Stage::WaitingForSpells,
	                         Stage::Resting, Stage::GettingUp})
	{
		const auto name = KnockedOutName(stage);
		EXPECT_NE(name, "unknown");
		names.insert(name);
	}
	EXPECT_EQ(names.size(), 7u);
	EXPECT_EQ(KnockedOutName(Stage::Resting), "resting");
	EXPECT_EQ(KnockedOutName(Stage::GettingUp), "getting up");
}

TEST(CreatureFightNames, EachStageOfAFightHasItsOwnName)
{
	using Stage = CreatureFighting::Stage;
	std::set<std::string_view> names;
	for (const auto stage : {Stage::FaceOff, Stage::Taunt, Stage::Point, Stage::Approach, Stage::Ready, Stage::Square,
	                         Stage::Duel, Stage::Celebrate, Stage::Respond, Stage::Stop})
	{
		const auto name = StageName(stage);
		EXPECT_NE(name, "unknown");
		names.insert(name);
	}
	EXPECT_EQ(names.size(), 10u);
}

TEST(CreatureFightNames, AValueOutsideTheStagesGivesANameAndDoesNotStopTheGame)
{
	EXPECT_EQ(KnockedOutName(static_cast<CreatureKnockedOut::Stage>(200)), "unknown");
	EXPECT_EQ(StageName(static_cast<CreatureFighting::Stage>(200)), "unknown");
}
