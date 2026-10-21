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
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/FootprintPaint.h"
#include "3D/L3DMesh.h"
#include "ECS/Components/Transform.h"

namespace openblack::graphics
{

/// Paints the footprints of the buildings, the features and the rivers into the land's block textures, as Black & White
/// does: on the processor, texel by texel, each footprint over the one before it (see footprint_paint and
/// ecs::land_footprints). The game paints a block's footprints again each time a thing with a footprint over it comes
/// onto the map, goes from it or is built, and each time the land under the block changes height. This paints a block
/// again when a thing with a footprint over it goes into the map's cells or comes out of them, moves or turns, and when
/// the land paints the block again.
class LandFootprints
{
public:
	/// Paints again the blocks whose footprints changed since the last time
	void Update();

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
	/// A stretch of a river whose footprint the blocks take
	struct RiverFootprint
	{
		const L3DMesh::Footprint* footprint {nullptr};
		footprint_paint::Placement placement {};
		std::pair<glm::vec2, glm::vec2> bounds {};
		bool channel {false};
	};

	/// The footprint of a model, none for a model without one
	[[nodiscard]] const L3DMesh::Footprint* FootprintOf(entt::id_type mesh);
	/// Finds the things with footprints that changed, and the blocks that take their footprints
	void FindChangedThings();
	/// Finds whether the rivers changed, and the blocks that take their footprints
	void FindChangedRivers();
	/// Adds the blocks within the bounds of a footprint to the blocks to paint again
	void MarkBlocks(glm::ivec2 lowBlock, glm::ivec2 highBlock);
	void PaintBlock(size_t block);
	void Reset(size_t blockCount);

	std::unordered_map<entt::id_type, const L3DMesh::Footprint*> _footprints;
	std::unordered_map<entt::entity, Known> _known;
	std::vector<RiverFootprint> _rivers;
	/// The points of each river, in the order of the rivers' numbers, as the rivers were the last time
	std::vector<std::vector<glm::vec3>> _riverPoints;
	/// Each block by its column and row on the land
	std::map<std::pair<int, int>, size_t> _blockAt;
	/// Whether each block holds footprints now
	std::vector<bool> _painted;
	std::set<size_t> _dirty;
	/// The own texture of each block that holds footprints, with no footprints, as the land paints it
	std::unordered_map<size_t, std::vector<uint16_t>> _ownTexels;
	std::vector<uint16_t> _texels;
};

} // namespace openblack::graphics
