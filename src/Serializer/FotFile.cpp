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

/// The things that can take a link of the land's footpath file, with their places. The buildings that stand come first,
/// then the plans of the towns, as the game looks for them.
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

LinkHolders HoldersOf(const ecs::Registry& registry)
{
	using namespace ecs::components;
	LinkHolders holders;
	const auto add = [&](entt::entity entity) { holders.Add(registry, entity); };
	registry.Each<const Abode>([&](entt::entity entity, const Abode&) { add(entity); });
	registry.Each<const Field>([&](entt::entity entity, const Field&) { add(entity); });
	registry.Each<const FishFarm>([&](entt::entity entity, const FishFarm&) { add(entity); });
	registry.Each<const WorshipSite>([&](entt::entity entity, const WorshipSite&) { add(entity); });
	registry.Each<const BigForest>([&](entt::entity entity, const BigForest&) { add(entity); });
	registry.Each<const Temple>([&](entt::entity entity, const Temple&) { add(entity); });
	// A plan takes a link only when no building that stands takes it: the plans of the town made last first
	std::ranges::for_each(ecs::player_towns::AllPlansInOrder(registry), add);
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

	// The buildings and the plans are the same for each link: the links only change their paths
	const auto holders = HoldersOf(registry);
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
