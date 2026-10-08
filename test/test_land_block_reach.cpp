/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land blocks built again after altitude edits: building only the blocks whose meshes read a changed corner
// (LandBlock::ReadsCorners, as LandIsland::RebuildAltitudes does) gives the same vertices as building every block
// again, on a small synthetic island with random altitudes, countries, split cells and colours. The edits come in
// rounds: lone corners and squares of them, on block borders, in block corners and on the island's and the map's
// edges. Every block that reads a changed corner is also one of the blocks around a changed block, the ones whose
// physics bodies RebuildAltitudes takes out and puts back.

#include <cstdint>
#include <cstring>

#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LNDFile.h>
#include <glm/common.hpp>
#include <gtest/gtest.h>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Dynamics/LandBlockBulletMeshInterface.h"
#include "Graphics/Mesh.h"

using namespace openblack;

namespace
{
constexpr int k_Side = 17;
constexpr int k_CellsPerBlock = 16;

/// Blocks on a grid of blocksX x blocksZ from block (0, 0), on a map of cellsPerSide cells; every corner is kept in
/// each block that stores it, as in a land file
class MeshIsland final: public LandIslandInterface
{
public:
	MeshIsland(glm::ivec2 blocks, uint16_t cellsPerSide, std::mt19937& random)
	    : _blocks(blocks)
	    , _cellsPerSide(cellsPerSide)
	{
		std::uniform_int_distribution<int> altitude(0, 255);
		std::uniform_int_distribution<int> byte(0, 255);
		std::uniform_int_distribution<int> country(0, 1);
		std::uniform_int_distribution<int> coin(0, 1);
		for (int bx = 0; bx < blocks.x; ++bx)
		{
			for (int bz = 0; bz < blocks.y; ++bz)
			{
				lnd::LNDBlock block {};
				block.blockX = static_cast<uint32_t>(bx);
				block.blockZ = static_cast<uint32_t>(bz);
				_landBlocks.emplace_back().SetLndBlock(block);
			}
		}
		// each corner once, then copied into every block that stores it
		for (int x = 0; x <= blocks.x * k_CellsPerBlock; ++x)
		{
			for (int z = 0; z <= blocks.y * k_CellsPerBlock; ++z)
			{
				lnd::LNDCell cell {};
				cell.altitude = static_cast<uint8_t>(altitude(random));
				cell.properties.country = static_cast<uint8_t>(country(random));
				cell.properties.split = static_cast<uint8_t>(coin(random));
				cell.r = static_cast<uint8_t>(byte(random));
				cell.g = static_cast<uint8_t>(byte(random));
				cell.b = static_cast<uint8_t>(byte(random));
				cell.luminosity = static_cast<uint8_t>(byte(random));
				ForEachCopy({x, z}, [&cell](lnd::LNDCell& copy) { copy = cell; });
			}
		}
		_countries.resize(2);
		std::uniform_int_distribution<uint32_t> material(0, 7);
		for (auto& c : _countries)
		{
			for (auto& entry : c.materials)
			{
				entry.indices = {material(random), material(random)};
				entry.coefficient = static_cast<uint32_t>(byte(random));
			}
		}
		_noise.resize(256 * 256);
		for (auto& n : _noise)
		{
			n = static_cast<uint8_t>(byte(random));
		}
	}

	/// The corner's copies: its own block's and, on a border, the 17th row or column of the blocks before it
	template <typename F>
	void ForEachCopy(glm::ivec2 corner, F&& f)
	{
		for (int dx = 0; dx <= 1; ++dx)
		{
			for (int dz = 0; dz <= 1; ++dz)
			{
				if ((dx == 1 && corner.x % k_CellsPerBlock != 0) || (dz == 1 && corner.y % k_CellsPerBlock != 0))
				{
					continue;
				}
				const glm::ivec2 block = corner / k_CellsPerBlock - glm::ivec2(dx, dz);
				if (block.x < 0 || block.y < 0 || block.x >= _blocks.x || block.y >= _blocks.y)
				{
					continue;
				}
				const glm::ivec2 local = corner - block * k_CellsPerBlock;
				f(_landBlocks[static_cast<size_t>(block.x * _blocks.y + block.y)].GetLndBlock()->cells.at(
				    static_cast<size_t>(local.x) * k_Side + static_cast<size_t>(local.y)));
			}
		}
	}

	/// An altitude edit (LandIsland::SetCellAltitude): every copy, and the blocks that store the corner
	void SetAltitude(glm::ivec2 corner, uint8_t altitude, std::vector<bool>& changedBlocks)
	{
		ForEachCopy(corner, [altitude](lnd::LNDCell& copy) { copy.altitude = altitude; });
		for (int dx = 0; dx <= 1; ++dx)
		{
			for (int dz = 0; dz <= 1; ++dz)
			{
				const glm::ivec2 block = corner / k_CellsPerBlock - glm::ivec2(dx, dz);
				const bool stores =
				    (dx == 0 || corner.x % k_CellsPerBlock == 0) && (dz == 0 || corner.y % k_CellsPerBlock == 0);
				if (stores && block.x >= 0 && block.y >= 0 && block.x < _blocks.x && block.y < _blocks.y)
				{
					changedBlocks[static_cast<size_t>(block.x * _blocks.y + block.y)] = true;
				}
			}
		}
	}

	[[nodiscard]] std::vector<LandVertex> Vertices(size_t block)
	{
		std::vector<LandVertex> vertices(LandBlock::k_VertexCount, LandVertex({}, {}, {}, {}, 0, {}, 0.0f, {}));
		_landBlocks[block].BuildVertexList(vertices, *this);
		return vertices;
	}

	[[nodiscard]] glm::ivec2 Size() const { return _blocks; }

	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& coordinates) const final
	{
		static const lnd::LNDCell k_Empty {};
		const glm::ivec2 block = glm::ivec2(coordinates) / k_CellsPerBlock;
		if (block.x >= _blocks.x || block.y >= _blocks.y)
		{
			return k_Empty;
		}
		const glm::ivec2 local = glm::ivec2(coordinates) - block * k_CellsPerBlock;
		return _landBlocks[static_cast<size_t>(block.x * _blocks.y + block.y)].GetLndBlock()->cells.at(
		    static_cast<size_t>(local.x) * k_Side + static_cast<size_t>(local.y));
	}
	[[nodiscard]] uint16_t GetCellsPerSide() const final { return _cellsPerSide; }
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
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
	uint8_t GetNoise(glm::u8vec2 pos) final { return _noise.at(static_cast<size_t>(pos.x) * 256 + pos.y); }

private:
	glm::ivec2 _blocks;
	uint16_t _cellsPerSide;
	std::vector<LandBlock> _landBlocks;
	std::vector<lnd::LNDCountry> _countries;
	std::vector<uint8_t> _noise;
};

bool SameVertices(const std::vector<LandVertex>& a, const std::vector<LandVertex>& b)
{
	// every field one by one (no padding in the comparison)
	for (size_t i = 0; i < a.size(); ++i)
	{
		const auto& u = a[i];
		const auto& v = b[i];
		if (u.position != v.position || u.weight != v.weight || u.firstMaterialID != v.firstMaterialID ||
		    u.secondMaterialID != v.secondMaterialID || u.materialBlendCoefficient != v.materialBlendCoefficient ||
		    u.lightLevel != v.lightLevel || std::memcmp(&u.waterAlpha, &v.waterAlpha, sizeof(float)) != 0 ||
		    std::memcmp(&u.normal, &v.normal, sizeof(glm::vec3)) != 0)
		{
			return false;
		}
	}
	return a.size() == b.size();
}

void ExpectPartialRebuildMatchesFull(glm::ivec2 blocks, uint16_t cellsPerSide, uint32_t seed)
{
	std::mt19937 random(seed);
	MeshIsland island(blocks, cellsPerSide, random);
	const auto count = static_cast<size_t>(blocks.x * blocks.y);
	std::vector<std::vector<LandVertex>> kept(count);
	for (size_t i = 0; i < count; ++i)
	{
		kept[i] = island.Vertices(i);
	}
	const glm::ivec2 corners = blocks * k_CellsPerBlock + 1;
	std::uniform_int_distribution<int> cornerX(0, corners.x - 1);
	std::uniform_int_distribution<int> cornerZ(0, corners.y - 1);
	std::uniform_int_distribution<int> altitude(0, 255);
	std::uniform_int_distribution<int> edits(1, 6);
	const std::vector<glm::ivec2> fixedCorners = {
	    {0, 0}, {corners.x - 1, corners.y - 1}, {16, 16}, {15, 17}, {17, 0}, {0, 31}, {32, corners.y - 1}, {33, 15},
	};
	for (int round = 0; round < 12; ++round)
	{
		std::vector<bool> changedBlocks(count, false);
		std::optional<CornerBox> box;
		std::vector<glm::ivec2> edited;
		const auto edit = [&](glm::ivec2 corner) {
			island.SetAltitude(corner, static_cast<uint8_t>(altitude(random)), changedBlocks);
			edited.push_back(corner);
			box = box.has_value() ? CornerBox {.min = glm::min(box->min, corner), .max = glm::max(box->max, corner)}
			                      : CornerBox {.min = corner, .max = corner};
		};
		if (round == 0)
		{
			for (const auto corner : fixedCorners)
			{
				edit(corner);
			}
		}
		else
		{
			const int n = edits(random);
			for (int e = 0; e < n; ++e)
			{
				// a lone corner or a square of 11 x 11 corners, as the vortex's and the temple's flattening
				const glm::ivec2 at(cornerX(random), cornerZ(random));
				const int half = e % 2 == 0 ? 5 : 0;
				for (int x = at.x - half; x <= at.x + half; ++x)
				{
					for (int z = at.y - half; z <= at.y + half; ++z)
					{
						if (x >= 0 && z >= 0 && x < corners.x && z < corners.y)
						{
							edit({x, z});
						}
					}
				}
			}
		}
		ASSERT_TRUE(box.has_value());
		for (size_t i = 0; i < count; ++i)
		{
			const glm::ivec2 position(static_cast<int>(i) / blocks.y, static_cast<int>(i) % blocks.y);
			// the blocks around a changed block, as RebuildAltitudes walks them
			bool near = false;
			for (size_t j = 0; j < count; ++j)
			{
				const glm::ivec2 other(static_cast<int>(j) / blocks.y, static_cast<int>(j) % blocks.y);
				const auto delta = glm::abs(other - position);
				near = near || (changedBlocks[j] && delta.x <= 1 && delta.y <= 1);
			}
			// a block that reads one of the edited corners is one whose body RebuildAltitudes takes out and puts back
			for (const auto corner : edited)
			{
				ASSERT_TRUE(!LandBlock::ReadsCorners(position, {corner, corner}) || near)
				    << "round " << round << ": block " << position.x << ", " << position.y << " reads corner " << corner.x
				    << ", " << corner.y << " but is not around a changed block";
			}
			// the partial rebuild: only the blocks around a changed one that read the box of the changed corners
			if (near && LandBlock::ReadsCorners(position, *box))
			{
				kept[i] = island.Vertices(i);
			}
			// the full rebuild builds every block again
			ASSERT_TRUE(SameVertices(kept[i], island.Vertices(i)))
			    << "round " << round << ": block " << position.x << ", " << position.y;
		}
	}
}
} // namespace

TEST(LandBlockReach, TheCornersABlockReads)
{
	// block (1, 2): its corners 16..32 x 32..48 and one more on each side for the normals
	EXPECT_TRUE(LandBlock::ReadsCorners({1, 2}, {{15, 31}, {15, 31}}));
	EXPECT_TRUE(LandBlock::ReadsCorners({1, 2}, {{33, 49}, {40, 60}}));
	EXPECT_FALSE(LandBlock::ReadsCorners({1, 2}, {{14, 31}, {14, 40}}));
	EXPECT_FALSE(LandBlock::ReadsCorners({1, 2}, {{34, 32}, {40, 48}}));
	EXPECT_FALSE(LandBlock::ReadsCorners({1, 2}, {{20, 50}, {30, 60}}));
	EXPECT_TRUE(LandBlock::ReadsCorners({0, 0}, {{0, 0}, {0, 0}}));
}

TEST(LandBlockReach, PartialRebuildMatchesTheFullOne)
{
	ExpectPartialRebuildMatchesFull({4, 3}, 512, 11u);
}

TEST(LandBlockReach, PartialRebuildMatchesAtTheMapEdge)
{
	// the map ends with the island: the normals clamp at its last cell
	ExpectPartialRebuildMatchesFull({3, 3}, 48, 12u);
}
