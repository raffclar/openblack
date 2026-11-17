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

#include <array>
#include <memory>
#include <optional>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "LandIslandInterface.h"
#include "SharedIndexLists.h"
#include "Visual/LandDetail.h"

namespace openblack
{

namespace lnd
{
struct LNDBlock;
struct LNDCell;
} // namespace lnd

namespace graphics
{
class IndexBuffer;
class VertexBuffer;
} // namespace graphics

/// A corner of a land cell as it is drawn
struct LandVertex
{
	/// Where the corner is drawn in its block: the land at sea level is drawn flat at height 0
	glm::vec3 position;
	/// The corner's own altitude, 0 to 255, before the land at sea level is flattened. The small bump detail and the
	/// shadows on the land go by it: they show on the flattened land of altitudes 2 and 3 but not below.
	float altitude;
};

/// The vertex of a cell corner at cornerPosition (x and z in its block) of the given altitude
[[nodiscard]] LandVertex MakeLandVertex(glm::vec2 cornerPosition, uint16_t altitude);

class LandIslandInterface;

class LandBlock
{
public:
	static constexpr uint16_t k_VertexCount = graphics::land_detail::k_CornerCount;

	LandBlock();
	LandBlock(LandBlock&&) noexcept;
	LandBlock& operator=(LandBlock&&) noexcept;
	~LandBlock();
	/// Fills the block's k_VertexCount vertices, its corners [x * 17 + z]
	void BuildMesh(LandIslandInterface& island, std::span<LandVertex> vertices);
	/// Where the triangles of a block are: a run of indices of the island's shared index pages
	using TriangleRun = SharedIndexLists<graphics::IndexBuffer>::Range;

	/// The block's triangles for a mesh and the sides it is stitched to coarser neighbours on, as indices of its
	/// corners. Open-sea cells are left out only when dropOpenSea. Each set is made the first time it is asked for, and
	/// shared with every other block of the island that draws the same triangles. A set with no triangles gives a run
	/// with no page, and the block then has nothing to draw (all of its cells are open sea and dropOpenSea is true).
	[[nodiscard]] TriangleRun GetTriangles(graphics::land_detail::Mesh mesh, uint8_t coarserSides, bool dropOpenSea) const;
	/// Where the block's vertices are on the GPU: a run of the vertex buffer the island's blocks share, so a land of a
	/// thousand blocks takes one of bgfx's buffers rather than a thousand
	void SetVertices(const graphics::VertexBuffer& buffer, uint32_t firstVertex);
	/// Where the block's triangle sets are kept: with the island's, so that blocks drawing the same triangles share one
	/// run of the island's index pages. It must outlive the block's draws.
	void SetTriangleSets(SharedIndexLists<graphics::IndexBuffer>& sets);
	/// Binds the block's vertices for a draw
	void BindVertices() const;
	[[nodiscard]] const lnd::LNDCell* GetCells() const;
	/// The heights in units of the block's corners as its vertices were last built with them, [x * 17 + z]
	[[nodiscard]] const std::array<uint16_t, graphics::land_detail::k_CornerCount>& GetCornerAltitudes() const
	{
		return _cornerAltitudes;
	}
	[[nodiscard]] glm::ivec2 GetBlockPosition() const;
	[[nodiscard]] glm::vec2 GetMapPosition() const;
	[[nodiscard]] const std::unique_ptr<lnd::LNDBlock>& GetLndBlock() const { return _block; };
	void SetLndBlock(const lnd::LNDBlock& block);
	/// Sets the altitude of one of the block's 17 by 17 cells, [x * 17 + z]. Returns true when the altitude changed.
	bool SetCellAltitude(size_t index, uint8_t altitude);
	/// Sets the country of one of the block's 17 by 17 cells, [x * 17 + z]. Returns true when the country changed.
	bool SetCellCountry(size_t index, uint8_t country);
	/// The highest height in units of the block's cells when the land was loaded: the highest altitude on a land of the
	/// story. A later change of the cells' altitudes does not change it. Every view test of the block uses this box top.
	[[nodiscard]] uint16_t GetHighestAltitude() const noexcept { return _highestAltitude; }
	/// Sets the box top of a block of a land with a height field
	void SetHighestUnits(uint16_t units) noexcept { _highestAltitude = units; }

private:
	/// Every mesh, coarser sides and open-sea choice
	static constexpr size_t k_TriangleSets = 3 * 16 * 2;

	std::unique_ptr<lnd::LNDBlock> _block;
	/// The block's sets once found among the island's. No value: the set is not found yet. A run with no page: the set
	/// has no triangles.
	mutable std::array<std::optional<TriangleRun>, k_TriangleSets> _triangles {};
	SharedIndexLists<graphics::IndexBuffer>* _triangleSets {nullptr};
	const graphics::VertexBuffer* _vertices {nullptr};
	uint32_t _firstVertex {0};
	/// The place of the block on the grid of blocks and on the map. The block keeps a copy, so the checks of each frame
	/// do not read the cells of the block.
	glm::ivec2 _blockPosition {-1};
	glm::vec2 _mapPosition {0.0f};
	uint16_t _highestAltitude {0};
	/// The corners' altitudes of the last build of the vertices. The land works out the drawn altitudes of a block that
	/// morphs from them each frame.
	std::array<uint16_t, graphics::land_detail::k_CornerCount> _cornerAltitudes {};

	void BuildVertexList(std::span<LandVertex> vertices, LandIslandInterface& island);
};
} // namespace openblack
