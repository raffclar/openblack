/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>

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

TEST(CreatureAttention, TheLookUsesTheGamesOrderOfSums)
{
	// On the edge of the view: the sums go from z to x, each vector is multiplied by the inverse of its length, and the
	// cosine of the angle stays wide. The order of glm and a float cosine would say that the camera does not see it
	const glm::vec3 origin {1000.0f, 50.0f, 1000.0f};
	const glm::vec3 focus {1000.0f, 0.0f, 1300.0f};
	const glm::vec3 creature {std::bit_cast<float>(0x449617B3u), std::bit_cast<float>(0x4196EACCu),
	                          std::bit_cast<float>(0x449459FDu)};
	EXPECT_TRUE(IsLookingAt(origin, focus, creature));
}

TEST(CreatureAttention, TheShareOfItsLifeItWasLookedAt)
{
	EXPECT_EQ(Share(0, 0), 0.0f);
	EXPECT_EQ(Share(5, 0), 0.0f);
	EXPECT_EQ(Share(25, 100), 0.25f);
	EXPECT_EQ(Share(100, 100), 1.0f);
}

TEST(CreatureAttention, TheShareRoundsToAFloatOneTime)
{
	// The game divides the two exact counts and keeps the quotient as a float
	EXPECT_EQ(std::bit_cast<uint32_t>(Share(2, 3)), 0x3F2AAAABu);
	EXPECT_EQ(std::bit_cast<uint32_t>(Share(1, 7)), 0x3E124925u);
	// A count above the float range of whole numbers is exact in the division
	EXPECT_EQ(std::bit_cast<uint32_t>(Share(16777219, 2)), 0x4B000002u);
	// The lived turns are a signed number
	EXPECT_EQ(Share(1, 0xFFFFFFFFu), -1.0f);
}

TEST(CreatureAttention, FiveWordsForHowMuch)
{
	EXPECT_EQ(HowMuch(0.0f), 0);
	EXPECT_EQ(HowMuch(0.19f), 0);
	EXPECT_EQ(HowMuch(0.2f), 1);
	EXPECT_EQ(HowMuch(0.79f), 3);
	EXPECT_EQ(HowMuch(0.8f), 4);
	EXPECT_EQ(HowMuch(1.0f), 4);
	EXPECT_EQ(HowMuch(3.0f), 4);
	// The game counts without a sign, so below nothing is the most
	EXPECT_EQ(HowMuch(-0.5f), 4);
	// A fifth just short of a whole one is the lower word
	EXPECT_EQ(HowMuch(Share(1, 5)), 1);
}
