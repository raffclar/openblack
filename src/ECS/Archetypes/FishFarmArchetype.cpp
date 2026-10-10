/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishFarmArchetype.h"

#include <limits>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// The middle of a map cell, in 1/65536 of a cell
constexpr int32_t k_HalfCell = 0x8000;

/// The town nearest a point across the land, of any player or none
entt::entity NearestTown(glm::vec2 point)
{
	entt::entity nearest = entt::null;
	float best = std::numeric_limits<float>::max();
	Locator::entitiesRegistry::value().Each<const Town, const Transform>(
	    [&](entt::entity entity, const Town&, const Transform& transform) {
		    const float distance = glm::distance(glm::xz(transform.position), point);
		    if (distance < best)
		    {
			    best = distance;
			    nearest = entity;
		    }
	    });
	return nearest;
}

/// Where the land's own altitude is that of the sea bed, with nothing raised above it
bool IsOpenSea(const LandIslandInterface& island, glm::vec2 point)
{
	const auto x = map_coords::FtoL(point.x * 65536.0f * 0.1f);
	const auto z = map_coords::FtoL(point.y * 65536.0f * 0.1f);
	return island.GetAltitude(x, z, false) == 0.0;
}
} // namespace

entt::entity FishFarmArchetype::Create(const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();

	// It stands in the middle of its cell, on the land there
	auto coords = map_coords::FromMetres(glm::xz(position));
	coords.x = static_cast<int32_t>((static_cast<uint32_t>(coords.x) & 0xFFFF0000u) | k_HalfCell);
	coords.z = static_cast<int32_t>((static_cast<uint32_t>(coords.z) & 0xFFFF0000u) | k_HalfCell);
	const auto centre = map_coords::ToWorld(island, coords);

	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, centre, glm::mat3(1.0f), glm::vec3(1.0f));

	FishFarm farm;
	farm.place = {coords.x, coords.z};
	// Whatever town a script names, it is the nearest town's
	farm.town = NearestTown(glm::xz(centre));
	farm.fish = fish_farm::Full(Locator::fishFarmSystem::value().GetType());
	if (const auto sea =
	        fish_shoal::FindCentre(glm::xz(centre), [&island](glm::vec2 point) { return IsOpenSea(island, point); }))
	{
		auto& random = Locator::gameRandom::value();
		farm.shoal = fish_shoal::MakeShoal(glm::vec3(sea->x, centre.y, sea->y),
		                                   [&random](float a, float b) { return random.CrtRandom(a, b); });
	}
	registry.Assign<FishFarm>(entity, std::move(farm));
	return entity;
}
