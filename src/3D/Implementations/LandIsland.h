/******************************************************************************
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
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "3D/BlockTexture.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
class LandIsland final: public LandIslandInterface
{
public:
	explicit LandIsland(const std::filesystem::path& path);
	~LandIsland() noexcept;

	void LoadFromFile(const std::filesystem::path& path);

	[[nodiscard]] float GetHeightAt(glm::vec2 vec) const override { return HeightAt(vec, true); }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2 vec) const override { return HeightAt(vec, false); }
	[[nodiscard]] float GetDrawnHeightAt(glm::vec2 vec) const override { return HeightAt(vec, false, true); }
	/// meshFlattening: every corner of 3 or less at 0, as the landscape mesh is drawn (GetDrawnHeightAt)
	[[nodiscard]] float HeightAt(glm::vec2 vec, bool seaFlattening, bool meshFlattening = false) const;
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const override;
	[[nodiscard]] const LandBlock* GetBlock(const glm::u8vec2& coordinates) const;
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& coordinates) const override;
	[[nodiscard]] bool HasBlockAt(const glm::u16vec2& coordinates) const override
	{
		return BlockIndexAt(coordinates >> static_cast<uint16_t>(0x4)) != 0;
	}
	[[nodiscard]] uint8_t GetAltitudeBits() const override { return _altitudeBits; }
	void SetCellAltitude(glm::u16vec2 cell, uint16_t altitude) override;
	void RebuildAltitudes() override;
	[[nodiscard]] uint16_t GetCellsPerSide() const override { return static_cast<uint16_t>(_blocksPerSide * k_CellCount); }
	/// From the cell's own block: its border row and column too, also where the next block is missing (the ray cell test)
	[[nodiscard]] std::array<uint16_t, 4> GetCellCorners(glm::u16vec2 cell) const override;

	// Debug
	void DumpTextures() const override;
	void DumpMaps() const override;

private:
	[[nodiscard]] std::vector<float> CreateHeightMap() const;
	/// Index + 1 of the block at these block coordinates, 0 where there is none
	[[nodiscard]] uint16_t BlockIndexAt(glm::u16vec2 blockCoordinates) const;
	[[nodiscard]] std::vector<uint8_t> CreateCellMap() const;
	/// Builds the block texture of the whole island (BlockTexture.h); OPENBLACK_DUMP_COAST_ALPHA writes it to a PNG
	void CreateBlockTexture();
	/// What the block texture is painted from
	[[nodiscard]] block_texture::Sources BlockTextureSources() const;
	/// Paints again the cells of the changed blocks whose corners moved, and uploads only those texels
	void RepaintChangedBlockTexels();
	std::vector<LandBlock> _landBlocks;
	std::vector<lnd::LNDCountry> _countries;
	std::vector<LandMaterialInfo> _materialInfo;

	/// Index + 1 of the block at (x * _blocksPerSide + z), 0 where there is none; built from the blocks' own
	/// coordinates because the header table only covers 32 x 32 blocks and 255 indices (BWLandEditor maps go beyond)
	std::vector<uint16_t> _blockIndexLookup;
	uint16_t _blocksPerSide {32};
	uint8_t _altitudeBits {8};
	/// A block whose altitudes changed since the last RebuildAltitudes, and the box of its cells that a changed corner
	/// touches
	struct ChangedBlock
	{
		size_t index;
		block_texture::CellBox cells;
	};
	std::vector<ChangedBlock> _changedBlocks;
	/// The corners SetCellAltitude changed since the last RebuildAltitudes
	std::optional<CornerBox> _changedCorners;

	// Renderer, Dynamics
public:
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() override { return _landBlocks; }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const override { return _landBlocks; }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const override { return _countries; }
	[[nodiscard]] const std::vector<LandMaterialInfo>& GetMaterialInfo() const override { return _materialInfo; }

	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const override { return *_materialArray; }
	[[nodiscard]] const graphics::Texture2D& GetBump() const override { return *_textureBumpMap; }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const override { return *_smallBump; }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const override { return *_heightMap; }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const override { return *_cellMap; }
	[[nodiscard]] const graphics::FrameBuffer& GetStaticShadowFramebuffer() const override { return *_staticShadowFrameBuffer; }
	[[nodiscard]] const graphics::FrameBuffer& GetLandAlphaFramebuffer() const override { return *_landAlphaFrameBuffer; }
	[[nodiscard]] const graphics::Texture2D* GetBlockTexture() const override { return _blockTexture.get(); }
	[[nodiscard]] const graphics::FrameBuffer& GetFootprintFramebuffer() const override { return *_footprintFrameBuffer; }

	[[nodiscard]] glm::mat4 GetOrthoView() const override { return _view; }
	[[nodiscard]] glm::mat4 GetOrthoProj() const override { return _proj; }
	[[nodiscard]] U16Extent2 GetIndexExtent() const override { return U16Extent2 {_extentIndexMin, _extentIndexMax}; }
	[[nodiscard]] Extent2 GetExtent() const override { return Extent2 {_extentMin, _extentMax}; }

	uint8_t GetNoise(glm::u8vec2 pos) override;

private:
	std::unique_ptr<graphics::Texture2D> _materialArray;
	std::unique_ptr<graphics::Texture2D> _countryLookup;

	std::unique_ptr<graphics::Texture2D> _heightMap;
	std::unique_ptr<graphics::Texture2D> _cellMap;
	std::unique_ptr<graphics::FrameBuffer> _staticShadowFrameBuffer;
	std::unique_ptr<graphics::FrameBuffer> _landAlphaFrameBuffer;
	std::unique_ptr<graphics::Texture2D> _blockTexture;
	uint16_t _texelsPerBlock {256};
	std::unique_ptr<graphics::Texture2D> _textureNoiseMap;
	std::unique_ptr<graphics::Texture2D> _textureBumpMap;
	std::shared_ptr<graphics::Texture2D> _smallBump; ///< from the texture cache

	std::unique_ptr<graphics::FrameBuffer> _footprintFrameBuffer;
	glm::mat4 _proj;
	glm::mat4 _view;
	glm::u16vec2 _extentIndexMin;
	glm::u16vec2 _extentIndexMax;
	glm::vec2 _extentMin;
	glm::vec2 _extentMax;

	std::array<uint8_t, 256 * 256> _noiseMap;
	std::array<uint8_t, 256 * 256> _bumpMap {};
	std::vector<uint16_t> _materialTexels; ///< the LND material textures one after another, raw B5G5R5 [x * 256 + z]
};
} // namespace openblack

/*
LH3DIsland methods:
AdjustAlti((void *))
BuildSoundTGA((char *))
Create((void))
CreateCommonPart((void))
CreateSmallBump((void))
Draw((void))
DrawUnderWater((void))
GetAltitude((LH3DMapCoords const &))
GetAltitudeAndSetColorSpecular((LH3DMapCoords const &,ulong *,ulong *))
GetCell((long,long))
GetColorAndSpecular((LH3DMapCoords const &,ulong *,ulong *))
GetColorAndSpecular((LHPoint const *,ulong *,ulong *))
GetCountry((long,long,uchar &))
GetFogValue((LHPoint const *,ulong,ulong *))
GetIndex((long,long))
GetNormal((LH3DMapCoords const &,LHPoint *))
GetTerrainMaterial((ulong,ulong))
IsCompressed((void))
LoadFromDisk((char *))
PreDraw((void))
RayCast((LHPoint const &,LHPoint const &,float *,float *))
RayCastFrom2DPoint((LHCoord const &,float *,float *,bool,float))
RayCastInternal((float,float,float,float,float,float))
Release((void))
ReleaseSmallBump((void))
RequestUpdateAllTextures((void))
SaveOnDisk((char *))
SetColor((long,long,uchar))
SetCountry((long,long,long))
SetFileToLoad((char *))
SetFogColor((float,float,float,ulong))
SetFogMinMax((float,float))
SetHeightAsByte((long,long,long))
UseLowResTexture((int))
*/
