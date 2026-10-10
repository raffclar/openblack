/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Where a thing in the physics is drawn: from its body's pose while it moves, nowhere once it has sunk wholly under the
// sea

#include <glm/ext/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/PhysicsDrawMatrix.h"
#include "ECS/Components/Physics.h"

using openblack::ecs::components::PhysicsDrawPose;
using openblack::physics_draw::ModelMatrix;

namespace
{
const glm::mat4 k_Standing = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 2.0f, -4.0f));
} // namespace

TEST(PhysicsDrawMatrix, AThingOutOfThePhysicsIsDrawnAsItStands)
{
	const auto drawn = ModelMatrix(k_Standing, nullptr);
	ASSERT_TRUE(drawn.has_value());
	EXPECT_EQ(*drawn, k_Standing);
}

TEST(PhysicsDrawMatrix, AMovingBodyIsDrawnFromItsPose)
{
	const PhysicsDrawPose pose {.axes = glm::mat3(2.0f), .origin = {1.0f, -0.5f, 3.0f}, .underSea = false};
	const auto drawn = ModelMatrix(k_Standing, &pose);
	ASSERT_TRUE(drawn.has_value());
	EXPECT_EQ((*drawn)[3], glm::vec4(1.0f, -0.5f, 3.0f, 1.0f));
	EXPECT_EQ((*drawn)[0], glm::vec4(2.0f, 0.0f, 0.0f, 0.0f));
}

TEST(PhysicsDrawMatrix, ABodySunkWhollyUnderTheSeaIsDrawnNowhere)
{
	// Nothing is left to sway, bend or reflect: a hidden matrix later leant by a tree's sway would draw its vertices out
	// at the horizon
	const PhysicsDrawPose pose {.axes = glm::mat3(1.0f), .origin = {1.0f, -6.0f, 3.0f}, .underSea = true};
	EXPECT_FALSE(ModelMatrix(k_Standing, &pose).has_value());
}
