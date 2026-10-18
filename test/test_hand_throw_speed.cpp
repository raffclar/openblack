/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <cmath>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Particles/ParticleMiracleMaths.h"

using namespace openblack::particles;

// The speed of the hand that throws a fireball adds its squares from z to x. With these parts the order from x to z
// gives a speed one step larger.
TEST(HandThrowSpeed, AddsTheSquaresFromZToX)
{
	const glm::vec3 hand {-106.26761627197266f, 198.2579345703125f, -11.894597053527832f};
	const float zyx = std::sqrt(((hand.z * hand.z) + (hand.y * hand.y)) + (hand.x * hand.x));
	const float xyz = std::sqrt(((hand.x * hand.x) + (hand.y * hand.y)) + (hand.z * hand.z));
	ASSERT_NE(std::bit_cast<uint32_t>(zyx), std::bit_cast<uint32_t>(xyz));
	EXPECT_EQ(std::bit_cast<uint32_t>(maths::HandThrow(hand).speed),
	          std::bit_cast<uint32_t>(maths::ThrowSpeedFromHand(zyx)));
}
