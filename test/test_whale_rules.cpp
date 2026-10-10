/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <numbers>

#include <gtest/gtest.h>

#include "ECS/WhaleRules.h"

using namespace openblack::ecs::whale_rules;

TEST(WhaleRules, AWhaleFacesTheWayItMoved)
{
	EXPECT_FLOAT_EQ(Heading({0, 0, 0}, {0, 0, 5}, 1.0f), std::numbers::pi_v<float> / 2.0f);
	// Turning the other way comes out as the angle from +x the long way round
	EXPECT_FLOAT_EQ(Heading({0, 0, 0}, {0, 0, -5}, 1.0f), 1.5f * std::numbers::pi_v<float>);
	// Standing still across the land, or only rising, it faces the way it did
	EXPECT_FLOAT_EQ(Heading({3, 0, 4}, {3, 7, 4}, 1.25f), 1.25f);
}

TEST(WhaleRules, AWhaleIsDrawnBetweenItsTurns)
{
	const auto drawn = Drawn({0, 0, 0}, 1.0f, {10, 0, 20}, 3.0f, 0.25f);
	EXPECT_FLOAT_EQ(drawn.x, 2.5f);
	EXPECT_FLOAT_EQ(drawn.y, 1.5f);
	EXPECT_FLOAT_EQ(drawn.z, 5.0f);
}

TEST(WhaleRules, TheWakeRingsComeOnceMoreThanFiftyMillisecondsHaveGone)
{
	int32_t timer = 0;
	EXPECT_FALSE(WakeRing(timer, {1, 2, 3}, 0.5f, 30).has_value());
	EXPECT_EQ(timer, 30);
	EXPECT_FALSE(WakeRing(timer, {1, 2, 3}, 0.5f, 20).has_value());
	EXPECT_EQ(timer, 50);
	EXPECT_FALSE(WakeRing(timer, {1, 2, 3}, 0.5f, 30).has_value());
	EXPECT_EQ(timer, 80);
	const auto ring = WakeRing(timer, {1, 2, 3}, 0.5f, 30);
	ASSERT_TRUE(ring.has_value());
	EXPECT_EQ(timer, 60);
	// On the water under the point, turned with the whale
	EXPECT_EQ(ring->position, glm::vec3(1, 0, 3));
	EXPECT_FLOAT_EQ(ring->angle, 0.5f);
	EXPECT_FLOAT_EQ(ring->growth, 10.0f);
	EXPECT_FLOAT_EQ(ring->rate, 0.5f);
	EXPECT_FLOAT_EQ(ring->aspect, 1.0f);
	EXPECT_EQ(ring->cell, 0x31);
	EXPECT_EQ(ring->argb, 0x90FFFFFFu);
}
