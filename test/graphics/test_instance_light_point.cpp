/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Graphics/InstanceLightPoint.h"

using namespace openblack::graphics;

TEST(InstanceLightPoint, AMatrixThatPlacesAnObjectHasNoOffset)
{
	auto model = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 2.0f, -4.0f));
	model = glm::rotate(model, 0.7f, glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(1.5f));
	EXPECT_EQ(instance_light::LightOffset(model), glm::vec2(0.0f));
}

TEST(InstanceLightPoint, TheOffsetChangesOnlyTheUnusedValuesOfTheMatrix)
{
	auto model = glm::translate(glm::mat4(1.0f), glm::vec3(1796.0f, 30.0f, 2808.0f));
	const auto placed = model;
	instance_light::SetLightOffset(model, glm::vec2(-15.0f, 2.0f));
	EXPECT_EQ(instance_light::LightOffset(model), glm::vec2(-15.0f, 2.0f));
	// The shader sets the two values to 0 again, which gives the matrix that places the object
	model[1].w = 0.0f;
	model[2].w = 0.0f;
	EXPECT_EQ(model, placed);
	// The place, in the fourth column, does not change
	EXPECT_EQ(model[3], glm::vec4(1796.0f, 30.0f, 2808.0f, 1.0f));
}
