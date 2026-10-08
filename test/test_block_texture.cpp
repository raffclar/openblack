/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The block texture painted again box by box after altitude edits (block_texture::PaintBox over the cells the edited
// corners touch, as LandIsland::RebuildAltitudes does) gives the same texels as painting the whole island again
// (BuildIslandBlockTexture), on a small synthetic island: random altitudes, countries, open sea cells, materials, noise
// and bump, random edits in several rounds, corners on block borders and on the island's edges, 8 and 10 altitude
// bits, and fewer texels per block than the original's 256.

#include <cstdint>

#include <algorithm>
#include <random>
#include <stdexcept>
#include <vector>

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/BlockTexture.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Dynamics/LandBlockBulletMeshInterface.h"
#include "Graphics/Mesh.h"

using namespace openblack;

namespace
{
constexpr int k_Side = block_texture::k_CellsPerSide;
constexpr size_t k_MapTexels = 256 * 256;

/// Blocks on a grid of blocksX x blocksZ from block (firstX, firstZ), their 17 x 17 cells from one grid of corners so
/// that the rows and columns two blocks share hold the same cells, as in a land file
class TextureIsland final: public LandIslandInterface
{
public:
	TextureIsland(glm::ivec2 first, glm::ivec2 blocks, uint8_t altitudeBits, std::mt19937& random)
	    : _first(first)
	    , _blocks(blocks)
	    , _altitudeBits(altitudeBits)
	{
		const glm::ivec2 corners = blocks * block_texture::k_CellsPerBlock + 1;
		std::vector<lnd::LNDCell> grid(static_cast<size_t>(corners.x) * static_cast<size_t>(corners.y));
		std::uniform_int_distribution<int> altitude(0, (1 << altitudeBits) - 1);
		std::uniform_int_distribution<int> country(0, 2);
		std::uniform_int_distribution<int> percent(0, 99);
		for (auto& cell : grid)
		{
			SetAltitude(cell, static_cast<uint16_t>(altitude(random)));
			cell.properties.country = static_cast<uint8_t>(country(random));
			cell.flags = percent(random) < 5 ? uint8_t {0x02} : uint8_t {0}; // some open sea cells, not drawn
		}
		for (int bx = 0; bx < blocks.x; ++bx)
		{
			for (int bz = 0; bz < blocks.y; ++bz)
			{
				lnd::LNDBlock block {};
				block.blockX = static_cast<uint32_t>(first.x + bx);
				block.blockZ = static_cast<uint32_t>(first.y + bz);
				for (int x = 0; x < k_Side; ++x)
				{
					for (int z = 0; z < k_Side; ++z)
					{
						const auto gx = static_cast<size_t>(bx * block_texture::k_CellsPerBlock + x);
						const auto gz = static_cast<size_t>(bz * block_texture::k_CellsPerBlock + z);
						block.cells.at(static_cast<size_t>(x) * k_Side + static_cast<size_t>(z)) =
						    grid[gx * static_cast<size_t>(corners.y) + gz];
					}
				}
				_landBlocks.emplace_back().SetLndBlock(block);
			}
		}
		// three countries over four materials, one and two material entries
		std::uniform_int_distribution<uint32_t> material(0, 3);
		std::uniform_int_distribution<uint32_t> coefficient(0, 256);
		_countries.resize(3);
		for (auto& c : _countries)
		{
			for (auto& entry : c.materials)
			{
				entry.indices = {material(random), percent(random) < 30 ? 0u : material(random)};
				entry.coefficient = coefficient(random);
			}
		}
	}

	void SetAltitude(lnd::LNDCell& cell, uint16_t altitude) const
	{
		cell.altitude = static_cast<uint8_t>(altitude & 0xFFu);
		if (_altitudeBits > 8)
		{
			const auto highMask = static_cast<uint8_t>(0xFFu >> (16u - _altitudeBits));
			cell.saveColor = static_cast<uint8_t>((cell.saveColor & ~highMask) | ((altitude >> 8u) & highMask));
		}
	}

	/// The edit of one corner of the island's grid of corners, in every block that holds it; the box of each changed
	/// block grows by the cells the corner touches (LandIsland::SetCellAltitude)
	void EditCorner(glm::ivec2 corner, uint16_t altitude, std::vector<block_texture::CellBox>& boxes,
	                std::vector<bool>& changed)
	{
		for (size_t i = 0; i < _landBlocks.size(); ++i)
		{
			const glm::ivec2 local = corner - (_landBlocks[i].GetBlockPosition() - _first) * block_texture::k_CellsPerBlock;
			if (local.x < 0 || local.y < 0 || local.x >= k_Side || local.y >= k_Side)
			{
				continue;
			}
			auto& cell =
			    _landBlocks[i].GetLndBlock()->cells.at(static_cast<size_t>(local.x) * k_Side + static_cast<size_t>(local.y));
			SetAltitude(cell, altitude);
			const auto touched = block_texture::CellsOfCorner(local);
			boxes[i] = changed[i] ? block_texture::Union(boxes[i], touched) : touched;
			changed[i] = true;
		}
	}

	[[nodiscard]] glm::ivec2 Corners() const { return _blocks * block_texture::k_CellsPerBlock + 1; }
	[[nodiscard]] glm::ivec2 First() const { return _first; }
	[[nodiscard]] glm::ivec2 Size() const { return _blocks; }

	[[nodiscard]] uint8_t GetAltitudeBits() const final { return _altitudeBits; }
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2&) const final { throw std::logic_error("no cells"); }
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { return _landBlocks; }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { return _landBlocks; }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { return _countries; }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::FrameBuffer& GetStaticShadowFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] const graphics::FrameBuffer& GetLandAlphaFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] const graphics::FrameBuffer& GetFootprintFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] glm::mat4 GetOrthoView() const final { return glm::mat4(1.0f); }
	[[nodiscard]] glm::mat4 GetOrthoProj() const final { return glm::mat4(1.0f); }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

private:
	glm::ivec2 _first;
	glm::ivec2 _blocks;
	uint8_t _altitudeBits;
	std::vector<LandBlock> _landBlocks;
	std::vector<lnd::LNDCountry> _countries;
};

struct Maps
{
	std::vector<uint16_t> materials;
	std::vector<uint8_t> noise;
	std::vector<uint8_t> bump;
};

Maps RandomMaps(std::mt19937& random)
{
	Maps maps {std::vector<uint16_t>(4 * k_MapTexels), std::vector<uint8_t>(k_MapTexels), std::vector<uint8_t>(k_MapTexels)};
	std::uniform_int_distribution<int> word(0, 0xFFFF);
	std::uniform_int_distribution<int> byte(0, 0xFF);
	for (auto& texel : maps.materials)
	{
		texel = static_cast<uint16_t>(word(random));
	}
	for (auto& n : maps.noise)
	{
		n = static_cast<uint8_t>(byte(random));
	}
	for (auto& b : maps.bump)
	{
		b = static_cast<uint8_t>(byte(random));
	}
	return maps;
}

/// Rounds of random corner edits; after each round the changed blocks' boxes are painted again into the image, which
/// must then be the whole island painted again
void ExpectRepaintMatchesFullBuild(uint8_t altitudeBits, uint16_t texelsPerBlock, uint32_t seed)
{
	std::mt19937 random(seed);
	TextureIsland island({2, 3}, {3, 2}, altitudeBits, random);
	const auto maps = RandomMaps(random);
	const block_texture::Materials materials {maps.materials, 4};
	const block_texture::Sources sources {
	    .countries = island.GetCountries(),
	    .materials = materials,
	    .noise = maps.noise,
	    .bump = maps.bump,
	    .altitudeBits = altitudeBits,
	};
	const auto extentMin = glm::u16vec2(island.First());
	const auto indexSize = glm::u16vec2(island.Size());
	const size_t width = static_cast<size_t>(indexSize.x) * texelsPerBlock;
	auto image =
	    block_texture::BuildIslandBlockTexture(island, extentMin, indexSize, texelsPerBlock, maps.noise, maps.bump, materials);

	const auto corners = island.Corners();
	std::uniform_int_distribution<int> cornerX(0, corners.x - 1);
	std::uniform_int_distribution<int> cornerZ(0, corners.y - 1);
	std::uniform_int_distribution<int> altitude(0, (1 << altitudeBits) - 1);
	std::uniform_int_distribution<int> edits(1, 12);
	// the island's four outer corners, a block border crossing and edge corners first, then random ones
	const std::vector<glm::ivec2> fixedCorners = {
	    {0, 0}, {corners.x - 1, corners.y - 1}, {0, corners.y - 1}, {corners.x - 1, 0}, {16, 16}, {32, 0}, {0, 16}, {47, 31},
	};
	for (int round = 0; round < 8; ++round)
	{
		const auto& blocks = island.GetBlocks();
		std::vector<block_texture::CellBox> boxes(blocks.size());
		std::vector<bool> changed(blocks.size(), false);
		if (round == 0)
		{
			for (const auto corner : fixedCorners)
			{
				island.EditCorner(corner, static_cast<uint16_t>(altitude(random)), boxes, changed);
			}
		}
		else
		{
			const int count = edits(random);
			for (int e = 0; e < count; ++e)
			{
				// either a lone corner or a small square of them, as the vortex's and the temple's flattening
				const glm::ivec2 at(cornerX(random), cornerZ(random));
				const int half = e % 3 == 0 ? 3 : 0;
				for (int x = at.x - half; x <= at.x + half; ++x)
				{
					for (int z = at.y - half; z <= at.y + half; ++z)
					{
						if (x >= 0 && z >= 0 && x < corners.x && z < corners.y)
						{
							island.EditCorner({x, z}, static_cast<uint16_t>(altitude(random)), boxes, changed);
						}
					}
				}
			}
		}
		for (size_t i = 0; i < blocks.size(); ++i)
		{
			if (!changed[i])
			{
				continue;
			}
			const auto patch = block_texture::PaintBox(std::span(blocks[i].GetCells(), static_cast<size_t>(k_Side) * k_Side),
			                                           sources, texelsPerBlock, boxes[i]);
			if (patch.texels.Empty())
			{
				continue;
			}
			const glm::ivec2 origin =
			    (blocks[i].GetBlockPosition() - glm::ivec2(extentMin)) * static_cast<int>(texelsPerBlock) + patch.texels.min;
			const auto patchWidth = static_cast<size_t>(patch.texels.max.x - patch.texels.min.x + 1);
			const auto patchRows = static_cast<size_t>(patch.texels.max.y - patch.texels.min.y + 1);
			ASSERT_EQ(patch.rgba.size(), patchWidth * patchRows * 4);
			for (size_t row = 0; row < patchRows; ++row)
			{
				std::copy_n(&patch.rgba[row * patchWidth * 4], patchWidth * 4,
				            &image[((static_cast<size_t>(origin.y) + row) * width + static_cast<size_t>(origin.x)) * 4]);
			}
		}
		const auto full = block_texture::BuildIslandBlockTexture(island, extentMin, indexSize, texelsPerBlock, maps.noise,
		                                                         maps.bump, materials);
		ASSERT_EQ(image.size(), full.size());
		size_t differing = 0;
		for (size_t t = 0; t < full.size(); ++t)
		{
			differing += image[t] != full[t] ? 1 : 0;
		}
		ASSERT_EQ(differing, 0u) << "round " << round << ", " << static_cast<int>(altitudeBits) << " altitude bits, "
		                         << texelsPerBlock << " texels per block";
	}
}
} // namespace

TEST(BlockTexture, TexelsOfABoxOfCells)
{
	const auto whole = block_texture::TexelsOf({}, 256);
	EXPECT_EQ(whole.min, glm::ivec2(0, 0));
	EXPECT_EQ(whole.max, glm::ivec2(255, 255));
	const auto one = block_texture::TexelsOf({{3, 15}, {3, 15}}, 256);
	EXPECT_EQ(one.min, glm::ivec2(48, 240));
	EXPECT_EQ(one.max, glm::ivec2(63, 255));
	// 64 texels a block: 4 a cell
	const auto quarter = block_texture::TexelsOf({{1, 2}, {5, 2}}, 64);
	EXPECT_EQ(quarter.min, glm::ivec2(4, 8));
	EXPECT_EQ(quarter.max, glm::ivec2(23, 11));
	// 8 texels a block: a cell with no texel of its own is an empty box
	EXPECT_TRUE(block_texture::TexelsOf({{1, 1}, {1, 1}}, 8).Empty());
	EXPECT_FALSE(block_texture::TexelsOf({{0, 0}, {1, 1}}, 8).Empty());
}

TEST(BlockTexture, CellsOfACorner)
{
	const auto inside = block_texture::CellsOfCorner({5, 9});
	EXPECT_EQ(inside.min, glm::ivec2(4, 8));
	EXPECT_EQ(inside.max, glm::ivec2(5, 9));
	// the block's first corner is only its first cell's; the shared last one only its last cell's
	const auto first = block_texture::CellsOfCorner({0, 16});
	EXPECT_EQ(first.min, glm::ivec2(0, 15));
	EXPECT_EQ(first.max, glm::ivec2(0, 15));
	const auto both = block_texture::Union(first, inside);
	EXPECT_EQ(both.min, glm::ivec2(0, 8));
	EXPECT_EQ(both.max, glm::ivec2(5, 15));
}

TEST(BlockTexture, RepaintedBoxesMatchTheWholeIsland)
{
	ExpectRepaintMatchesFullBuild(8, 256, 1u);
}

TEST(BlockTexture, RepaintedBoxesMatchWithWideAltitudes)
{
	ExpectRepaintMatchesFullBuild(10, 256, 2u);
}

TEST(BlockTexture, RepaintedBoxesMatchWithFewerTexels)
{
	ExpectRepaintMatchesFullBuild(8, 64, 3u);
	ExpectRepaintMatchesFullBuild(8, 40, 4u);
}
