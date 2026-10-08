/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/ConsciencesModel.h"

using namespace openblack;
using namespace openblack::debug::consciences;

namespace
{
SetRow MakeRow(uint32_t set, std::string_view script)
{
	return {.set = set, .first = 10, .last = 12, .mode = 0, .category = 1, .script = script, .sent = 0};
}
} // namespace

TEST(ConsciencesMenu, controlStateNames)
{
	using help::spirits::ControlState;
	EXPECT_EQ(ControlStateName(ControlState::Home), "at home");
	EXPECT_EQ(ControlStateName(ControlState::GoingHome), "going home");
	EXPECT_EQ(ControlStateName(ControlState::Out), "out");
	EXPECT_EQ(ControlStateName(ControlState::Clinging), "clinging");
}

TEST(ConsciencesMenu, spiritStateNames)
{
	namespace dude_state = help::spirits::dude_state;
	EXPECT_EQ(DudeStateName(dude_state::k_Hover), "hovering");
	EXPECT_EQ(DudeStateName(dude_state::k_PointHoldL), "pointing left");
	EXPECT_EQ(DudeStateName(dude_state::k_ClingLeave), "leaving the edge");
	EXPECT_EQ(DudeStateName(dude_state::k_ScriptedAnim), "playing a clip");
	EXPECT_EQ(DudeStateName(3), "state 3");
}

TEST(ConsciencesMenu, setModeNames)
{
	EXPECT_EQ(SetModeName(0), "its texts");
	EXPECT_EQ(SetModeName(1), "one text at random");
	EXPECT_EQ(SetModeName(2), "the thing's texts");
	EXPECT_EQ(SetModeName(3), "a script");
	EXPECT_EQ(SetModeName(9), "unknown");
}

TEST(ConsciencesMenu, aSearchMatchesTheSetNumberOrItsScript)
{
	const auto row = MakeRow(42, "HelpSpiritsFood");
	EXPECT_TRUE(MatchesSearch(row, ""));
	EXPECT_TRUE(MatchesSearch(row, "42"));
	EXPECT_FALSE(MatchesSearch(row, "4"));
	EXPECT_TRUE(MatchesSearch(row, "spirits"));
	EXPECT_TRUE(MatchesSearch(row, "FOOD"));
	EXPECT_FALSE(MatchesSearch(row, "wood"));
	// A number followed by letters is a name search
	EXPECT_FALSE(MatchesSearch(row, "42x"));
	EXPECT_FALSE(MatchesSearch(MakeRow(1, ""), "food"));
}

TEST(ConsciencesMenu, clipsAreTheNamedSlotsAfterTheStopSlot)
{
	const std::vector<std::string> names {"Stand", "Hover", "", "HoverLeft", ""};
	const auto clips = ClipsOf(names);
	// Slot 0 stops a clip when played, so it is not listed
	ASSERT_EQ(clips.size(), 2);
	EXPECT_EQ(clips[0].slot, 1);
	EXPECT_EQ(clips[0].name, "Hover");
	EXPECT_EQ(clips[1].slot, 3);
	EXPECT_EQ(clips[1].name, "HoverLeft");
	EXPECT_TRUE(ClipsOf({}).empty());
	EXPECT_TRUE(ClipsOf(std::vector<std::string> {"Stand"}).empty());
}

TEST(ConsciencesMenu, clipsPlayWhereEachSpiritAppears)
{
	EXPECT_EQ(ClipSpot(help::spirits::k_GoodDude, {640, 480}), glm::ivec2(160, 240));
	EXPECT_EQ(ClipSpot(help::spirits::k_EvilDude, {640, 480}), glm::ivec2(480, 240));
}

TEST(ConsciencesMenu, overrideNames)
{
	using help::SpeakerOverride;
	EXPECT_EQ(OverrideName(SpeakerOverride::None), "as the game says");
	EXPECT_EQ(OverrideName(SpeakerOverride::SilenceEvil), "evil spirit silent");
	EXPECT_EQ(OverrideName(SpeakerOverride::ForceGood), "good spirit says everything");
}
