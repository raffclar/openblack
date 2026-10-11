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

#include <numbers>

#include <gtest/gtest.h>

#include "ECS/SharkRules.h"

using namespace openblack::ecs::shark_rules;

TEST(SharkRules, ASharkFacesTheWayItMoved)
{
	EXPECT_FLOAT_EQ(Heading({0, 0, 0}, {0, 0, 5}, 1.0f), std::numbers::pi_v<float> / 2.0f);
	// Turning the other way comes out as the angle from +x the long way round
	EXPECT_FLOAT_EQ(Heading({0, 0, 0}, {0, 0, -5}, 1.0f), 1.5f * std::numbers::pi_v<float>);
	// Standing still across the land, or only rising, it faces the way it did
	EXPECT_FLOAT_EQ(Heading({3, 0, 4}, {3, 7, 4}, 1.25f), 1.25f);
}

TEST(SharkRules, ItsNoseLeadsTheWayItSwims)
{
	// The model's +x lies along the heading, radians from +x towards +z
	const auto x = Facing(std::numbers::pi_v<float> / 2.0f) * glm::vec3(1.0f, 0.0f, 0.0f);
	EXPECT_NEAR(x.x, 0.0f, 1e-6f);
	EXPECT_NEAR(x.z, 1.0f, 1e-6f);
	const auto turned = Facing(0.5f) * glm::vec3(1.0f, 0.0f, 0.0f);
	EXPECT_NEAR(turned.x, std::cos(0.5f), 1e-6f);
	EXPECT_NEAR(turned.z, std::sin(0.5f), 1e-6f);
	// Upright
	EXPECT_NEAR((Facing(1.2f) * glm::vec3(0.0f, 1.0f, 0.0f)).y, 1.0f, 1e-6f);
}

TEST(SharkRules, ASharkIsDrawnBetweenItsTurns)
{
	const auto drawn = Drawn({0, 0, 0}, 1.0f, {10, 0, 20}, 3.0f, 0.25f);
	EXPECT_FLOAT_EQ(drawn.x, 2.5f);
	EXPECT_FLOAT_EQ(drawn.y, 1.5f);
	EXPECT_FLOAT_EQ(drawn.z, 5.0f);
}

TEST(SharkRules, TheWakeRingsComeOnceMoreThanFiftyMillisecondsHaveGone)
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
	// On the water under the point, turned with the shark
	EXPECT_EQ(ring->position, glm::vec3(1, 0, 3));
	EXPECT_FLOAT_EQ(ring->angle, 0.5f);
	EXPECT_FLOAT_EQ(ring->growth, 10.0f);
	EXPECT_FLOAT_EQ(ring->rate, 0.5f);
	// Half as deep as wide
	EXPECT_FLOAT_EQ(ring->aspect, 0.5f);
	EXPECT_EQ(ring->cell, 0x31);
	EXPECT_EQ(ring->argb, 0x90FFFFFFu);
}

TEST(SharkRules, AWakeRingIsLongAlongTheWayTheSharkSwims)
{
	int32_t timer = 51;
	// Swimming towards +z
	auto ring = WakeRing(timer, {10, 0, 20}, std::numbers::pi_v<float> / 2.0f, 16);
	ASSERT_TRUE(ring.has_value());
	ring->age = 700;
	const auto corners = openblack::water_rings::Corners(*ring);
	// 20 along the way it swims, 10 across
	EXPECT_NEAR(std::abs(corners[1].z - corners[0].z), 20.0f, 1e-4f);
	EXPECT_NEAR(std::abs(corners[1].x - corners[0].x), 0.0f, 1e-4f);
	EXPECT_NEAR(std::abs(corners[2].x - corners[1].x), 10.0f, 1e-4f);
}

TEST(SharkRules, ItsFinsShowOnlyWhileAScriptWalksIt)
{
	const auto walking = LookOf(true);
	EXPECT_TRUE(walking.aboveSea);
	const auto still = LookOf(false);
	EXPECT_FALSE(still.aboveSea);
	// Under the sea it is always its own dark blue
	EXPECT_EQ(walking.underSea, glm::vec3(48.0f, 48.0f, 112.0f));
	EXPECT_EQ(still.underSea, glm::vec3(48.0f, 48.0f, 112.0f));
}
