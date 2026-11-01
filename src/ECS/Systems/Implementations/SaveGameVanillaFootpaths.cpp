/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <unordered_map>
#include <vector>

#include <VanillaWorld.h>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/VanillaFootpathImport.h"
#include "Locator.h"
#include "SaveGameSystem.h"
#include "SaveGameVanillaPlaces.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace footpath_import = openblack::ecs::vanilla_footpath_import;
using openblack::ecs::systems::vanilla_import::EntityOf;

namespace
{

std::shared_ptr<spdlog::logger> ImportLog()
{
	return spdlog::get("game");
}

/// The footpath of openblack with the nodes and the word of a footpath of the file. Each node is on the land at its
/// place, raised by its height.
Footpath FootpathOf(const footpath_import::ImportedFootpath& file)
{
	Footpath footpath {.walkable = file.walkable != 0};
	footpath.nodes.reserve(file.nodes.size());
	for (const auto& node : file.nodes)
	{
		float landHeight = 0.0f;
		if (Locator::terrainSystem::has_value())
		{
			const map_coords::MapCoords coords {.x = node.coords.x, .z = node.coords.z, .altitude = 0.0f};
			landHeight = Locator::terrainSystem::value().GetHeightAt(map_coords::ToMetres(coords));
		}
		footpath.nodes.push_back(footpath_import::NodeOf(node, landHeight));
	}
	return footpath;
}

} // namespace

void SaveGameSystem::ImportVanillaFootpaths(const gamesave::VanillaWorld& world, const VanillaMade& made,
                                            const VanillaFootpathWalks& walks)
{
	if (!Locator::footpathSystem::has_value())
	{
		return;
	}
	const auto imported = footpath_import::ReadFootpaths(world);
	for (const auto& why : imported.skipped)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The import does not take {}", why);
	}
	if (imported.finders != 0)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The import does not take the {} searches for footpaths of the file", imported.finders);
	}
	auto& registry = Locator::entitiesRegistry::value();

	std::vector<Footpath> footpaths;
	footpaths.reserve(imported.paths.size());
	std::unordered_map<uint32_t, size_t> places;
	for (const auto& file : imported.paths)
	{
		places.emplace(file.number, footpaths.size());
		footpaths.push_back(FootpathOf(file));
	}
	const auto entities = Locator::footpathSystem::value().PutLoadedFootpaths(footpaths);
	const auto footpathEntity = [&places, &entities](uint32_t number) {
		const auto found = places.find(number);
		return found != places.end() ? entities.at(found->second) : entt::null;
	};

	// Each thing of the file takes its link, with its footpaths in the order of the file
	uint32_t linked = 0;
	for (const auto& link : imported.links)
	{
		const auto holder = EntityOf(made, link.holder);
		if (holder == entt::null || !registry.Valid(holder))
		{
			SPDLOG_LOGGER_WARN(ImportLog(),
			                   "The thing {} of the link {} of the file is not made, so its footpaths lead from no thing",
			                   link.holder, link.number);
			continue;
		}
		std::vector<Footpath::Id> ids;
		ids.reserve(link.footpaths.size());
		for (const auto number : link.footpaths)
		{
			if (const auto entity = footpathEntity(number); entity != entt::null)
			{
				ids.push_back(static_cast<Footpath::Id>(entity));
			}
		}
		const auto* transform = registry.TryGet<const Transform>(holder);
		registry.AssignOrReplace<FootpathLink>(holder, transform != nullptr ? transform->position : glm::vec3 {0.0f},
		                                       std::move(ids));
		++linked;
	}

	// Each villager that walks a footpath in the file goes on along it: the same footpath and node, the same way, and
	// the same footpath after it
	uint32_t walking = 0;
	for (const auto& [villager, file] : walks)
	{
		const auto footpath = footpathEntity(file.footpath);
		if (footpath == entt::null)
		{
			SPDLOG_LOGGER_WARN(ImportLog(), "The footpath {} of a walk of the file is not made", file.footpath);
			continue;
		}
		const auto& path = imported.paths.at(places.at(file.footpath));
		registry.AssignOrReplace<FootpathWalk>(villager, FootpathWalk {.footpath = footpath,
		                                                               .towardsStart = file.towardsStart,
		                                                               .node = footpath_import::NodeIndexOf(path, file.node),
		                                                               .then = footpathEntity(file.next)});
		++walking;
	}
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(ImportLog(), "Imported {} footpaths with {} links and {} walks of the game's file", footpaths.size(),
	                   linked, walking);
}
