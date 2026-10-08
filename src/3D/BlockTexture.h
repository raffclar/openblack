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
#include <span>
#include <vector>

#include <LNDFile.h>
#include <glm/vec2.hpp>

namespace openblack
{
class LandIslandInterface;

/// The texture Black & White paints each block of land with: 16 by 16 texels a cell, 4 bits a channel. Each texel
/// takes the materials of its country for its height, lifted by the land's noise, shaded by its bump map, and an alpha
/// that fades the land into the sea drawn under it towards the coast (coast::CoastAlphaNibble). Where a cell's corners
/// lie in different countries, each corner's country paints the texel and the results are blended. No render
/// dependencies.
namespace block_texture
{
/// Cells along a block side, and the corners a block keeps along a side (its own plus the next block's first)
constexpr int k_CellsPerBlock = 16;
constexpr int k_CellsPerSide = 17;

/// The LND material textures, each 256 x 256 raw B5G5R5 words [x * 256 + z] (LNDMaterial::texels), one after another
struct Materials
{
	std::span<const uint16_t> texels;
	size_t count;
};

/// A texel of weighted altitude h (255 times the altitude on a flat cell) painted by one country, as an ARGB4444 word:
/// clear below altitude 1, else the country's materials for min((h >> 8) + noise, 255), the first weighing the
/// coefficient out of 256 and the second the rest, shaded by bump / 256 and saturated at 15 a channel, with the coast
/// alpha. texelIndex is x * 256 + z in the block, the same in the materials, the noise and the bump map.
uint16_t CountryTexel(const lnd::LNDCountry& country, int32_t h, uint8_t noise, uint8_t bump, const Materials& materials,
                      size_t texelIndex);

/// Four texels, one per corner country, blended by the cone weights in each 4-bit channel, alpha too:
/// floor(sum(channel_k w_k) / 255)
uint16_t BlendCorners(const std::array<uint16_t, 4>& texels, const std::array<uint8_t, 4>& weights);

/// What a block's texture is painted from: the countries, the material textures, the LND noise and bump maps (256 x
/// 256, [x * 256 + z], the same for every block) and the island's altitude bits (LNDCell::Altitude)
struct Sources
{
	std::span<const lnd::LNDCountry> countries;
	Materials materials;
	std::span<const uint8_t> noise;
	std::span<const uint8_t> bump;
	uint8_t altitudeBits {8};
};

/// The cells of a block to paint, from min to max inclusive on each axis, each 0 to 15 (x along the block's x, y along
/// its z)
struct CellBox
{
	glm::ivec2 min {0, 0};
	glm::ivec2 max {k_CellsPerBlock - 1, k_CellsPerBlock - 1};
};

/// Where a block's texels go: an RGBA8 image `width` texels wide, rows along z and columns along x, in which the
/// block's first texel is at (origin.x, origin.y); texelsPerBlock texels per block side
struct Target
{
	std::span<uint8_t> rgba;
	size_t width {0};
	glm::ivec2 origin {0, 0};
	uint16_t texelsPerBlock {0};
};

/// The texels of a box of cells, from texel `min` to texel `max` inclusive (block texels, x then z); empty when the
/// box holds no texel at this texel density
struct TexelBox
{
	glm::ivec2 min {0, 0};
	glm::ivec2 max {-1, -1};
	[[nodiscard]] bool Empty() const { return max.x < min.x || max.y < min.y; }
};
[[nodiscard]] TexelBox TexelsOf(const CellBox& box, uint16_t texelsPerBlock);

/// Paints the cells of `box` of one block from its 17 x 17 cells ([x * 17 + z]) into the target, RGBA8 with each
/// nibble x 17. With fewer than 256 texels per block side each texel takes the original's texel under its corner.
/// The open sea's cells, which aren't drawn, and an island without countries are left clear. A texel only
/// reads the four corners of its own cell, so a box painted again after its corners changed gives the same texels
/// as painting the whole island again.
void BuildBlock(std::span<const lnd::LNDCell> cells, const Sources& sources, const Target& target, const CellBox& box = {});

/// The cells a corner of a block (0 to 16 on each axis, 16 being the next block's first) is a corner of: the one
/// before it and its own along each axis, within the block
[[nodiscard]] CellBox CellsOfCorner(glm::ivec2 corner);
/// The smallest box holding both
[[nodiscard]] CellBox Union(const CellBox& a, const CellBox& b);

/// The texels of a box of cells painted on their own: the box's block texels and their RGBA8 rows (along z, each
/// texels.max.x - texels.min.x + 1 texels wide), ready to replace that rectangle of the block's texture
struct Patch
{
	TexelBox texels;
	std::vector<uint8_t> rgba;
};
[[nodiscard]] Patch PaintBox(std::span<const lnd::LNDCell> cells, const Sources& sources, uint16_t texelsPerBlock,
                             const CellBox& box);

/// The whole island, RGBA8 (each nibble x 17), rows along z and columns along x (row 0 = the lowest z of the block
/// extent), texelsPerBlock texels per block side: BuildBlock for every block in the extent
std::vector<uint8_t> BuildIslandBlockTexture(const LandIslandInterface& island, glm::u16vec2 extentIndexMin,
                                             glm::u16vec2 indexSize, uint16_t texelsPerBlock, std::span<const uint8_t> noise,
                                             std::span<const uint8_t> bump, const Materials& materials);
} // namespace block_texture
} // namespace openblack
