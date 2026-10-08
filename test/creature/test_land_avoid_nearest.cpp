/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// land_avoid::NearestValid, where a creature put back on the land stands: on a mask given as a function, and on the
// walkable mask itself

#include <cmath>

#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/LandAvoid.h"
#include "3D/LandAvoidState.h"
#include "ECS/Systems/LandAvoidSystemInterface.h"
#include "Locator.h"

using namespace openblack;

TEST(LandAvoidNearest, ThePointItselfWhenValid)
{
	int asked = 0;
	const auto found = land_avoid::NearestValid({10.0f, 20.0f}, 7.1f, 100.0f, [&asked](glm::vec3 point, float radius) {
		++asked;
		EXPECT_FLOAT_EQ(radius, 7.1f);
		EXPECT_FLOAT_EQ(point.y, 0.0f);
		return true;
	});
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(*found, glm::vec2(10.0f, 20.0f));
	EXPECT_EQ(asked, 1);
}

TEST(LandAvoidNearest, TheFirstOfSixteenDirectionsAtTheNearestStep)
{
	// valid only from 2 away on: the search goes out in steps of 0.5, and at each step from +x round towards +z
	const auto valid = [](glm::vec3 point, float) { return glm::length(glm::vec2(point.x, point.z)) >= 1.99f; };
	const auto found = land_avoid::NearestValid({0.0f, 0.0f}, 1.0f, 10.0f, valid);
	ASSERT_TRUE(found.has_value());
	EXPECT_NEAR(found->x, 2.0f, 1e-5f);
	EXPECT_NEAR(found->y, 0.0f, 1e-5f);

	// only the half with negative x: the first direction of the 16 there is a half turn round
	const auto behind = [](glm::vec3 point, float) { return point.x < -0.5f && std::abs(point.z) < 1e-3f; };
	const auto back = land_avoid::NearestValid({0.0f, 0.0f}, 1.0f, 10.0f, behind);
	ASSERT_TRUE(back.has_value());
	EXPECT_NEAR(back->x, -1.0f, 1e-5f);
	EXPECT_NEAR(back->y, 0.0f, 1e-4f);
}

TEST(LandAvoidNearest, NothingWithinTheDistance)
{
	int asked = 0;
	const auto found = land_avoid::NearestValid({0.0f, 0.0f}, 1.0f, 2.0f, [&asked](glm::vec3, float) {
		++asked;
		return false;
	});
	EXPECT_FALSE(found.has_value());
	// the point, then 16 directions at 0.5, 1, 1.5 and 2
	EXPECT_EQ(asked, 1 + (16 * 4));
}

TEST(LandAvoidNearest, OnTheWalkableMask)
{
	// a 32 x 32 mask, land but for an avoided block of cells 10..19 in x and z
	auto& state = Locator::landAvoidSystem::value().GetState();
	const auto previous = state;
	state.size = 32;
	state.avoid.assign(32 * 32, land_avoid::k_Land);
	for (int z = 10; z < 20; ++z)
	{
		for (int x = 10; x < 20; ++x)
		{
			state.avoid[static_cast<size_t>(z * 32 + x)] = land_avoid::k_Avoid;
		}
	}
	// in the middle of the block: the nearest valid point is the first one clear of the block by the radius
	const auto found = land_avoid::NearestValid({150.0f, 150.0f}, land_avoid::k_CreatureRadius, 200.0f);
	ASSERT_TRUE(found.has_value());
	EXPECT_TRUE(land_avoid::IsPosValid({found->x, 0.0f, found->y}));
	EXPECT_FALSE(land_avoid::IsPosValid({150.0f, 0.0f, 150.0f}));
	// along +x, the first of the directions: the first step out that is valid, so the step before it is not
	EXPECT_NEAR(found->y, 150.0f, 1e-4f);
	EXPECT_GT(found->x, 150.0f);
	EXPECT_FALSE(land_avoid::IsPosValid({found->x - 0.5f, 0.0f, found->y}));
	state = previous;
}
