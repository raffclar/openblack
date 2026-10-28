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

#include <glm/vec2.hpp>

namespace openblack
{

/// How Black & White paints a building's or a feature's footprint into the land's block textures. The footprint is a
/// small picture (16 to 128 texels a side, 4 bits a channel) that a few triangles lay on the ground. The game paints it
/// into the texels of each block that it covers, on the processor, at 16 texels a cell:
///
/// - each corner of a triangle goes to the nearest texel of the block, and each edge is walked from corner to corner,
///   one texel at a time along its longer axis;
/// - each row of the block's texture (one texel along x) is filled from the lowest to the highest texel that an edge
///   reached on it, both ends too;
/// - each texel takes the footprint texel nearest to the picture coordinates there, kept inside the picture;
/// - that texel's 4-bit alpha, times 16, out of 255, mixes its colour into the land's, each 4-bit channel rounded
///   down. The land keeps its own alpha (its coast fade).
///
/// The rivers' channels use the same triangles, but they lower the land's alpha to the picture's alpha and keep its
/// colour. Each footprint is painted over the one before it, so each step rounds. The land is filtered only when it is
/// drawn.
namespace footprint_paint
{
/// Texels along a side of a block's texture
constexpr int k_BlockTexels = 256;
/// Units along a side of a block
constexpr float k_BlockUnits = 160.0f;
/// One 160th, as the game keeps it in a float: a unit as a part of the side of a block
constexpr float k_UnitPerBlock = 0.00625f;

/// How much a footprint texel of a 4-bit alpha covers the land under it, out of 255. Even the most opaque leaves a
/// little of the land showing.
[[nodiscard]] constexpr int Coverage(uint8_t alpha4)
{
	return alpha4 * 16;
}

/// A 4-bit channel of the land with a footprint texel's channel mixed into it by that texel's 4-bit alpha
[[nodiscard]] constexpr uint8_t MixChannel(uint8_t land4, uint8_t footprint4, uint8_t alpha4)
{
	const int coverage = Coverage(alpha4);
	return static_cast<uint8_t>((land4 * (255 - coverage) + footprint4 * coverage) / 255);
}

/// A land texel (ARGB, 4 bits a channel) with a footprint texel painted over it. The land keeps its alpha.
[[nodiscard]] uint16_t Paint(uint16_t land, uint16_t footprint);

/// A land texel with a river channel's texel painted over it: the land's alpha falls to the channel's where that is
/// lower, and the land keeps its colour
[[nodiscard]] uint16_t LowerAlpha(uint16_t land, uint16_t channel);

/// What a footprint does to the land under it
enum class Mix : uint8_t
{
	/// Mix the footprint's colour into the land's
	Colour,
	/// Lower the land's alpha to the footprint's, as a river's channel does
	LowerAlpha,
};

/// A footprint's picture: ARGB texels at 4 bits a channel, a row of `width` texels for each texel along v
struct Picture
{
	int width {0};
	int height {0};
	std::span<const uint16_t> texels;
};

/// A triangle of a footprint: its corners on the ground, along the model's x and z, and in the picture's texels
struct Triangle
{
	std::array<glm::vec2, 3> corners;
	std::array<glm::vec2, 3> texels;
};

/// The parts of a placed thing's matrix that move a point of its footprint on the ground: the world's x is
/// `xFromX * x + xFromZ * z + origin.x`, the world's z is `zFromX * x + zFromZ * z + origin.y`
struct Placement
{
	float xFromX {1.0f};
	float xFromZ {0.0f};
	float zFromX {0.0f};
	float zFromZ {1.0f};
	glm::vec2 origin {0.0f};
};

/// Paints a footprint into a block's texture. `block` holds side by side ARGB texels at 4 bits a channel, a row for each
/// texel along x ([x * side + z]). The side is k_BlockTexels, or 128 at the low resolution of the lowest detail levels.
/// The footprint's corners then go to the nearest texel of the smaller texture, and its picture stays the same.
/// `blockIndex` is the block's column and row on the land, along x and z. The parts of the footprint outside the block
/// are not painted.
void PaintFootprint(std::span<uint16_t> block, glm::ivec2 blockIndex, const Picture& picture,
                    std::span<const Triangle> triangles, const Placement& placement, Mix mix, int side = k_BlockTexels);
} // namespace footprint_paint

} // namespace openblack
