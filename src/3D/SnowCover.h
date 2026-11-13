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
#include <span>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// Snow lying on the island: storms that snow pile it up on a grid of 40 unit cells, and it melts away slowly
/// everywhere. The land turns white where it is deep enough, raggedly by a noise image, and objects standing in it
/// show a snow texture over themselves, more of it the deeper it lies.
namespace openblack::snow_cover
{

/// The texture objects show where snow lies on them, with its alpha, and the noise image the land's snow is ragged by,
/// once across each land block
inline constexpr entt::hashed_string k_TextureId = entt::hashed_string("raw/SNOW");
inline constexpr entt::hashed_string k_AlphaTextureId = entt::hashed_string("raw/SNOWA");
inline constexpr entt::hashed_string k_NoiseTextureId = entt::hashed_string("raw/snowmap");

/// The grid's cells along each side, and their size
inline constexpr int32_t k_GridSize = 128;
inline constexpr size_t k_Cells = static_cast<size_t>(k_GridSize) * k_GridSize;
inline constexpr float k_CellSize = 40.0f;
/// The deepest the snow lies
inline constexpr float k_MaxDepth = 255.0f;

/// The snow a storm lays on each cell in a turn of 0.1 seconds, from its snow in percent and its strength, 0 to 1. The
/// storm lays 1.2 times its snow, and the game cuts that amount to a whole number before the turn's share.
[[nodiscard]] float StormSnowPerTurn(int8_t snow, float strength);

/// A land block's texture covers a square of 4 x 4 cells of the grid, thus 32 blocks along each side
inline constexpr int32_t k_BlockCells = 4;
inline constexpr int32_t k_BlocksPerSide = k_GridSize / k_BlockCells;
inline constexpr size_t k_Blocks = static_cast<size_t>(k_BlocksPerSide) * k_BlocksPerSide;
/// The grid points that a block's texture takes its snow from, along each side: its cells and the far edge
inline constexpr int32_t k_BlockCorners = k_BlockCells + 1;
/// A block in the order of the grid: row by row along z, k_BlocksPerSide blocks along x in each row
[[nodiscard]] constexpr uint16_t BlockIndex(glm::ivec2 block)
{
	return static_cast<uint16_t>((block.y * k_BlocksPerSide) + block.x);
}

/// The game shows the new snow on a block only when it paints the block's texture again. It adds up the change of the
/// snow under each block, and it lists the blocks whose change is large enough for a paint.
/// A block that changed by more than this since the game last listed it goes in the list
inline constexpr float k_RepaintChange = 96.0f;
/// The list holds this many blocks at most
inline constexpr size_t k_RepaintListSize = 16;
struct BlockRepaint
{
	/// The change of the snow under each block since the game last listed it, in the order of BlockIndex
	std::array<float, k_Blocks> changes {};
	/// The blocks in the list, each with the size of its change. The places from `count` on hold a size of 0.
	std::array<float, k_RepaintListSize> sizes {};
	std::array<uint16_t, k_RepaintListSize> blocks {};
	int32_t count {0};

	/// Gives the values that a saved game keeps to a visitor
	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self.changes, self.sizes, self.blocks, self.count);
	}
};

/// A storm snowing on the cells within its outer radius of its centre: as much as `amount` within its inner radius,
/// less beyond, and towards its edge it takes snow away. The depth stays between 0 and k_MaxDepth.
void AddStorm(std::span<float> grid, glm::vec2 centre, float innerRadius, float outerRadius, float amount);
/// The same, and the change of each cell is added to the change of its block. A cell on the low x or low z edge of its
/// block is also on the texture of the block next to it there, so its change is also added to that block. At a corner of
/// four blocks the game adds it to the block on the low x side and to the block on the low x and low z side, but not to
/// the block on the low z side.
void AddStorm(std::span<float> grid, std::span<float> changes, glm::vec2 centre, float innerRadius, float outerRadius,
              float amount);

/// The snow melts by 2 in one of eight bands of the grid's rows every 0.3 seconds, so all of it in 2.4 seconds
struct Melting
{
	float clock {0.0f};
	int32_t band {0};

	/// Gives the values that a saved game keeps to a visitor
	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self.clock, self.band);
	}
};
/// The melting clock after `seconds` more, melting each band whose turn has come
void Melt(std::span<float> grid, Melting& melting, float seconds);
/// The same, and the snow that melts is taken from the change of the cell's own block only. After each band melts, the
/// blocks of its rows are listed for a paint (see ListRepaints). The blocks that the game paints again at once are added
/// to `paintNow`.
void Melt(std::span<float> grid, Melting& melting, float seconds, BlockRepaint& repaint, std::vector<uint16_t>& paintNow);
/// Lists the blocks of `rows` rows of blocks from `firstRow` for a paint. When the first row is 0, the game first paints
/// again every block in the list and empties it. Then each block whose change is more than k_RepaintChange takes the
/// first place in the list whose size is smaller than its change, and its change starts again from 0. A block that finds
/// no such place stays out, and keeps its change. In a full list the block in that place is painted again at once and
/// the new block takes its place. In a list with space the blocks from that place on move one place down.
void ListRepaints(BlockRepaint& repaint, int32_t firstRow, int32_t rows, std::vector<uint16_t>& paintNow);

/// Writes the snow at the k_BlockCorners x k_BlockCorners grid points of a block into `painted`, which holds each
/// block's points in a square of its own: k_BlocksPerSide squares along each side, row by row along z. A point off the
/// grid has no snow.
inline constexpr int32_t k_PaintedSide = k_BlocksPerSide * k_BlockCorners;
void PaintBlockCorners(std::span<const float> grid, glm::ivec2 block, std::span<float> painted);

/// The snow that each block's texture shows: the snow at its grid points as it was when the texture was last painted.
/// The game paints a block's texture again only when it draws the block with its own texture after a request for a
/// paint. Snow is not the only reason for a request.
class PaintedBlocks
{
public:
	PaintedBlocks();
	/// Asks for a paint of a block, or of every block
	void Request(uint16_t block);
	void RequestAll();
	/// Paints the blocks that wait for a paint among `drawn`, from the snow as it lies now. It gives true when it painted
	/// a block.
	bool PaintDrawn(std::span<const glm::ivec2> drawn, std::span<const float> grid);
	/// The painted snow, as PaintBlockCorners writes it
	[[nodiscard]] std::span<const float> GetPainted() const { return _painted; }
	[[nodiscard]] bool Waits(uint16_t block) const { return _waiting.test(block); }

private:
	std::vector<float> _painted;
	std::bitset<k_Blocks> _waiting;
};

/// How deep the snow lies at a point in the world, blended between the four cells around it; none off the grid
[[nodiscard]] float DepthAt(std::span<const float> grid, glm::vec2 xz);

/// How much snow shows on an object standing where the snow is `depth` deep, 0 to `cap`: none below 20, then as deep
/// as it lies beyond that, scaled by `rate` of 256
inline constexpr int32_t k_ObjectRate = 255;
[[nodiscard]] int32_t ObjectLevel(float depth, int32_t rate, int32_t cap);
/// A tree that the game drew out of the land one time carries less snow from then on: this rate of 256, and this
/// maximum level
inline constexpr int32_t k_OutOfLandTreeRate = 128;
inline constexpr int32_t k_OutOfLandTreeCap = 64;
/// How much snow shows on the broken model of a building where the snow is `depth` deep, 0 to 255: the whole metres
/// of the depth less 20
[[nodiscard]] int32_t BrokenModelLevel(float depth);
/// The broken model of a building, and each piece that broke off it, shows snow from this much of it
inline constexpr int32_t k_BrokenModelLevelShown = 6;
/// The level of snow that the broken model shows at `level`: none below k_BrokenModelLevelShown
[[nodiscard]] constexpr int32_t BrokenModelShownLevel(int32_t level)
{
	return level < k_BrokenModelLevelShown ? 0 : level;
}
/// A thing's level of snow moves towards the level of the snow where it stands. In each step it moves by this much at
/// most: the game time of the step in milliseconds, divided by 16. A step at 60 steps a second moves it by 1, so a thing
/// takes about 4 seconds to change from no snow to all of it.
[[nodiscard]] int32_t LevelStep(int32_t gameMilliseconds);
/// The new level of a thing: `target`, but no more than `step` from its `level`
[[nodiscard]] int32_t EaseLevel(int32_t level, int32_t target, int32_t step);
/// The snow texture's alpha, 0 to 255, at which an object shows snow at a level: the more snow, the more of it
[[nodiscard]] int32_t ObjectThreshold(int32_t level);

/// How white the land is, 0 to 16, where the snow is `depth` deep over a texel of the noise image
[[nodiscard]] int32_t LandLevel(float depth, uint8_t noise);
/// The land turns no whiter where no corner of its cell has more snow than this
inline constexpr float k_LandLeast = 5.0f;
/// The grey the land turns to under snow, of 15, by the noise image's texel: 14 or 15
[[nodiscard]] uint8_t LandWhite(uint8_t noise);

/// A side of a land block's texture covers four cells of the grid, so each cell has this many texels along a side
inline constexpr int32_t k_TexelsPerCell = 64;
/// The game paints the snow into each texel of a land block's texture. Then the texture is filtered as it is drawn.
/// A texel is in the cell `cell` (x, z) of the grid, `step` texels (along x, along z) from the lowest corner of the
/// cell. The depth there is in 65536ths, blended in whole steps from the four corners of the cell.
[[nodiscard]] int32_t TexelDepth(std::span<const float> grid, glm::ivec2 cell, glm::ivec2 step);
/// The game paints snow into the texels of a cell only when a corner of the cell has more snow than k_LandLeast
[[nodiscard]] bool CellTakesSnow(std::span<const float> grid, glm::ivec2 cell);
/// A texel's colour, three 4-bit levels, with the snow painted over it at `level` (0 to 16) in the grey `white`
[[nodiscard]] glm::u8vec3 PaintTexel(glm::u8vec3 colour, int32_t level, uint8_t white);

} // namespace openblack::snow_cover
