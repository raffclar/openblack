/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <filesystem>
#include <vector>

#include <entt/core/hashed_string.hpp>
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

/// What the engine knows about one landscape material (LND texture)
struct LandMaterialInfo
{
	uint16_t type {0};       ///< the LND material type: a TerrainMaterialType (18 grass, 7 sand, 15 solid rock...)
	glm::vec3 colour {0.0f}; ///< average texel colour 0..1 (the type often doesn't match the look: green "Earth")
	/// k_SmallSize x k_SmallSize box averages of the texels, rgb 0..255 by rows (row = texture v): the ground colour a
	/// few mip levels down, like the foliage shader samples it
	std::vector<uint8_t> small;
	static constexpr int k_SmallSize = 32;
};

class LandIslandInterface
{
public:
	virtual ~LandIslandInterface() = default;

	static const uint8_t k_CellCount;
	static const float k_HeightUnit;
	static const float k_CellSize;

	[[nodiscard]] virtual float GetHeightAt(glm::vec2) const = 0;
	/// The altitude with the sea flattening off (as a fish farm's creation asks for it)
	[[nodiscard]] virtual float GetUnflattenedHeightAt(glm::vec2) const = 0;
	/// The height of the landscape mesh as it is drawn: every vertex of altitude 3 or less at 0, not only next to a
	/// base corner of 4 or less like GetHeightAt
	[[nodiscard]] virtual float GetDrawnHeightAt(glm::vec2 vec) const { return GetUnflattenedHeightAt(vec); }
	[[nodiscard]] virtual glm::vec3 GetNormalAt(glm::vec2) const = 0;
	[[nodiscard]] virtual const lnd::LNDCell& GetCell(const glm::u16vec2& coordinates) const = 0;
	/// A block holds this cell (the block index at [x >> 4][z >> 4] is set); GetCell returns an empty cell where none
	/// does
	[[nodiscard]] virtual bool HasBlockAt(const glm::u16vec2& /*coordinates*/) const { return true; }
	/// Altitude bits of the loaded LND: 8 in the original, up to 16 in BWLandEditor maps (EXT0 chunk)
	[[nodiscard]] virtual uint8_t GetAltitudeBits() const { return 8; }
	/// Cells per side of the block grid: 512 (32 blocks of 16) in the original, up to 2048 in BWLandEditor maps
	[[nodiscard]] virtual uint16_t GetCellsPerSide() const { return 512; }
	/// The cell's altitude in height units (k_HeightUnit), with the extra altitude bits of BWLandEditor maps
	[[nodiscard]] uint16_t GetCellAltitude(const lnd::LNDCell& cell) const;
	/// The altitudes (height units) of a cell's corners (x, z), (x, z + 1), (x + 1, z), (x + 1, z + 1) as the original
	/// reads them from the cell's own block, its shared border row included. The default goes through GetCell;
	/// LandIsland reads the block
	[[nodiscard]] virtual std::array<uint16_t, 4> GetCellCorners(glm::u16vec2 cell) const;
	/// The land ray cast (implemented in Implementations/LandIsland.cpp): the RAY from `from` through `to` (cell units:
	/// x, z x 0.1, y / 0.67), extended to the map's edge, walked cell by cell against the two triangles of each cell
	/// (RayCastCells). On a hit `hit` is its x, z in metres and the answer is true. Without one, a ray going down
	/// (to.y <= from.y, |dy| >= 0.0001) gets its crossing of y = 0 in `hit`, and true when that point is within 7500 m
	/// (in x z) of `camera` (the engine's camera).
	/// Users: the lightning's fork (Particles/Rules/Lightning.cpp) and, not ported yet, the camera update, the camera modes,
	/// the landscape draw and a few more
	[[nodiscard]] bool RayCast(const glm::vec3& from, const glm::vec3& to, glm::vec2& hit, const glm::vec3& camera) const;
	/// The ray cast in cell units (x, z, y / 0.67): the hit's x, z in cells
	[[nodiscard]] bool RayCastCells(float x0, float z0, float y0, float x1, float z1, float y1, glm::vec2& hit) const;

	// Debug
	virtual void DumpTextures() const = 0;
	virtual void DumpMaps() const = 0;

	[[nodiscard]] virtual std::vector<LandBlock>& GetBlocks() = 0;
	[[nodiscard]] virtual const std::vector<LandBlock>& GetBlocks() const = 0;
	[[nodiscard]] virtual const std::vector<lnd::LNDCountry>& GetCountries() const = 0;

	[[nodiscard]] virtual const graphics::Texture2D& GetAlbedoArray() const = 0;
	[[nodiscard]] virtual const graphics::Texture2D& GetBump() const = 0;
	/// Detail texture blended over the land near the camera: rgb = smallbump.raw, a = smallbumpa.raw.
	[[nodiscard]] virtual const graphics::Texture2D& GetSmallBump() const = 0;
	[[nodiscard]] virtual const graphics::Texture2D& GetHeightMap() const = 0;
	/// Per cell: rgb = the cell colour as the original reads it for model specular (a D3DCOLOR: R and B swapped), a =
	/// luminosity; same layout as the height map, nearest filtering
	[[nodiscard]] virtual const graphics::Texture2D& GetCellMap() const = 0;
	[[nodiscard]] virtual const graphics::FrameBuffer& GetFootprintFramebuffer() const = 0;
	/// Static object shadows over the whole island, same layout as the footprints (256 texels per block, like the
	/// original's block textures); red = coverage
	[[nodiscard]] virtual const graphics::FrameBuffer& GetStaticShadowFramebuffer() const = 0;
	/// Island-wide land alpha, one texel per footprint texel: 1, or lower in the river channels (min of the river.l3d
	/// footprints, like the alpha nibble of the original's block textures)
	[[nodiscard]] virtual const graphics::FrameBuffer& GetLandAlphaFramebuffer() const = 0;
	/// Island-wide block texture (BlockTexture.h), RGBA8, same layout as the land alpha but rows along +z from the
	/// extent minimum: the original's ARGB4444 block textures (each nibble x 17), colour and coast alpha, 0 in the open
	/// sea cells; nullptr while none is built
	[[nodiscard]] virtual const graphics::Texture2D* GetBlockTexture() const { return nullptr; }

	[[nodiscard]] virtual U16Extent2 GetIndexExtent() const = 0;
	[[nodiscard]] virtual glm::mat4 GetOrthoView() const = 0;
	[[nodiscard]] virtual glm::mat4 GetOrthoProj() const = 0;
	[[nodiscard]] virtual Extent2 GetExtent() const = 0;
	[[nodiscard]] virtual uint8_t GetNoise(glm::u8vec2 pos) = 0;

	/// Changes a cell's altitude (height units) in every block that stores it, the shared border row and column too;
	/// the land follows once RebuildAltitudes runs
	virtual void SetCellAltitude(glm::u16vec2 /*cell*/, uint16_t /*altitude*/) {}
	/// Rebuilds the meshes and physics shapes of the blocks whose meshes read a changed corner (the smooth normals read
	/// across the border, so a neighbour can be one), the height map and the changed texels of the block texture. The
	/// rebuilt blocks get new rigid bodies; in the physics world they take the old ones' places.
	virtual void RebuildAltitudes() {}
	/// One entry per material of the LND (empty while no island is loaded)
	[[nodiscard]] virtual const std::vector<LandMaterialInfo>& GetMaterialInfo() const
	{
		static const std::vector<LandMaterialInfo> k_None;
		return k_None;
	}
};
} // namespace openblack
