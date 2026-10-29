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
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFind.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "Locator.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// The middle of a map cell, in 1/65536 of a cell
constexpr int32_t k_HalfCell = 0x8000;

/// The town nearest a map position by the game's map distance, of any player or none. Each player's town list in turn
/// and the neutral list last: the first met wins a tie.
entt::entity NearestTown(const map_coords::MapCoords& from)
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::vector<ecs::script_find::Candidate> ordered;
	for (const auto town : ecs::player_towns::AllInOrder(registry))
	{
		if (const auto* transform = registry.TryGet<const Transform>(town))
		{
			ordered.push_back({.entity = town, .at = map_coords::FromMetres(glm::xz(transform->position))});
		}
	}
	return ecs::script_find::FindNearestTown(from, std::numeric_limits<float>::max(), ordered);
}

/// Where the land's own altitude is that of the sea bed, with nothing raised above it
bool IsOpenSea(const LandIslandInterface& island, glm::vec2 point)
{
	const auto x = map_coords::FtoL(point.x * 65536.0f * 0.1f);
	const auto z = map_coords::FtoL(point.y * 65536.0f * 0.1f);
	return island.GetAltitude(x, z, false) == 0.0f;
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
	// It takes its place in the order of the things made
	TakeThingMadePlace();
	registry.Assign<Transform>(entity, centre, glm::mat3(1.0f), glm::vec3(1.0f));
	ecs::thing_map_place::Keep(registry, entity, {.x = coords.x, .z = coords.z});

	FishFarm farm;
	farm.place = {coords.x, coords.z};
	// Whatever town a script names, it is the nearest town's
	farm.town = NearestTown(coords);
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

entt::entity FishFarmArchetype::CreateLoaded(const glm::vec3& position, entt::entity town, float fish)
{
	const auto entity = Create(position);
	auto& farm = Locator::entitiesRegistry::value().Get<FishFarm>(entity);
	farm.town = town;
	farm.fish = fish;
	return entity;
}
