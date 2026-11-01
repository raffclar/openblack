/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaFootpathImport.h"

#include <cstring>

#include <algorithm>

#include "3D/MapCoords.h"
#include "VanillaWorld.h"

namespace openblack::ecs::vanilla_footpath_import
{
namespace
{

using gamesave::VanillaRecord;
using gamesave::VanillaType;
using gamesave::VanillaWorld;

std::optional<ImportedNode> ReadNode(const VanillaWorld& world, uint32_t number, std::string& why)
{
	const auto* record = world.Record(number);
	if (record == nullptr || !record->Is(VanillaType::FootpathNode))
	{
		why = "a node " + std::to_string(number) + " that is not a node of a footpath";
		return std::nullopt;
	}
	const auto coords = world.Raw(*record, "pos");
	if (coords.size() != sizeof(gamesave::MapCoords))
	{
		why = "the node " + std::to_string(number) + " with no place";
		return std::nullopt;
	}
	ImportedNode node {.number = number, .flags = world.Byte(*record, "nodeFlags").value_or(0)};
	std::memcpy(&node.coords, coords.data(), sizeof(node.coords));
	const auto followers = world.Refs(*record, "followers");
	node.followers.assign(followers.begin(), followers.end());
	return node;
}

std::optional<ImportedFootpath> ReadFootpath(const VanillaWorld& world, uint32_t number, std::string& why)
{
	const auto* record = world.Record(number);
	if (record == nullptr || !record->Is(VanillaType::Footpath))
	{
		why = "the object " + std::to_string(number) + " of the list of the footpaths, which is not a footpath";
		return std::nullopt;
	}
	ImportedFootpath footpath {.number = number, .walkable = world.Word(*record, "walkable").value_or(0)};
	for (const auto node : world.Refs(*record, "nodes"))
	{
		auto read = ReadNode(world, node, why);
		if (!read.has_value())
		{
			why = "the footpath " + std::to_string(number) + " with " + why;
			return std::nullopt;
		}
		footpath.nodes.push_back(std::move(*read));
	}
	return footpath;
}

} // namespace

ImportedFootpaths ReadFootpaths(const VanillaWorld& world)
{
	ImportedFootpaths imported;
	const auto lists = world.Fields(world.Game(), "lists");
	if (lists.size() <= k_FootpathList)
	{
		imported.skipped.emplace_back("the footpaths of a game object with no list of the footpaths");
		return imported;
	}
	for (const auto number : world.Refs(*lists.at(k_FootpathList)))
	{
		std::string why;
		if (auto footpath = ReadFootpath(world, number, why); footpath.has_value())
		{
			imported.paths.push_back(std::move(*footpath));
		}
		else
		{
			imported.skipped.push_back(std::move(why));
		}
	}
	for (const auto& record : world.Records())
	{
		if (record.Is(VanillaType::FootpathFinder))
		{
			++imported.finders;
			continue;
		}
		const auto link = world.Ref(record, "footpathLink").value_or(0);
		if (link == 0)
		{
			continue;
		}
		const auto* linkRecord = world.Record(link);
		if (linkRecord == nullptr || !linkRecord->Is(VanillaType::FootpathLink))
		{
			imported.skipped.push_back("the link " + std::to_string(link) + " of the object " + std::to_string(record.number) +
			                           ", which is not a link of footpaths");
			continue;
		}
		const auto footpaths = world.Refs(*linkRecord, "footpaths");
		imported.links.push_back(
		    {.holder = record.number, .number = link, .footpaths = std::vector<uint32_t>(footpaths.begin(), footpaths.end())});
	}
	return imported;
}

components::Footpath::Node NodeOf(const ImportedNode& node, float landHeight)
{
	const map_coords::MapCoords coords {.x = node.coords.x, .z = node.coords.z, .altitude = node.coords.altitude};
	const auto ground = map_coords::ToMetres(coords);
	return {.position = {ground.x, node.coords.altitude + landHeight, ground.y},
	        .place = {node.coords.x, node.coords.z},
	        .planned = (node.flags & k_NodePlanned) != 0,
	        .hidden = (node.flags & k_NodeHidden) != 0,
	        .marker = (node.flags & k_NodeMarker) != 0};
}

std::optional<uint32_t> NodeIndexOf(const ImportedFootpath& footpath, uint32_t node)
{
	if (node == 0)
	{
		return std::nullopt;
	}
	const auto found = std::ranges::find(footpath.nodes, node, &ImportedNode::number);
	if (found == footpath.nodes.end())
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(found - footpath.nodes.begin());
}

} // namespace openblack::ecs::vanilla_footpath_import
