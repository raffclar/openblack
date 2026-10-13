/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <3D/TempleToolTips.h>
#include <Temple/ChallengeRoomRules.h>
#include <gtest/gtest.h>

namespace
{
namespace rules = openblack::temple::challenge_room;
using openblack::TempleChallengeControl;
using openblack::TempleRoom;
using openblack::TempleToolTip;
using openblack::TempleToolTipInput;
using rules::Click;
using rules::Control;
using rules::Part;

constexpr std::array<rules::Challenge, 3> k_Challenges {{
    {.done = false, .quest = false},
    {.done = true, .quest = true},
    {.done = false, .quest = true},
}};
} // namespace

TEST(ChallengeRoom, FindsItsPartsByName)
{
	EXPECT_EQ(rules::ParseControl("LH_Picture_Challenge_01"), (Control {.part = Part::Picture, .place = 0}));
	EXPECT_EQ(rules::ParseControl("LH_FRAME_CHALLENGE_21"), (Control {.part = Part::Frame, .place = 20}));
	EXPECT_EQ(rules::ParseControl("LH_FRAME_CHALLENGE_SILVER_03"), (Control {.part = Part::SilverFrame, .place = 2}));
	EXPECT_EQ(rules::ParseControl("lh_frame_challenge_gold_10"), (Control {.part = Part::GoldFrame, .place = 9}));
	EXPECT_EQ(rules::ParseControl("LH_Button_Challenge_07"), (Control {.part = Part::ReplayButton, .place = 6}));
	EXPECT_FALSE(rules::ParseControl("LH_Scroll_Challenge").has_value());
	EXPECT_FALSE(rules::ParseControl("LH_Picture_Challenge_22").has_value());
	EXPECT_FALSE(rules::ParseControl("LH_Picture_Challenge_00").has_value());
}

TEST(ChallengeRoom, ShowsAPictureAndFramesForEachChallengeAndAButtonUntilItIsDone)
{
	EXPECT_TRUE(rules::IsShown({Part::Picture, 2}, k_Challenges));
	EXPECT_FALSE(rules::IsShown({Part::Picture, 3}, k_Challenges));
	EXPECT_FALSE(rules::IsShown({Part::Frame, 3}, k_Challenges));
	EXPECT_TRUE(rules::IsShown({Part::Frame, 0}, k_Challenges));
	// Silver for a challenge, gold for a quest
	EXPECT_TRUE(rules::IsShown({Part::SilverFrame, 0}, k_Challenges));
	EXPECT_FALSE(rules::IsShown({Part::GoldFrame, 0}, k_Challenges));
	EXPECT_TRUE(rules::IsShown({Part::GoldFrame, 1}, k_Challenges));
	EXPECT_FALSE(rules::IsShown({Part::SilverFrame, 1}, k_Challenges));
	EXPECT_TRUE(rules::IsShown({Part::ReplayButton, 0}, k_Challenges));
	EXPECT_FALSE(rules::IsShown({Part::ReplayButton, 1}, k_Challenges));
}

TEST(ChallengeRoom, APictureIsLookedAtThenReplaysUnlessDone)
{
	EXPECT_EQ(rules::Clicked({Part::Picture, 1}, false, k_Challenges), Click::LookCloser);
	EXPECT_EQ(rules::Clicked({Part::Picture, 0}, true, k_Challenges), Click::Replay);
	EXPECT_EQ(rules::Clicked({Part::Picture, 1}, true, k_Challenges), Click::Nothing);
	EXPECT_EQ(rules::Clicked({Part::ReplayButton, 2}, false, k_Challenges), Click::Replay);
	EXPECT_EQ(rules::Clicked({Part::ReplayButton, 1}, true, k_Challenges), Click::Nothing);
	EXPECT_EQ(rules::Clicked({Part::Frame, 0}, true, k_Challenges), Click::Nothing);
	EXPECT_EQ(rules::Clicked({Part::Picture, 5}, false, k_Challenges), Click::Nothing);
}

TEST(ChallengeRoom, TellsTheTimeTakenAndTheSuccess)
{
	// Ten turns a second: done after 37,234 turns is an hour, two minutes and three seconds
	const auto done = rules::TimeTaken(1000, 38234, 99999, 100);
	EXPECT_EQ(done.hours, 1u);
	EXPECT_EQ(done.minutes, 2u);
	EXPECT_EQ(done.seconds, 3u);
	// Not done, it counts to now, the odd turns dropped
	const auto going = rules::TimeTaken(1000, std::nullopt, 1609, 100);
	EXPECT_EQ(going.hours, 0u);
	EXPECT_EQ(going.minutes, 1u);
	EXPECT_EQ(going.seconds, 0u);
	EXPECT_EQ(rules::DetailsLine(u"Time Taken:", done, u"Success:", 0.999f), u"Time Taken: 01:02:03  Success: 99%");
	EXPECT_EQ(rules::TextAlpha(1.0f), 255);
	EXPECT_EQ(rules::TextAlpha(0.5f), 127);
	EXPECT_EQ(rules::TextAlpha(0.0f), 0);
}

TEST(ChallengeRoom, ToolTips)
{
	constexpr uint32_t k_Picture = 10;
	constexpr uint32_t k_Button = 12;
	constexpr uint32_t k_DonePicture = 11;
	const std::array controls {
	    TempleChallengeControl {.subMesh = k_Picture, .button = false, .place = 0, .done = false},
	    TempleChallengeControl {.subMesh = k_DonePicture, .button = false, .place = 1, .done = true},
	    TempleChallengeControl {.subMesh = k_Button, .button = true, .place = 0, .done = false},
	};
	TempleToolTip toolTip = openblack::k_FirstTempleToolTip;
	TempleToolTipInput input {.room = TempleRoom::Challenge, .inControl = true, .hoveredSubMesh = k_Picture};
	input.challenges = controls;
	// From afar a picture zooms in
	openblack::UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE75 - 0xE73);
	// Close up to it, it replays its challenge, and off it the camera zooms back out
	input.zoom = 1.0f;
	input.lookingAtScroll = true;
	input.challengeHighDetail = 0;
	input.pictureLookedAt = true;
	openblack::UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE9F - 0xE73);
	input.hoveredSubMesh = std::nullopt;
	openblack::UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE8B - 0xE73);
	// A done challenge's picture says nothing close up
	input.challengeHighDetail = 1;
	input.hoveredSubMesh = k_DonePicture;
	openblack::UpdateTempleToolTip(toolTip, input);
	EXPECT_FALSE(toolTip.index.has_value());
	EXPECT_EQ(toolTip.action, openblack::gui::ToolTipAction::None);
	// The button replays it too
	input.hoveredSubMesh = k_Button;
	openblack::UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE9F - 0xE73);
}
