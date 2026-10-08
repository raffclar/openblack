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
#include "ECS/Components/Footpath.h"
#include "ECS/Footpaths.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "FileSystem/MemoryStream.h"
#include "GameThingSerializer.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::filesystem;

FotFile::FotFile(Game& game)
    : _game(game)
{
}

void FotFile::Load(const std::filesystem::path& path)
{
	// the footpath file's bytes from the byte cache, read afresh with every map load (as its map script), through a
	// memory stream (a copy: the stream owns its data)
	auto& blobs = Locator::resources::value().GetBlobs();
	blobs.Erase(resources::BlobId(path));
	auto stream = std::make_unique<filesystem::MemoryStream>(std::vector<uint8_t>(resources::LoadBlob(blobs, path)));
	serializer::GameThingSerializer serializer(*stream);
	auto footpathLinkSaves = serializer.DeserializeList<serializer::GameThingSerializer::FootpathLinkSave>();
	auto footpaths = serializer.DeserializeList<serializer::GameThingSerializer::Footpath>();
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();

	// Keep track of footpath entities in order to associate them to the footpath link saves later
	std::vector<ecs::components::Footpath::Id> footpathEntities;

	for (const auto& footpath : footpaths)
	{
		// One new footpath per saved footpath (the original pushes each at the list's head)
		const auto entity = ecs::footpaths::Create();
		auto& footpathEntt = registry.Get<ecs::components::Footpath>(entity);
		footpathEntt.nodes.reserve(footpath.nodes.size());
		for (const auto& node : footpath.nodes)
		{
			glm::vec3 position = glm::vec3 {
			    10.0f * node.coords.x / static_cast<float>(0xFFFF),
			    node.coords.altitude,
			    10.0f * node.coords.z / static_cast<float>(0xFFFF),
			};

			// This bit is mainly for visualization, it could be that using these offsets causes uses for path planning
			// if that is the case, this bit should be moved to rendering code
			position.y += island.GetHeightAt({position.x, position.z});

			// A saved node: its MapCoords, then a byte of flags
			const map_coords::MapCoords coords {static_cast<int32_t>(node.coords.x), static_cast<int32_t>(node.coords.z),
			                                    node.coords.altitude};
			footpathEntt.nodes.push_back({position, coords, node.unknown, footpathEntt.nextNodeId++});
		}
		// A saved footpath: the nodes, then its active flag
		footpathEntt.active = footpath.unknown != 0;
		footpathEntities.push_back(static_cast<ecs::components::Footpath::Id>(entity));
	}

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
		glm::vec3 position = glm::vec3 {
		    10.0f * save.coords.x / static_cast<float>(0xFFFF),
		    save.coords.altitude,
		    10.0f * save.coords.z / static_cast<float>(0xFFFF),
		};
		const auto entity = registry.Create();
		registry.Assign<ecs::components::FootpathLink>(entity, position, std::move(linkFootpathEntities));
		// Each link save is resolved as it is read: to the fixed building or the planned abode at its point, else
		// deleted
		const map_coords::MapCoords coords {static_cast<int32_t>(save.coords.x), static_cast<int32_t>(save.coords.z),
		                                    save.coords.altitude};
		ecs::footpaths::AttachLoadedLink(entity, coords);
	}
}
