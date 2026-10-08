/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "3D/TempleToggles.h"
#include "3D/TempleToolTips.h"
#include "Audio/Device/Sound.h"

using namespace openblack;
using Display = TempleToggles::Display;

namespace
{
/// The main room's submeshes, as its mesh names them, among others
const std::vector<std::string> k_Names = {
    "LH_Wall",
    "LH_Checkbox_DisplayCitadel_checked",
    "LH_Checkbox_DisplayCitadel_unchecked",
    "LH_Checkbox_DisplayCreature_checked",
    "LH_Checkbox_DisplayCreature_unchecked",
    "LH_Checkbox_Displaymagicactivity_checked",
    "LH_Checkbox_Displaymagicactivity_unchecked",
    "LH_Checkbox_DisplayInfluence_checked",
    "LH_Checkbox_DisplayInfluence_unchecked",
    "LH_Checkbox_DisplayChallenges_checked",
    "LH_Checkbox_DisplayChallenges_unchecked",
    "LH_SCROLL_WORLD",
};
constexpr uint32_t k_CitadelChecked = 1;
constexpr uint32_t k_CitadelUnchecked = 2;
constexpr uint32_t k_Scroll = 11;

struct Toggles
{
	std::vector<entt::id_type> sounds;
	std::vector<temple_sounds::Options> clicks;
	TempleToggles toggles {[this](const temple_sounds::Options& click) {
		sounds.push_back(click.sound);
		clicks.push_back(click);
	}};
	Toggles() { toggles.Find(k_Names); }
};

constexpr auto k_Up = static_cast<entt::id_type>(audio::SoundId::G_CitadelButtonUp_01);
constexpr auto k_Down = static_cast<entt::id_type>(audio::SoundId::G_CitadelButtonDown_01);
} // namespace

TEST(TempleToggles, ShowEverythingAtFirst)
{
	Toggles t;
	for (const auto display :
	     {Display::Temples, Display::Creatures, Display::MiracleActivity, Display::Influence, Display::Challenges})
	{
		EXPECT_TRUE(t.toggles.IsShown(display));
	}
	// The checked button of each pair is drawn
	EXPECT_EQ(t.toggles.GetHidden(), (std::vector<uint32_t> {2, 4, 6, 8, 10}));
	ASSERT_EQ(t.toggles.GetControls().size(), 5);
	EXPECT_EQ(t.toggles.GetControls()[0].subMesh, k_CitadelChecked);
	EXPECT_EQ(t.toggles.GetControls()[0].toolTip, 0xE91 - 0xE73);
	EXPECT_TRUE(t.toggles.IsControl(k_CitadelChecked));
	EXPECT_FALSE(t.toggles.IsControl(k_CitadelUnchecked));
}

TEST(TempleToggles, TurnOverAsTheyAreLetGoOf)
{
	Toggles t;
	// The press takes the mouse, and nothing happens until it is let go
	EXPECT_TRUE(t.toggles.Hold(true, k_CitadelChecked));
	EXPECT_TRUE(t.toggles.Hold(true, k_CitadelChecked));
	EXPECT_TRUE(t.toggles.IsShown(Display::Temples));
	EXPECT_TRUE(t.toggles.Hold(false, k_CitadelChecked));
	EXPECT_FALSE(t.toggles.IsShown(Display::Temples));
	EXPECT_FALSE(t.toggles.IsHeld());
	ASSERT_EQ(t.sounds.size(), 1);
	EXPECT_EQ(t.sounds[0], static_cast<entt::id_type>(audio::SoundId::G_CitadelButtonDown_01));
	EXPECT_TRUE(t.toggles.IsControl(k_CitadelUnchecked));

	// And back on with the other button of the pair
	t.toggles.Hold(true, k_CitadelUnchecked);
	t.toggles.Hold(false, k_CitadelUnchecked);
	EXPECT_TRUE(t.toggles.IsShown(Display::Temples));
	EXPECT_EQ(t.sounds.back(), static_cast<entt::id_type>(audio::SoundId::G_CitadelButtonUp_01));
}

TEST(TempleToggles, EachClicksAtItsOwnPitch)
{
	Toggles t;
	// Each button off, then each back on: the checked submesh of a button is 1 + 2 * its number, the unchecked the
	// one after it
	for (uint32_t button = 0; button < TempleToggles::k_Count; ++button)
	{
		t.toggles.Hold(true, 1 + 2 * button);
		t.toggles.Hold(false, 1 + 2 * button);
	}
	for (uint32_t button = 0; button < TempleToggles::k_Count; ++button)
	{
		t.toggles.Hold(true, 2 + 2 * button);
		t.toggles.Hold(false, 2 + 2 * button);
	}
	ASSERT_EQ(t.clicks.size(), 2 * TempleToggles::k_Count);
	const std::array<int, TempleToggles::k_Count> downPitches {100, 110, 95, 105, 108};
	const std::array<int, TempleToggles::k_Count> upPitches {100, 100, 95, 105, 108};
	for (size_t button = 0; button < TempleToggles::k_Count; ++button)
	{
		const auto& down = t.clicks[button];
		const auto& up = t.clicks[TempleToggles::k_Count + button];
		EXPECT_EQ(down.sound, k_Down) << button;
		EXPECT_EQ(up.sound, k_Up) << button;
		EXPECT_EQ(down.pitch, downPitches.at(button)) << button;
		EXPECT_EQ(up.pitch, upPitches.at(button)) << button;
		EXPECT_EQ(down.owner, audio::Owner::None()) << button;
	}
	// The creatures' button keys its click up by its 110, the others by nothing
	EXPECT_EQ(t.clicks[TempleToggles::k_Count + 1].owner, audio::Owner::Key(110));
	EXPECT_EQ(t.clicks[TempleToggles::k_Count].owner, audio::Owner::None());
	EXPECT_EQ(t.clicks[TempleToggles::k_Count + 4].owner, audio::Owner::None());
	// Without a place, once, starting again while it still plays, at the bank's volume, the game's default tracking
	for (const auto& click : t.clicks)
	{
		EXPECT_FALSE(click.is3D);
		EXPECT_TRUE(click.track);
		EXPECT_EQ(click.loops, 0);
		EXPECT_EQ(click.mode, 3);
		EXPECT_EQ(click.volume, 127);
		EXPECT_EQ(click.callerMask, 0u);
	}
	EXPECT_EQ(TempleToggles::ClickOf(Display::Influence, false).pitch, 105);
	EXPECT_EQ(TempleToggles::ClickOf(Display::Challenges, true).sound, k_Up);
}

TEST(TempleToggles, StayAsTheyAreLetGoOfElsewhere)
{
	Toggles t;
	t.toggles.Hold(true, k_CitadelChecked);
	EXPECT_TRUE(t.toggles.Hold(false, k_Scroll));
	EXPECT_TRUE(t.toggles.IsShown(Display::Temples));
	EXPECT_TRUE(t.sounds.empty());
	// A press elsewhere, or on the button not drawn, isn't theirs
	EXPECT_FALSE(t.toggles.Hold(true, k_Scroll));
	t.toggles.Hold(false, std::nullopt);
	EXPECT_FALSE(t.toggles.Hold(true, k_CitadelUnchecked));
}

TEST(TempleToggles, SayWhatTheyShowWhenHovered)
{
	Toggles t;
	const auto toggles = t.toggles.GetControls();
	const std::array scrolls = {TempleScrolls::Control {.subMesh = k_Scroll, .focused = false}};
	TempleToolTip toolTip = k_FirstTempleToolTip;
	TempleToolTipInput input {.room = TempleRoom::Main,
	                          .inControl = true,
	                          .hoveredSubMesh = k_CitadelChecked,
	                          .scrolls = scrolls,
	                          .toggles = toggles};
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE91 - 0xE73);
	EXPECT_EQ(toolTip.arrows, gui::ToolTipArrows::k_None);

	// The scroll's callback comes after the buttons', so looking at it shows the way back over them
	input.zoom = 1.0f;
	input.lookingAtScroll = true;
	UpdateTempleToolTip(toolTip, input);
	EXPECT_EQ(toolTip.index, 0xE8B - 0xE73);
}
