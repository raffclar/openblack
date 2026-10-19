/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <glm/gtx/euler_angles.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "Common/X87Angle.h"
#include "ECS/Archetypes/DeadTreeArchetype.h"

using openblack::ecs::archetypes::DeadTreeArchetype;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(DeadTreeLean, AnUprightTreeTurnsAsBefore)
{
	// With no lean, the land script's dead tree turns about the vertical as a dead tree that the game drops
	const auto rotation = DeadTreeArchetype::LandScriptRotation(0.0f, 0.7f, 0.0f);
	const auto upright = glm::mat3(glm::eulerAngleY(-0.7f));
	for (glm::length_t c = 0; c < 3; ++c)
	{
		for (glm::length_t r = 0; r < 3; ++r)
		{
			EXPECT_NEAR(rotation[c][r], upright[c][r], 1e-6f);
		}
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(DeadTreeLean, TheTreeLeansAboutXAndZ)
{
	// Values from the second dead tree of Land 1
	constexpr float k_X = 0.039619f;
	constexpr float k_Y = -0.000025f;
	constexpr float k_Z = 0.001211f;
	const auto rotation = DeadTreeArchetype::LandScriptRotation(k_X, k_Y, k_Z);
	const auto rows = openblack::x87::GameTurnYxz(k_Y, k_X, k_Z);
	for (glm::length_t c = 0; c < 3; ++c)
	{
		EXPECT_EQ(rotation[c], rows[static_cast<size_t>(c)]);
	}
	// The top of the tree leans: the up axis of the model moves along z by the sine of the x angle, in the negative direction
	EXPECT_NEAR(rotation[1].z, -std::sin(k_X), 1e-5f);
	EXPECT_LT(rotation[1].y, 1.0f);
}
