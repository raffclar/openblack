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
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Graphics/LandDetail.h"
#include "LandIslandInterface.h"

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
	/// The altitudes the corner morphs towards beyond the land's detail lines: on the half mesh, then on the quarter
	/// mesh (see graphics::land_detail::TargetsAt)
	float halfAltitude;
	float quarterAltitude;
};

/// The vertex of a cell corner at cornerPosition (x and z in its block) of the given altitude
[[nodiscard]] LandVertex MakeLandVertex(glm::vec2 cornerPosition, uint8_t altitude);

class LandIslandInterface;

class LandBlock
{
public:
	// 16*16 cells, drawn from their 17*17 corners
	static constexpr auto k_Resolution = glm::u8vec2(16, 16);
	static constexpr uint16_t k_VertexCount = graphics::land_detail::k_CornerCount;

	LandBlock();
	LandBlock(LandBlock&&) noexcept;
	LandBlock& operator=(LandBlock&&) noexcept;
	~LandBlock();
	/// Fills the block's k_VertexCount vertices, its corners [x * 17 + z]
	void BuildMesh(LandIslandInterface& island, std::span<LandVertex> vertices);
	/// The block's triangles for a mesh and the sides it is stitched to coarser neighbours on, as indices of its
	/// corners. Open-sea cells are left out only when dropOpenSea. Each set is made the first time it is asked for.
	[[nodiscard]] const graphics::IndexBuffer& GetTriangles(graphics::land_detail::Mesh mesh, uint8_t coarserSides,
	                                                        bool dropOpenSea) const;
	/// Where the block's vertices are on the GPU: a run of the vertex buffer the island's blocks share, so a land of a
	/// thousand blocks takes one of bgfx's buffers rather than a thousand
	void SetVertices(const graphics::VertexBuffer& buffer, uint32_t firstVertex);
	/// Binds the block's vertices for a draw
	void BindVertices() const;
	[[nodiscard]] const lnd::LNDCell* GetCells() const;
	[[nodiscard]] glm::ivec2 GetBlockPosition() const;
	[[nodiscard]] glm::vec2 GetMapPosition() const;
	[[nodiscard]] const std::unique_ptr<lnd::LNDBlock>& GetLndBlock() const { return _block; };
	void SetLndBlock(const lnd::LNDBlock& block);
	/// Sets the altitude of one of the block's 17 by 17 cells, [x * 17 + z]
	void SetCellAltitude(size_t index, uint8_t altitude);

private:
	/// Every mesh, coarser sides and open-sea choice
	static constexpr size_t k_TriangleSets = 3 * 16 * 2;

	std::unique_ptr<lnd::LNDBlock> _block;
	mutable std::array<std::unique_ptr<graphics::IndexBuffer>, k_TriangleSets> _triangles;
	const graphics::VertexBuffer* _vertices {nullptr};
	uint32_t _firstVertex {0};

	void BuildVertexList(std::span<LandVertex> vertices, LandIslandInterface& island);
};
} // namespace openblack
