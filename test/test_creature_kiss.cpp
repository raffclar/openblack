/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureKiss.h"

using namespace openblack;
using creature_kiss::KissPoints;

namespace
{
/// The lips of a tall creature: high at 30, low at 10, in front of the body by 4 and 6
constexpr KissPoints k_Tall {.high = {0.0f, 30.0f, -4.0f}, .low = {0.0f, 10.0f, -6.0f}};
/// The lips of a short creature: high at 20, low at 5
constexpr KissPoints k_Short {.high = {0.0f, 20.0f, -2.0f}, .low = {0.0f, 5.0f, -3.0f}};
} // namespace

TEST(CreatureKiss, TallerBendsDownToTheHighKissOfTheShorter)
{
	// The lips of the tall one go to 20 of its range from 10 to 30: half way
	const auto weight = creature_kiss::HighWeight(k_Tall, k_Short);
	ASSERT_TRUE(weight.has_value());
	EXPECT_FLOAT_EQ(*weight, 0.5f);
	EXPECT_FLOAT_EQ(creature_kiss::LipsAt(k_Tall, *weight).y, 20.0f);
}

TEST(CreatureKiss, ShorterUsesItsHighKiss)
{
	const auto weight = creature_kiss::HighWeight(k_Short, k_Tall);
	ASSERT_TRUE(weight.has_value());
	EXPECT_FLOAT_EQ(*weight, 1.0f);
	// Two of the same height both use the high kiss
	EXPECT_EQ(creature_kiss::HighWeight(k_Tall, k_Tall), 1.0f);
}

TEST(CreatureKiss, NoKissWhenTheShorterIsTooShort)
{
	constexpr KissPoints tiny {.high = {0.0f, 8.0f, -1.0f}, .low = {0.0f, 2.0f, -1.0f}};
	EXPECT_FALSE(creature_kiss::HighWeight(k_Tall, tiny).has_value());
	EXPECT_FALSE(creature_kiss::HighWeight(tiny, k_Tall).has_value());
	EXPECT_FLOAT_EQ(creature_kiss::KissingDistance(k_Tall, tiny), 0.0f);
}

TEST(CreatureKiss, KissingDistanceIsHowFarTheLipsAreInFront)
{
	// The tall one at weight a half: z is -5
	EXPECT_FLOAT_EQ(creature_kiss::KissingDistance(k_Tall, k_Short), 5.0f);
	EXPECT_FLOAT_EQ(creature_kiss::KissingDistance(k_Short, k_Tall), 2.0f);
	EXPECT_FLOAT_EQ(creature_kiss::MeanKissingDistance(k_Tall, k_Short), 3.5f);
}

TEST(CreatureKiss, PlacesOnEachSideOfTheMiddle)
{
	const auto [self, other] = creature_kiss::KissPlaces({0.0f, 0.0f}, {10.0f, 0.0f}, 2.0f);
	EXPECT_FLOAT_EQ(self.x, 3.0f);
	EXPECT_FLOAT_EQ(other.x, 7.0f);
	EXPECT_FLOAT_EQ(self.y, 0.0f);
	// Two on the same point both stand on it
	const auto [same, sameOther] = creature_kiss::KissPlaces({4.0f, 4.0f}, {4.0f, 4.0f}, 2.0f);
	EXPECT_FLOAT_EQ(same.x, 4.0f);
	EXPECT_FLOAT_EQ(sameOther.y, 4.0f);
}
