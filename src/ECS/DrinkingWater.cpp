/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DrinkingWater.h"

#include <algorithm>
#include <vector>

#include <LNDFile.h>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/DrinkingWater.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ShepherdRules.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using map_coords::MapCoords;

std::optional<MapCoords> drinking_water::Find(const MapCoords& from, float reach)
{
	if (!Locator::terrainSystem::has_value() || !Locator::terrainSystem::value().IsLoaded() ||
	    !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& land = Locator::terrainSystem::value();
	auto& registry = Locator::entitiesRegistry::value();
	// The rivers newest first, each from its first point to its last
	std::vector<std::pair<components::Stream::Id, entt::entity>> streams(registry.Context().streams.begin(),
	                                                                     registry.Context().streams.end());
	std::ranges::sort(streams, [](const auto& a, const auto& b) { return a.first > b.first; });
	std::vector<MapCoords> points;
	for (const auto& [id, entity] : streams)
	{
		if (const auto* stream = registry.TryGet<const components::Stream>(entity))
		{
			for (const auto& point : stream->points)
			{
				points.push_back(MapCoords {map_coords::ToFixed(point.x), map_coords::ToFixed(point.z), 0.0f});
			}
		}
	}
	const shepherd_rules::River river {
	    .points = points,
	    .groundHeight = [&land](const MapCoords& at) { return land.GetHeightAt(map_coords::ToMetres(at)); },
	};
	const auto isCoast = [&land](const MapCoords& at) {
		const auto* cell = land.FindCell(glm::u16vec2(map_coords::CellX(at), map_coords::CellZ(at)));
		return cell != nullptr && cell->properties.hasWater == 0 && cell->properties.coastLine != 0;
	};
	return shepherd_rules::NearestDrinkingWater(from, river, isCoast, reach);
}

void drinking_water::FindFor(entt::entity building, float reach)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const components::Transform>(building);
	if (transform == nullptr)
	{
		return;
	}
	registry.AssignOrReplace<components::DrinkingWater>(
	    building, components::DrinkingWater {.place = Find(map_coords::FromMetres(glm::xz(transform->position)), reach)});
}
