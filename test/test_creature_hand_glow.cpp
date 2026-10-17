/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The glows on a creature's body: their powers, how their textures slide, the circling glow, and where they are placed

#include <cstdint>

#include <array>
#include <limits>
#include <vector>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureHandGlow.h"

using namespace openblack;
using creature_hand_glow::Place;

TEST(CreatureHandGlow, APowerMakesAGlowAndNoPowerRemovesIt)
{
	creature_hand_glow::Glows glows;
	creature_hand_glow::Set(glows, Place::RightHand, 1.5f, 2.0f);
	ASSERT_TRUE(glows.at(2).has_value());
	EXPECT_FLOAT_EQ(glows.at(2)->power, 2.0f);
	EXPECT_FLOAT_EQ(glows.at(2)->scale, 1.5f);
	EXPECT_FALSE(glows.at(0).has_value());

	// A glow that has a new power keeps how far its texture has slid
	glows.at(2)->slide = {0.25f, 0.5f};
	creature_hand_glow::Set(glows, Place::RightHand, 1.0f, 3.0f);
	EXPECT_FLOAT_EQ(glows.at(2)->slide.x, 0.25f);
	EXPECT_FLOAT_EQ(creature_hand_glow::Zoom(*glows.at(2)), 6.0f);

	creature_hand_glow::Set(glows, Place::RightHand, 1.0f, 0.0f);
	EXPECT_FALSE(glows.at(2).has_value());
}

TEST(CreatureHandGlow, TheTextureSlidesByRealTimeAndWrapsRound)
{
	creature_hand_glow::Glow glow;
	creature_hand_glow::Slide(glow, 16);
	const float seconds = 16.0f * 0.001f;
	EXPECT_EQ(glow.slide.x, seconds * 2.0f);
	EXPECT_EQ(glow.slide.y, seconds * 0.2f);
	// Half a second slides it one whole sheet across, which comes back to where it was
	const auto before = glow.slide.x;
	creature_hand_glow::Slide(glow, 500);
	EXPECT_NEAR(glow.slide.x, before, 1e-6f);
	EXPECT_LT(glow.slide.x, 1.0f);
}

TEST(CreatureHandGlow, TheCirclingGlowTurnsByTheGameTimeAboveTheBody)
{
	creature_hand_glow::Glow glow;
	creature_hand_glow::Turn(glow, 1000);
	EXPECT_FLOAT_EQ(glow.turn, 1.0f);
	creature_hand_glow::Turn(glow, 0);
	EXPECT_FLOAT_EQ(glow.turn, 1.0f);

	const auto axes = creature_hand_glow::CirclingAxes(0.0f);
	EXPECT_EQ(axes, glm::mat3(1.0f));
	const auto quarter = creature_hand_glow::CirclingAxes(1.5707964f);
	EXPECT_NEAR(quarter[0].z, 1.0f, 1e-6f);
	EXPECT_NEAR(quarter[2].x, -1.0f, 1e-6f);

	const auto point = creature_hand_glow::CirclingPoint({10.0f, 2.0f, -3.0f}, 2.0f);
	EXPECT_FLOAT_EQ(point.x, 10.0f);
	EXPECT_FLOAT_EQ(point.y, 2.0f + (1.3f * 2.0f * 15.0f));
	EXPECT_FLOAT_EQ(point.z, -3.0f);
}

TEST(CreatureHandGlow, AHandGlowIsAtTheEndsOfTheBranchesInTheOrderWalked)
{
	// Bone 1 is the hand under the root. Its children are 2 and 3, and 2 has a child 4. Bone 5 comes after the hand
	// under the root.
	constexpr auto k_None = std::numeric_limits<uint32_t>::max();
	const std::vector<uint32_t> parents {k_None, 0, 1, 1, 2, 0};
	EXPECT_EQ(creature_hand_glow::BranchEnds(parents, 1), (std::vector<uint32_t> {4, 3, 5}));
	EXPECT_EQ(creature_hand_glow::BranchEnds(parents, 4), (std::vector<uint32_t> {4}));
	EXPECT_TRUE(creature_hand_glow::BranchEnds(parents, 9).empty());

	const std::array<glm::vec3, 2> points {glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(3.0f, 4.0f, 5.0f)};
	EXPECT_EQ(creature_hand_glow::Middle(points), glm::vec3(2.0f, 3.0f, 4.0f));
}

TEST(CreatureHandGlow, TheModelIsScaledWithItsBackAxisInMirrorImage)
{
	const auto unit = creature_hand_glow::UnitAxes(glm::mat3(2.0f));
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_NEAR(glm::length(unit[i]), 1.0f, 1e-5f);
	}

	const auto model = creature_hand_glow::ModelMatrix(glm::mat3(1.0f), {1.0f, 2.0f, 3.0f}, 2.0f, 1.5f);
	EXPECT_EQ(model[0], glm::vec4(3.0f, 0.0f, 0.0f, 0.0f));
	EXPECT_EQ(model[1], glm::vec4(0.0f, 3.0f, 0.0f, 0.0f));
	EXPECT_EQ(model[2], glm::vec4(0.0f, 0.0f, -3.0f, 0.0f));
	EXPECT_EQ(model[3], glm::vec4(1.0f, 2.0f, 3.0f, 1.0f));
}
