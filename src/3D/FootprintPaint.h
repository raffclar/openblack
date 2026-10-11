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

#include <glm/vec2.hpp>

#include "BlockTexture.h"

namespace openblack
{

/// How Black & White paints a building's or a feature's footprint into the land's block textures. The footprint is a
/// small texture (16 to 64 texels a side, 4 bits a channel) laid on the ground by a few triangles. It is painted into
/// the block's own texels, so it shows at the land's resolution, 16 texels a cell, whatever its own:
///
/// - each triangle's corners are moved to the nearest corner of a land texel, and the footprint's coordinates are
///   spread linearly over the moved triangle;
/// - each land texel takes the footprint texel nearest to the coordinates at its corner (rounded, kept inside the
///   footprint), unfiltered;
/// - that texel's 4-bit alpha, times 16, out of 255, mixes its colour into the land's, each 4-bit channel rounded
///   down; the land's own alpha (its coast fade) is kept.
///
/// The land's texture, footprints and all, is filtered only when it is drawn. The shaders vs_footprint_paint,
/// fs_footprint and fs_terrain do this on the GPU; these functions are the same rules, for tests.
namespace footprint_paint
{
/// Land texels a unit of the world: a block's 256 texels across its 160 units
constexpr float k_TexelsPerUnit = static_cast<float>(block_texture::k_Side) / 160.0f;

/// Where a footprint's corner at a point on the ground is painted from: the nearest corner of a land texel, in texels
/// from the world's origin
[[nodiscard]] glm::vec2 SnapToTexelCorner(glm::vec2 world);

/// The footprint texel along one axis that a land texel takes, from the footprint coordinate at its corner (in
/// footprint texels): the nearest, kept inside the footprint
[[nodiscard]] int FootprintTexel(float coordinate, int size);

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

/// The same mix as the GPU does it: the footprint layer is kept as a premultiplied colour and a coverage (each in
/// [0, 1], stored at 16 bits), the land's 4-bit channel is mixed with them in floating point and rounded down to 4 bits
/// again. Returns the 4-bit channel.
[[nodiscard]] uint8_t MixChannelAsDrawn(uint8_t land4, float premultiplied, float coverage);
} // namespace footprint_paint

} // namespace openblack
