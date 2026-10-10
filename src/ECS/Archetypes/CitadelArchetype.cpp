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

#include "ECS/Components/Construction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TempleExterior.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity CitadelArchetype::Create(const glm::vec3& position, PlayerNames playerOwner, float facing,
                                      const glm::mat4& rotation, const glm::vec3& size)
{
	return Create(position, playerOwner, facing, rotation, size, 1.0f);
}

entt::entity CitadelArchetype::Create(const glm::vec3& position, PlayerNames playerOwner, float facing,
                                      const glm::mat4& rotation, const glm::vec3& size, float built)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, rotation, size);
	registry.Assign<Temple>(entity, playerOwner);
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
	// It takes its worship sites' places; standing built, its player's towns are given their sites at once, and one
	// under construction gives them once it is finished
	if (Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().AddTemple(entity, facing, built >= 1.0f);
	}
	return entity;
}

entt::entity CitadelArchetype::CreatePlan(int32_t townId, const glm::vec3& position, PlayerNames playerOwner, float facing,
                                          const glm::mat4& rotation, const glm::vec3& size)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, rotation, size);
	registry.Assign<PlannedTemple>(entity, townId, playerOwner, facing);
	return entity;
}
