/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CitadelArchetype.h"

#include <algorithm>

#include <entt/fwd.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/mat3x3.hpp>

#include "ECS/Components/Construction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TempleExterior.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// The game's turn goes the other way round from the drawn one
glm::mat3 Rotation(float yAngle)
{
	return glm::mat3(glm::eulerAngleY(-yAngle));
}
} // namespace

entt::entity CitadelArchetype::Create(const glm::vec3& position, PlayerNames playerOwner, float yAngle, const glm::vec3& size)
{
	return Create(position, playerOwner, yAngle, size, 1.0f);
}

entt::entity CitadelArchetype::Create(const glm::vec3& position, PlayerNames playerOwner, float yAngle, const glm::vec3& size,
                                      float built)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	const auto rotation = Rotation(yAngle);
	registry.Assign<Transform>(entity, position, rotation, size);
	registry.Assign<Temple>(entity, Temple {.owner = playerOwner, .yAngle = yAngle});
	const auto meshId = entt::hashed_string("temple/b_first_temple_l3d");
	registry.Assign<Mesh>(entity, meshId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	// Its outside is blended for its player's alignment, each vertex then set on the land
	// Its model shows as much of it built as its heart is from the start
	registry.Assign<TempleExterior>(entity).drawnBuilt = std::clamp(built, 0.0f, 1.0f);
	registry.Assign<MorphWithTerrain>(entity);
	// Under construction until all of it is built
	if (built < 1.0f)
	{
		registry.Assign<BuildProgress>(entity, built < 0.0f ? 0.0f : built);
	}
	// The temple's heart, as it is made, puts its entrance where it is, turned as it is
	const auto entrance = registry.Create();
	registry.Assign<Transform>(entrance, position, rotation, size);
	registry.Assign<TempleEntrance>(entrance, entity);
	return entity;
}

entt::entity CitadelArchetype::CreatePlan(int32_t townId, const glm::vec3& position, PlayerNames playerOwner, float yAngle,
                                          const glm::vec3& size)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, Rotation(yAngle), size);
	registry.Assign<PlannedTemple>(entity, PlannedTemple {.townId = townId, .owner = playerOwner, .yAngle = yAngle});
	return entity;
}
