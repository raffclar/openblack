/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandFootprints.h"

#include <cmath>

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <span>

#include <glm/common.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/X87Angle.h"
#include "ECS/Components/GroundPrint.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Stream.h"
#include "ECS/LandFootprintRules.h"
#include "ECS/Map.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::graphics
{

namespace
{
using MeshComponent = ecs::components::Mesh;
using ecs::components::GroundPrint;
using ecs::components::MapCellResident;
using ecs::components::Stream;
using ecs::components::StreamSegment;
using ecs::components::Transform;
namespace land_footprints = ecs::land_footprints;

[[nodiscard]] bool SameTransform(const Transform& a, const Transform& b)
{
	return a.position == b.position && a.rotation == b.rotation && a.scale == b.scale;
}

/// Where a placed thing's footprint lies: the game turns the thing by its angles and scales it, and each product rounds
/// to a float
[[nodiscard]] footprint_paint::Placement PlacementFor(const ecs::Registry& registry, entt::entity entity,
                                                      const Transform& transform)
{
	const bool canLean = ecs::placed_turn::CanLean(registry, entity);
	const auto angles = ecs::placed_turn::AnglesOf(registry, entity);
	const float scale = transform.scale.x;
	const auto turn =
	    canLean ? x87::GameTurnMatrixScaled(angles.x, angles.y, angles.z, scale) : x87::GameUprightTurnScaled(angles.y, scale);
	return land_footprints::PlacementOf(transform.position, turn);
}

/// The blocks, along x and z, that the bounds of a footprint reach. A unit more on each side holds the corners that
/// move to the nearest texel.
[[nodiscard]] std::pair<glm::ivec2, glm::ivec2> BlocksOf(const std::pair<glm::vec2, glm::vec2>& bounds)
{
	const auto low = glm::floor((bounds.first - 1.0f) / footprint_paint::k_BlockUnits);
	const auto high = glm::floor((bounds.second + 1.0f) / footprint_paint::k_BlockUnits);
	return {glm::ivec2(low), glm::ivec2(high)};
}

void Paint(std::span<uint16_t> texels, glm::ivec2 block, const L3DMesh::Footprint& footprint,
           const footprint_paint::Placement& placement, footprint_paint::Mix mix)
{
	const footprint_paint::Picture picture {.width = footprint.width, .height = footprint.height, .texels = footprint.texels};
	footprint_paint::PaintFootprint(texels, block, picture, footprint.triangles, placement, mix);
}
} // namespace

const L3DMesh::Footprint* LandFootprints::FootprintOf(entt::id_type mesh)
{
	if (_lastFootprint.has_value() && _lastFootprint->first == mesh)
	{
		return _lastFootprint->second;
	}
	const auto found = _footprints.find(mesh);
	if (found != _footprints.end())
	{
		_lastFootprint = *found;
		return found->second;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const L3DMesh::Footprint* footprint = nullptr;
	if (meshes.Contains(mesh))
	{
		const auto handle = meshes.Handle(mesh);
		// The game paints the first footprint of the model of a landscape feature
		if (handle->ContainsLandscapeFeature() && !handle->GetFootprints().empty() &&
		    !handle->GetFootprints().front().triangles.empty())
		{
			footprint = &handle->GetFootprints().front();
		}
	}
	_footprints.emplace(mesh, footprint);
	_lastFootprint = std::pair(mesh, footprint);
	return footprint;
}

void LandFootprints::Reset(size_t blockCount)
{
	_known.clear();
	_rivers.clear();
	_riverPoints.clear();
	_groundPrints.clear();
	_groundPrintThings.clear();
	_footprints.clear();
	_lastFootprint.reset();
	_dirty.clear();
	_ownTexels.clear();
	_painted.assign(blockCount, false);
	_blockAt.clear();
	const auto& blocks = Locator::terrainSystem::value().GetBlocks();
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		const auto position = blocks[i].GetBlockPosition();
		_blockAt.emplace(std::pair(position.x, position.y), i);
	}
}

void LandFootprints::MarkBlocks(glm::ivec2 lowBlock, glm::ivec2 highBlock)
{
	for (int x = lowBlock.x; x <= highBlock.x; ++x)
	{
		for (int z = lowBlock.y; z <= highBlock.y; ++z)
		{
			const auto found = _blockAt.find(std::pair(x, z));
			if (found != _blockAt.end())
			{
				_dirty.insert(found->second);
			}
		}
	}
}

void LandFootprints::FindChangedThings()
{
	const auto& registry = Locator::entitiesRegistry::value();
	for (auto& known : _known)
	{
		known.seen = false;
	}
	// Only a thing filed in the map's cells counts: the game paints a footprint when its thing goes into the cells, and
	// paints it no more once the thing comes out of them. A known thing that is not in the cells now is not seen, and the
	// loop after this one removes it.
	registry.Each<const MapCellResident, const MeshComponent, const Transform>(
	    [&](entt::entity entity, const MapCellResident& resident, const MeshComponent& mesh, const Transform& transform) {
		    const auto* footprint = FootprintOf(mesh.id);
		    if (_known.contains(entity))
		    {
			    auto& known = _known.get(entity);
			    if (footprint != nullptr && known.mesh == mesh.id && known.cells == resident.cells &&
			        SameTransform(known.transform, transform))
			    {
				    known.seen = true;
				    return;
			    }
			    MarkBlocks(known.lowBlock, known.highBlock);
			    _known.erase(entity);
		    }
		    if (footprint == nullptr)
		    {
			    return;
		    }
		    const auto placement = PlacementFor(registry, entity, transform);
		    const auto [lowBlock, highBlock] = BlocksOf(land_footprints::GroundBounds(footprint->triangles, placement));
		    MarkBlocks(lowBlock, highBlock);
		    _known.emplace(entity, Known {.mesh = mesh.id,
		                                  .cells = resident.cells,
		                                  .transform = transform,
		                                  .placement = placement,
		                                  .lowBlock = lowBlock,
		                                  .highBlock = highBlock,
		                                  .seen = true});
	    });
	std::vector<entt::entity> unseen;
	for (const auto [entity, known] : _known.each())
	{
		if (!known.seen)
		{
			MarkBlocks(known.lowBlock, known.highBlock);
			unseen.push_back(entity);
		}
	}
	_known.erase(unseen.begin(), unseen.end());
}

void LandFootprints::FindChangedRivers()
{
	const auto& registry = Locator::entitiesRegistry::value();
	using RiverPoints = std::pair<Stream::Id, const std::vector<glm::vec3>*>;
	std::vector<RiverPoints> streams;
	registry.Each<const Stream>([&streams](const Stream& stream) { streams.emplace_back(stream.id, &stream.points); });
	// The script of the land makes its rivers in the order of their numbers
	std::ranges::sort(streams, {}, &RiverPoints::first);
	// The code copies the rivers only when they changed
	if (std::ranges::equal(streams, _riverPoints, {}, [](const RiverPoints& stream) -> const auto& { return *stream.second; }))
	{
		return;
	}
	std::vector<std::vector<glm::vec3>> points;
	points.reserve(streams.size());
	std::ranges::transform(streams, std::back_inserter(points), [](const RiverPoints& stream) { return *stream.second; });
	for (const auto& river : _rivers)
	{
		const auto [lowBlock, highBlock] = BlocksOf(river.bounds);
		MarkBlocks(lowBlock, highBlock);
	}
	_rivers.clear();
	_riverPoints = points;

	// The game keeps each point of a river at its place on the map, on the land
	const auto& island = Locator::terrainSystem::value();
	std::vector<size_t> counts;
	counts.reserve(points.size());
	for (auto& river : points)
	{
		counts.push_back(river.size());
		for (auto& point : river)
		{
			const auto x = map_coords::ToFixedGUtils(point.x);
			const auto z = map_coords::ToFixedGUtils(point.z);
			point = {map_coords::ToMetres(x), island.GetAltitude(x, z), map_coords::ToMetres(z)};
		}
	}
	const auto* channel = FootprintOf(StreamSegment::k_ChannelMeshId);
	const auto* bed = FootprintOf(StreamSegment::k_BedMeshId);
	for (const auto& print : land_footprints::RiverPrintOrder(counts))
	{
		const auto* footprint = print.channel ? channel : bed;
		if (footprint == nullptr)
		{
			continue;
		}
		const auto& river = points[print.river];
		const auto placement = land_footprints::RiverStretch(river[print.from], river[print.from + 1]);
		const auto bounds = land_footprints::GroundBounds(footprint->triangles, placement);
		_rivers.push_back({.footprint = footprint, .placement = placement, .bounds = bounds, .channel = print.channel});
		const auto [lowBlock, highBlock] = BlocksOf(bounds);
		MarkBlocks(lowBlock, highBlock);
	}
}

void LandFootprints::FindChangedGroundPrints()
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::vector<land_footprints::GroundPrintThing> things;
	registry.Each<const GroundPrint, const MeshComponent, const Transform>(
	    [&things](const GroundPrint& print, const MeshComponent& mesh, const Transform& transform) {
		    things.push_back({.mesh = mesh.id, .position = transform.position, .made = print.made});
	    });
	// The same things in the same order paint the same prints
	std::ranges::sort(things, {}, &land_footprints::GroundPrintThing::made);
	if (things == _groundPrintThings)
	{
		return;
	}
	for (const auto& print : _groundPrints)
	{
		const auto [lowBlock, highBlock] = BlocksOf(print.bounds);
		MarkBlocks(lowBlock, highBlock);
	}
	_groundPrints.clear();
	_groundPrintThings = things;
	for (const auto index : land_footprints::GroundPrintOrder(things))
	{
		const auto* footprint = FootprintOf(things[index].mesh);
		if (footprint == nullptr)
		{
			continue;
		}
		const auto placement = land_footprints::GroundPrintPlacement(things[index].position);
		const auto bounds = land_footprints::GroundBounds(footprint->triangles, placement);
		_groundPrints.push_back({.footprint = footprint, .placement = placement, .bounds = bounds, .channel = false});
		const auto [lowBlock, highBlock] = BlocksOf(bounds);
		MarkBlocks(lowBlock, highBlock);
	}
}

void LandFootprints::PaintBlock(size_t block)
{
	auto& island = Locator::terrainSystem::value();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& map = Locator::entitiesMap::value();
	const auto& landBlock = island.GetBlocks().at(block);
	const auto position = landBlock.GetBlockPosition();
	const auto corner = landBlock.GetMapPosition();

	const auto things = land_footprints::ThingsOverBlock(
	    position, [&map](glm::ivec2 cell) { return map.GetFiledFixedInGridCell(ecs::MapInterface::CellId(cell)); },
	    [&map](glm::ivec2 cell) { return map.GetFiledMobileInGridCell(ecs::MapInterface::CellId(cell)); },
	    [&](entt::entity entity) {
		    const auto* mesh = registry.TryGet<const MeshComponent>(entity);
		    return mesh != nullptr && registry.AllOf<Transform, MapCellResident>(entity) && FootprintOf(mesh->id) != nullptr;
	    });
	// After the things come the loose prints, the newest first: the prints of the things that show only as prints, then
	// the rivers, which the land made before its scripts started
	std::vector<const LoosePrint*> loose;
	for (const auto* prints : {&_groundPrints, &_rivers})
	{
		for (const auto& print : *prints)
		{
			if (land_footprints::ReachesBlock(print.bounds, corner))
			{
				loose.push_back(&print);
			}
		}
	}
	const bool hasFootprints = !things.empty() || !loose.empty();
	if (!hasFootprints && !_painted.at(block))
	{
		return;
	}

	// The block's own texture, painted once and kept until the land paints the block again
	auto& own = _ownTexels[block];
	if (own.empty())
	{
		own.resize(static_cast<size_t>(footprint_paint::k_BlockTexels) * footprint_paint::k_BlockTexels);
		island.PaintBlockTexels(block, own);
	}
	_texels = own;
	for (const auto entity : things)
	{
		const auto& transform = registry.Get<const Transform>(entity);
		const auto* footprint = FootprintOf(registry.Get<const MeshComponent>(entity).id);
		const auto* known = _known.contains(entity) ? &_known.get(entity) : nullptr;
		const auto placement = known != nullptr && SameTransform(known->transform, transform)
		                           ? known->placement
		                           : PlacementFor(registry, entity, transform);
		Paint(_texels, position, *footprint, placement, footprint_paint::Mix::Colour);
	}
	for (const auto* print : loose)
	{
		Paint(_texels, position, *print->footprint, print->placement,
		      print->channel ? footprint_paint::Mix::LowerAlpha : footprint_paint::Mix::Colour);
	}
	island.SetBlockTexels(block, _texels);
	SPDLOG_LOGGER_DEBUG(spdlog::get("graphics"),
	                    "Land footprints: block {} ({}, {}) painted with {} things and {} loose prints", block, position.x,
	                    position.y, things.size(), loose.size());
	_painted.at(block) = hasFootprints;
}

void LandFootprints::Update()
{
	if (!Locator::terrainSystem::has_value() || !Locator::entitiesRegistry::has_value() || !Locator::entitiesMap::has_value() ||
	    !Locator::resources::has_value())
	{
		return;
	}
	auto& island = Locator::terrainSystem::value();
	if (!island.IsLoaded() || island.GetBlocks().empty())
	{
		return;
	}
	const auto blockCount = island.GetBlocks().size();
	const auto repainted = island.TakeRepaintedBlocks();
	// A new land paints each of its blocks
	if (_painted.size() != blockCount || repainted.size() == blockCount)
	{
		Reset(blockCount);
	}
	for (const auto block : repainted)
	{
		_ownTexels.erase(block);
		_dirty.insert(block);
	}
	FindChangedThings();
	FindChangedRivers();
	FindChangedGroundPrints();
	for (const auto block : _dirty)
	{
		if (block < blockCount)
		{
			PaintBlock(block);
		}
	}
	_dirty.clear();
}

} // namespace openblack::graphics
