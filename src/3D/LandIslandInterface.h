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

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/mat4x4.hpp>

#include "Extent.h"

namespace openblack
{
class LandBlock;
namespace graphics
{
class FrameBuffer;
class Texture2D;
} // namespace graphics
namespace lnd
{
struct LNDCell;
struct LNDCountry;
} // namespace lnd
class LandIslandInterface
{
public:
	static const uint8_t k_CellCount;
	/// Cells along each side of the map
	static constexpr int32_t k_MapCellsPerSide = 512;
	static const float k_HeightUnit;
	static const float k_CellSize;
	/// The small bump detail drawn over the land near the camera: its colour and its alpha
	static constexpr entt::hashed_string k_SmallBumpTextureId = entt::hashed_string("raw/smallbump");
	static constexpr entt::hashed_string k_SmallBumpAlphaTextureId = entt::hashed_string("raw/smallbumpa");

	/// Whether a land is loaded; before the first one is, nothing else may be asked
	[[nodiscard]] virtual bool IsLoaded() const { return true; }
	/// The height of the land at a world position, as the game works it out
	[[nodiscard]] virtual float GetHeightAt(glm::vec2) const = 0;
	[[nodiscard]] virtual glm::vec3 GetNormalAt(glm::vec2) const = 0;
	[[nodiscard]] virtual const lnd::LNDCell& GetCell(const glm::u16vec2& coordinates) const = 0;
	/// The cell at the given coordinates in the 17x17 cell array of its block, so the neighbours at +1 (z + 1),
	/// +17 (x + 1) and +18 are also valid. Null outside the map or where there is no block.
	[[nodiscard]] virtual const lnd::LNDCell* FindCell(const glm::u16vec2& coordinates) const = 0;
	/// Sets a cell corner's altitude while the game runs, in every block that shares the corner. The land the game reads
	/// changes at once; its drawing follows at the next CommitAltitudeChanges. Lands that can't change ignore it.
	virtual void SetCellAltitude(glm::u16vec2 /*coordinates*/, uint8_t /*altitude*/) {}
	/// The land's drawing catches up with the altitudes set since the last time: the shapes of the blocks that changed,
	/// the height map, and the blocks' textures, whose ground follows the height
	virtual void CommitAltitudeChanges() {}
	/// Puts each cell corner back to the altitude that it had when the land was built. Only the blocks that change are
	/// made again, as for CommitAltitudeChanges. Returns the count of the block cells that changed.
	virtual size_t RestoreBuiltAltitudes() { return 0; }

	// Debug
	virtual void DumpTextures() const = 0;
	virtual void DumpMaps() const = 0;

	[[nodiscard]] virtual std::vector<LandBlock>& GetBlocks() = 0;
	[[nodiscard]] virtual const std::vector<LandBlock>& GetBlocks() const = 0;
	[[nodiscard]] virtual const std::vector<lnd::LNDCountry>& GetCountries() const = 0;
	/// The terrain type of each of the land's materials, which the countries blend by index: what the ground is
	/// made of, for its sounds. Empty when no land is loaded.
	[[nodiscard]] virtual std::span<const uint16_t> GetMaterialTypes() const { return {}; }

	[[nodiscard]] virtual const graphics::Texture2D& GetHeightMap() const = 0;
	/// Each cell corner's luminosity, laid out as the height map, 255 where there is no block
	[[nodiscard]] virtual const graphics::Texture2D& GetLuminosityMap() const = 0;
	/// Each cell corner's colour, laid out as the height map and black where there is no block. The game reads a
	/// cell's colour bytes with red and blue swapped, as Direct3D's colours keep them, and so does this.
	[[nodiscard]] virtual const graphics::Texture2D& GetCellColourMap() const = 0;
	/// The blocks' painted textures (see block_texture), a layer for each block in the order of GetBlocks
	[[nodiscard]] virtual const graphics::Texture2D& GetBlockTextures() const = 0;
	/// The coarse texture pages the landscape file keeps, a layer each, or null when it has none. The coarse land
	/// (detail level 0) draws every block from them.
	[[nodiscard]] virtual const graphics::Texture2D* GetCoarsePages() const { return nullptr; }
	/// Where a block's coarse texture is on the pages: x the layer, yz where its square starts (0 to 1 across the
	/// page), w 1; all 0 when there are no pages
	[[nodiscard]] virtual glm::vec4 GetCoarsePageSlot(size_t /*block*/) const { return glm::vec4(0.0f); }
	/// The texels along a side of each block's texture: 256, or 128 at the low resolution of the lowest detail levels
	[[nodiscard]] virtual int GetBlockTextureSide() const { return 256; }
	/// Paints a block's own texture again into ARGB texels at 4 bits a channel, with no footprints (see block_texture).
	/// The texels are GetBlockTextureSide on each side, a row for each texel along x.
	virtual void PaintBlockTexels(size_t /*block*/, std::span<uint16_t> /*texels*/) const {}
	/// Puts ARGB texels at 4 bits a channel in place of a block's texture, for example with its footprints painted in
	virtual void SetBlockTexels(size_t /*block*/, std::span<const uint16_t> /*texels*/) {}
	/// A block whose texture the land painted again with no footprints
	struct RepaintedBlock
	{
		/// The texels of a rectangle of the block's cells, as the land painted them (see block_texture::PaintCellTexels)
		struct Cells
		{
			glm::ivec2 first {0};
			glm::ivec2 count {0};
			std::vector<uint16_t> texels;
		};
		size_t block {0};
		/// The cells that the land painted again, or none when it painted the whole block
		std::optional<Cells> cells;
	};
	/// The blocks whose textures the land painted again with no footprints since the last call, in the order it painted
	/// them: each block after a land loads, and the cells of the blocks that change height
	[[nodiscard]] virtual std::vector<RepaintedBlock> TakeRepaintedBlocks() { return {}; }

	[[nodiscard]] virtual U16Extent2 GetIndexExtent() const = 0;
	[[nodiscard]] virtual glm::mat4 GetOrthoView() const = 0;
	[[nodiscard]] virtual glm::mat4 GetOrthoProj() const = 0;
	[[nodiscard]] virtual Extent2 GetExtent() const = 0;
	virtual uint8_t GetNoise(glm::u8vec2 pos) = 0;

	/// The height of the land at a map position, in 1/65536 of a cell. It is interpolated
	/// over the triangle of the cell the position is in, the way the land is drawn, with land at sea level flat unless
	/// told otherwise (as the game only does to find a fish farm's open sea).
	[[nodiscard]] float GetAltitude(int32_t mapX, int32_t mapZ, bool seaLevelFlat = true) const;
	/// A world coordinate in 1/65536 of a cell (map_coords::ToFixed)
	[[nodiscard]] static int32_t ToMapCoords(float unit);
	/// The height the game draws land of a cell altitude at: land at or below sea level is drawn flat at height 0
	[[nodiscard]] static float GetDrawnAltitude(uint8_t altitude);
};
} // namespace openblack
