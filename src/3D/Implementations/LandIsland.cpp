/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "LandIsland.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <stdexcept>

#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LNDFile.h>
#include <bgfx/bgfx.h>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/vector_relational.hpp>
#include <spdlog/spdlog.h>
#include <stb_image_write.h>

#include "3D/BlockTexture.h"
#include "3D/CoastAlpha.h"
#include "3D/LandBlock.h"
#include "3D/LandNormal.h"
#include "3D/MapCoords.h"
#include "Dynamics/LandBlockBulletMeshInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Argb4444.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

const uint8_t LandIslandInterface::k_CellCount = 16;
const float LandIslandInterface::k_HeightUnit = 0.67f;
const float LandIslandInterface::k_CellSize = 10.0f;

uint16_t LandIslandInterface::GetCellAltitude(const lnd::LNDCell& cell) const
{
	return cell.Altitude(GetAltitudeBits());
}

namespace
{
/// The small bump texture as the original builds it ("smallbump.raw" loaded with the alpha flag, packed to ARGB4444
/// with "smallbumpa.raw" ORed in as the alpha nibble).
std::shared_ptr<Texture2D> CreateSmallBumpTexture()
{
	constexpr uint16_t k_Size = 256;
	constexpr auto k_Name = "LandIslandSmallBump";
	if (Locator::resources::has_value())
	{
		auto& textures = Locator::resources::value().GetTextures();
		const auto id = entt::hashed_string("land/smallbump").value();
		try
		{
			if (!textures.Contains(id))
			{
				// a missing smallbumpa.raw is allowed: PackRaw takes the alpha from the colour bytes left in its buffer
				auto& fileSystem = Locator::filesystem::value();
				const auto directory = fileSystem.GetPath<filesystem::Path::Textures>();
				textures.Load(id, resources::Texture2DLoader::FromColourAlphaTag {}, k_Name,
				              resources::Texture2DLoader::ColourAlphaDesc {
				                  .colour = directory / "smallbump.raw",
				                  .alpha = directory / "smallbumpa.raw",
				                  .size = k_Size,
				                  .packing = resources::Texture2DLoader::ColourAlpha::Argb4444,
				                  .wrap = Wrapping::Repeat,
				                  .filter = Filter::Linear,
				              });
			}
			return textures.Handle(id).handle();
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "[LandIsland] no small bump detail (smallbump.raw / smallbumpa.raw): {}",
			                   e.what());
		}
	}
	// no small bump detail: a blank texture (not cached, so the next land tries the files again)
	const std::vector<uint8_t> blank(static_cast<size_t>(k_Size) * k_Size * 4, 0);
	auto texture = std::make_shared<Texture2D>(k_Name);
	texture->Create(k_Size, k_Size, 1, TextureFormat::RGBA8, Wrapping::Repeat, Filter::Linear,
	                bgfx::copy(blank.data(), static_cast<uint32_t>(blank.size())));
	return texture;
}
} // namespace

LandIsland::LandIsland(const std::filesystem::path& path)
{
	LoadFromFile(path);
}

LandIsland::~LandIsland() noexcept = default;

void LandIsland::LoadFromFile(const std::filesystem::path& path)
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading Land from file: {}", path.string());
	lnd::LNDFile lnd;

	const auto result = lnd.ReadFile(*Locator::filesystem::value().GetData(path));
	if (result != lnd::LNDResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to open lnd file from filesystem {}: {}", path.string(),
		                    lnd::ResultToStr(result));
		throw lnd::ResultToStr(result);
	}

	const auto& lndBlocks = lnd.GetBlocks();
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "[LandIsland] loading {} blocks", lndBlocks.size());
	// BWLandEditor maps can have up to 128 x 128 blocks, more than 255 of them and up to 16 altitude bits; the original
	// ones have 32 x 32 and a header lookup table that matches the blocks' own coordinates (checked on all of them)
	_altitudeBits = lnd.GetAltitudeBits();
	_blocksPerSide = lnd.GetBlocksPerSide();
	constexpr uint16_t k_MaxBlocksPerSide = 128;
	if (_blocksPerSide > k_MaxBlocksPerSide || lndBlocks.size() >= std::numeric_limits<uint16_t>::max())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "[LandIsland] {}: {} blocks per side (at most {}), {} blocks", path.string(),
		                    _blocksPerSide, k_MaxBlocksPerSide, lndBlocks.size());
		throw std::runtime_error("LND block grid too large");
	}
	if (_altitudeBits != 8 || _blocksPerSide != 32)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "[LandIsland] {}: BWLandEditor map, {} x {} blocks, {} altitude bits",
		                   path.filename().string(), _blocksPerSide, _blocksPerSide, static_cast<int>(_altitudeBits));
	}
	_blockIndexLookup.assign(static_cast<size_t>(_blocksPerSide) * _blocksPerSide, 0);
	_landBlocks.resize(lndBlocks.size());
	for (size_t i = 0; i < _landBlocks.size(); i++)
	{
		auto block = lndBlocks[i];
		// like BWLandEditor, the block coordinates win over a map position that doesn't match them
		const auto expected = glm::vec2(block.blockX, block.blockZ) * (k_CellSize * k_CellCount);
		if (block.mapX != expected.x || block.mapZ != expected.y)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "[LandIsland] block {} at ({}, {}) has map position ({}, {})", i,
			                   block.blockX, block.blockZ, block.mapX, block.mapZ);
			block.mapX = expected.x;
			block.mapZ = expected.y;
		}
		_landBlocks[i].SetLndBlock(block);
		auto& entry = _blockIndexLookup.at(static_cast<size_t>(block.blockX) * _blocksPerSide + block.blockZ);
		if (entry == 0)
		{
			entry = static_cast<uint16_t>(i + 1);
		}
	}

	_extentIndexMin.x = std::numeric_limits<uint16_t>::max();
	_extentIndexMin.y = std::numeric_limits<uint16_t>::max();
	_extentIndexMax.x = 0;
	_extentIndexMax.y = 0;
	for (auto& b : _landBlocks)
	{
		if (_extentIndexMin.x > b.GetLndBlock()->blockX)
		{
			_extentIndexMin.x = static_cast<uint16_t>(b.GetLndBlock()->blockX);
			_extentMin.x = b.GetMapPosition().x;
		}
		if (_extentIndexMax.x < b.GetLndBlock()->blockX)
		{
			_extentIndexMax.x = static_cast<uint16_t>(b.GetLndBlock()->blockX);
			_extentMax.x = b.GetMapPosition().x;
		}
		if (_extentIndexMin.y > b.GetLndBlock()->blockZ)
		{
			_extentIndexMin.y = static_cast<uint16_t>(b.GetLndBlock()->blockZ);
			_extentMin.y = b.GetMapPosition().y;
		}
		if (_extentIndexMax.y < b.GetLndBlock()->blockZ)
		{
			_extentIndexMax.y = static_cast<uint16_t>(b.GetLndBlock()->blockZ);
			_extentMax.y = b.GetMapPosition().y;
		}
	}
	_extentMax += k_CellSize * k_CellCount;

	const auto indexSize = _extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1);

	_heightMap = std::make_unique<Texture2D>("Height Map");
	// per cell: altitude (height units) and split bit, point sampled: vs_object computes GetAltitude from them exactly
	const auto heightMapData = CreateHeightMap();
	_heightMap->Create(
	    indexSize.x * k_CellCount + 1, indexSize.y * k_CellCount + 1, 1, graphics::TextureFormat::RG32F, Wrapping::ClampEdge,
	    Filter::Nearest,
	    bgfx::copy(heightMapData.data(), static_cast<uint32_t>(heightMapData.size() * sizeof(heightMapData[0]))));

	_cellMap = std::make_unique<Texture2D>("Cell Map");
	const auto cellMapData = CreateCellMap();
	_cellMap->Create(indexSize.x * k_CellCount + 1, indexSize.y * k_CellCount + 1, 1, graphics::TextureFormat::RGBA8,
	                 Wrapping::ClampEdge, Filter::Nearest,
	                 bgfx::copy(cellMapData.data(), static_cast<uint32_t>(cellMapData.size())));

	// 256 texels per block like the original's block textures, fewer on BWLandEditor maps wider than 32 blocks (the
	// textures would pass 8192 texels)
	constexpr uint16_t k_MaxIslandTexture = 8192;
	const auto texelsPerBlock = static_cast<uint16_t>(
	    std::min<int>(lnd::LNDMaterial::k_Width, k_MaxIslandTexture / std::max(indexSize.x, indexSize.y)));
	const auto res = indexSize * texelsPerBlock;
	_texelsPerBlock = texelsPerBlock;
	_footprintFrameBuffer = std::make_unique<FrameBuffer>("Footprints", res.x, res.y, graphics::TextureFormat::RGBA8);
	_staticShadowFrameBuffer = std::make_unique<FrameBuffer>("StaticShadows", res.x, res.y, graphics::TextureFormat::R8);
	_landAlphaFrameBuffer = std::make_unique<FrameBuffer>("LandAlpha", res.x, res.y, graphics::TextureFormat::R8);

	_proj = glm::ortho(_extentMin.x, _extentMax.x, _extentMin.y, _extentMax.y);
	_view = glm::rotate(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "[LandIsland] loading {} countries", lnd.GetCountries().size());
	_countries = lnd.GetCountries();

	auto materialCount = static_cast<uint16_t>(lnd.GetMaterials().size());
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "[LandIsland] loading {} textures", materialCount);
	// kept: the block texture (BlockTexture.h) is built from them again when the altitudes change
	auto& rgba5TextureData = _materialTexels;
	rgba5TextureData.assign(lnd::LNDMaterial::k_Width * lnd::LNDMaterial::k_Height * lnd.GetMaterials().size(), 0);
	for (size_t i = 0; i < lnd.GetMaterials().size(); i++)
	{
		std::memcpy(&rgba5TextureData[lnd::LNDMaterial::k_Width * lnd::LNDMaterial::k_Height * i],
		            lnd.GetMaterials()[i].texels.data(),
		            sizeof(lnd.GetMaterials()[i].texels[0]) * lnd.GetMaterials()[i].texels.size());
	}
	_materialArray = std::make_unique<Texture2D>("LandIslandMaterialArray");
	_materialArray->Create(
	    lnd::LNDMaterial::k_Width, lnd::LNDMaterial::k_Height, materialCount, TextureFormat::BGR5A1, Wrapping::ClampEdge,
	    Filter::Linear,
	    bgfx::copy(rgba5TextureData.data(), static_cast<uint32_t>(rgba5TextureData.size() * sizeof(rgba5TextureData[0]))));

	// read noise map into Texture2D
	_noiseMap = lnd.GetExtra().noise.texels;
	_textureNoiseMap = std::make_unique<Texture2D>("LandIslandNoiseMap");
	_textureNoiseMap->Create(lnd::LNDBumpMap::k_Width, lnd::LNDBumpMap::k_Height, 1, TextureFormat::R8, Wrapping::ClampEdge,
	                         Filter::Linear,
	                         bgfx::copy(_noiseMap.data(), static_cast<uint32_t>(_noiseMap.size() * sizeof(_noiseMap[0]))));

	// read bump map into Texture2D (and keep it for the block texture)
	_bumpMap = lnd.GetExtra().bump.texels;
	_textureBumpMap = std::make_unique<Texture2D>("LandIslandBumpMap");
	_textureBumpMap->Create(
	    lnd::LNDBumpMap::k_Width, lnd::LNDBumpMap::k_Height, 1, TextureFormat::R8, Wrapping::Repeat, Filter::Linear,
	    bgfx::copy(lnd.GetExtra().bump.texels.data(),
	               static_cast<uint32_t>(sizeof(lnd.GetExtra().bump.texels[0]) * lnd.GetExtra().bump.texels.size())));

	_smallBump = CreateSmallBumpTexture();
	CreateBlockTexture();

	_materialInfo.clear();
	for (size_t i = 0; i < lnd.GetMaterials().size(); ++i)
	{
		const auto& material = lnd.GetMaterials()[i];
		glm::vec3 sum(0.0f);
		for (const auto& texel : material.texels)
		{
			sum += glm::vec3(texel.r, texel.g, texel.b);
		}
		const auto colour = sum / (31.0f * static_cast<float>(material.texels.size()));
		constexpr int k_Small = LandMaterialInfo::k_SmallSize;
		constexpr int k_Box = lnd::LNDMaterial::k_Width / k_Small;
		std::vector<uint8_t> small(static_cast<size_t>(k_Small) * k_Small * 3);
		for (int y = 0; y < k_Small; ++y)
		{
			for (int x = 0; x < k_Small; ++x)
			{
				glm::vec3 box(0.0f);
				for (int dy = 0; dy < k_Box; ++dy)
				{
					for (int dx = 0; dx < k_Box; ++dx)
					{
						const auto& texel = material.texels[static_cast<size_t>(y * k_Box + dy) * lnd::LNDMaterial::k_Width +
						                                    static_cast<size_t>(x * k_Box + dx)];
						box += glm::vec3(texel.r, texel.g, texel.b);
					}
				}
				box *= 255.0f / (31.0f * k_Box * k_Box);
				for (int c = 0; c < 3; ++c)
				{
					small[(static_cast<size_t>(y) * k_Small + x) * 3 + c] = static_cast<uint8_t>(box[c] + 0.5f);
				}
			}
		}
		_materialInfo.push_back({material.type, colour, std::move(small)});
	}

	// build the meshes (we could move this elsewhere)
	for (auto& block : _landBlocks)
	{
		block.BuildMesh(*this);
	}
	bgfx::frame();
}

float LandIsland::HeightAt(glm::vec2 vec, bool seaFlattening, bool meshFlattening) const
{
	// The height of the landscape triangle under the point, in the original's integer arithmetic. MapCoords are 16.16
	// fixed point with 10 units per cell; each cell is split into two triangles along the diagonal chosen by its split
	// bit, and the fourth corner is extrapolated from the other three so that the bilinear blend below is planar on that
	// triangle.
	// The MapCoords of the point: ToFixed, as GetNormalAt
	const int32_t fixedX = map_coords::ToFixed(vec.x);
	const int32_t fixedZ = map_coords::ToFixed(vec.y);
	// the high words read signed, 0 when negative or past the side (the original checks against 512; openblack's
	// islands may be larger)
	const int32_t signedX = map_coords::SignedCellOf(fixedX);
	const int32_t signedZ = map_coords::SignedCellOf(fixedZ);
	const int32_t cellsPerSide = GetCellsPerSide();
	if (signedX < 0 || signedZ < 0 || signedX >= cellsPerSide || signedZ >= cellsPerSide)
	{
		return 0.0f;
	}
	const auto cellX = static_cast<uint16_t>(signedX);
	const auto cellZ = static_cast<uint16_t>(signedZ);
	const auto fracX = static_cast<uint32_t>(fixedX) & 0xFFFFu;
	const auto fracZ = static_cast<uint32_t>(fixedZ) & 0xFFFFu;

	// The block stores 17 x 17 cells (one shared border row), so the neighbours are +1 (z) and +17 (x).
	const auto mapCoordinates = glm::u16vec2(cellX, cellZ) >> static_cast<uint16_t>(0x4);
	const auto blockIndex = BlockIndexAt(mapCoordinates);
	if (blockIndex == 0)
	{
		return 0.0f;
	}
	const auto* cells = _landBlocks[blockIndex - 1].GetCells();
	const auto* base = &cells[(cellX & 0xF) * 0x11u + (cellZ & 0xF)];
	// (64 bits: with 16 altitude bits of BWLandEditor maps the products below overflow 32)
	int64_t v00 = GetCellAltitude(base[0]);
	int64_t v01 = GetCellAltitude(base[1]);
	int64_t v10 = GetCellAltitude(base[0x11]);
	int64_t v11 = GetCellAltitude(base[0x12]);
	// Next to the sea (base corner at most 4) heights of 3 or less count as 0 (a global switch, on by default).
	if (seaFlattening && v00 <= 4)
	{
		const auto sea = [](int64_t v) { return v > 3 ? v : int64_t {0}; };
		v00 = sea(v00);
		v01 = sea(v01);
		v10 = sea(v10);
		v11 = sea(v11);
	}
	else if (meshFlattening)
	{
		// the drawn mesh: every vertex of 3 or less at 0
		const auto sea = [](int64_t v) { return v > 3 ? v : int64_t {0}; };
		v00 = sea(v00);
		v01 = sea(v01);
		v10 = sea(v10);
		v11 = sea(v11);
	}
	int64_t c00 = v00;
	int64_t c01 = v01;
	int64_t c10 = v10;
	int64_t c11 = v11;
	if (base[0].properties.split)
	{
		if (fracZ > 0xFFFFu - fracX)
		{
			c00 = v10 + v01 - v11;
		}
		else
		{
			c11 = v10 + v01 - v00;
		}
	}
	else if (fracX > fracZ)
	{
		c01 = v00 + v11 - v10;
	}
	else
	{
		c10 = v00 + v11 - v01;
	}
	const int64_t fx = fracX >> 8;
	const int64_t fz = fracZ >> 8;
	const int64_t atX1 = (c11 - c10) * fz + (c10 << 8);
	const int64_t atX0 = (c01 - c00) * fz + (c00 << 8);
	const int64_t height = (((atX1 - atX0) * fx) >> 8) + atX0;
	return static_cast<float>(height) * LandIsland::k_HeightUnit * (1.0f / 256.0f);
}

glm::vec3 LandIsland::GetNormalAt(glm::vec2 vec) const
{
	// The normal at the MapCoords of the point: every caller builds them as x * 65536 * 0.1, and 65536 * 0.1f is exactly
	// 6553.6f: ToFixed
	const glm::vec3 up(0.0f, 1.0f, 0.0f);
	const int32_t fixedX = map_coords::ToFixed(vec.x);
	const int32_t fixedZ = map_coords::ToFixed(vec.y);
	// the high words read signed (the original checks against 512; openblack's islands may be larger)
	const int32_t cellX = map_coords::SignedCellOf(fixedX);
	const int32_t cellZ = map_coords::SignedCellOf(fixedZ);
	const int32_t cellsPerSide = GetCellsPerSide();
	if (cellX < 0 || cellX >= cellsPerSide || cellZ < 0 || cellZ >= cellsPerSide)
	{
		return up;
	}
	// no block there -> up
	const auto blockIndex = BlockIndexAt(glm::u16vec2(cellX, cellZ) >> static_cast<uint16_t>(0x4));
	if (blockIndex == 0)
	{
		return up;
	}
	// the cell inside its block of 17 x 17 (the shared border row): +17 cells = x + 1, +1 = z + 1; raw altitudes, no sea
	// flattening
	const auto* cells = _landBlocks[blockIndex - 1].GetCells();
	const auto* base = &cells[static_cast<uint32_t>(cellX & 0xF) * 0x11u + static_cast<uint32_t>(cellZ & 0xF)];
	return land_normal::OfCell(static_cast<uint32_t>(fixedX) & 0xFFFFu, static_cast<uint32_t>(fixedZ) & 0xFFFFu,
	                           base[0].properties.split, GetCellAltitude(base[0]), GetCellAltitude(base[1]),
	                           GetCellAltitude(base[0x11]), GetCellAltitude(base[0x12]));
}

uint8_t LandIsland::GetNoise(glm::u8vec2 pos)
{
	return _noiseMap.at(pos.x * 256 + pos.y);
}

uint16_t LandIsland::BlockIndexAt(glm::u16vec2 blockCoordinates) const
{
	if (blockCoordinates.x >= _blocksPerSide || blockCoordinates.y >= _blocksPerSide)
	{
		return 0;
	}
	return _blockIndexLookup[static_cast<size_t>(blockCoordinates.x) * _blocksPerSide + blockCoordinates.y];
}

const LandBlock* LandIsland::GetBlock(const glm::u8vec2& coordinates) const
{
	const auto blockIndex = BlockIndexAt(glm::u16vec2(coordinates));
	if (blockIndex == 0)
	{
		return nullptr;
	}

	return &_landBlocks[blockIndex - 1];
}

constexpr lnd::LNDCell EmptyCell() noexcept
{
	lnd::LNDCell cell {};
	cell.properties.fullWater = true;
	return cell;
}

constexpr lnd::LNDCell k_EmptyCell = EmptyCell();

void LandIsland::SetCellAltitude(glm::u16vec2 cell, uint16_t altitude)
{
	// the box of the changed corners, for the block meshes that read them
	const glm::ivec2 changed(cell);
	_changedCorners = _changedCorners.has_value() ? CornerBox {.min = glm::min(_changedCorners->min, changed),
	                                                           .max = glm::max(_changedCorners->max, changed)}
	                                              : CornerBox {.min = changed, .max = changed};
	// the corner is in its own block at (x & 15, z & 15) and, on a block border, also in the blocks before it as
	// their shared row / column 16
	for (int dx = 0; dx <= 1; ++dx)
	{
		for (int dz = 0; dz <= 1; ++dz)
		{
			if ((dx == 1 && (cell.x & 0xF) != 0) || (dz == 1 && (cell.y & 0xF) != 0))
			{
				continue;
			}
			const int blockX = (cell.x >> 4) - dx;
			const int blockZ = (cell.y >> 4) - dz;
			if (blockX < 0 || blockZ < 0)
			{
				continue;
			}
			const auto blockIndex = BlockIndexAt(glm::u16vec2(blockX, blockZ));
			if (blockIndex == 0)
			{
				continue;
			}
			auto& block = *_landBlocks[blockIndex - 1].GetLndBlock();
			const glm::ivec2 corner((cell.x & 0xF) + 16 * dx, (cell.y & 0xF) + 16 * dz);
			auto& target = block.cells.at(static_cast<size_t>(corner.x) * 17 + static_cast<size_t>(corner.y));
			target.altitude = static_cast<uint8_t>(altitude & 0xFFu);
			if (_altitudeBits > 8)
			{
				const auto highMask = static_cast<uint8_t>(0xFFu >> (16u - _altitudeBits));
				target.saveColor = static_cast<uint8_t>((target.saveColor & ~highMask) | ((altitude >> 8u) & highMask));
			}
			// the cells whose texels read this corner
			const auto touched = block_texture::CellsOfCorner(corner);
			const size_t index = blockIndex - 1u;
			if (auto found = std::ranges::find(_changedBlocks, index, &ChangedBlock::index); found != _changedBlocks.end())
			{
				found->cells = block_texture::Union(found->cells, touched);
			}
			else
			{
				_changedBlocks.push_back({.index = index, .cells = touched});
			}
		}
	}
}

void LandIsland::RebuildAltitudes()
{
	if (_changedBlocks.empty())
	{
		return;
	}
	for (size_t i = 0; i < _landBlocks.size(); ++i)
	{
		const auto position = _landBlocks[i].GetBlockPosition();
		const bool near = std::ranges::any_of(_changedBlocks, [&](const ChangedBlock& changed) {
			const auto delta = glm::abs(_landBlocks[changed.index].GetBlockPosition() - position);
			return delta.x <= 1 && delta.y <= 1;
		});
		if (!near)
		{
			continue;
		}
		// Every block around a changed one leaves the physics world and comes back, in this order, so the world's
		// bodies keep the order they always had here. Only a block whose mesh reads a changed corner is built again
		// (a new mesh and a new rigid body with the same identity, DynamicsSystem::RegisterIslandRigidBodies); the
		// others would build the same mesh and shape, so their own body goes back unchanged.
		auto& body = _landBlocks[i].GetRigidBody();
		const bool inWorld = body != nullptr && body->getBroadphaseHandle() != nullptr && Locator::dynamicsSystem::has_value();
		const int userIndex = inWorld ? body->getUserIndex() : -1;
		const int userIndex2 = inWorld ? body->getUserIndex2() : -1;
		void* userPointer = inWorld ? body->getUserPointer() : nullptr;
		if (inWorld)
		{
			Locator::dynamicsSystem::value().RemoveRigidBody(body.get());
		}
		if (!_changedCorners.has_value() || LandBlock::ReadsCorners(position, *_changedCorners))
		{
			_landBlocks[i].BuildMesh(*this);
		}
		if (inWorld)
		{
			auto& rebuilt = _landBlocks[i].GetRigidBody();
			rebuilt->setUserIndex(userIndex);
			rebuilt->setUserIndex2(userIndex2);
			rebuilt->setUserPointer(userPointer);
			Locator::dynamicsSystem::value().AddRigidBody(rebuilt.get());
		}
	}
	const auto indexSize = _extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1);
	const auto heightMapData = CreateHeightMap();
	_heightMap = std::make_unique<Texture2D>("Height Map");
	_heightMap->Create(
	    indexSize.x * k_CellCount + 1, indexSize.y * k_CellCount + 1, 1, graphics::TextureFormat::RG32F, Wrapping::ClampEdge,
	    Filter::Nearest,
	    bgfx::copy(heightMapData.data(), static_cast<uint32_t>(heightMapData.size() * sizeof(heightMapData[0]))));
	// only the texels of the cells whose corners moved change: the rest of the block texture stays as it is
	RepaintChangedBlockTexels();
	_changedBlocks.clear();
	_changedCorners.reset();
}

block_texture::Sources LandIsland::BlockTextureSources() const
{
	return {
	    .countries = _countries,
	    .materials = {_materialTexels, _materialTexels.size() / lnd::LNDMaterial::k_Width / lnd::LNDMaterial::k_Height},
	    .noise = _noiseMap,
	    .bump = _bumpMap,
	    .altitudeBits = _altitudeBits,
	};
}

void LandIsland::RepaintChangedBlockTexels()
{
	if (_blockTexture == nullptr)
	{
		return;
	}
	const auto sources = BlockTextureSources();
	const auto indexSize = glm::ivec2(_extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1));
	for (const auto& changed : _changedBlocks)
	{
		const auto& block = _landBlocks[changed.index];
		const auto* cells = block.GetCells();
		const auto position = block.GetBlockPosition() - glm::ivec2(_extentIndexMin);
		if (cells == nullptr || glm::any(glm::lessThan(position, glm::ivec2(0))) ||
		    glm::any(glm::greaterThanEqual(position, indexSize)))
		{
			continue;
		}
		const auto patch = block_texture::PaintBox(
		    std::span(cells, static_cast<size_t>(block_texture::k_CellsPerSide) * block_texture::k_CellsPerSide), sources,
		    _texelsPerBlock, changed.cells);
		if (patch.texels.Empty())
		{
			continue;
		}
		const auto origin = position * static_cast<int>(_texelsPerBlock) + patch.texels.min;
		const auto size = patch.texels.max - patch.texels.min + 1;
		_blockTexture->UpdateRegion(glm::u16vec2(origin), glm::u16vec2(size), patch.rgba.data(),
		                            static_cast<uint32_t>(patch.rgba.size()));
	}
}

void LandIsland::CreateBlockTexture()
{
	const auto indexSize = _extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1);
	const auto texels = block_texture::BuildIslandBlockTexture(*this, _extentIndexMin, indexSize, _texelsPerBlock, _noiseMap,
	                                                           _bumpMap, BlockTextureSources().materials);
	const auto size = indexSize * _texelsPerBlock;
	// bilinear like the D3D sampling of the block textures (one texture over the whole island here, so the filter
	// also runs across block borders)
	// made without texels and then filled, so that it stays updatable: RebuildAltitudes replaces only the rectangles
	// whose cells changed
	_blockTexture = std::make_unique<Texture2D>("BlockTexture");
	_blockTexture->Create(size.x, size.y, 1, TextureFormat::RGBA8, Wrapping::ClampEdge, Filter::Linear, nullptr);
	_blockTexture->Update(texels.data(), static_cast<uint32_t>(texels.size()));

	// Debug: OPENBLACK_DUMP_COAST_ALPHA=1 (or =<file>.png) writes the alpha nibble (x 17) and OPENBLACK_DUMP_BLOCK_TEXTURE
	// the RGBA texture, x to the right and z down from the block extent minimum
	if (const char* dump = std::getenv("OPENBLACK_DUMP_COAST_ALPHA"); dump != nullptr && dump[0] != '\0')
	{
		const std::string file = std::string(dump) == "1" ? std::string("coast_alpha.png") : std::string(dump);
		std::vector<uint8_t> alpha(texels.size() / 4);
		for (size_t i = 0; i < alpha.size(); ++i)
		{
			alpha[i] = texels[i * 4 + 3];
		}
		const int ok = stbi_write_png(file.c_str(), size.x, size.y, 1, alpha.data(), size.x);
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "[LandIsland] coast alpha {} x {} ({} texels per block, first block {}, {}) -> {} ({})", size.x,
		                   size.y, _texelsPerBlock, _extentIndexMin.x, _extentIndexMin.y, file, ok != 0 ? "ok" : "FAILED");
	}
	if (const char* dump = std::getenv("OPENBLACK_DUMP_BLOCK_TEXTURE"); dump != nullptr && dump[0] != '\0')
	{
		const std::string file = std::string(dump) == "1" ? std::string("block_texture.png") : std::string(dump);
		const int ok = stbi_write_png(file.c_str(), size.x, size.y, 4, texels.data(), size.x * 4);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "[LandIsland] block texture {} x {} -> {} ({})", size.x, size.y, file,
		                   ok != 0 ? "ok" : "FAILED");
	}
}

const lnd::LNDCell& LandIsland::GetCell(const glm::u16vec2& coordinates) const
{
	const auto mapCoordinates = coordinates >> static_cast<uint16_t>(0x4);
	const auto cellCoordinates = static_cast<glm::u8vec2>(coordinates) & static_cast<uint8_t>(0xF);
	const auto cellIndex = cellCoordinates.x * 0x11u + cellCoordinates.y;

	const auto blockIndex = BlockIndexAt(mapCoordinates);

	if (blockIndex == 0)
	{
		return k_EmptyCell;
	}
	assert(_landBlocks.size() >= blockIndex);
	return _landBlocks[blockIndex - 1].GetCells()[cellIndex];
}

void LandIsland::DumpTextures() const
{
	_materialArray->DumpTexture();
}

std::vector<float> LandIsland::CreateHeightMap() const
{
	// 16x16 cells but the last is shared
	// 32x32 block grid in the original maps (512 x 512 pixels), up to 128x128 in BWLandEditor ones
	// extra pixel at the end of the map
	// Two floats per cell (x along the texture's u, z along v): the altitude in height units and the split bit. All
	// 17 x 17 cells of each block, so the last row and column of the map are there too (0 where there is no block,
	// as GetAltitude gives over the sea).
	std::vector<float> data;
	const auto extentSize = _extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1);
	const auto resolution = extentSize * static_cast<uint16_t>(k_CellCount) + static_cast<uint16_t>(1);
	data.resize(static_cast<size_t>(resolution.x) * resolution.y * 2, 0.0f);

	for (const auto& block : _landBlocks)
	{
		const auto* cells = block.GetCells();
		const auto mapPos = block.GetBlockPosition() - static_cast<glm::ivec2>(_extentIndexMin);
		for (int z = 0; z <= k_CellCount; z++)
		{
			for (int x = 0; x <= k_CellCount; x++)
			{
				const auto cellPos = mapPos * static_cast<int>(k_CellCount) + glm::ivec2(x, z);
				if (cellPos.x >= resolution.x || cellPos.y >= resolution.y)
				{
					continue;
				}
				const auto& cell = cells[x * (k_CellCount + 1) + z];
				auto* texel = &data[(static_cast<size_t>(cellPos.y) * resolution.x + cellPos.x) * 2];
				texel[0] = static_cast<float>(GetCellAltitude(cell));
				texel[1] = cell.properties.split ? 1.0f : 0.0f;
			}
		}
	}
	return data;
}

std::vector<uint8_t> LandIsland::CreateCellMap() const
{
	const auto extentSize = _extentIndexMax - _extentIndexMin + glm::u16vec2(1, 1);
	const auto resolution = extentSize * static_cast<uint16_t>(k_CellCount) + static_cast<uint16_t>(1);
	// Where there is no block (the open sea) the light is full (the last entry of the light table) and there is no
	// specular: colour 0, luminosity 255
	std::vector<uint8_t> data(static_cast<size_t>(resolution.x) * resolution.y * 4, 0);
	for (size_t i = 3; i < data.size(); i += 4)
	{
		data[i] = 255;
	}
	for (const auto& block : _landBlocks)
	{
		const auto blockOffset = static_cast<glm::u16vec2>(block.GetBlockPosition() * 16);
		const auto mapPos = block.GetBlockPosition() - static_cast<glm::ivec2>(_extentIndexMin);
		// 17 x 17: the last row and column are the first ones of the next block
		for (int y = 0; y <= k_CellCount; y++)
		{
			for (int x = 0; x <= k_CellCount; x++)
			{
				const auto cellPos = mapPos * static_cast<int>(k_CellCount) + glm::ivec2(x, y);
				if (cellPos.x >= resolution.x || cellPos.y >= resolution.y)
				{
					continue;
				}
				const auto& cell = GetCell(blockOffset + glm::u16vec2(x, y));
				auto* texel = &data[(static_cast<size_t>(cellPos.y) * resolution.x + cellPos.x) * 4];
				// the cell's first dword is read as a Direct3D colour: bytes r, g, b -> blue, green, red
				texel[0] = cell.b;
				texel[1] = cell.g;
				texel[2] = cell.r;
				texel[3] = cell.luminosity;
			}
		}
	}
	return data;
}

void LandIsland::DumpMaps() const
{
	const auto heights = CreateHeightMap();
	std::vector<uint8_t> data(heights.size());
	std::ranges::transform(heights, data.begin(), [](float h) { return static_cast<uint8_t>(std::min(h, 255.0f)); });
	FILE* fptr = fopen("dump.raw", "wb");
	fwrite(data.data(), data.size() * sizeof(data[0]), 1, fptr);
	fclose(fptr);
}

// ---------------------------------------------------------------------------------------------------------------------
// The island ray cast: the ray clipped to the map, walked from cell to cell and each cell's two triangles tested.
// Float arithmetic where the original keeps x87 extended values between the stores: (approximate) only at the exact
// borders of a cell or of a triangle.

std::array<uint16_t, 4> LandIslandInterface::GetCellCorners(glm::u16vec2 cell) const
{
	const auto next = [](uint16_t v) { return static_cast<uint16_t>(v + 1u); };
	return {GetCellAltitude(GetCell(cell)), GetCellAltitude(GetCell({cell.x, next(cell.y)})),
	        GetCellAltitude(GetCell({next(cell.x), cell.y})), GetCellAltitude(GetCell({next(cell.x), next(cell.y)}))};
}

std::array<uint16_t, 4> LandIsland::GetCellCorners(glm::u16vec2 cell) const
{
	const auto blockIndex = BlockIndexAt(cell >> static_cast<uint16_t>(0x4));
	if (blockIndex == 0)
	{
		return {0, 0, 0, 0};
	}
	// the cell is at (x & 15) x 17 + (z & 15) in its block; the neighbours are +1 (z) and +17 (x)
	const auto* base = &_landBlocks[blockIndex - 1].GetCells()[(cell.x & 0xFu) * 0x11u + (cell.y & 0xFu)];
	return {GetCellAltitude(base[0]), GetCellAltitude(base[1]), GetCellAltitude(base[0x11]), GetCellAltitude(base[0x12])};
}

namespace
{
constexpr float k_RayCellsPerMetre = 0.1f;                   ///< x and z in cells
constexpr float k_RayMetresPerCell = 10.0f;                  ///< the hit back in metres
constexpr float k_RayClipMargin = 0.1f;                      ///< the walk is clipped to 0.1 .. cells - 0.1
constexpr float k_RayNoBorder = 1e20f;                       ///< no map edge in that direction
constexpr double k_RayFlatTriangle = 9.9999997473787516e-05; ///< |denominator| below it, no hit
constexpr float k_RayTriangleMax = 1.1f;                     ///< the cell's local u, v up to 1.1
constexpr float k_RayTriangleMin = -0.1f;                    ///< and from -0.1
constexpr float k_RayDiagonalLow = 1.1f;                     ///< the first triangle, u + v <= 1.1
constexpr float k_RayDiagonalHigh = 0.9f;                    ///< the second one, u + v >= 0.9
constexpr float k_RayMinDrop = 0.0001f;                      ///< a ray flatter than this never meets y = 0
constexpr float k_RaySeaDistanceSq = 5.625e7f;               ///< 7500^2: the y = 0 point counts this close to the camera

/// The segment p0 -> p1 (x, z, y in cell units) against the two triangles of the cell, (0,0) (1,0) (0,1) then
/// (1,1) (1,0) (0,1), as planes through its corner altitudes. A hit must lie in front of p0 (dot with the segment
/// >= 0) but may be past p1: only the cell's altitude range bounds it
bool RayCellTest(const LandIslandInterface& island, int32_t cellX, int32_t cellZ, float p0x, float p0z, float p0y, float p1x,
                 float p1z, float p1y, glm::vec2& hit)
{
	// 0 .. 511 on both axes in the original (BWLandEditor maps have more cells)
	const auto last = static_cast<int32_t>(island.GetCellsPerSide()) - 1;
	if (cellX < 0 || cellX > last || cellZ < 0 || cellZ > last)
	{
		return false;
	}
	// the cell must be in a block
	const glm::u16vec2 cell(static_cast<uint16_t>(cellX), static_cast<uint16_t>(cellZ));
	if (!island.HasBlockAt(cell))
	{
		return false;
	}
	const auto corners = island.GetCellCorners(cell);
	const int32_t h00 = corners[0];
	const int32_t h01 = corners[1];
	const int32_t h10 = corners[2];
	const int32_t h11 = corners[3];
	// the highest and the lowest corner
	const int32_t highX0 = h00 > h10 ? h00 : h10;
	const int32_t highX1 = h01 > h11 ? h01 : h11;
	const auto high = static_cast<float>(highX0 > highX1 ? highX0 : highX1);
	const int32_t lowX0 = h00 < h10 ? h00 : h10;
	const int32_t lowX1 = h01 < h11 ? h01 : h11;
	const auto low = static_cast<float>(lowX0 < lowX1 ? lowX0 : lowX1);
	// both ends under the lowest corner, or both over the highest: no hit
	if (p0y < low && p1y < low)
	{
		return false;
	}
	if (!(p0y <= high) && !(p1y <= high))
	{
		return false;
	}
	const float dx = p1x - p0x;
	const float dz = p1z - p0z;
	const float dy = p1y - p0y;
	const float u0 = p0x - static_cast<float>(cellX);
	const float v0 = p0z - static_cast<float>(cellZ);
	const auto localAt = [&](float t) {
		// u = (t dx + p0x) - x, v = (t dz + p0z) - z, stored as floats
		return glm::vec2(t * dx + p0x - static_cast<float>(cellX), t * dz + p0z - static_cast<float>(cellZ));
	};
	const auto inFront = [&](glm::vec2 uv) {
		// the point (x + u, z + v) is not behind p0: (wx - p0x) dx + (wz - p0z) dz >= 0
		const glm::vec2 world(static_cast<float>(cellX) + uv.x, static_cast<float>(cellZ) + uv.y);
		if ((world.x - p0x) * dx + (world.y - p0z) * dz < 0.0f)
		{
			return false;
		}
		hit = world;
		return true;
	};
	// The first triangle: the plane h00 + a u + b v, a = h10 - h00, b = h01 - h00. A flat denominator ends the test
	// without trying the second triangle
	const auto a = static_cast<float>(h10 - h00);
	const auto b = static_cast<float>(h01 - h00);
	const float denominator = (dy - a * dx) - b * dz;
	if (static_cast<double>(std::fabs(denominator)) < k_RayFlatTriangle)
	{
		return false;
	}
	const float t = (((v0 * b + u0 * a) + static_cast<float>(h00)) - p0y) / denominator;
	const auto uv = localAt(t);
	if (!(uv.x > k_RayTriangleMax) && !(uv.y > k_RayTriangleMax) && !(uv.x < k_RayTriangleMin) && !(uv.y < k_RayTriangleMin) &&
	    !(k_RayDiagonalLow - uv.y < uv.x) && inFront(uv))
	{
		return true;
	}
	// The second triangle: the plane h11 + sx (u - 1) + sz (v - 1), sx = h11 - h01, sz = h11 - h10, written as
	// (h10 - sx) + v sz + u sx
	const auto sz = static_cast<float>(h11 - h10);
	const auto sx = static_cast<float>(h11 - h01);
	const float denominator2 = (dy - sx * dx) - sz * dz;
	if (static_cast<double>(std::fabs(denominator2)) < k_RayFlatTriangle)
	{
		return false;
	}
	const float t2 = ((((static_cast<float>(h10) - sx) + v0 * sz) + u0 * sx) - p0y) / denominator2;
	const auto uv2 = localAt(t2);
	if (uv2.x > k_RayTriangleMax || uv2.y > k_RayTriangleMax || uv2.x < k_RayTriangleMin || uv2.y < k_RayTriangleMin ||
	    k_RayDiagonalHigh - uv2.y > uv2.x)
	{
		return false;
	}
	return inFront(uv2);
}

/// The Cohen-Sutherland code of the clip: 1 x < 0.1, 2 x > 511.9, 4 z < 0.1, 8 z > 511.9 (for 512 cells)
uint32_t RayOutCode(float x, float z, float high)
{
	uint32_t code = 0;
	if (x < k_RayClipMargin)
	{
		code = 1;
	}
	else if (x > high)
	{
		code = 2;
	}
	if (z < k_RayClipMargin)
	{
		code |= 4;
	}
	else if (z > high)
	{
		code |= 8;
	}
	return code;
}
} // namespace

bool LandIslandInterface::RayCastCells(float x0, float z0, float y0, float x1, float z1, float y1, glm::vec2& hit) const
{
	const auto cells = static_cast<float>(GetCellsPerSide()); // 512 in the original
	const float high = cells - k_RayClipMargin;               // 511.9 for 512 cells
	// the ray parameter t of the first map edge (0 or 512) it meets in x and in z, the smaller one
	float t = k_RayNoBorder;
	if (x0 > x1)
	{
		t = x0 / (x0 - x1);
	}
	else if (x0 < x1)
	{
		t = (cells - x0) / (x1 - x0);
	}
	float tz = k_RayNoBorder;
	if (z0 > z1)
	{
		tz = z0 / (z0 - z1);
	}
	else if (z0 < z1)
	{
		tz = (cells - z0) / (z1 - z0);
	}
	if (!(t <= tz))
	{
		t = tz;
	}
	// the END is moved on by t x (end - start), to the edge plus one more (end - start): the walk below casts a ray to
	// the map's edge, not the segment. The original's `t == 1e20` guard compares the float 1e20 with the double 1e20,
	// which are not equal: the end is always moved, also without an edge
	x1 = (x1 - x0) * t + x1;
	z1 = (z1 - z0) * t + z1;
	y1 = (y1 - y0) * t + y1;
	// clipped to 0.1 .. 511.9 (Cohen-Sutherland, x then z)
	uint32_t code0 = RayOutCode(x0, z0, high);
	uint32_t code1 = RayOutCode(x1, z1, high);
	if ((code0 & code1) != 0)
	{
		return false;
	}
	if ((code0 | code1) != 0)
	{
		uint32_t differ = code0 ^ code1;
		// x = edge: the end that is out on that side moves there and gets the code of its new z alone
		const auto clipX = [&](float edge, uint32_t bit) {
			const float s = (edge - x0) / (x1 - x0);
			const float z = (z1 - z0) * s + z0;
			const float y = (y1 - y0) * s + y0;
			if ((code1 & bit) != 0)
			{
				y1 = y;
				z1 = z;
				x1 = edge;
				code1 = RayOutCode(k_RayClipMargin, z, high);
			}
			else
			{
				y0 = y;
				z0 = z;
				x0 = edge;
				code0 = RayOutCode(k_RayClipMargin, z, high);
			}
		};
		// z = edge: that end's code becomes 0, its x is not tested again
		const auto clipZ = [&](float edge, uint32_t bit) {
			const float s = (edge - z0) / (z1 - z0);
			const float x = (x1 - x0) * s + x0;
			const float y = (y1 - y0) * s + y0;
			if ((code1 & bit) != 0)
			{
				y1 = y;
				z1 = edge;
				code1 = 0;
				x1 = x;
			}
			else
			{
				y0 = y;
				z0 = edge;
				code0 = 0;
				x0 = x;
			}
		};
		if ((differ & 1u) != 0)
		{
			clipX(k_RayClipMargin, 1u);
			differ = code0 ^ code1;
		}
		if ((code0 & code1) != 0)
		{
			return false;
		}
		if ((differ & 2u) != 0)
		{
			clipX(high, 2u);
			differ = code0 ^ code1;
		}
		if ((code0 & code1) != 0)
		{
			return false;
		}
		if ((differ & 4u) != 0)
		{
			clipZ(k_RayClipMargin, 4u);
			differ = code0 ^ code1;
		}
		if ((code0 & code1) != 0)
		{
			return false;
		}
		if ((differ & 8u) != 0)
		{
			clipZ(high, 8u); // (no new `differ` after this one)
		}
		if ((code0 & code1) != 0)
		{
			return false;
		}
	}
	// the cells of both ends (truncated towards 0), and the start as the first piece's start
	int32_t xi = map_coords::FtoL(x0);
	int32_t zi = map_coords::FtoL(z0);
	const int32_t lastZ = map_coords::FtoL(z1);
	const int32_t lastX = map_coords::FtoL(x1);
	glm::vec3 previous(x0, z0, y0); // x, z, y
	const glm::vec3 start(x0, z0, y0);
	const auto test = [&](int32_t cx, int32_t cz, const glm::vec3& from, float tx, float tz, float ty) {
		return RayCellTest(*this, cx, cz, from.x, from.y, from.z, tx, tz, ty, hit);
	};
	// the last cell gets the piece to the (moved) end, and its answer is the answer
	const auto last = [&]() { return test(xi, zi, previous, x1, z1, y1); };
	if (xi == lastX)
	{
		// one column: every cell is tested with the whole clipped segment
		for (; zi < lastZ; ++zi)
		{
			if (test(xi, zi, start, x1, z1, y1))
			{
				return true;
			}
		}
		for (; zi > lastZ; --zi)
		{
			if (test(xi, zi, start, x1, z1, y1))
			{
				return true;
			}
		}
		return last();
	}
	if (zi == lastZ)
	{
		// one row, the same
		for (; xi < lastX; ++xi)
		{
			if (test(xi, zi, start, x1, z1, y1))
			{
				return true;
			}
		}
		for (; xi > lastX; --xi)
		{
			if (test(xi, zi, start, x1, z1, y1))
			{
				return true;
			}
		}
		return last();
	}
	// the slopes, kxz = dx / dz, kzx = 1 / kxz (not dz / dx), kyz = dy / dz, kyx = dy / dx
	const float dx = x1 - x0;
	const float dz = z1 - z0;
	const float kxz = dx / dz;
	const float kzx = 1.0f / kxz;
	const float dy = y1 - y0;
	const float kyz = dy / dz;
	const float kyx = dy / dx;
	const bool zUp = z0 <= z1;
	const bool xUp = x0 <= x1;
	// (openblack guard) the walk visits at most |dx| + |dz| + 1 cells; a float drift that kept it going ends it here
	const int32_t guard = 4 * static_cast<int32_t>(GetCellsPerSide()) + 4;
	// The current point (x0, z0, y0) moves from cell to cell: across the z edge when the x there is still in the
	// column (FtoL(x) == the column), else across the x edge. Each cell is tested with the piece from the previous point
	for (int32_t steps = 0; steps < guard; ++steps)
	{
		const bool more = (xUp ? xi < lastX : xi > lastX) || (zUp ? zi < lastZ : zi > lastZ);
		if (!more)
		{
			return last();
		}
		const int32_t cellX = xi;
		const int32_t cellZ = zi;
		if (!zUp)
		{
			// the cell's lower z edge
			const auto edge = static_cast<float>(zi);
			const float toEdge = z0 - edge;
			const float xAt = x0 - toEdge * kxz;
			if (map_coords::FtoL(xAt) == xi)
			{
				x0 = xAt;
				--zi;
				y0 = y0 - toEdge * kyz;
				z0 = edge;
			}
			else if (xUp)
			{
				// the column's upper x edge
				++xi;
				const auto edgeX = static_cast<float>(xi);
				const float across = edgeX - x0;
				z0 = z0 + across * kzx;
				zi = map_coords::FtoL(z0);
				y0 = y0 + across * kyx;
				x0 = edgeX;
			}
			else
			{
				// the column's lower x edge
				const auto edgeX = static_cast<float>(xi);
				const float across = x0 - edgeX;
				z0 = z0 - across * kzx;
				zi = map_coords::FtoL(z0);
				--xi;
				y0 = y0 - across * kyx;
				x0 = edgeX;
			}
		}
		else
		{
			// the cell's upper z edge
			++zi;
			const auto edge = static_cast<float>(zi);
			const float toEdge = edge - z0;
			const float xAt = x0 + toEdge * kxz;
			if (map_coords::FtoL(xAt) == xi)
			{
				x0 = xAt;
				y0 = toEdge * kyz + y0;
				z0 = edge;
			}
			else if (xUp)
			{
				++xi;
				const auto edgeX = static_cast<float>(xi);
				const float across = edgeX - x0;
				z0 = z0 + across * kzx;
				zi = map_coords::FtoL(z0);
				y0 = y0 + across * kyx;
				x0 = edgeX;
			}
			else
			{
				const auto edgeX = static_cast<float>(xi);
				const float across = x0 - edgeX;
				z0 = z0 - across * kzx;
				zi = map_coords::FtoL(z0);
				--xi;
				y0 = y0 - across * kyx;
				x0 = edgeX;
			}
		}
		// the cell just left, tested with the piece from the previous point to the new one
		if (test(cellX, cellZ, previous, x0, z0, y0))
		{
			return true;
		}
		previous = glm::vec3(x0, z0, y0);
	}
	return false;
}

bool LandIslandInterface::RayCast(const glm::vec3& from, const glm::vec3& to, glm::vec2& hit, const glm::vec3& camera) const
{
	// The ray in cell units: x and z x 0.1, y / 0.67 (k_HeightUnit)
	glm::vec2 cellsHit(0.0f);
	if (RayCastCells(from.x * k_RayCellsPerMetre, from.z * k_RayCellsPerMetre, from.y / k_HeightUnit, to.x * k_RayCellsPerMetre,
	                 to.z * k_RayCellsPerMetre, to.y / k_HeightUnit, cellsHit))
	{
		hit = cellsHit * k_RayMetresPerCell;
		return true;
	}
	// a ray going down (to.y <= from.y) and not flat meets y = 0 at t = -(from.y / dy); the point is written either way,
	// and counts when (dx^2 + dz^2) from the camera <= 7500^2
	if (!(to.y <= from.y))
	{
		return false;
	}
	const float dy = to.y - from.y;
	if (std::fabs(dy) < k_RayMinDrop)
	{
		return false;
	}
	const float t = -(from.y / dy);
	hit.x = (to.x - from.x) * t + from.x;
	hit.y = (to.z - from.z) * t + from.z;
	const float cx = hit.x - camera.x;
	const float cz = hit.y - camera.z;
	return cx * cx + cz * cz <= k_RaySeaDistanceSq;
}
