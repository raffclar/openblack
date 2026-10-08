/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// ecs::DrawnPosition is DrawnModel's translation for every source (HandDrawPose, PhysicsDrawPose, DrawPosition with
// its shear, Transform), and the priority between them

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/PhysicsDrawPose.h"
#include "ECS/Components/Transform.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
glm::mat3 Turn(float angle)
{
	return glm::mat3(glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.3f, 1.0f, 0.2f)));
}

void ExpectSame(const Registry& registry, entt::entity entity, const glm::vec3& expected)
{
	const auto position = DrawnPosition(registry, entity);
	const auto model = glm::vec3(DrawnModel(registry, entity)[3]);
	EXPECT_EQ(position, model);
	EXPECT_EQ(position, expected);
}
} // namespace

TEST(DrawnPosition, EverySourceIsDrawnModelsTranslation)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(1.0f, 2.0f, 3.0f), Turn(0.4f), glm::vec3(1.5f));
	ExpectSame(registry, entity, glm::vec3(1.0f, 2.0f, 3.0f));

	auto& draw = registry.Assign<DrawPosition>(entity);
	draw.position = glm::vec3(4.0f, 5.0f, 6.0f);
	draw.rotation = Turn(1.1f);
	draw.shearX = 0.25f;
	draw.shearZ = -0.5f;
	ExpectSame(registry, entity, glm::vec3(4.0f, 5.0f, 6.0f));

	registry.Assign<PhysicsDrawPose>(entity, glm::vec3(7.0f, 8.0f, 9.0f), Turn(2.0f));
	ExpectSame(registry, entity, glm::vec3(7.0f, 8.0f, 9.0f));

	registry.Assign<HandDrawPose>(entity, glm::vec3(10.0f, 11.0f, 12.0f), Turn(-0.7f));
	ExpectSame(registry, entity, glm::vec3(10.0f, 11.0f, 12.0f));
}
