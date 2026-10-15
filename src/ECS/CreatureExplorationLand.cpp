/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureExplorationLand.h"

#include <algorithm>

#include <LNDFile.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/CreatureExploration.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
namespace explore = openblack::creature_exploration;

namespace
{
bool OnMapCell(glm::ivec2 cell)
{
	constexpr int32_t k_Cells = explore::k_RegionsPerSide * explore::k_CellsPerRegion;
	return cell.x >= 0 && cell.y >= 0 && cell.x < k_Cells && cell.y < k_Cells;
}

/// Whether a building of a town stands in a cell, by what the map keeps in the cell
bool TownAt(glm::ivec2 cell)
{
	if (!Locator::entitiesMap::has_value() || !OnMapCell(cell))
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& map = Locator::entitiesMap::value();
	const auto id = MapInterface::CellId {static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y)};
	const auto isAbode = [&registry](entt::entity thing) {
		return registry.Valid(thing) && registry.AllOf<components::Abode>(thing);
	};
	return std::ranges::any_of(map.GetFixedInGridCell(id), isAbode) ||
	       std::ranges::any_of(map.GetMobileInGridCell(id), isAbode);
}

std::optional<explore::CellLand> CellAt(glm::ivec2 cell)
{
	if (!OnMapCell(cell))
	{
		return std::nullopt;
	}
	const auto* land = Locator::terrainSystem::value().FindCell({static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y)});
	if (land == nullptr)
	{
		return std::nullopt;
	}
	return explore::CellLand {
	    .height = static_cast<float>(land->altitude) * LandIslandInterface::k_HeightUnit,
	    .water = land->properties.hasWater != 0,
	    .coast = land->properties.coastLine != 0,
	};
}

bool LandReady()
{
	return Locator::entitiesRegistry::has_value() && Locator::terrainSystem::has_value() &&
	       Locator::terrainSystem::value().IsLoaded();
}
} // namespace

void creature_exploration_land::MakeMap()
{
	if (!LandReady())
	{
		return;
	}
	Locator::entitiesRegistry::value().Context().explorationMap = explore::RegionMap::Make(CellAt, TownAt);
}

const explore::RegionMap* creature_exploration_land::Map()
{
	if (!LandReady())
	{
		return nullptr;
	}
	auto& context = Locator::entitiesRegistry::value().Context();
	if (!context.explorationMap.has_value())
	{
		MakeMap();
	}
	return context.explorationMap.has_value() ? &*context.explorationMap : nullptr;
}

void creature_exploration_land::Visit(entt::entity creature, glm::vec2 place)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* exploration = registry.TryGet<components::CreatureExploration>(creature);
	if (exploration == nullptr)
	{
		exploration = &registry.Assign<components::CreatureExploration>(creature);
	}
	explore::Visit(exploration->visited, explore::RegionOf(place));
}

std::optional<glm::vec2> creature_exploration_land::FindPlace(entt::entity creature, explore::Kind kind, glm::vec2 from,
                                                              bool preferUnvisited)
{
	const auto* map = Map();
	if (map == nullptr)
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* exploration = registry.TryGet<const components::CreatureExploration>(creature);
	const auto visited = exploration != nullptr ? exploration->visited : explore::Visited {};
	const auto region = explore::FindNearest(
	    *map, visited, {.kind = kind, .from = explore::RegionOf(from), .preferUnvisited = preferUnvisited});
	if (!region.has_value())
	{
		return std::nullopt;
	}
	return explore::PlaceOf(explore::CellFor(*map, *region, kind, TownAt));
}
