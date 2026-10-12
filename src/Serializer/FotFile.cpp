/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FotFile.h"

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameThingSerializer.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::filesystem;

FotFile::FotFile(Game& game)
    : _game(game)
{
}

namespace
{

/// A link holds the paths of the building standing within this of it (the square of 0.05 m)
constexpr float k_LinkReachSq = 0.0025f;

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

/// The building a link of the land's footpath file belongs to: one standing at its place, else one a town plans to
/// build there; none when nothing is there, and the link is then dropped
entt::entity BuildingAt(const ecs::Registry& registry, const map_coords::MapCoords& place)
{
	using namespace ecs::components;
	entt::entity found = entt::null;
	const auto consider = [&](entt::entity entity) {
		if (found == entt::null && registry.AllOf<Transform>(entity) &&
		    gutils::GetMetresDistanceSq(PlaceOf(registry, entity), place) < k_LinkReachSq)
		{
			found = entity;
		}
	};
	registry.Each<const Abode>([&](entt::entity entity, const Abode&) { consider(entity); });
	registry.Each<const Field>([&](entt::entity entity, const Field&) { consider(entity); });
	registry.Each<const FishFarm>([&](entt::entity entity, const FishFarm&) { consider(entity); });
	registry.Each<const WorshipSite>([&](entt::entity entity, const WorshipSite&) { consider(entity); });
	registry.Each<const BigForest>([&](entt::entity entity, const BigForest&) { consider(entity); });
	registry.Each<const Temple>([&](entt::entity entity, const Temple&) { consider(entity); });
	registry.Each<const PlannedAbode>([&](entt::entity entity, const PlannedAbode&) { consider(entity); });
	registry.Each<const PlannedTemple>([&](entt::entity entity, const PlannedTemple&) { consider(entity); });
	return found;
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
			footpathEntt.nodes.push_back({.position = position, .place = {coords.x, coords.z}});
		}
		footpathEntities.push_back(static_cast<ecs::components::Footpath::Id>(entity));
	}

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
		const auto building = BuildingAt(registry, coords);
		if (building == entt::null)
		{
			++dropped;
			continue;
		}
		const auto ground = map_coords::ToMetres(coords);
		const glm::vec3 position {ground.x, coords.altitude + island.GetHeightAt(ground), ground.y};
		registry.AssignOrReplace<ecs::components::FootpathLink>(building, position, std::move(linkFootpathEntities));
		++linked;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Footpaths: {} paths, {} links on buildings, {} links with no building",
	                   footpathEntities.size(), linked, dropped);
}
