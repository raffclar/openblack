/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/storage.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/FootprintPaint.h"
#include "3D/L3DMesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/LandFootprintRules.h"

namespace openblack
{
class SideThread;
class WorkerPool;
} // namespace openblack

namespace openblack::graphics
{

/// Paints the footprints of the buildings, the features, the prints of things and the rivers into the land's block
/// textures, as Black & White does: on the processor, texel by texel, each footprint over the one before it (see
/// footprint_paint and ecs::land_footprints). The game paints a block's footprints again each time a thing with a
/// footprint over it comes onto the map, goes from it or is built, and each time the land under the block changes
/// height. This paints a block again when a thing with a footprint over it goes into the map's cells or comes out of
/// them, moves or turns, when a print of a thing changes, and when the land paints the block again.
class LandFootprints
{
public:
	LandFootprints();
	LandFootprints(const LandFootprints&) = delete;
	LandFootprints& operator=(const LandFootprints&) = delete;
	~LandFootprints();

	/// Paints again the blocks whose footprints changed since the last time
	void Update();
	/// Starts the search for the changed things on a thread of its own, before the next update needs it. The draw only
	/// reads the world, thus the thread reads it while the draw continues. The thread reads only the component stores
	/// that this thread finds before it starts. Call it only in a draw that calls Update.
	void StartSearch();
	/// Waits for the search that StartSearch started. The blocks that it found stay for the next update. Call it at the
	/// end of the draw.
	void ForgetSearch();
	/// The next update paints again each block that has footprints, as after the land painted all of its blocks again
	void PaintAgain();

private:
	/// A thing with a footprint, as it was the last time
	struct Known
	{
		entt::id_type mesh {0};
		/// The map's cells it is filed in
		std::vector<uint32_t> cells;
		ecs::components::Transform transform {};
		footprint_paint::Placement placement {};
		/// The lowest and highest block, along x and z, that the footprint reaches
		glm::ivec2 lowBlock {0};
		glm::ivec2 highBlock {0};
		bool seen {false};
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
	/// Finds the things with footprints that changed, and the blocks that take their footprints. It reads only the given
	/// stores and the registry's list of entities, thus a side thread can do it while the game's thread draws.
	void FindChangedThings(const ThingStores& stores);
	/// Finds whether the rivers changed, and the blocks that take their footprints
	void FindChangedRivers();
	/// Finds whether the prints of the things that show only as prints changed, and the blocks that take them
	void FindChangedGroundPrints();
	/// Adds the blocks within the bounds of a footprint to the blocks to paint again
	void MarkBlocks(glm::ivec2 lowBlock, glm::ivec2 highBlock);
	/// The things and loose prints that paint a block. It changes nothing.
	[[nodiscard]] BlockPaint FindBlockPaint(size_t block, const ThingStores& stores) const;
	/// Paints the block's own texture with its footprints into texels. It changes only the block's own texture that is
	/// kept, which must already have its place in the kept textures, and texels.
	void PaintBlock(size_t block, const BlockPaint& paint, const ThingStores& stores, std::span<uint16_t> texels);
	/// The threads that share the work of many blocks, made the first time that many blocks change at once
	[[nodiscard]] WorkerPool& Workers();
	void Reset(size_t blockCount);

	std::unordered_map<entt::id_type, const L3DMesh::Footprint*> _footprints;
	/// The known things by their entities. A look-up by an entity costs little, as the walk of the things does it for each
	/// thing in each frame.
	entt::storage<Known> _known;
	/// The last model that FootprintOf found, as things of one model often come one after the other
	std::optional<std::pair<entt::id_type, const L3DMesh::Footprint*>> _lastFootprint;
	std::vector<LoosePrint> _rivers;
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
	std::set<size_t> _dirty;
	/// The own texture of each block that holds footprints, with no footprints, as the land paints it
	std::unordered_map<size_t, std::vector<uint16_t>> _ownTexels;
	/// The texels of a block that this thread paints alone, kept to use their memory again
	std::vector<uint16_t> _texels;
	std::unique_ptr<WorkerPool> _workers;
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
