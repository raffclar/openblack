/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishFarmArchetype.h"

#include <cmath>

#include <array>
#include <limits>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/FishShoals.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity FishFarmArchetype::Create(const glm::vec3& position, uint32_t info)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();

	const float y = island.GetHeightAt(glm::vec2(position.x, position.z));
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	registry.Assign<Transform>(entity, glm::vec3(position.x, y, position.z), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& farm = registry.Assign<FishFarm>(entity);
	farm.info = info;
	// the nearest town of any tribe, in x and z, replaces the script's town
	float nearest = std::numeric_limits<float>::max();
	registry.Each<const Town, const Transform>([&](entt::entity town, const Town&, const Transform& transform) {
		const glm::vec2 delta(transform.position.x - position.x, transform.position.z - position.z);
		const float distance2 = glm::dot(delta, delta);
		if (distance2 < nearest)
		{
			nearest = distance2;
			farm.town = town;
		}
	});

	// Rings of radius 2, 4, ... < 50, 32 directions each: the first direction with sea (altitude 0) at two radii in a
	// row gives the shoal centre, at the second of them
	std::array<uint8_t, 32> seaCount {};
	for (float radius = 2.0f; radius < 50.0f; radius += 2.0f)
	{
		for (size_t i = 0; i < seaCount.size(); ++i)
		{
			const float angle = static_cast<float>(i) * glm::two_pi<float>() * 0.03125f;
			const glm::vec2 point(position.x + std::cos(angle) * radius, position.z + std::sin(angle) * radius);
			// with the sea flattening off during the search
			if (island.GetUnflattenedHeightAt(point) != 0.0f)
			{
				seaCount[i] = 0;
				continue;
			}
			if (++seaCount[i] == 2)
			{
				FishShoal shoal;
				ecs::InitFishShoal(shoal, glm::vec3(point.x, y, point.y));
				farm.shoal = shoal;
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fish farm at ({}, {}): shoal at ({}, {}, {})", position.x, position.z,
				                   shoal.centre.x, shoal.centre.y, shoal.centre.z);
				// the farm goes at the head of the fixed list of its own cell
				ecs::map_cells::InsertMapObject(entity);
				return entity;
			}
		}
	}
	ecs::map_cells::InsertMapObject(entity); // as above
	return entity;
}
