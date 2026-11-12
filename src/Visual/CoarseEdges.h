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

#include <array>
#include <bitset>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// The cells round the edges of the blocks' coarse textures.
///
/// The game samples a block's coarse texture from the block's painted texture each time that it paints the block. The
/// two texels round each edge are shared with the neighbouring block. After the samples, the game mixes each shared edge
/// with the texels that the neighbour keeps from its own last paint, in the order z - 1, z + 1, x - 1, x + 1. Thus an
/// edge keeps a running mix: each paint of either block halves the old mix with its new sample. The 2 x 2 texels at a
/// corner go through the mix along z and then along x, so they depend on the order of the paints.
///
/// The renderer keeps the cells in a texture and a pass on the GPU makes them again at each frame with paints
/// (fs_land_coarse_edges). CellAfterFrame is the rule of that pass.
namespace openblack::graphics::coarse_edges
{

/// Blocks along each side of the map
inline constexpr int32_t k_BlocksPerSide = 32;
inline constexpr size_t k_Blocks = static_cast<size_t>(k_BlocksPerSide) * k_BlocksPerSide;

/// The texture of the cells: a row for each place of the map, and a run of texels for each side of the block in the
/// order z - 1, z + 1, x - 1, x + 1
inline constexpr uint16_t k_SideTexels = 64;
inline constexpr uint16_t k_TextureWidth = k_SideTexels * 4;
inline constexpr uint16_t k_TextureHeight = static_cast<uint16_t>(k_Blocks);

/// The 4-bit levels of a texel, 0 to 15: red, green, blue and alpha
using Levels = glm::u8vec4;

/// Two texels mixed: each 4-bit level is the half of the sum, rounded down
[[nodiscard]] constexpr Levels Mix(Levels a, Levels b)
{
	return {static_cast<uint8_t>((a.r + b.r) >> 1), static_cast<uint8_t>((a.g + b.g) >> 1),
	        static_cast<uint8_t>((a.b + b.b) >> 1), static_cast<uint8_t>((a.a + b.a) >> 1)};
}

/// Whether a block's place, x and z, is on the map
[[nodiscard]] constexpr bool OnMap(glm::ivec2 block)
{
	return block.x >= 0 && block.y >= 0 && block.x < k_BlocksPerSide && block.y < k_BlocksPerSide;
}

/// The index of a place of the map, x * 32 + z
[[nodiscard]] constexpr size_t PlaceOf(glm::ivec2 block)
{
	return static_cast<size_t>((block.x * k_BlocksPerSide) + block.y);
}

/// A request for a paint of a block: the block's index in the land's blocks, and the middle of the block on the ground
struct PaintRequest
{
	size_t block;
	glm::vec2 centre;
};

/// The order in which the game paints the blocks that waited for a paint in a frame. The nearest block to the camera
/// goes first. The camera's distance is to the middle of the block at height 0. Of two blocks at the same distance, the
/// block that came later in the requests goes first. A block that comes again keeps the place of its first request.
[[nodiscard]] std::vector<size_t> PaintOrder(std::span<const PaintRequest> requests, glm::vec3 camera);

/// The paint of a place of the map in a frame
struct FramePaint
{
	/// The place of the paint in the order of the frame's paints, 1 for the first; 0 when the block gets no paint
	uint16_t rank {0};
	/// Whether the block got a paint before the frame
	bool paintedBefore {false};
	/// The block's index in the land's blocks; none when the place has no block
	std::optional<uint16_t> block;
};
using FramePaints = std::array<FramePaint, k_Blocks>;

/// The paints of a frame. places gives the place of each of the land's blocks. order gives the blocks painted, in the
/// order of the paints. paintedBefore gives the places whose blocks got a paint before the frame.
[[nodiscard]] FramePaints MakeFramePaints(std::span<const glm::ivec2> places, std::span<const size_t> order,
                                          const std::bitset<k_Blocks>& paintedBefore);

/// The paints as the texture that the pass reads: four floats for each place, a row for each x and a column for each
/// z. They are the rank, 1 when the block got a paint before, and the block's index plus 1 (0 for no block), then 0.
[[nodiscard]] std::vector<float> FramePaintTexels(const FramePaints& paints);

/// A cell of a block's coarse texture: z and x, 0 to samples - 1
using Cell = glm::ivec2;
/// The levels of a cell of a block: as the texture keeps them before the frame, or as a new sample of a paint
using CellLevels = std::function<Levels(glm::ivec2 block, Cell cell)>;

/// A cell round the edge of a block's coarse texture after the paints of a frame. A cell that is not at a corner
/// changes only at the paints of its block and of the neighbour across its edge. The cells at a corner of four blocks
/// change only at the paints of these four blocks. The cell finds its value from these paints, in their order.
[[nodiscard]] Levels CellAfterFrame(const FramePaints& paints, int32_t samples, glm::ivec2 block, Cell cell,
                                    const CellLevels& kept, const CellLevels& fresh);

/// The texel of the texture that keeps a cell round the edge of a block. The texture keeps a corner cell only in the run
/// of its side along z.
[[nodiscard]] glm::ivec2 EdgeTexel(glm::ivec2 block, Cell cell, int32_t samples);

/// Where the land shader takes a block's coarse texture from
enum class CoarseTexture : uint8_t
{
	/// The samples of the block's painted texture, with the edges from the paints
	Samples = 0,
	/// The block's square of its page in the landscape file: the coarse land draws all blocks from the pages
	CoarseLand = 1,
	/// The block's square of its page as the landscape file keeps it. The game loads the pages into the coarse
	/// textures, and the first paint of a block samples its painted texture into its square.
	FilePage = 2,
};

/// Where a block's coarse texture comes from. hasPage tells if the land has the block's page, and painted tells if
/// the block got its first paint.
[[nodiscard]] constexpr CoarseTexture CoarseTextureOf(bool coarseLand, bool hasPage, bool painted)
{
	if (!hasPage)
	{
		return CoarseTexture::Samples;
	}
	if (coarseLand)
	{
		return CoarseTexture::CoarseLand;
	}
	return painted ? CoarseTexture::Samples : CoarseTexture::FilePage;
}

} // namespace openblack::graphics::coarse_edges
