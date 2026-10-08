/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// LandIslandInterface::RayCast (the ray cast, its walk over the cells and the cell test) on a small hand-made island:
// a ray against a ridge, over it, onto flat land, beyond its `to` point (the walk goes to the map's edge), the y = 0
// fallback near and far from the camera, and a diagonal walk.

#include <stdexcept>
#include <vector>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"

using namespace openblack;

namespace
{
/// Just the cells: blocks of 16 x 16 cells on a grid of `blocksPerSide`, missing blocks allowed (as test_sea_cells)
class RayIsland final: public LandIslandInterface
{
public:
	explicit RayIsland(uint16_t blocksPerSide)
	    : _blocksPerSide(blocksPerSide)
	    , _cells(static_cast<size_t>(blocksPerSide) * 16 * blocksPerSide * 16)
	    , _hasBlock(static_cast<size_t>(blocksPerSide) * blocksPerSide, true)
	{
	}

	void SetBlock(glm::u16vec2 block, bool present)
	{
		_hasBlock[static_cast<size_t>(block.x) * _blocksPerSide + block.y] = present;
	}
	void SetAltitude(glm::u16vec2 cell, uint8_t altitude)
	{
		_cells[static_cast<size_t>(cell.x) * _blocksPerSide * 16 + cell.y].altitude = altitude;
	}

	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& cell) const final
	{
		static const lnd::LNDCell k_Empty {};
		return HasBlockAt(cell) ? _cells[static_cast<size_t>(cell.x) * _blocksPerSide * 16 + cell.y] : k_Empty;
	}
	[[nodiscard]] bool HasBlockAt(const glm::u16vec2& cell) const final
	{
		const auto block = cell >> static_cast<uint16_t>(4);
		return block.x < _blocksPerSide && block.y < _blocksPerSide &&
		       _hasBlock[static_cast<size_t>(block.x) * _blocksPerSide + block.y];
	}
	[[nodiscard]] uint16_t GetCellsPerSide() const final { return static_cast<uint16_t>(_blocksPerSide * 16); }
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("no blocks"); }
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
	uint16_t _blocksPerSide;
	std::vector<lnd::LNDCell> _cells;
	std::vector<bool> _hasBlock;
	std::vector<lnd::LNDCountry> _countries;
};

/// 32 x 32 cells at altitude 0 with a ridge of altitude 100 (67 m) on the cell columns x = 10 .. 12
RayIsland RidgeIsland()
{
	RayIsland island(2);
	for (uint16_t x = 10; x <= 12; ++x)
	{
		for (uint16_t z = 0; z < 32; ++z)
		{
			island.SetAltitude({x, z}, 100);
		}
	}
	return island;
}

const glm::vec3 k_NearCamera(50.0f, 100.0f, 50.0f);
} // namespace

TEST(LandRayCast, HorizontalRayHitsTheRidgeSide)
{
	const auto island = RidgeIsland();
	// y = 30 m is 44.776 height units: the slope of cell 9 (corner x + 1 at 100) is met at u = 0.4478
	glm::vec2 hit(0.0f);
	ASSERT_TRUE(island.RayCast(glm::vec3(25.0f, 30.0f, 55.0f), glm::vec3(40.0f, 30.0f, 55.0f), hit, k_NearCamera));
	EXPECT_NEAR(hit.x, 90.0f + 10.0f * (30.0f / 0.67f) / 100.0f, 0.01f);
	EXPECT_NEAR(hit.y, 55.0f, 0.001f);
}

TEST(LandRayCast, TheRayGoesPastItsEndPoint)
{
	const auto island = RidgeIsland();
	// the walk moves the end to the map's edge: `to` one metre ahead still finds the ridge 65 m on
	glm::vec2 hit(0.0f);
	ASSERT_TRUE(island.RayCast(glm::vec3(25.0f, 30.0f, 55.0f), glm::vec3(26.0f, 30.0f, 55.0f), hit, k_NearCamera));
	EXPECT_NEAR(hit.x, 90.0f + 10.0f * (30.0f / 0.67f) / 100.0f, 0.01f);
}

TEST(LandRayCast, AboveTheRidgeAndLevelNoHit)
{
	const auto island = RidgeIsland();
	// 80 m = 119 units, over every corner; level, so no y = 0 point either (|dy| < 0.0001)
	glm::vec2 hit(-1.0f);
	EXPECT_FALSE(island.RayCast(glm::vec3(25.0f, 80.0f, 55.0f), glm::vec3(40.0f, 80.0f, 55.0f), hit, k_NearCamera));
	// going up: no fallback
	EXPECT_FALSE(island.RayCast(glm::vec3(25.0f, 80.0f, 55.0f), glm::vec3(40.0f, 90.0f, 55.0f), hit, k_NearCamera));
}

TEST(LandRayCast, DownOntoFlatLand)
{
	const auto island = RidgeIsland();
	// 20 m down to 10 m over 10 m of x: y = 0 at x = 70, before the ridge
	glm::vec2 hit(0.0f);
	ASSERT_TRUE(island.RayCast(glm::vec3(50.0f, 20.0f, 155.0f), glm::vec3(60.0f, 10.0f, 155.0f), hit, k_NearCamera));
	EXPECT_NEAR(hit.x, 70.0f, 0.01f);
	EXPECT_NEAR(hit.y, 155.0f, 0.001f);
}

TEST(LandRayCast, SeaFallbackNearTheCameraOnly)
{
	auto island = RidgeIsland();
	island.SetBlock({1, 1}, false); // cells 16 .. 31 x 16 .. 31: no land to hit
	const glm::vec3 from(200.0f, 20.0f, 255.0f);
	const glm::vec3 to(210.0f, 10.0f, 255.0f);
	glm::vec2 hit(0.0f);
	// no cell is hit: the ray's crossing of y = 0 (x = 220), counted within 7500 m of the camera
	ASSERT_TRUE(island.RayCast(from, to, hit, glm::vec3(220.0f, 50.0f, 255.0f)));
	EXPECT_NEAR(hit.x, 220.0f, 0.001f);
	EXPECT_NEAR(hit.y, 255.0f, 0.001f);
	// (dx^2 + dz^2) > 7500^2: written but not a hit
	hit = glm::vec2(0.0f);
	EXPECT_FALSE(island.RayCast(from, to, hit, glm::vec3(220.0f + 7600.0f, 50.0f, 255.0f)));
	EXPECT_NEAR(hit.x, 220.0f, 0.001f);
}

TEST(LandRayCast, DiagonalWalk)
{
	const auto island = RidgeIsland();
	// x = z, at 50 units: the ridge's slope (y = 100 u in the cells of column 9) at u = 0.5
	glm::vec2 hit(0.0f);
	ASSERT_TRUE(island.RayCastCells(2.5f, 2.5f, 50.0f, 3.5f, 3.5f, 50.0f, hit));
	EXPECT_NEAR(hit.x, 9.5f, 0.001f);
	EXPECT_NEAR(hit.y, 9.5f, 0.001f);
	// the other way, x down and z up, from the far side of the ridge: its slope down (cell 12, corner x + 1 at 0)
	ASSERT_TRUE(island.RayCastCells(20.5f, 2.5f, 50.0f, 19.5f, 3.5f, 50.0f, hit));
	EXPECT_NEAR(hit.x, 12.5f, 0.001f);
	EXPECT_NEAR(hit.y, 10.5f, 0.001f);
}

TEST(LandRayCast, OffTheMap)
{
	const auto island = RidgeIsland();
	glm::vec2 hit(0.0f);
	// heading away from the map, both ends (the moved one too) are under x = 0.1: Cohen-Sutherland rejects it
	EXPECT_FALSE(island.RayCastCells(-5.0f, 3.0f, 50.0f, -8.0f, 3.0f, 60.0f, hit));
	// heading into it from outside, the ray is clipped at x = 0.1 and crosses the whole map: 5 units up per cell
	// clears the ridge (65.5 units at x = 0.1, 115 at x = 10; on cell 9's slope u would be 1.16), so nothing is hit
	EXPECT_FALSE(island.RayCastCells(-5.0f, 3.0f, 40.0f, -3.0f, 3.0f, 50.0f, hit));
	// 2.5 units up per cell does not: cell 9's slope at u = 0.769 (75 + 2.5 u = 100 u)
	ASSERT_TRUE(island.RayCastCells(-5.0f, 3.0f, 40.0f, -3.0f, 3.0f, 45.0f, hit));
	EXPECT_NEAR(hit.x, 9.0f + 75.0f / 97.5f, 0.001f);
}
