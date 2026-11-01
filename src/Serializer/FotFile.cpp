/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FotFile.h"

#include <algorithm>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/FootpathRules.h"
#include "ECS/Map.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameThingSerializer.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::filesystem;

namespace
{

/// The bit of a node's flags that says a plan laid it
constexpr uint8_t k_PlannedNode = 0x2;

map_coords::MapCoords ToMapCoords(const serializer::MapCoords& coords)
{
	return {.x = static_cast<int32_t>(coords.x), .z = static_cast<int32_t>(coords.z), .altitude = coords.altitude};
}

/// Where a building stands, as the land's footpath file names it
map_coords::MapCoords PlaceOf(const ecs::Registry& registry, entt::entity entity)
{
	if (const auto* farm = registry.TryGet<const ecs::components::FishFarm>(entity))
	{
		return {.x = farm->place.x, .z = farm->place.y, .altitude = 0.0f};
	}
	const auto& position = registry.Get<const ecs::components::Transform>(entity).position;
	return {.x = map_coords::ToFixed(position.x), .z = map_coords::ToFixed(position.z), .altitude = 0.0f};
}

/// The things that can take a link of the land's footpath file, with their places, in the order that the game looks at
/// them
struct LinkHolders
{
	std::vector<entt::entity> things;
	std::vector<glm::ivec2> places;

	void Add(const ecs::Registry& registry, entt::entity entity)
	{
		if (!registry.AllOf<ecs::components::Transform>(entity))
		{
			return;
		}
		const auto place = PlaceOf(registry, entity);
		things.push_back(entity);
		places.emplace_back(place.x, place.z);
	}
};

/// Whether a building can keep the paths of the land's footpath file
bool CanHoldLink(const ecs::Registry& registry, entt::entity entity)
{
	using namespace ecs::components;
	return registry.AnyOf<Abode, Field, FishFarm, WorshipSite, BigForest, Temple>(entity);
}

/// The things that can take a link at a place. First the buildings filed in the map cell of the place, in the order
/// that a search of the cell meets them. Then the plans of the towns. A building that only stands near the place, and
/// is not in its cell, does not take the link.
LinkHolders HoldersAt(const ecs::Registry& registry, glm::ivec2 place, std::span<const entt::entity> plans)
{
	LinkHolders holders;
	if (Locator::entitiesMap::has_value())
	{
		const auto cell = ecs::MapInterface::CellId(ecs::footpath_rules::FileLinkCell(place));
		for (const auto entity : Locator::entitiesMap::value().GetFixedInGridCell(cell))
		{
			if (registry.Valid(entity) && CanHoldLink(registry, entity))
			{
				holders.Add(registry, entity);
			}
		}
	}
	for (const auto plan : plans)
	{
		holders.Add(registry, plan);
	}
	return holders;
}

} // namespace

void FotFile::Load(const std::filesystem::path& path)
{
	auto stream = Locator::filesystem::value().Open(path, Stream::Mode::Read);
	serializer::GameThingSerializer serializer(*stream);
	auto footpathLinkSaves = serializer.DeserializeList<serializer::GameThingSerializer::FootpathLinkSave>();
	auto footpaths = serializer.DeserializeList<serializer::GameThingSerializer::Footpath>();
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();

	// Keep track of footpath entities in order to associate them to the footpath link saves later
	std::vector<ecs::components::Footpath::Id> footpathEntities;

	for (const auto& footpath : footpaths)
	{
		const auto entity = registry.Create();
		auto& footpathEntt = registry.Assign<ecs::components::Footpath>(entity);
		footpathEntt.walkable = footpath.unknown != 0;
		footpathEntt.nodes.reserve(footpath.nodes.size());
		for (const auto& node : footpath.nodes)
		{
			const auto coords = ToMapCoords(node.coords);
			// Drawn on the land, raised by the node's height above it
			const auto ground = map_coords::ToMetres(coords);
			const glm::vec3 position {ground.x, coords.altitude + island.GetHeightAt(ground), ground.y};
			// The file's nodes keep whether a plan laid them; none of them is hidden or a marker
			footpathEntt.nodes.push_back(
			    {.position = position, .place = {coords.x, coords.z}, .planned = (node.unknown & k_PlannedNode) != 0});
		}
		footpathEntities.push_back(static_cast<ecs::components::Footpath::Id>(entity));
	}

	// The plans are the same for each link: the links only change their paths
	const auto plans = ecs::player_towns::AllPlansInOrder(registry);
	uint32_t linked = 0;
	uint32_t dropped = 0;
	for (const auto& save : footpathLinkSaves)
	{
		std::vector<ecs::components::Footpath::Id> linkFootpathEntities;
		for (const auto& footpath : save.link.footpaths)
		{
			// Save links have duplicates to entries in footpaths
			const auto footpathListItr = std::find(footpaths.cbegin(), footpaths.cend(), footpath);
			assert(footpathListItr != footpaths.cend());
			const auto footpathIndex = std::distance(footpaths.cbegin(), footpathListItr);
			linkFootpathEntities.push_back(footpathEntities[footpathIndex]);
		}
		// The link goes to the building standing at its place, replacing any it had
		const auto coords = ToMapCoords(save.coords);
		const auto holders = HoldersAt(registry, {coords.x, coords.z}, plans);
		const auto holder = ecs::footpath_rules::FileLinkHolder({coords.x, coords.z}, holders.places);
		if (!holder.has_value())
		{
			++dropped;
			continue;
		}
		const auto building = holders.things[*holder];
		const auto ground = map_coords::ToMetres(coords);
		const glm::vec3 position {ground.x, coords.altitude + island.GetHeightAt(ground), ground.y};
		registry.AssignOrReplace<ecs::components::FootpathLink>(building, position, std::move(linkFootpathEntities));
		++linked;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Footpaths: {} paths, {} links on buildings, {} links with no building",
	                   footpathEntities.size(), linked, dropped);
}
