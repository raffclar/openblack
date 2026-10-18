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
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/LandPointLight.h"

/// The land's luminosity of a drawn frame. The game starts from the luminosity that the land was laid with. The
/// shadows of the clouds then darken it and, at night, the lights of the hand and the villages brighten it. When the
/// frame is drawn, all of it goes back to the laid luminosity. So the land is shaded only while a frame is drawn: a thing
/// that the game makes in a turn sees the laid luminosity, and a thing that it lights as it draws sees the shaded one.
///
/// Each block of the land keeps its own 17 by 17 cells: its last row and column are the first ones of the next blocks.
/// A shadow darkens only the blocks in view, each in its own cells. A light brightens the cells of every block and also
/// the copies of a cell that the blocks before it keep.
namespace openblack::land_shade
{

/// The cells that a block keeps along each side, and the cells from one block to the next
inline constexpr int32_t k_BlockSide = 17;
inline constexpr int32_t k_BlockStep = 16;
inline constexpr int32_t k_BlockCells = k_BlockSide * k_BlockSide;
/// The map has this many blocks and cells on each side
inline constexpr int32_t k_BlocksPerSide = 32;
inline constexpr int32_t k_MapCells = 512;
/// A shadow never takes a cell below this luminosity
inline constexpr uint8_t k_DarkestShadow = 48;
/// The game keeps at most this many stamps of light and shadow on the land in a frame. The particles' light maps and
/// shadows come first, then the flashes of lightning, then the sky's clouds, then the storms.
inline constexpr size_t k_MostStamps = 200;
/// A sky cloud's shadow image and a storm's are this many texels across
inline constexpr int32_t k_ShadowSide = 40;

/// The luminosity of a block's cells, along z first: the cell at x, z is at x * 17 + z
using BlockCells = std::array<uint8_t, k_BlockCells>;

/// A block of the land as it was laid: its place on the map in blocks, and its cells' luminosity
struct LaidBlock
{
	glm::ivec2 at;
	BlockCells luminosity;
};

/// The land's luminosity of the frame, a copy of each block's cells
class ShadedLand
{
public:
	/// Lays a land: every block at its place with its laid luminosity. All blocks are out of view.
	void Lay(std::span<const LaidBlock> blocks);
	/// Puts every block that a stamp changed since the last time back to its laid luminosity
	void Restore();
	/// Whether a block is in view: only blocks in view take shadows
	void SetInView(glm::ivec2 block, bool inView);
	[[nodiscard]] bool InView(glm::ivec2 block) const;

	/// The cells of the block at a place on the map, none where there is no block
	[[nodiscard]] const BlockCells* Find(glm::ivec2 block) const;
	/// As Find, for a stamp that changes the cells: the block counts as changed
	[[nodiscard]] BlockCells* Change(glm::ivec2 block);
	/// The blocks that stamps changed since the last restore, in the order they first changed
	[[nodiscard]] std::span<const glm::ivec2> Changed() const { return _changed; }
	/// The places of all the blocks, in the order they were laid
	[[nodiscard]] std::span<const glm::ivec2> Blocks() const { return _places; }

	/// The luminosity of the four corners of the cell at a cell position, from the block that holds the cell (see
	/// land_point_light::Corners). None off the map or where there is no block.
	[[nodiscard]] std::optional<land_point_light::Corners> CornersAt(int32_t x, int32_t z) const;

private:
	[[nodiscard]] int32_t SlotOf(glm::ivec2 block) const;

	/// The slot of each place on the map, -1 for none
	std::array<int16_t, static_cast<size_t>(k_BlocksPerSide* k_BlocksPerSide)> _slots {};
	std::vector<glm::ivec2> _places;
	std::vector<BlockCells> _laid;
	std::vector<BlockCells> _shaded;
	std::vector<uint8_t> _inView;
	std::vector<uint8_t> _isChanged;
	std::vector<glm::ivec2> _changed;
};

/// A shadow stamp: an image of how much light each texel lets through, laid from its first texel at a point in the
/// world's x and z, one texel a cell along x by rows and along z by columns
struct Shadow
{
	glm::vec2 corner;
	std::span<const uint8_t> image;
	int32_t side;
	/// How strongly the image darkens the land, of 255
	uint8_t alpha;
};

/// A shadow's alpha from its strength: of 255, cut towards zero, and between 0 and 255
[[nodiscard]] uint8_t Alpha(float strength);
/// The strength of a sky cloud's shadow from the cloud's alpha, 0 to 255
[[nodiscard]] float CloudStrength(uint32_t alpha);
/// The first texel of a shadow centred on a point: back by half its side, less half a cell
[[nodiscard]] glm::vec2 Centred(glm::vec2 centre, int32_t side);
/// A particle's shadow: its image centred on a point, as strong as the particle's stamp
[[nodiscard]] Shadow ParticleShadow(glm::vec3 centre, std::span<const uint8_t> image, int32_t side, float strength);

/// Where a shadow falls: its first cell, the weights of 255 that its image is blended with along x and z, how many
/// cells it covers past its first one along x and z, the texel of its image at its first cell, and whether it is on the
/// map at all
struct ShadowPlace
{
	glm::ivec2 cell;
	glm::ivec2 weight;
	glm::ivec2 span;
	int32_t first;
	bool onMap;
};
[[nodiscard]] ShadowPlace PlaceShadow(glm::vec2 corner, int32_t side);

/// A shadow darkens each cell under it in the blocks in view: the four texels from the cell's place in the image are
/// blended along z, then along x; the cell takes the darker of its luminosity and 255 less the blend's darkness times
/// the alpha, but never less than 48
void CastShadow(ShadedLand& land, const Shadow& shadow);

/// A texel of a light's image as the game reads it: brought down to the land light's 48 warm levels
[[nodiscard]] uint8_t WarmLevel(uint8_t texel);
/// A light's image with each texel at its warm level
[[nodiscard]] std::vector<uint8_t> WarmImage(std::span<const uint8_t> image);

/// A light stamp: an image of brightness at the warm levels, laid from its first texel at a point in the world's x and
/// z as a shadow is, with a strength of 0 to 1
struct Light
{
	glm::vec2 corner;
	std::span<const uint8_t> image;
	int32_t side;
	float strength;
};

/// A light brightens each cell under it, up to the second to last cell of the map: the four texels from the cell's place
/// are blended as a shadow's are, then scaled by the strength. Over a cell at a warm level it keeps the brighter level.
/// Over a cell above the warm levels it takes the cell to its level only when it outshines the cell: when it is more than
/// the cell's luminosity times the threshold of 256 (see village_lights::Threshold). Copies of the cell in the blocks
/// before it take the same level by the same rule.
void CastLight(ShadedLand& land, const Light& light, uint8_t fullLightGreen);

} // namespace openblack::land_shade
