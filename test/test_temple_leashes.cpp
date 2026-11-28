/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <optional>

#include <glm/gtc/matrix_transform.hpp>
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

TEST(TempleLeashes, TheyStopWithTheDrawOfTheirTemple)
{
	// A temple that stands, or that is in its destruction until 21.8 seconds, draws its leashes
	EXPECT_EQ(StateOf(false, 0.0f), TempleState::Drawn);
	EXPECT_EQ(StateOf(false, 30.0f), TempleState::Drawn);
	EXPECT_EQ(StateOf(true, 21.8f), TempleState::Drawn);
	EXPECT_EQ(StateOf(true, 21.81f), TempleState::NotDrawn);
	EXPECT_TRUE(ShowsOn(TempleState::NoTemple));
	EXPECT_TRUE(ShowsOn(TempleState::Drawn));
	EXPECT_FALSE(ShowsOn(TempleState::NotDrawn));
	EXPECT_FALSE(ShowsOn(TempleState::Gone));
}

TEST(TempleLeashes, TheyGoOnlyWithTheirTemple)
{
	EXPECT_TRUE(GoesWith(TempleState::Gone));
	EXPECT_FALSE(GoesWith(TempleState::NotDrawn));
	EXPECT_FALSE(GoesWith(TempleState::Drawn));
	EXPECT_FALSE(GoesWith(TempleState::NoTemple));
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
	const glm::mat4 still(turn);
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

TEST(TempleLeashes, TheCarriedLeashStaysOnTheWristOfTheMovedHand)
{
	// One posed wrist bone, in the model of the hand
	const auto boneTurn = TurnByAngles({0.4f, -0.3f, 1.1f});
	glm::mat4 bone(boneTurn);
	bone[3] = glm::vec4(-229.0f, -555.0f, -5.0f, 1.0f);
	// The hand at rest, then on a creature: in another place, turned another way and larger
	struct Hand
	{
		glm::vec3 position;
		glm::vec3 angles;
		float scale;
	};
	const std::array<Hand, 3> hands {{
	    {.position = {10.0f, 20.0f, 30.0f}, .angles = {0.0f, 0.0f, 0.0f}, .scale = 0.0058f},
	    {.position = {-40.0f, 5.0f, 12.0f}, .angles = {1.3f, 0.7f, -0.2f}, .scale = 0.0082f},
	    {.position = {2560.0f, 28.0f, 2556.0f}, .angles = {-2.1f, 0.2f, 0.9f}, .scale = 0.0116f},
	}};
	for (const bool mirrored : {false, true})
	{
		std::optional<glm::vec3> first;
		for (const auto& hand : hands)
		{
			auto model = glm::mat4(TurnByAngles(hand.angles));
			model[3] = glm::vec4(hand.position, 1.0f);
			const auto mirror = glm::vec3(mirrored ? -hand.scale : hand.scale, hand.scale, hand.scale);
			const auto wrist = model * glm::scale(glm::mat4(1.0f), mirror) * bone;
			const auto carried = CarriedAt(wrist, hand.scale);
			// Seen from the wrist, the leash is at the same place for each hand
			const auto local = glm::vec3(glm::inverse(wrist) * glm::vec4(carried.position, 1.0f));
			if (!first.has_value())
			{
				first = local;
			}
			EXPECT_NEAR(glm::distance(local, *first), 0.0f, 0.1f);
			EXPECT_NEAR(glm::length(local), glm::length(glm::vec2(0.05f, 0.25f)) * k_HandScale, 0.1f);
			// Its ring keeps the axis of the wrist, and its size is the same share of the hand
			const auto axis = glm::normalize(glm::vec3(wrist[2]));
			EXPECT_NEAR(glm::dot(carried.rotation[2], axis), 1.0f, k_Tolerance);
			EXPECT_NEAR(carried.scale / hand.scale, k_HandScale * 0.5f, 1e-2f);
		}
	}
}

TEST(TempleLeashes, EachTempleMakesItsLeashesOnce)
{
	EXPECT_EQ(SetFor(true, false), TempleSet::Has);
	EXPECT_EQ(SetFor(true, true), TempleSet::Has);
	// A game saved before the leashes kept their temple: the temple takes the leashes at its points
	EXPECT_EQ(SetFor(false, true), TempleSet::TakesUnlinked);
	// A temple made after the land started, by the editor, an undo or a script
	EXPECT_EQ(SetFor(false, false), TempleSet::Makes);
}

TEST(TempleLeashes, ALeashWithNoLinkHangsOnlyAtThePointItself)
{
	const glm::vec3 point {10.0f, 20.0f, 30.0f};
	EXPECT_TRUE(HangsAt(point, point));
	EXPECT_TRUE(HangsAt(point + glm::vec3(0.005f, 0.0f, 0.0f), point));
	EXPECT_FALSE(HangsAt(point + glm::vec3(0.0f, 0.02f, 0.0f), point));
	EXPECT_FALSE(HangsAt(point + glm::vec3(6.0f, 0.0f, 0.0f), point));
}

TEST(TempleLeashes, TheLeashesHangAtTheFirstThreePointsOfTheTemple)
{
	const auto at = [](float x, float y, float z) { return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z)); };
	const std::array<glm::mat4, 4> points {at(1.0f, 2.0f, 3.0f), at(-1.0f, 2.0f, 0.0f), at(0.0f, 5.0f, -2.0f),
	                                       at(9.0f, 9.0f, 9.0f)};
	// The temple stands at (100, 10, 50), turned a quarter turn about the up axis
	const auto temple = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 10.0f, 50.0f)) *
	                    glm::rotate(glm::mat4(1.0f), k_FullTurn / 4.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	const auto hung = PointsOn(temple, points);
	ASSERT_TRUE(hung.has_value());
	EXPECT_NEAR(hung->at(0).x, 103.0f, k_Tolerance);
	EXPECT_NEAR(hung->at(0).y, 12.0f, k_Tolerance);
	EXPECT_NEAR(hung->at(0).z, 49.0f, k_Tolerance);
	EXPECT_NEAR(hung->at(1).x, 100.0f, k_Tolerance);
	EXPECT_NEAR(hung->at(1).z, 51.0f, k_Tolerance);
	EXPECT_NEAR(hung->at(2).x, 98.0f, k_Tolerance);
	EXPECT_NEAR(hung->at(2).y, 15.0f, k_Tolerance);
	// A mesh with fewer than three points hangs no leashes
	EXPECT_FALSE(PointsOn(temple, std::span(points).first(2)).has_value());
}
