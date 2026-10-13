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

TEST(TempleLeashes, EachPlayersLeashSlidesItsTextureByItself)
{
	// Each leash slides its texture from its own random start
	const auto first = Advance(Start({1000, 0, 0, 0}), 1.0f);
	const auto second = Advance(Start({20000, 0, 0, 0}), 1.0f);
	EXPECT_NE(first.scroll, second.scroll);
	// Each draw is scaled to its range as the game scales the C library's numbers
	EXPECT_NEAR(Start({16384, 0, 0, 0}).scroll, 16384.0f / 32767.0f, k_Tolerance);
}

TEST(TempleLeashes, AnglesGiveBackTheirTurn)
{
	// A turn made of three angles gives back those angles, and so the same turn
	const glm::vec3 angles {0.4f, -0.3f, 1.2f};
	const auto turn = TurnByAngles(angles);
	const auto back = AnglesOf(turn);
	EXPECT_NEAR(back.x, angles.x, k_Tolerance);
	EXPECT_NEAR(back.y, angles.y, k_Tolerance);
	EXPECT_NEAR(back.z, angles.z, k_Tolerance);
	// Growing the thing evenly changes none of them
	const auto grown = AnglesOf(turn * 3.0f);
	EXPECT_NEAR(grown.x, angles.x, k_Tolerance);
	EXPECT_NEAR(grown.z, angles.z, k_Tolerance);
	// The hanging leash's tumble is the same turn with no first angle
	const auto tumble = Turn({.pitch = 0.7f, .roll = 2.1f});
	const auto same = TurnByAngles({0.0f, 0.7f, 2.1f});
	for (int column = 0; column < 3; ++column)
	{
		for (int row = 0; row < 3; ++row)
		{
			EXPECT_NEAR(tumble[column][row], same[column][row], k_Tolerance);
		}
	}
}

TEST(TempleLeashes, TheCarriedLeashSitsRoundTheWrist)
{
	// The wrist turned a quarter turn about the upward axis, a little grown, somewhere in the world
	const auto turn = TurnByAngles({0.0f, 0.0f, 0.0f});
	const auto quarter = TurnByAngles({k_FullTurn * 0.25f, 0.0f, 0.0f});
	glm::mat4 wrist(quarter * 2.0f);
	wrist[3] = glm::vec4(10.0f, 20.0f, 30.0f, 1.0f);
	const float handScale = 0.01f;
	const auto carried = CarriedAt(wrist, handScale);
	const auto size = handScale * k_HandScale;
	// Half the hand's size across, turned as the wrist without its growth
	EXPECT_NEAR(carried.scale, size * 0.5f, k_Tolerance);
	for (int column = 0; column < 3; ++column)
	{
		EXPECT_NEAR(glm::length(carried.rotation[column]), 1.0f, k_Tolerance);
		for (int row = 0; row < 3; ++row)
		{
			EXPECT_NEAR(carried.rotation[column][row], quarter[column][row], k_Tolerance);
		}
	}
	// Shifted along its own axes from the wrist
	const auto shift = quarter * (glm::vec3(0.05f, 0.25f, 0.0f) * size);
	EXPECT_NEAR(carried.position.x, 10.0f + shift.x, k_Tolerance);
	EXPECT_NEAR(carried.position.y, 20.0f + shift.y, k_Tolerance);
	EXPECT_NEAR(carried.position.z, 30.0f + shift.z, k_Tolerance);
	// Unturned, it is shifted up the hand a quarter of its size and a twentieth across
	glm::mat4 still(turn);
	const auto plain = CarriedAt(still, handScale);
	EXPECT_NEAR(plain.position.x, 0.05f * size, k_Tolerance);
	EXPECT_NEAR(plain.position.y, 0.25f * size, k_Tolerance);
}

TEST(TempleLeashes, TheCarriedLeashGoesWithThePutAwayHand)
{
	EXPECT_TRUE(Shows(false, true));
	EXPECT_TRUE(Shows(false, false));
	EXPECT_TRUE(Shows(true, true));
	EXPECT_FALSE(Shows(true, false));
}
