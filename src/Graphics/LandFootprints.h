/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/storage.hpp>
#include <entt/resource/resource.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/FootprintPaint.h"
#include "3D/L3DMesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/LandFootprintRules.h"
#include "ECS/LandRepaintRules.h"

namespace openblack
{
class SideThread;
class WorkerPool;
} // namespace openblack

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct MapCellResident;
struct Mesh;
} // namespace openblack::ecs::components

namespace openblack::graphics
{

/// Paints the footprints of the buildings, the features, the prints of things and the rivers into the land's block
/// textures, as Black & White does: on the processor, texel by texel, each footprint over the one before it (see
/// footprint_paint and ecs::land_footprints). The game paints a block's footprints again each time a thing with a
/// footprint over it comes onto the map, goes from it or is built, and each time the land under the block changes
/// height. This paints a block again when a thing with a footprint over it goes into the map's cells or comes out of
/// them, if the thing asks for a paint then (see ecs::land_repaint), when a print of a thing changes, and when the land
/// paints the block again for another cause. A thing that only changes its size in its cells, as a tree that grows, or a
/// thing that moves, does not paint a block again. A block that is painted again for another cause then shows the thing
/// as it is at that time.
class LandFootprints
{
public:
	LandFootprints();
	LandFootprints(const LandFootprints&) = delete;
	LandFootprints& operator=(const LandFootprints&) = delete;
	~LandFootprints();

	/// Paints again the blocks whose footprints changed since the last time
	void Update();
	/// Starts the search for the changed things on a thread of its own, before the next update needs it. The fill of the
	/// drawn frame only reads the world, thus the thread reads it while the fill continues. The thread reads only the
	/// component stores that this thread finds before it starts. Call it only in a fill that calls Update.
	void StartSearch();
	/// Waits for the search that StartSearch started. The blocks that it found stay for the next update.
	void ForgetSearch();
	/// The next update paints again each block that has footprints, as after the land painted all of its blocks again
	void PaintAgain();
	/// The blocks, by their places in the land's list, whose footprints the updates asked to paint again since the last
	/// call. The game paints all of a block's texture again for each of them, its snow too.
	[[nodiscard]] std::vector<size_t> TakeRepainted() { return std::exchange(_repainted, {}); }
	/// The land painted the blocks again for another cause, by their index along x and z: the baked shadows or the snow.
	/// The game paints the footprints in the same paint, thus a later update paints each of these blocks that keeps an
	/// old footprint.
	void PaintedElsewhere(std::span<const glm::ivec2> blocks);
	/// The blocks that the updates painted since the last call, by their index along x and z. The game paints the baked
	/// shadows of these blocks in the same paint.
	[[nodiscard]] std::vector<glm::ivec2> TakePaintedBlocks() { return std::exchange(_paintedBlocks, {}); }

private:
	/// A thing with a footprint, as it was the last time
	struct Known
	{
		/// The map's cells it is filed in
		std::vector<uint32_t> cells;
		footprint_paint::Placement placement {};
		/// The lowest and highest x and z of the footprint on the ground
		std::pair<glm::vec2, glm::vec2> bounds {};
		/// The lowest and highest block, along x and z, that the footprint reaches
		glm::ivec2 lowBlock {0};
		glm::ivec2 highBlock {0};
	};
	/// The part of a known thing that the search compares. The search keeps it by the number of the entity,
	/// thus it reads one small row for each thing. A number with no known thing holds the null entity.
	struct KnownLook
	{
		entt::entity entity {entt::null};
		/// The number of the last search that saw it in the map's cells
		uint32_t seen {0};
		entt::id_type mesh {0};
		/// The count of the map's cells it is filed in, and the first of them
		uint32_t cellCount {0};
		uint32_t firstCell {0};
		/// How many times it went into the map's cells
		uint32_t filings {0};
		ecs::components::Transform transform {};
		/// How it is kept in the map's cells, and whether its model asks for a paint each time it goes in or comes out
		ecs::land_repaint::Filing filing {ecs::land_repaint::Filing::Outline};
		std::optional<ecs::land_repaint::Flag> flag;
	};
	/// A print that no thing in the map's cells gives: the print of a thing that shows only as a print, or a stretch of
	/// a river. A channel lowers the land's alpha; every other print mixes its colour into the land.
	struct LoosePrint
	{
		const L3DMesh::Footprint* footprint {nullptr};
		footprint_paint::Placement placement {};
		std::pair<glm::vec2, glm::vec2> bounds {};
		bool channel {false};
	};

	/// The component stores that the search and the paint read, found on the game's thread
	struct ThingStores;

	/// What paints a block: its things and its loose prints in the order that they paint it
	struct BlockPaint
	{
		std::vector<entt::entity> things;
		std::vector<const LoosePrint*> loose;
	};

	/// The footprint of a model, none for a model without one
	[[nodiscard]] const L3DMesh::Footprint* FootprintOf(entt::id_type mesh);
	/// The footprint of a model that FootprintOf found before, none for a model without one or not found before. It
	/// changes nothing, so the lanes of a pool can ask it at the same time.
	[[nodiscard]] const L3DMesh::Footprint* FoundFootprintOf(entt::id_type mesh) const;
	/// Takes the blocks that the land painted again. It gives false when there is no land to paint.
	[[nodiscard]] bool TakeLandChanges();
	/// Finds the changes of the rivers and the prints, and paints again the blocks that changed
	void PaintChanges(const ThingStores& stores);
	/// Finds the things with footprints that changed, and the blocks that take their footprints. It looks only at the
	/// marked things, or at all the things when there are no marks. It reads only the given stores and the registry's
	/// list of entities, thus a side thread can do it while the game's thread draws.
	void FindChangedThings(const ThingStores& stores, const std::optional<std::vector<uint32_t>>& marked);
	/// Finds the changes of the things with the marked entity numbers
	void FindMarkedThings(const ThingStores& stores, std::span<const uint32_t> marked);
	/// Finds the changes of all the things in the map's cells and of all the known things
	void FindChangedThingsInAll(const ThingStores& stores);
	/// Compares a thing in the map's cells that shows its footprint with what is known of it. A changed or new thing
	/// marks its blocks and becomes known. A thing whose model has no footprint does not become known.
	void LookAt(const ThingStores& stores, uint32_t search, entt::entity entity,
	            const ecs::components::MapCellResident& resident, const ecs::components::Mesh& mesh,
	            const ecs::components::Transform& transform);
	/// Takes the registry's marks of the things that can have changed since the last take. None when the search must
	/// look at all the things.
	[[nodiscard]] std::optional<std::vector<uint32_t>> TakeMarkedThings();
	/// Finds whether the rivers changed, and the blocks that take their footprints
	void FindChangedRivers();
	/// Finds whether the prints of the things that show only as prints changed, and the blocks that take them
	void FindChangedGroundPrints();
	/// Adds the blocks within the bounds of a footprint to the blocks to paint again in full
	void MarkBlocks(glm::ivec2 lowBlock, glm::ivec2 highBlock);
	/// Adds the blocks under a known thing's footprint to the blocks to paint again, with the part of each block that
	/// the footprint can paint
	void MarkThing(entt::entity entity, const Known& known);
	/// The part of a block to paint again, none when all of the block must be painted again. Only the changed things
	/// can change the texels of a part: the other things, the loose prints and the block's own texture stay the same.
	[[nodiscard]] std::optional<footprint_paint::Region> PartToPaint(size_t block, const BlockPaint& paint) const;
	/// Paints a part of the block's own texture with its footprints into the same part of texels, as PaintBlock paints
	/// all of it. It changes only that part of texels.
	void PaintBlockPart(size_t block, const BlockPaint& paint, const ThingStores& stores, const footprint_paint::Region& part,
	                    std::span<uint16_t> texels) const;
	/// Keeps a thing as known, with what the search compares of it
	void Learn(entt::entity entity, const KnownLook& look, Known&& known);
	/// Removes a known thing
	void Forget(entt::entity entity);
	/// The compared part of a known thing, none for a thing that is not known
	[[nodiscard]] const KnownLook* LookOf(entt::entity entity) const;
	/// Whether a known thing has the same cells as when it was seen last, and did not go into the cells again. It changes
	/// nothing.
	[[nodiscard]] bool SameCells(const KnownLook& look, const ecs::components::MapCellResident& resident) const;
	/// Whether a known thing has the same model and cells as when it was seen last, and did not go into the cells again.
	/// It changes nothing.
	[[nodiscard]] bool SameFiling(const KnownLook& look, entt::id_type mesh,
	                              const ecs::components::MapCellResident& resident) const;
	/// Whether a known thing has the same filing and transform as when it was seen last. It changes nothing.
	[[nodiscard]] bool SameLook(const KnownLook& look, entt::id_type mesh, const ecs::components::MapCellResident& resident,
	                            const ecs::components::Transform& transform) const;
	/// Marks the blocks under a known thing's footprint as blocks that keep an old footprint: their next paint paints all
	/// of them
	void MarkStale(const Known& known);
	/// Removes a known thing that came out of the map's cells. Its blocks are painted again if it asks for a paint as it
	/// comes out, else they keep its footprint.
	void TakeOff(entt::entity entity);
	/// Marks to paint again the blocks that the land painted again for another cause and that keep an old footprint
	void TakePaintedElsewhere();
	/// The things and loose prints that paint a block. It changes nothing.
	[[nodiscard]] BlockPaint FindBlockPaint(size_t block, const ThingStores& stores) const;
	/// Paints the block's own texture with its footprints into texels. It changes only the block's own texture that is
	/// kept, which must already have its place in the kept textures, and texels.
	void PaintBlock(size_t block, const BlockPaint& paint, const ThingStores& stores, std::span<uint16_t> texels);
	/// The threads that share the work of many blocks, made the first time that many blocks change at once
	[[nodiscard]] WorkerPool& Workers();
	/// How many shares of the given count of things the lanes of the pool take
	[[nodiscard]] size_t Shares(size_t count);
	/// Does the job for each share of the given count of things, on the lanes of the pool. Each share gets its number
	/// and its first and last place, after its last thing.
	void ForShares(size_t count, const std::function<void(size_t share, size_t from, size_t to)>& job);
	void Reset(size_t blockCount);

	/// The footprint of each model that the search found, as a copy. The game can make a model again with the same id,
	/// for example a damaged building or a temple that changes its shape, and remove the old model from the resources.
	/// The copy keeps the first footprint until the land is new, and the old model with its buffers on the graphics
	/// device can go.
	struct FoundFootprint
	{
		std::unique_ptr<const L3DMesh::Footprint> copy;
		const L3DMesh::Footprint* footprint {nullptr};
	};
	std::unordered_map<entt::id_type, FoundFootprint> _footprints;
	/// The known things by their entities. A look-up by an entity costs little, as the search does it for each thing that
	/// it looks at.
	entt::storage<Known> _known;
	/// The compared part of each known thing, by the number of its entity
	std::vector<KnownLook> _looks;
	/// The last model that FootprintOf found, as things of one model often come one after the other
	std::optional<std::pair<entt::id_type, const L3DMesh::Footprint*>> _lastFootprint;
	std::vector<LoosePrint> _rivers;
	/// The blocks that the updates asked to paint again, for TakeRepainted
	std::vector<size_t> _repainted;
	/// The prints of the things that show only as prints, the newest first
	std::vector<LoosePrint> _groundPrints;
	/// The things that show only as prints, as they were the last time
	std::vector<ecs::land_footprints::GroundPrintThing> _groundPrintThings;
	/// The points of each river, in the order of the rivers' numbers, as the rivers were the last time
	std::vector<std::vector<glm::vec3>> _riverPoints;
	/// Each block by its column and row on the land
	std::map<std::pair<int, int>, size_t> _blockAt;
	/// Whether each block holds footprints now
	std::vector<bool> _painted;
	/// Whether each block keeps an old footprint: of a thing at a size or a place that it does not have now, or of a thing
	/// that is gone. Such a block is painted in full the next time.
	std::vector<bool> _stale;
	/// The blocks that the land painted again for another cause since the last update
	std::set<std::pair<int, int>> _paintedElsewhere;
	/// The blocks that the updates painted since the last TakePaintedBlocks
	std::vector<glm::ivec2> _paintedBlocks;
	std::set<size_t> _dirty;
	/// The blocks of _dirty that must be painted again in full
	std::set<size_t> _wholeDirty;
	/// The part of each block of _dirty that the changed things can paint
	std::unordered_map<size_t, footprint_paint::Region> _dirtyParts;
	/// The things that changed since the last paint, in order of the entities' values after the search
	std::vector<entt::entity> _changedThings;
	/// The things that painted each block the last time, in the order that they painted it
	std::vector<std::vector<entt::entity>> _blockThings;
	/// The texels along a side of each block's texture
	int _side {footprint_paint::k_BlockTexels};
	/// The texels of each lane that paints a part of a block
	std::vector<std::vector<uint16_t>> _laneTexels;
	/// The own texture of each block that holds footprints, with no footprints, as the land paints it
	std::unordered_map<size_t, std::vector<uint16_t>> _ownTexels;
	/// The texels of a block that this thread paints alone, kept to use their memory again
	std::vector<uint16_t> _texels;
	std::unique_ptr<WorkerPool> _workers;
	/// The count of the searches for the changed things
	uint32_t _searches {0};
	/// The next search looks at all the things, as after a new land
	bool _lookAtAll {true};
	/// The registry whose marks the last search took
	const ecs::Registry* _markedRegistry {nullptr};
	/// The state of the search that StartSearch started
	enum class Search : uint8_t
	{
		/// No search started
		None,
		/// The search runs or ended on the side thread
		Started,
		/// There was no land to paint, thus no search started
		NoLand,
	};
	Search _search {Search::None};
	/// The last member, so that its thread stops before the other members go
	std::unique_ptr<SideThread> _searchThread;
};

} // namespace openblack::graphics
