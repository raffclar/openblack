/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Hand/ScriptHandState.h"
#include "Help/ScriptHelpText.h"

using namespace openblack;

namespace
{
using hand::script_state::Facts;
using hand::script_state::State;
using hand::script_state::StateOf;
using help::script_help_text::Texts;

constexpr auto k_RewardText = static_cast<HelpText>(40);
constexpr auto k_MiracleText = static_cast<HelpText>(50);
constexpr auto k_KindText = static_cast<HelpText>(60);
} // namespace

TEST(ScriptHelpText, ARewardSaysItsOwnText)
{
	const Texts reward {.reward = k_RewardText, .dispenser = std::nullopt, .kind = k_KindText};
	EXPECT_EQ(help::script_help_text::Help(reward), k_RewardText);
	// Its first help is its kind's
	EXPECT_EQ(help::script_help_text::FirstHelp(reward), k_KindText);
}

TEST(ScriptHelpText, ADispenserSaysItsMiraclesText)
{
	const Texts dispenser {.reward = std::nullopt, .dispenser = k_MiracleText, .kind = std::nullopt};
	EXPECT_EQ(help::script_help_text::Help(dispenser), k_MiracleText);
	EXPECT_EQ(help::script_help_text::FirstHelp(dispenser), k_MiracleText);
}

TEST(ScriptHelpText, OtherObjectsSayTheirKindsTextAndAnythingElseNothing)
{
	EXPECT_EQ(help::script_help_text::Help({.reward = std::nullopt, .dispenser = std::nullopt, .kind = k_KindText}),
	          k_KindText);
	EXPECT_EQ(help::script_help_text::Help({}), HelpText {});
	EXPECT_EQ(help::script_help_text::FirstHelp({}), HelpText {});
}

TEST(ScriptHandState, NormalOverTheLand)
{
	EXPECT_EQ(StateOf({}), State::Normal);
	// The other button alone grips nothing
	EXPECT_EQ(StateOf({.otherButton = true}), State::Normal);
}

TEST(ScriptHandState, GrippingTheLandAndZoomingWithIt)
{
	EXPECT_EQ(StateOf({.gripsLand = true}), State::GripLandscape);
	EXPECT_EQ(StateOf({.gripsLand = true, .otherButton = true}), State::ZoomLandscape);
	// Without zooming allowed, both buttons still only grip
	EXPECT_EQ(StateOf({.gripsLand = true, .otherButton = true, .cameraFeatures = camera_drag::feature::k_Rotate}),
	          State::GripLandscape);
	EXPECT_EQ(static_cast<int32_t>(State::GripLandscape), 20);
	EXPECT_EQ(static_cast<int32_t>(State::ZoomLandscape), 22);
}

TEST(ScriptHandState, OffWhileAScriptHoldsTheInterfaceAndInTheTemple)
{
	EXPECT_EQ(StateOf({.interfaceActive = false, .gripsLand = true}), State::Off);
	EXPECT_EQ(StateOf({.inTemple = true, .gripsLand = true}), State::InsideTemple);
	EXPECT_EQ(static_cast<int32_t>(State::Off), 25);
	EXPECT_EQ(static_cast<int32_t>(State::InsideTemple), 30);
}

TEST(ScriptHandState, TheTurningStateIsNeverGiven)
{
	// The turning lesson replays its demonstration unless the hand state is the turning one, which the game never gives
	for (const bool grips : {false, true})
	{
		for (const bool other : {false, true})
		{
			EXPECT_NE(StateOf({.gripsLand = grips, .otherButton = other}), State::RotateLandscape);
		}
	}
}
