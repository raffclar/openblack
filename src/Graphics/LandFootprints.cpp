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
#include <array>
#include <initializer_list>
#include <iterator>
#include <span>
#include <utility>
#include <vector>

#include <glm/common.hpp>
#include <spdlog/spdlog.h>

#include "3D/BlockTexture.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/IntegerSteps.h"
#include "Common/SideThread.h"
#include "Common/WorkerPools.h"
#include "Common/X87Angle.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/GroundPrint.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Stream.h"
#include "ECS/DrawnRegistry.h"
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
/// The fewest things in each share of the search for the changed things
constexpr size_t k_LeastThingsToShare = 4096;
} // namespace

/// The registry keeps its component stores in a list that grows when the registry gets the first component of a kind.
/// The fill can do that on the game's thread. Thus the search thread must not look in that list while the fill
/// continues. A store stays at the same place while the registry exists, thus the search reads the stores that the
/// game's thread found before the search started. A store that did not exist then holds no component during the fill.
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
	ecs::ComponentLookups<MeshComponent, MapCellResident, ecs::components::DeadTree, ecs::components::Flowers> things;
	ecs::placed_turn::TurnLookups turns;
};

LandFootprints::LandFootprints() = default;

LandFootprints::~LandFootprints() = default;

WorkerPool& LandFootprints::Workers()
{
	if (!_workers)
	{
		_workers = MakeWorkerPool();
	}
	return *_workers;
}

const L3DMesh::Footprint* LandFootprints::FoundFootprintOf(entt::id_type mesh) const
{
	const auto found = _footprints.find(mesh);
	return found != _footprints.end() ? found->second.footprint : nullptr;
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
		_lastFootprint = std::pair(mesh, found->second.footprint);
		return found->second.footprint;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	FoundFootprint kept;
	if (meshes.Contains(mesh))
	{
		if (const auto model = meshes.Handle(mesh); model)
		{
			if (const auto* own = model->GetGroundFootprint(); own != nullptr)
			{
				kept.copy = std::make_unique<const L3DMesh::Footprint>(*own);
				kept.footprint = kept.copy.get();
			}
		}
	}
	const auto* footprint = kept.footprint;
	_footprints.emplace(mesh, std::move(kept));
	_lastFootprint = std::pair(mesh, footprint);
	return footprint;
}

void LandFootprints::Reset(size_t blockCount)
{
	_lookAtAll = true;
	_known.clear();
	_looks.clear();
	_rivers.clear();
	_riverPoints.clear();
	_groundPrints.clear();
	_groundPrintThings.clear();
	_footprints.clear();
	_lastFootprint.reset();
	_dirty.clear();
	_wholeDirty.clear();
	_dirtyParts.clear();
	_changedThings.clear();
	_blockThings.assign(blockCount, {});
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
				_wholeDirty.insert(found->second);
			}
		}
	}
}

void LandFootprints::MarkThing(entt::entity entity, const Known& known)
{
	_changedThings.push_back(entity);
	for (int x = known.lowBlock.x; x <= known.highBlock.x; ++x)
	{
		for (int z = known.lowBlock.y; z <= known.highBlock.y; ++z)
		{
			const auto found = _blockAt.find(std::pair(x, z));
			if (found != _blockAt.end())
			{
				_dirty.insert(found->second);
				auto& part = _dirtyParts[found->second];
				part = footprint_paint::Join(part, footprint_paint::RegionOf(known.bounds, {x, z}, _side));
			}
		}
	}
}

void LandFootprints::Learn(entt::entity entity, const KnownLook& look, Known&& known)
{
	const auto index = static_cast<size_t>(entt::to_entity(entity));
	if (index >= _looks.size())
	{
		_looks.resize(index + 1);
	}
	_looks[index] = look;
	_looks[index].entity = entity;
	_known.emplace(entity, std::move(known));
}

void LandFootprints::Forget(entt::entity entity)
{
	const auto index = static_cast<size_t>(entt::to_entity(entity));
	if (index < _looks.size() && _looks[index].entity == entity)
	{
		_looks[index].entity = entt::null;
	}
	_known.erase(entity);
}

const LandFootprints::KnownLook* LandFootprints::LookOf(entt::entity entity) const
{
	const auto index = static_cast<size_t>(entt::to_entity(entity));
	return index < _looks.size() && _looks[index].entity == entity ? &_looks[index] : nullptr;
}

bool LandFootprints::SameLook(const KnownLook& look, entt::id_type mesh, const std::vector<uint32_t>& cells,
                              const Transform& transform) const
{
	if (look.mesh != mesh || look.cellCount != cells.size() || !SameTransform(look.transform, transform))
	{
		return false;
	}
	// Most things stand in one cell. Only a thing in more cells reads all of its known cells.
	if (cells.empty())
	{
		return true;
	}
	return look.firstCell == cells.front() && (cells.size() == 1 || _known.get(look.entity).cells == cells);
}

size_t LandFootprints::Shares(size_t count)
{
	return std::max<size_t>(1, std::min(count / k_LeastThingsToShare, Workers().GetLaneCount()));
}

void LandFootprints::ForShares(size_t count, const std::function<void(size_t share, size_t from, size_t to)>& job)
{
	const size_t shares = Shares(count);
	if (shares == 1)
	{
		job(0, 0, count);
		return;
	}
	Workers().ForEach(shares, [&job, count, shares](size_t share, size_t /*lane*/) {
		job(share, count * share / shares, count * (share + 1) / shares);
	});
}

void LandFootprints::LookAt(const ThingStores& stores, uint32_t search, entt::entity entity, const MapCellResident& resident,
                            const MeshComponent& mesh, const Transform& transform)
{
	const auto* footprint = FootprintOf(mesh.id);
	// The registry can give the number of a removed thing to a new thing. The removed thing is not on the map now,
	// so its blocks must get their footprints again.
	const auto index = static_cast<size_t>(entt::to_entity(entity));
	if (const auto held = index < _looks.size() ? _looks[index].entity : entt::entity {entt::null};
	    held != entt::null && held != entity)
	{
		MarkThing(held, _known.get(held));
		Forget(held);
	}
	if (_known.contains(entity))
	{
		auto& known = _looks[index];
		if (footprint != nullptr && SameLook(known, mesh.id, resident.cells, transform))
		{
			known.seen = search;
			return;
		}
		MarkThing(entity, _known.get(entity));
		Forget(entity);
	}
	if (footprint == nullptr)
	{
		return;
	}
	const auto placement = PlacementFor(stores.turns, entity, transform);
	const auto bounds = land_footprints::GroundBounds(footprint->triangles, placement);
	const auto [lowBlock, highBlock] = BlocksOf(bounds);
	Known known {
	    .cells = resident.cells, .placement = placement, .bounds = bounds, .lowBlock = lowBlock, .highBlock = highBlock};
	MarkThing(entity, known);
	Learn(entity,
	      KnownLook {.seen = search,
	                 .mesh = mesh.id,
	                 .cellCount = static_cast<uint32_t>(resident.cells.size()),
	                 .firstCell = resident.cells.empty() ? 0u : resident.cells.front(),
	                 .transform = transform},
	      std::move(known));
}

void LandFootprints::FindChangedThings(const ThingStores& stores, const std::optional<std::vector<uint32_t>>& marked)
{
	// Only a thing filed in the map's cells counts: the game paints a footprint when its thing goes into the cells, and
	// paints it no more once the thing comes out of them
	if (marked.has_value())
	{
		FindMarkedThings(stores, *marked);
	}
	else
	{
		FindChangedThingsInAll(stores);
	}
}

void LandFootprints::FindMarkedThings(const ThingStores& stores, std::span<const uint32_t> marked)
{
	const auto& registry = *stores.registry;
	const auto search = ++_searches;
	const auto* entities = registry.Underlying().storage<entt::entity>();
	for (const auto number : marked)
	{
		// The thing that has this number now, if any
		auto entity = entt::entity {entt::null};
		if (entities != nullptr)
		{
			const auto version = entities->current(entt::entt_traits<entt::entity>::construct(number, 0));
			const auto candidate = entt::entt_traits<entt::entity>::construct(number, version);
			if (registry.Valid(candidate))
			{
				entity = candidate;
			}
		}
		// A known thing that went, or that left its number to a new thing, takes its footprint off
		if (const auto held = number < _looks.size() ? _looks[number].entity : entt::entity {entt::null};
		    held != entt::null && held != entity)
		{
			MarkThing(held, _known.get(held));
			Forget(held);
		}
		if (entity == entt::null)
		{
			continue;
		}
		// A thing that does not show its footprint counts as a thing outside the cells
		if (stores.filed.contains(entity) && land_footprints::ShowsFootprint(stores.things, entity))
		{
			const auto& [resident, mesh, transform] = stores.filed.get(entity);
			LookAt(stores, search, entity, resident, mesh, transform);
		}
		else if (_known.contains(entity))
		{
			MarkThing(entity, _known.get(entity));
			Forget(entity);
		}
	}
}

void LandFootprints::FindChangedThingsInAll(const ThingStores& stores)
{
	const auto& registry = *stores.registry;
	// Each search marks the known things that it sees with a number of its own, so that no search clears the marks first
	const auto search = ++_searches;
	const entt::sparse_set& knownEntities = _known;

	// A thing that went while the land was not drawn, as in the temple of a network game where the world goes on, can
	// leave its number to a new thing. The footprint of the thing that went comes off first. The lanes of the pool find
	// these things, each lane in its own share of the known things.
	std::vector<std::vector<entt::entity>> found;
	const auto findGone = [&](size_t share, size_t from, size_t to) {
		for (auto it = knownEntities.begin() + static_cast<std::ptrdiff_t>(from);
		     it != knownEntities.begin() + static_cast<std::ptrdiff_t>(to); ++it)
		{
			if (!registry.Valid(*it))
			{
				found[share].push_back(*it);
			}
		}
	};
	found.assign(Shares(knownEntities.size()), {});
	ForShares(knownEntities.size(), findGone);
	for (const auto& gone : found)
	{
		for (const auto entity : gone)
		{
			MarkThing(entity, _known.get(entity));
			Forget(entity);
		}
	}

	// A known thing that is not in the cells now is not seen, and the last step removes it.
	// Most things keep the footprint they had, or have none. The lanes of the pool find these things first, each lane in
	// its own share of the things, and change nothing but the marks of the known things that they see. This thread then
	// looks at the other things in the order of the walk, as a walk of all the things would.
	const auto& filed = stores.filed;
	const auto* first = filed.handle();
	const size_t count = first != nullptr ? first->size() : 0;
	const auto quick = [&](size_t share, size_t from, size_t to) {
		auto& left = found[share];
		// The things of a few models often come mixed, as the trees of a wood do. Each share keeps the models that it
		// found last, so that it searches the found footprints less often.
		constexpr size_t k_KeptModels = 8;
		std::array<std::pair<entt::id_type, decltype(_footprints)::const_iterator>, k_KeptModels> kept;
		kept.fill({entt::id_type {0}, _footprints.cend()});
		for (auto it = first->begin() + static_cast<std::ptrdiff_t>(from);
		     it != first->begin() + static_cast<std::ptrdiff_t>(to); ++it)
		{
			const auto entity = *it;
			// A thing that does not show its footprint counts as a thing outside the cells
			if (!filed.contains(entity) || !land_footprints::ShowsFootprint(stores.things, entity))
			{
				continue;
			}
			const auto& [resident, mesh, transform] = filed.get(entity);
			auto& slot = kept.at(mesh.id % k_KeptModels);
			if (slot.second == _footprints.cend() || slot.first != mesh.id)
			{
				slot = {mesh.id, _footprints.find(mesh.id)};
			}
			const auto footprint = slot.second;
			const auto index = static_cast<size_t>(entt::to_entity(entity));
			auto* known = index < _looks.size() ? &_looks[index] : nullptr;
			const auto held = known != nullptr ? known->entity : entt::entity {entt::null};
			if (footprint == _footprints.cend() || (held != entt::null && held != entity))
			{
				left.push_back(entity);
				continue;
			}
			if (held == entt::null)
			{
				// Not known: a thing whose model has no footprint stays so
				if (footprint->second.footprint != nullptr)
				{
					left.push_back(entity);
				}
				continue;
			}
			if (footprint->second.footprint != nullptr && SameLook(*known, mesh.id, resident.cells, transform))
			{
				known->seen = search;
				continue;
			}
			left.push_back(entity);
		}
	};
	found.assign(Shares(count), {});
	ForShares(count, quick);
	for (const auto& left : found)
	{
		for (const auto entity : left)
		{
			const auto& [resident, mesh, transform] = filed.get(entity);
			LookAt(stores, search, entity, resident, mesh, transform);
		}
	}

	// The lanes of the pool find the known things that this search did not see, as above. These things come off.
	const auto findUnseen = [&](size_t share, size_t from, size_t to) {
		for (auto it = knownEntities.begin() + static_cast<std::ptrdiff_t>(from);
		     it != knownEntities.begin() + static_cast<std::ptrdiff_t>(to); ++it)
		{
			if (_looks[entt::to_entity(*it)].seen != search)
			{
				found[share].push_back(*it);
			}
		}
	};
	found.assign(Shares(knownEntities.size()), {});
	ForShares(knownEntities.size(), findUnseen);
	for (const auto& unseen : found)
	{
		for (const auto entity : unseen)
		{
			MarkThing(entity, _known.get(entity));
			Forget(entity);
		}
	}
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
	// The prints that only the frames make, as the print of the dinosaur of the fourth land, lie with the turn and the
	// scale of their model. The frames make them on the first frame that draws the land, thus they are older than the
	// prints that the scripts make.
	if (Locator::drawnRegistry::has_value())
	{
		Locator::drawnRegistry::value().Each<const GroundPrint, const MeshComponent, const Transform>(
		    [&things](const GroundPrint& /*unused*/, const MeshComponent& mesh, const Transform& transform) {
			    auto scaledTurn = transform.rotation;
			    scaledTurn[0] *= transform.scale.x;
			    scaledTurn[1] *= transform.scale.y;
			    scaledTurn[2] *= transform.scale.z;
			    things.push_back({.mesh = mesh.id, .position = transform.position, .made = 0, .scaledTurn = scaledTurn});
		    });
	}
	registry.Each<const GroundPrint, const MeshComponent, const Transform>(
	    [&things](const GroundPrint& print, const MeshComponent& mesh, const Transform& transform) {
		    things.push_back({.mesh = mesh.id, .position = transform.position, .made = print.made + 1});
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
		const auto placement = land_footprints::GroundPrintPlacement(things[index]);
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
		           land_footprints::ShowsFootprint(stores.things, entity) && FoundFootprintOf(mesh->id) != nullptr;
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
		const auto* look = LookOf(entity);
		const auto placement = look != nullptr && SameTransform(look->transform, transform)
		                           ? _known.get(entity).placement
		                           : PlacementFor(stores.turns, entity, transform);
		Paint(texels, position, *footprint, placement, footprint_paint::Mix::Colour, side);
	}
	for (const auto* print : paint.loose)
	{
		Paint(texels, position, *print->footprint, print->placement,
		      print->channel ? footprint_paint::Mix::LowerAlpha : footprint_paint::Mix::Colour, side);
	}
}

std::optional<footprint_paint::Region> LandFootprints::PartToPaint(size_t block, const BlockPaint& paint) const
{
	if (_wholeDirty.contains(block) || !_painted.at(block) || (paint.things.empty() && paint.loose.empty()))
	{
		return std::nullopt;
	}
	const auto own = _ownTexels.find(block);
	const auto part = _dirtyParts.find(block);
	if (own == _ownTexels.end() || own->second.empty() || part == _dirtyParts.end() || part->second.Empty() ||
	    part->second == footprint_paint::WholeBlock(_side))
	{
		return std::nullopt;
	}
	// Outside the part, each texel keeps its paint only when the other things paint it in the same order as before
	if (block >= _blockThings.size() || !land_footprints::SameOrderBesides(_blockThings[block], paint.things, _changedThings))
	{
		return std::nullopt;
	}
	return part->second;
}

void LandFootprints::PaintBlockPart(size_t block, const BlockPaint& paint, const ThingStores& stores,
                                    const footprint_paint::Region& part, std::span<uint16_t> texels) const
{
	const auto position = Locator::terrainSystem::value().GetBlocks().at(block).GetBlockPosition();
	const auto side = static_cast<size_t>(_side);
	const auto& own = _ownTexels.at(block);
	for (int row = part.firstRow; row <= part.lastRow; ++row)
	{
		const auto first = static_cast<size_t>(row) * side + static_cast<size_t>(part.firstColumn);
		const auto count = ToIndex(part.lastColumn - part.firstColumn + 1);
		std::copy_n(own.begin() + static_cast<std::ptrdiff_t>(first), count,
		            texels.begin() + static_cast<std::ptrdiff_t>(first));
	}
	const auto paintPart = [&](const L3DMesh::Footprint& footprint, const footprint_paint::Placement& placement,
	                           footprint_paint::Mix mix) {
		const footprint_paint::Picture picture {
		    .width = footprint.width, .height = footprint.height, .texels = footprint.texels};
		footprint_paint::PaintFootprint(texels, position, picture, footprint.triangles, placement, mix, _side, part);
	};
	for (const auto entity : paint.things)
	{
		const auto& transform = *stores.turns.TryGet<const Transform>(entity);
		const auto* footprint = FoundFootprintOf(stores.things.TryGet<const MeshComponent>(entity)->id);
		const auto* look = LookOf(entity);
		if (look != nullptr && SameTransform(look->transform, transform))
		{
			const auto& known = _known.get(entity);
			// A footprint that paints no texel of the part changes nothing there
			if (!footprint_paint::Overlap(footprint_paint::RegionOf(known.bounds, position, _side), part))
			{
				continue;
			}
			paintPart(*footprint, known.placement, footprint_paint::Mix::Colour);
			continue;
		}
		paintPart(*footprint, PlacementFor(stores.turns, entity, transform), footprint_paint::Mix::Colour);
	}
	for (const auto* print : paint.loose)
	{
		if (footprint_paint::Overlap(footprint_paint::RegionOf(print->bounds, position, _side), part))
		{
			paintPart(*print->footprint, print->placement,
			          print->channel ? footprint_paint::Mix::LowerAlpha : footprint_paint::Mix::Colour);
		}
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
			_wholeDirty.insert(block);
		}
	}
}

void LandFootprints::Update()
{
	switch (std::exchange(_search, Search::None))
	{
	case Search::Started:
		// The side thread finds the changed things. When it did not start yet, this thread does the search.
		_searchThread->WaitOrRun();
		break;
	case Search::NoLand:
		return;
	case Search::None:
		if (!TakeLandChanges())
		{
			return;
		}
		FindChangedThings(ThingStores(Locator::entitiesRegistry::value()), TakeMarkedThings());
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
	_searchThread->Start([this, stores = ThingStores(Locator::entitiesRegistry::value()), marked = TakeMarkedThings()] {
		FindChangedThings(stores, marked);
	});
}

std::optional<std::vector<uint32_t>> LandFootprints::TakeMarkedThings()
{
	auto& registry = Locator::entitiesRegistry::value();
	auto marked = registry.ThingChanges().Take(ecs::ThingWrites::Reader::LandFootprints);
	// The marks belong to one registry. After a new registry or a new land, the search looks at all the things one time.
	if (std::exchange(_lookAtAll, false) || std::exchange(_markedRegistry, &registry) != &registry)
	{
		return std::nullopt;
	}
	return marked;
}

void LandFootprints::ForgetSearch()
{
	if (std::exchange(_search, Search::None) == Search::Started)
	{
		_searchThread->WaitOrRun();
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
	// A new land paints each of its blocks whole. Other repaints can come in the same list, such as the height changes
	// of the land's script before the first frame. Without the reset, the kept models of the last land stay alive.
	std::vector<size_t> wholeBlocks;
	wholeBlocks.reserve(repainted.size());
	for (const auto& paint : repainted)
	{
		if (!paint.cells.has_value())
		{
			wholeBlocks.push_back(paint.block);
		}
	}
	if (_painted.size() != blockCount || footprint_paint::PaintsEveryBlock(wholeBlocks, blockCount))
	{
		Reset(blockCount);
	}
	// With a texture of another side, each block that is painted next paints all of its texture
	if (const int side = island.GetBlockTextureSide(); side != _side)
	{
		_side = side;
		_blockThings.assign(blockCount, {});
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
		_wholeDirty.insert(block);
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
	std::ranges::sort(_changedThings);
	if (blocks.empty())
	{
		_wholeDirty.clear();
		_dirtyParts.clear();
		_changedThings.clear();
		return;
	}
	_repainted.insert(_repainted.end(), blocks.begin(), blocks.end());

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

	// A block whose changed things paint only a part of it paints only that part again. The other blocks paint all of
	// their textures again.
	std::vector<size_t> painted;
	std::vector<const BlockPaint*> paintOf;
	std::vector<size_t> partBlocks;
	std::vector<const BlockPaint*> partPaintOf;
	std::vector<footprint_paint::Region> parts;
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		if (toPaint[i] == 0)
		{
			continue;
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("graphics"), "Land footprints: block {} painted with {} things and {} loose prints",
		                    blocks[i], paints[i].things.size(), paints[i].loose.size());
		if (const auto part = PartToPaint(blocks[i], paints[i]))
		{
			partBlocks.push_back(blocks[i]);
			partPaintOf.push_back(&paints[i]);
			parts.push_back(*part);
		}
		else
		{
			painted.push_back(blocks[i]);
			paintOf.push_back(&paints[i]);
		}
	}
	_wholeDirty.clear();
	_dirtyParts.clear();
	_changedThings.clear();
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		if (toPaint[i] != 0)
		{
			_painted.at(blocks[i]) = !paints[i].things.empty() || !paints[i].loose.empty();
			_blockThings.at(blocks[i]) = paints[i].things;
		}
	}
	// Then the paint of each block, which goes straight to the block's texture
	const auto side = static_cast<size_t>(island.GetBlockTextureSide());
	if (painted.size() < k_LeastBlocksToShare)
	{
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
	// The parts of the blocks: each lane paints into texels of its own, and this thread gives the parts to the land
	if (!partBlocks.empty())
	{
		auto& workers = Workers();
		_laneTexels.resize(std::max(_laneTexels.size(), workers.GetLaneCount()));
		for (auto& texels : _laneTexels)
		{
			texels.resize(side * side);
		}
		std::vector<std::vector<uint16_t>> partTexels(partBlocks.size());
		const auto paintPart = [&](size_t i, size_t lane) {
			auto& texels = _laneTexels.at(lane);
			const auto& part = parts[i];
			PaintBlockPart(partBlocks[i], *partPaintOf[i], stores, part, texels);
			const auto columns = ToIndex(part.lastColumn - part.firstColumn + 1);
			auto& out = partTexels[i];
			out.resize(static_cast<size_t>(part.lastRow - part.firstRow + 1) * columns);
			for (int row = part.firstRow; row <= part.lastRow; ++row)
			{
				const auto first = static_cast<size_t>(row) * side + static_cast<size_t>(part.firstColumn);
				std::copy_n(texels.begin() + static_cast<std::ptrdiff_t>(first), columns,
				            out.begin() + static_cast<std::ptrdiff_t>(static_cast<size_t>(row - part.firstRow) * columns));
			}
		};
		if (partBlocks.size() < k_LeastBlocksToShare)
		{
			paintPart(0, 0);
		}
		else
		{
			workers.ForEach(partBlocks.size(), paintPart);
		}
		for (size_t i = 0; i < partBlocks.size(); ++i)
		{
			const auto& part = parts[i];
			island.SetBlockTexelsPart(partBlocks[i], {part.firstRow, part.firstColumn},
			                          {part.lastRow - part.firstRow + 1, part.lastColumn - part.firstColumn + 1},
			                          partTexels[i]);
		}
	}
}

} // namespace openblack::graphics
