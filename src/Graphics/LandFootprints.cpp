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
#include <utility>

#include <glm/common.hpp>
#include <spdlog/spdlog.h>

#include "3D/BlockTexture.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/SideThread.h"
#include "Common/WorkerPool.h"
#include "Common/X87Angle.h"
#include "ECS/Components/GroundPrint.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Stream.h"
#include "ECS/EntityStorage.h"
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
[[nodiscard]] footprint_paint::Placement PlacementFor(const ecs::placed_turn::TurnLookups& turns, entt::entity entity,
                                                      const Transform& transform)
{
	const bool canLean = ecs::placed_turn::CanLean(turns, entity);
	const auto angles = ecs::placed_turn::AnglesOf(turns, entity);
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
           const footprint_paint::Placement& placement, footprint_paint::Mix mix, int side)
{
	const footprint_paint::Picture picture {.width = footprint.width, .height = footprint.height, .texels = footprint.texels};
	footprint_paint::PaintFootprint(texels, block, picture, footprint.triangles, placement, mix, side);
}
/// With fewer blocks to paint than this, this thread paints them all
constexpr size_t k_LeastBlocksToShare = 2;
} // namespace

/// The registry keeps its component stores in a list that grows when the registry gets the first component of a kind.
/// The draw can do that on the game's thread. Thus the search thread must not look in that list while the draw
/// continues. A store stays at the same place while the registry exists, thus the search reads the stores that the
/// game's thread found before the search started. A store that did not exist then holds no component during the draw.
struct LandFootprints::ThingStores
{
	explicit ThingStores(const ecs::Registry& source)
	    : registry(&source)
	    , filed(source.Underlying().view<const MapCellResident, const MeshComponent, const Transform>())
	    , things(source)
	    , turns(source)
	{
	}
	/// Only for the registry's list of entities, which does not change while the draw continues
	const ecs::Registry* registry;
	/// The things filed in the map's cells that have a model and a transform
	decltype(std::declval<const entt::registry&>().view<const MapCellResident, const MeshComponent, const Transform>()) filed;
	ecs::ComponentLookups<MeshComponent, MapCellResident> things;
	ecs::placed_turn::TurnLookups turns;
};

LandFootprints::LandFootprints() = default;

LandFootprints::~LandFootprints() = default;

WorkerPool& LandFootprints::Workers()
{
	if (!_workers)
	{
		_workers = std::make_unique<WorkerPool>(WorkerPool::DefaultThreadCount());
	}
	return *_workers;
}

const L3DMesh::Footprint* LandFootprints::FoundFootprintOf(entt::id_type mesh) const
{
	const auto found = _footprints.find(mesh);
	return found != _footprints.end() ? found->second : nullptr;
}

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

void LandFootprints::FindChangedThings(const ThingStores& stores)
{
	const auto& registry = *stores.registry;
	// A thing that went while the land was not drawn, as in the temple of a network game where the world goes on, can
	// leave its number to a new thing. The footprint of the thing that went comes off first.
	std::vector<entt::entity> gone;
	for (const auto [entity, known] : _known.each())
	{
		known.seen = false;
		if (!registry.Valid(entity))
		{
			MarkBlocks(known.lowBlock, known.highBlock);
			gone.push_back(entity);
		}
	}
	_known.erase(gone.begin(), gone.end());
	// Only a thing filed in the map's cells counts: the game paints a footprint when its thing goes into the cells, and
	// paints it no more once the thing comes out of them. A known thing that is not in the cells now is not seen, and the
	// loop after this one removes it.
	stores.filed.each(
	    [&](entt::entity entity, const MapCellResident& resident, const MeshComponent& mesh, const Transform& transform) {
		    const auto* footprint = FootprintOf(mesh.id);
		    // The registry can give the number of a removed thing to a new thing. The removed thing is not on the map now,
		    // so its blocks must get their footprints again.
		    if (const auto older = ecs::entity_storage::TakeOlder(_known, entity))
		    {
			    MarkBlocks(older->lowBlock, older->highBlock);
		    }
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
		    const auto placement = PlacementFor(stores.turns, entity, transform);
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

LandFootprints::BlockPaint LandFootprints::FindBlockPaint(size_t block, const ThingStores& stores) const
{
	const auto& island = Locator::terrainSystem::value();
	const auto& map = Locator::entitiesMap::value();
	const auto& landBlock = island.GetBlocks().at(block);
	const auto position = landBlock.GetBlockPosition();
	const auto corner = landBlock.GetMapPosition();

	// Each thing filed in the map's cells went through FindChangedThings, which found the footprint of its model
	BlockPaint paint;
	paint.things = land_footprints::ThingsOverBlock(
	    position, [&map](glm::ivec2 cell) { return map.GetFiledFixedInGridCell(ecs::MapInterface::CellId(cell)); },
	    [&map](glm::ivec2 cell) { return map.GetFiledMobileInGridCell(ecs::MapInterface::CellId(cell)); },
	    [&](entt::entity entity) {
		    const auto* mesh = stores.things.TryGet<const MeshComponent>(entity);
		    return mesh != nullptr && stores.turns.AllOf<Transform>(entity) && stores.things.AllOf<MapCellResident>(entity) &&
		           FoundFootprintOf(mesh->id) != nullptr;
	    });
	// After the things come the loose prints, the newest first: the prints of the things that show only as prints, then
	// the rivers, which the land made before its scripts started
	for (const auto* prints : {&_groundPrints, &_rivers})
	{
		for (const auto& print : *prints)
		{
			if (land_footprints::ReachesBlock(print.bounds, corner))
			{
				paint.loose.push_back(&print);
			}
		}
	}
	return paint;
}

void LandFootprints::PaintBlock(size_t block, const BlockPaint& paint, const ThingStores& stores, std::span<uint16_t> texels)
{
	const auto& island = Locator::terrainSystem::value();
	const auto position = island.GetBlocks().at(block).GetBlockPosition();
	const auto side = island.GetBlockTextureSide();

	// The block's own texture, painted once and kept until the land paints the block again
	auto& own = _ownTexels.at(block);
	if (own.empty())
	{
		own.resize(static_cast<size_t>(side) * static_cast<size_t>(side));
		island.PaintBlockTexels(block, own);
	}
	std::ranges::copy(own, texels.begin());
	for (const auto entity : paint.things)
	{
		const auto& transform = *stores.turns.TryGet<const Transform>(entity);
		const auto* footprint = FoundFootprintOf(stores.things.TryGet<const MeshComponent>(entity)->id);
		const auto* known = _known.contains(entity) ? &_known.get(entity) : nullptr;
		const auto placement = known != nullptr && SameTransform(known->transform, transform)
		                           ? known->placement
		                           : PlacementFor(stores.turns, entity, transform);
		Paint(texels, position, *footprint, placement, footprint_paint::Mix::Colour, side);
	}
	for (const auto* print : paint.loose)
	{
		Paint(texels, position, *print->footprint, print->placement,
		      print->channel ? footprint_paint::Mix::LowerAlpha : footprint_paint::Mix::Colour, side);
	}
}

void LandFootprints::PaintAgain()
{
	ForgetSearch();
	for (size_t block = 0; block < _painted.size(); ++block)
	{
		if (_painted[block])
		{
			_dirty.insert(block);
		}
	}
}

void LandFootprints::Update()
{
	switch (std::exchange(_search, Search::None))
	{
	case Search::Started:
		// The search found the changed things on the side thread
		_searchThread->Wait();
		break;
	case Search::NoLand:
		return;
	case Search::None:
		if (!TakeLandChanges())
		{
			return;
		}
		FindChangedThings(ThingStores(Locator::entitiesRegistry::value()));
		break;
	}
	// The lanes of the pool that paint the blocks read only the stores that this thread finds now
	PaintChanges(ThingStores(Locator::entitiesRegistry::value()));
}

void LandFootprints::StartSearch()
{
	ForgetSearch();
	if (!TakeLandChanges())
	{
		_search = Search::NoLand;
		return;
	}
	if (_searchThread == nullptr)
	{
		_searchThread = std::make_unique<SideThread>();
	}
	_search = Search::Started;
	// The side thread reads only the stores that this thread finds now
	_searchThread->Start([this, stores = ThingStores(Locator::entitiesRegistry::value())] { FindChangedThings(stores); });
}

void LandFootprints::ForgetSearch()
{
	if (std::exchange(_search, Search::None) == Search::Started)
	{
		_searchThread->Wait();
	}
}

bool LandFootprints::TakeLandChanges()
{
	if (!Locator::terrainSystem::has_value() || !Locator::entitiesRegistry::has_value() || !Locator::entitiesMap::has_value() ||
	    !Locator::resources::has_value())
	{
		return false;
	}
	auto& island = Locator::terrainSystem::value();
	if (!island.IsLoaded() || island.GetBlocks().empty())
	{
		return false;
	}
	const auto blockCount = island.GetBlocks().size();
	const auto repainted = island.TakeRepaintedBlocks();
	// A new land paints each of its blocks
	if (_painted.size() != blockCount || repainted.size() == blockCount)
	{
		Reset(blockCount);
	}
	for (const auto& [block, cells] : repainted)
	{
		// The block's own texture that is kept takes the cells painted again. It is then as a new paint of the whole block,
		// because each texel comes only from its own cell.
		if (const auto own = _ownTexels.find(block); own != _ownTexels.end() && !own->second.empty() && cells.has_value())
		{
			block_texture::PutCellTexels(cells->texels, cells->first, cells->count, own->second);
		}
		else
		{
			_ownTexels.erase(block);
		}
		_dirty.insert(block);
	}
	return true;
}

void LandFootprints::PaintChanges(const ThingStores& stores)
{
	auto& island = Locator::terrainSystem::value();
	const auto blockCount = island.GetBlocks().size();
	FindChangedRivers();
	FindChangedGroundPrints();
	std::vector<size_t> blocks;
	std::ranges::copy_if(_dirty, std::back_inserter(blocks), [blockCount](size_t block) { return block < blockCount; });
	_dirty.clear();
	if (blocks.empty())
	{
		return;
	}

	// Each block gets its place in the kept textures first, so the lanes only change what is in a place
	for (const auto block : blocks)
	{
		_ownTexels.try_emplace(block);
	}
	// First the things and the prints over each block. A block with no footprints that held none before keeps its
	// texture. Each lane writes only to the places of its blocks, so the blocks to paint are bytes, not the bits of a
	// vector of bools.
	std::vector<BlockPaint> paints(blocks.size());
	std::vector<uint8_t> toPaint(blocks.size(), 0);
	const auto findOne = [&](size_t i, size_t /*lane*/) {
		paints[i] = FindBlockPaint(blocks[i], stores);
		toPaint[i] = !paints[i].things.empty() || !paints[i].loose.empty() || _painted.at(blocks[i]) ? 1 : 0;
	};
	if (blocks.size() < k_LeastBlocksToShare)
	{
		findOne(0, 0);
	}
	else
	{
		Workers().ForEach(blocks.size(), findOne);
	}

	std::vector<size_t> painted;
	std::vector<const BlockPaint*> paintOf;
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		if (toPaint[i] == 0)
		{
			continue;
		}
		painted.push_back(blocks[i]);
		paintOf.push_back(&paints[i]);
		SPDLOG_LOGGER_DEBUG(spdlog::get("graphics"), "Land footprints: block {} painted with {} things and {} loose prints",
		                    blocks[i], paints[i].things.size(), paints[i].loose.size());
		_painted.at(blocks[i]) = !paints[i].things.empty() || !paints[i].loose.empty();
	}
	// Then the paint of each block, which goes straight to the block's texture
	if (painted.size() < k_LeastBlocksToShare)
	{
		const auto side = static_cast<size_t>(island.GetBlockTextureSide());
		_texels.resize(side * side);
		for (size_t i = 0; i < painted.size(); ++i)
		{
			PaintBlock(painted[i], *paintOf[i], stores, _texels);
			island.SetBlockTexels(painted[i], _texels);
		}
	}
	else
	{
		island.PaintBlocksTexels(
		    painted,
		    [this, &painted, &paintOf, &stores](size_t i, std::span<uint16_t> texels) {
			    PaintBlock(painted[i], *paintOf[i], stores, texels);
		    },
		    Workers());
	}
}

} // namespace openblack::graphics
