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
#include <iterator>
#include <span>
#include <utility>
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
			footpathEntt.nodes.push_back({.position = position,
			                              .place = {coords.x, coords.z},
			                              .altitude = coords.altitude,
			                              .planned = (node.unknown & k_PlannedNode) != 0});
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

namespace
{

/// Writes the records of a footpath file and keeps the check sum of the bytes: for each value, its first byte and its
/// size
class FotWriter
{
public:
	template <typename T>
	void Put(const T& value)
	{
		const auto* first = reinterpret_cast<const uint8_t*>(&value);
		_bytes.insert(_bytes.end(), first, first + sizeof(T));
		_sum += static_cast<uint32_t>(*first) + static_cast<uint32_t>(sizeof(T));
	}

	/// The start of a new record: its number, its kind, its player, the check sum and the two fields of every thing
	void StartRecord(serializer::GameThingType type)
	{
		Put(++_records);
		Put(type);
		Put(uint32_t {0});
		Put(_sum);
		Put(uint32_t {0});
		Put(uint8_t {0});
	}

	/// The number of the record that the writer wrote last
	[[nodiscard]] uint32_t LastRecord() const { return _records; }

	[[nodiscard]] std::vector<uint8_t> Take() { return std::move(_bytes); }

private:
	std::vector<uint8_t> _bytes;
	uint32_t _sum {0};
	uint32_t _records {0};
};

serializer::MapCoords CoordsOf(glm::ivec2 place, float altitude)
{
	return {.x = static_cast<uint32_t>(place.x), .z = static_cast<uint32_t>(place.y), .altitude = altitude};
}

/// A footpath, as a full record the first time and as its number after that
void PutPath(FotWriter& writer, std::span<const fot_file::Path> paths, std::vector<uint32_t>& written, size_t index)
{
	if (written.at(index) != 0)
	{
		writer.Put(written.at(index));
		return;
	}
	const auto& path = paths[index];
	writer.StartRecord(serializer::GameThingType::Footpath);
	written.at(index) = writer.LastRecord();
	writer.Put(static_cast<uint32_t>(path.nodes.size()));
	for (const auto& node : path.nodes)
	{
		writer.StartRecord(serializer::GameThingType::FootpathNode);
		writer.Put(CoordsOf(node.place, node.altitude));
		writer.Put(node.flags);
	}
	writer.Put(static_cast<uint32_t>(path.walkable ? 1 : 0));
}

} // namespace

std::vector<uint8_t> openblack::fot_file::Write(std::span<const LinkSave> links, std::span<const Path> paths)
{
	FotWriter writer;
	std::vector<uint32_t> written(paths.size(), 0);
	writer.Put(static_cast<uint32_t>(links.size()));
	for (const auto& link : links)
	{
		writer.StartRecord(serializer::GameThingType::FootpathLinkSave);
		writer.Put(CoordsOf(link.place, link.altitude));
		// A thing with no link gives the number 0
		if (!link.paths.has_value())
		{
			writer.Put(uint32_t {0});
			continue;
		}
		std::vector<size_t> valid;
		std::ranges::copy_if(*link.paths, std::back_inserter(valid), [&paths](size_t index) { return index < paths.size(); });
		writer.StartRecord(serializer::GameThingType::FootpathLink);
		writer.Put(static_cast<uint32_t>(valid.size()));
		for (const auto index : valid)
		{
			PutPath(writer, paths, written, index);
		}
	}
	writer.Put(static_cast<uint32_t>(paths.size()));
	for (size_t index = 0; index < paths.size(); ++index)
	{
		PutPath(writer, paths, written, index);
	}
	return writer.Take();
}
