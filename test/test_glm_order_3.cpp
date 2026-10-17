/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's sum orders and steps in the camera's land drag and the particle rules. Each expected value is written out
// in the order of the game's instructions. The eyes, the hair step, the flock falloff and the set length have oracle
// data (test/oracle/test_oracle_glm_order_3.cpp).

#include <cmath>
#include <cstdint>

#include <bit>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/CameraPan.h"
#include "Particles/ParticleFlocking.h"

using namespace openblack;

namespace
{
void ExpectSameBits(const glm::vec3& ours, const glm::vec3& expected)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(ours.x), std::bit_cast<uint32_t>(expected.x));
	EXPECT_EQ(std::bit_cast<uint32_t>(ours.y), std::bit_cast<uint32_t>(expected.y));
	EXPECT_EQ(std::bit_cast<uint32_t>(ours.z), std::bit_cast<uint32_t>(expected.z));
}
} // namespace

TEST(GlmOrder3, TheLandDragMovesBetweenTheTwoHitsFromTheCamera)
{
	const camera_pan::GripPlane plane {.normal = {0.0f, 1.0f, 0.0f}, .distance = 3.7f};
	const glm::vec3 origin {1000.3f, 251.1f, 2000.7f};
	const glm::vec3 focus {1100.9f, 3.7f, 2100.1f};
	const float distance = 180.3f;
	const glm::vec3 now {0.3137f, -0.9013f, 0.2911f};
	const glm::vec3 atGrip {0.3311f, -0.8977f, 0.2789f};
	const auto place = camera_pan::Pan(plane, origin, focus, distance, now, atGrip, {900, 700}, {100, 100});
	ASSERT_TRUE(place.has_value());

	// The normal times the line now, y then z then x; the line at the grip, z then y then x
	const float dotNow = (plane.normal.y * now.y + plane.normal.z * now.z) + plane.normal.x * now.x;
	const float dotGrip = (plane.normal.z * atGrip.z + plane.normal.y * atGrip.y) + plane.normal.x * atGrip.x;
	const float toPlane =
	    plane.distance - ((plane.normal.z * origin.z + plane.normal.y * origin.y) + plane.normal.x * origin.x);
	const float alongNow = toPlane / dotNow;
	const float alongGrip = toPlane / dotGrip;
	// Both hits are points from the camera, and the move is their difference
	const glm::vec3 hitNow {now.x * alongNow + origin.x, now.y * alongNow + origin.y, now.z * alongNow + origin.z};
	const glm::vec3 hitGrip {atGrip.x * alongGrip + origin.x, atGrip.y * alongGrip + origin.y, atGrip.z * alongGrip + origin.z};
	const glm::vec3 moved = hitNow - hitGrip;
	const glm::vec3 newFocus = focus - moved;
	const glm::vec3 back = (origin - moved) - newFocus;
	// The way back is made one long with its squares added z, x, then y
	const float share = 1.0f / std::sqrt((back.z * back.z + back.x * back.x) + back.y * back.y);
	const glm::vec3 unit {back.x * share, back.y * share, back.z * share};
	ExpectSameBits(place->focus, newFocus);
	ExpectSameBits(place->origin,
	               {unit.x * distance + newFocus.x, unit.y * distance + newFocus.y, unit.z * distance + newFocus.z});
}

TEST(GlmOrder3, TheFlockLimitsTestTheLengthOrItsSquare)
{
	// The acceleration limit tests the length, the speed limit the squares; both then set the length z first
	const glm::vec3 v {3.1f, -4.7f, 2.3f};
	const float squared = (v.z * v.z + v.y * v.y) + v.x * v.x;
	const float share = 2.5f / std::sqrt(squared);
	const glm::vec3 limited {v.x * share, v.y * share, v.z * share};
	ExpectSameBits(particles::flocking::LimitedLength(v, 2.5f), limited);
	ExpectSameBits(particles::flocking::LimitedSquaredLength(v, 2.5f), limited);
	ExpectSameBits(particles::flocking::LimitedLength(v, 10.0f), v);
	ExpectSameBits(particles::flocking::LimitedSquaredLength(glm::vec3(0.0f), 0.0f), glm::vec3(0.0f));
}

TEST(GlmOrder3, AFlockDirectionTakesOneOverTheLength)
{
	const glm::vec3 offset {7.3f, -1.9f, 3.3f};
	float distance = 0.0f;
	const auto unit = particles::flocking::Direction<x87::SumOrder::Xzy>(offset, distance);
	const float length = std::sqrt((offset.x * offset.x + offset.z * offset.z) + offset.y * offset.y);
	EXPECT_EQ(std::bit_cast<uint32_t>(distance), std::bit_cast<uint32_t>(length));
	const float share = 1.0f / length;
	ExpectSameBits(unit, {offset.x * share, offset.y * share, offset.z * share});
	// A zero offset has no length and no way
	ExpectSameBits(particles::flocking::Direction<x87::SumOrder::Zyx>(glm::vec3(0.0f), distance), glm::vec3(0.0f));
	EXPECT_EQ(distance, 0.0f);
}
