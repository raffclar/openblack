/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <gtest/gtest.h>

#include "Creature/TempleLeashes.h"

using namespace openblack;
using namespace openblack::temple_leashes;

namespace
{
constexpr float k_Tolerance = 1e-4f;
}

TEST(TempleLeashes, EachLeashHasItsBandOfTheTexture)
{
	// The spiked blade, the twisted rope and the rainbow fur
	EXPECT_FLOAT_EQ(Band(LeashType::Evil), 0.375f);
	EXPECT_FLOAT_EQ(Band(LeashType::Rope), 0.125f);
	EXPECT_FLOAT_EQ(Band(LeashType::Good), 0.25f);
}

TEST(TempleLeashes, TheyStartAtRandomPoints)
{
	const auto look = Start({0, 32767, 16384, 32767});
	EXPECT_FLOAT_EQ(look.scroll, 0.0f);
	EXPECT_NEAR(look.pitch, k_FullTurn, k_Tolerance);
	EXPECT_NEAR(look.roll, k_FullTurn * 0.5f, k_Tolerance);
	EXPECT_NEAR(look.glow, k_GlowPictures, k_Tolerance);
}

TEST(TempleLeashes, TheyTumbleScrollAndGlowGoingRound)
{
	const auto look = Advance({.scroll = 0.9f, .pitch = 1.0f, .roll = 6.0f, .glow = 14.5f}, 0.5f);
	// Half a turn a second of the texture, round past one
	EXPECT_NEAR(look.scroll, 0.15f, k_Tolerance);
	EXPECT_NEAR(look.pitch, 1.05f, k_Tolerance);
	// A radian a second round past a whole turn
	EXPECT_NEAR(look.roll, 6.5f - k_FullTurn, k_Tolerance);
	// Ten pictures a second round the first fifteen
	EXPECT_NEAR(look.glow, 4.5f, k_Tolerance);
	EXPECT_EQ(GlowPicture(look), 4);
}

TEST(TempleLeashes, TheTumbleTurnsAboutTwoAxes)
{
	// Unturned at no angle
	const auto still = Turn({});
	EXPECT_NEAR(still[0][0], 1.0f, k_Tolerance);
	EXPECT_NEAR(still[1][1], 1.0f, k_Tolerance);
	EXPECT_NEAR(still[2][2], 1.0f, k_Tolerance);
	// A quarter turn of the second angle takes the leash's own x straight down
	const auto rolled = Turn({.roll = k_FullTurn * 0.25f});
	EXPECT_NEAR(rolled[0][1], -1.0f, k_Tolerance);
	EXPECT_NEAR(rolled[1][0], 1.0f, k_Tolerance);
	// A quarter turn of the first takes its z to the world's y
	const auto pitched = Turn({.pitch = k_FullTurn * 0.25f});
	EXPECT_NEAR(pitched[2][1], 1.0f, k_Tolerance);
	EXPECT_NEAR(pitched[1][2], -1.0f, k_Tolerance);
}

TEST(TempleLeashes, TheGlowFollowsTheLandsLight)
{
	// A sixth of the sum of the light's colours: about half by day, nothing in the dark
	EXPECT_EQ(GlowAlpha(0xFFFFFF), 127);
	EXPECT_EQ(GlowAlpha(0x000000), 0);
	EXPECT_EQ(GlowAlpha(0x0C0C0C), 6);
	const auto plain = GlowOf(false, 127);
	EXPECT_FALSE(plain.additive);
	EXPECT_FLOAT_EQ(plain.tint.r, 1.0f);
	EXPECT_NEAR(plain.tint.a, 127.0f / 255.0f, k_Tolerance);
	// The picked leash glows orange, added to what is behind
	const auto picked = GlowOf(true, 127);
	EXPECT_TRUE(picked.additive);
	EXPECT_NEAR(picked.tint.r, 0xC1 / 255.0f, k_Tolerance);
	EXPECT_NEAR(picked.tint.g, 0x81 / 255.0f, k_Tolerance);
	EXPECT_NEAR(picked.tint.b, 0x19 / 255.0f, k_Tolerance);
}

TEST(TempleLeashes, OnlyKnownLeashesHang)
{
	EXPECT_TRUE(Hung(true, true));
	EXPECT_FALSE(Hung(true, false));
	EXPECT_FALSE(Hung(false, true));
}

TEST(TempleLeashes, TheirToolTips)
{
	EXPECT_EQ(ToolTipOf(LeashType::Evil), k_AggressionTip);
	EXPECT_EQ(ToolTipOf(LeashType::Rope), k_LearningTip);
	EXPECT_EQ(ToolTipOf(LeashType::Good), k_CompassionTip);
}
