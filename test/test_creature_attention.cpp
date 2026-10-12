/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureAttention.h"

using namespace openblack;
using namespace openblack::creature_attention;

namespace
{
/// A creature `distance` from a camera at the origin looking along +x, `degrees` off the way it looks
glm::vec3 Around(float distance, float degrees)
{
	const auto radians = degrees * 3.14159265f / 180.0f;
	return {distance * std::cos(radians), 0.0f, distance * std::sin(radians)};
}
constexpr glm::vec3 k_Origin {0.0f, 0.0f, 0.0f};
constexpr glm::vec3 k_Ahead {10.0f, 0.0f, 0.0f};
} // namespace

TEST(CreatureAttention, TheCameraLooksAtACreatureNearAndAheadOfIt)
{
	EXPECT_TRUE(IsLookingAt(k_Origin, k_Ahead, Around(100.0f, 0.0f)));
	// Within two thirds of seventy degrees either way
	EXPECT_TRUE(IsLookingAt(k_Origin, k_Ahead, Around(100.0f, 46.0f)));
	EXPECT_TRUE(IsLookingAt(k_Origin, k_Ahead, Around(100.0f, -46.0f)));
	EXPECT_FALSE(IsLookingAt(k_Origin, k_Ahead, Around(100.0f, 47.5f)));
	EXPECT_FALSE(IsLookingAt(k_Origin, k_Ahead, Around(100.0f, 180.0f)));
}

TEST(CreatureAttention, NotACreatureTooFarAway)
{
	EXPECT_TRUE(IsLookingAt(k_Origin, k_Ahead, Around(399.0f, 0.0f)));
	EXPECT_FALSE(IsLookingAt(k_Origin, k_Ahead, Around(400.0f, 0.0f)));
}

TEST(CreatureAttention, NotACreatureAtTheCameraNorFromACameraLookingNowhere)
{
	EXPECT_FALSE(IsLookingAt(k_Origin, k_Ahead, k_Origin));
	EXPECT_FALSE(IsLookingAt(k_Origin, k_Origin, Around(100.0f, 0.0f)));
}

TEST(CreatureAttention, TheShareOfItsLifeItWasLookedAt)
{
	EXPECT_DOUBLE_EQ(Share(0, 0), 0.0);
	EXPECT_DOUBLE_EQ(Share(5, 0), 0.0);
	EXPECT_DOUBLE_EQ(Share(25, 100), 0.25);
	EXPECT_DOUBLE_EQ(Share(100, 100), 1.0);
}

TEST(CreatureAttention, FiveWordsForHowMuch)
{
	EXPECT_EQ(HowMuch(0.0), 0);
	EXPECT_EQ(HowMuch(0.19), 0);
	EXPECT_EQ(HowMuch(0.2), 1);
	EXPECT_EQ(HowMuch(0.79), 3);
	EXPECT_EQ(HowMuch(0.8), 4);
	EXPECT_EQ(HowMuch(1.0), 4);
	EXPECT_EQ(HowMuch(3.0), 4);
	// The game counts without a sign, so below nothing is the most
	EXPECT_EQ(HowMuch(-0.5), 4);
	// A fifth just short of a whole one is the lower word
	EXPECT_EQ(HowMuch(Share(1, 5)), 1);
}
