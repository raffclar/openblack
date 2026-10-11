/*******************************************************************************
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
#include <glm/vec3.hpp>

namespace openblack::graphics
{

/// The shadows of trees and big forests, which Black & White bakes into the land's textures as it paints each land
/// block, from its fixed far sun. They never move: they don't sway with the trees, and change only when a tree is
/// planted, grows, is felled or taken away.
///
/// Every triangle of the tree's nearest level of detail (not the parts moved by bones, nor the physics' own parts) is
/// flattened from the sun onto the flat plane through the tree's foot: a point's height above the foot (nothing below
/// it) sends it along the line from the sun, as far again as height / (sun height - height) of the way. The flattened
/// triangle is scanned into the block's 256 by 256 texels: each edge is stepped a texel at a time between its corners
/// taken down to whole texels, and every texel of a row from the edges' first to their last is filled. A filled texel
/// takes the alpha of the tree's texture there, read from 64 by 64 places over the texture (the place a whole step on
/// from the nearest), and keeps the alpha's four bits joined with whatever covered it before, so where shadows overlap
/// the land gets no darker. The land's 4-bit colour is then scaled by 255 less half of an eighth of 255 for each bit
/// set, over 256 and rounded down: a fully solid leaf darkens it by about a quarter.
///
/// A block takes the shadows of the trees standing within 4 cells before it and 5 after it, along both x and z, so a
/// shadow cast further than that across a block's edge is cut off there.
namespace tree_shadows
{

/// The game's fixed far sun
inline constexpr glm::vec3 k_Sun {-500000.0f, 500000.0f, -500000.0f};
/// A land block's side, in units and in texels of its texture
inline constexpr float k_BlockSize = 160.0f;
inline constexpr int k_BlockTexels = 256;
/// A block's texels, a row along x at a time, each down z
using Coverage = std::array<uint8_t, k_BlockTexels * k_BlockTexels>;
/// The places over a texture its alpha is read from, along each side
inline constexpr int k_MaskSide = 64;
/// A texture's alpha in its top four bits at each of 64 by 64 places, by rows down the texture
using AlphaMask = std::array<uint8_t, k_MaskSide * k_MaskSide>;
/// The cells along each side of a block, and how many before and after it its trees' shadows are taken from
inline constexpr int k_BlockCells = 16;
inline constexpr int k_CellsBefore = 4;
inline constexpr int k_CellsAfter = 5;

/// The mask of a texture of 4-bit channels, alpha in each texel's top 4 bits, by rows of width texels: the texel at the
/// start of each of the 64 parts along each side
[[nodiscard]] AlphaMask MaskOf(std::span<const uint16_t> texels, uint32_t width, uint32_t height);

/// How a compressed texture keeps its alpha
enum class Compression : uint8_t
{
	/// Each texel solid or a hole
	Dxt1,
	/// Four bits of alpha a texel
	Dxt3,
	/// Alpha blended between two values a block
	Dxt5,
};

/// The mask of a compressed texture: its alpha as it is turned into 4 bits a channel (a hole none, solid all, four
/// bits kept as they are, a blend cut down to its top four bits), read as MaskOf reads it
[[nodiscard]] AlphaMask MaskOfCompressed(std::span<const uint8_t> blocks, uint32_t width, uint32_t height,
                                         Compression compression);

/// What a block's land colour is scaled by, over 256, under a texel the shadows cover with these bits
[[nodiscard]] uint8_t Shade(uint8_t coverage);

/// A corner of a flattened triangle: where it lies in the block's texels (x, then z) and the texture's coordinates
/// spread over the mask's places
struct Corner
{
	glm::vec2 texel;
	glm::vec2 uv;
};

/// Where a point of a tree falls in a block's texels. offset is the point less the tree's foot, turned and scaled as the
/// tree stands; foot is the tree's foot along x and z in the world and block the block's corner there.
[[nodiscard]] Corner Flatten(glm::vec3 offset, glm::vec2 foot, glm::vec2 block, glm::vec2 uv);

/// Scans a flattened triangle into a block's texels, joining in the mask's alpha where it covers them
void Fill(Coverage& coverage, const std::array<Corner, 3>& corners, const AlphaMask& mask);

/// Whether a triangle can reach any of a block's texels: the fill touches only those between its corners taken down to
/// whole texels
[[nodiscard]] bool Reaches(const std::array<Corner, 3>& corners);

/// Whether a block (its index along x and z) takes the shadow of a tree standing in a cell
[[nodiscard]] bool TakesShadowFrom(glm::ivec2 block, glm::ivec2 cell);

/// The blocks along one axis taking the shadows of the trees in a cell along it: first and last
[[nodiscard]] glm::ivec2 BlocksTakingShadowFrom(int cell);

} // namespace tree_shadows

} // namespace openblack::graphics
