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
#include <functional>
#include <span>

#include <LNDFile.h>
#include <glm/vec2.hpp>

namespace openblack
{
class WorkerPool;

/// The texture Black & White paints each block of land with: 16 by 16 texels a cell, 4 bits a channel. Each texel
/// takes the materials of its country for its height, lifted by the land's noise, shaded by its bump map, and an alpha
/// that fades the land into the sea drawn under it towards the coast. Where a cell's corners lie in different
/// countries, each corner's country paints the texel and the results are blended.
namespace block_texture
{
constexpr int k_TexelsPerCell = 16;
/// Texels along a side of a block, and of the land's material, noise and bump maps
constexpr int k_Side = 256;
constexpr int k_CellsPerSide = 17;
/// The texels a block's texture takes at the low resolution of the lowest detail levels: 8 by 8 a cell
constexpr int k_LowSide = 128;

/// How finely the game paints a block's texture
enum class Resolution : uint8_t
{
	/// 16 by 16 texels a cell
	Full,
	/// 8 by 8 texels a cell. Each texel is the texel of the full texture at twice its place along each axis.
	Low,
};

/// Texels along a side of a block's texture at a resolution
[[nodiscard]] constexpr int SideOf(Resolution resolution)
{
	return resolution == Resolution::Low ? k_LowSide : k_Side;
}

/// Texels along a side of a cell at a resolution
[[nodiscard]] constexpr int TexelsPerCellOf(Resolution resolution)
{
	return SideOf(resolution) / (k_Side / k_TexelsPerCell);
}

/// The RGBA8 bytes of a block's texture at a resolution
[[nodiscard]] constexpr size_t BlockBytesOf(Resolution resolution)
{
	return static_cast<size_t>(SideOf(resolution)) * static_cast<size_t>(SideOf(resolution)) * 4;
}
/// The cells a block's texture covers along each side
constexpr int k_BlockCells = k_Side / k_TexelsPerCell;

/// How much each of a cell's four corners counts for each of its 16 by 16 texels, adding up to 255. A corner weighs
/// 14 less the texel's distance from it, nothing beyond. Indexed [i * 16 + j] with i along x and j along z; corners
/// (x, z), (x, z + 1), (x + 1, z + 1) and (x + 1, z).
using ConeWeights = std::array<std::array<uint8_t, 4>, k_TexelsPerCell * k_TexelsPerCell>;
[[nodiscard]] ConeWeights ComputeConeWeights();

/// The coast alpha, 0 to 15, of a texel of weighted altitude h (255 times the altitude on a flat cell) and noise n:
/// clear below altitude 1, opaque from altitude 4 up, and in between rising with the noise-shifted height
[[nodiscard]] uint8_t CoastAlpha(int32_t h, uint8_t noise);

/// A texel of weighted altitude h painted by one country, as an ARGB4444 word. materials are the land's material
/// textures, k_Side by k_Side B5G5R5A1 words each, one after another; texelIndex is x * k_Side + z in the block, the
/// same in the materials, the noise and the bump map.
[[nodiscard]] uint16_t CountryTexel(const lnd::LNDCountry& country, int32_t h, uint8_t noise, uint8_t bump,
                                    std::span<const uint16_t> materials, size_t texelIndex);

/// Four texels, one per corner country, blended by the cone weights in each 4-bit channel, alpha too
[[nodiscard]] uint16_t BlendCorners(const std::array<uint16_t, 4>& texels, const std::array<uint8_t, 4>& weights);

/// What a block's texture is painted from
struct Sources
{
	std::span<const lnd::LNDCountry> countries;
	std::span<const uint16_t> materials;
	/// k_Side by k_Side, [x * k_Side + z], the same for every block
	std::span<const uint8_t> noise;
	std::span<const uint8_t> bump;
	/// The resolution of the texture to paint. The maps above always hold k_Side by k_Side texels.
	Resolution resolution {Resolution::Full};
};

/// The bytes of a block's texture: k_Side by k_Side RGBA8 texels
constexpr size_t k_BlockBytes = static_cast<size_t>(k_Side) * k_Side * 4;

/// Paints a block from its 17 by 17 cells ([x * 17 + z]) into rgba, BlockBytesOf(sources.resolution) long: each 4-bit channel
/// times 17, a row for each texel along x. The open sea's cells, which aren't drawn, are left clear.
void BuildBlock(std::span<const lnd::LNDCell> cells, const Sources& sources, std::span<uint8_t> rgba);
/// Paints a block as BuildBlock does, into texels as ARGB words at 4 bits a channel, SideOf(sources.resolution) on each
/// side, a row for each texel along x
void BuildBlockTexels(std::span<const lnd::LNDCell> cells, const Sources& sources, std::span<uint16_t> texels);
/// Paints the texels of a rectangle of a block's cells as PaintCells does, into ARGB words at 4 bits a channel
void PaintCellTexels(std::span<const lnd::LNDCell> cells, const Sources& sources, glm::ivec2 firstCell, glm::ivec2 cellCount,
                     std::span<uint16_t> texels);
/// ARGB words at 4 bits a channel as RGBA8 texels, each 4-bit channel times 17
void ToRgba(std::span<const uint16_t> texels, std::span<uint8_t> rgba);
/// Paints the texels of a rectangle of a block's cells, cellCount (along x, along z) from firstCell, as BuildBlock
/// would, into rgba: a row for each texel along x, each row the rectangle's texels along z, so the rectangle can replace
/// that part of the block's texture
void PaintCells(std::span<const lnd::LNDCell> cells, const Sources& sources, glm::ivec2 firstCell, glm::ivec2 cellCount,
                std::span<uint8_t> rgba);

/// The cells of a block by its index
using CellsOfBlock = std::function<std::span<const lnd::LNDCell>(size_t block)>;
/// The memory of a block's texture, BlockBytesOf(sources.resolution) long, that BuildBlocks paints into. The lanes of the pool call it, so it
/// must only give memory that is ready for the block.
using MemoryOfBlock = std::function<std::span<uint8_t>(size_t block)>;

/// Paints the blocks as BuildBlock does, each into its own memory, and shares the work among the pool's lanes. It
/// returns when it painted all the blocks.
void BuildBlocks(size_t count, const CellsOfBlock& cellsOf, const Sources& sources, WorkerPool& pool,
                 const MemoryOfBlock& into);
} // namespace block_texture
} // namespace openblack
