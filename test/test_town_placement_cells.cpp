/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The position's own cell test of the fixed placement: the town placement refuses water and
// off-map cells; IsSuitableForFixedObject (the scaffolds, GetNewPlannedBuilding) also refuses a land cell on
// the coast line. A fake island of cells only (test_sea_cells' CellIsland).

#include <cstdint>

#include <stdexcept>
#include <vector>

#include <LNDFile.h>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Town/TownPlacement.h"
#include "Locator.h"

using namespace openblack;
namespace town_placement = openblack::ecs::town_placement;
namespace map_coords = openblack::map_coords;

namespace
{
/// Just the cells: blocks of 16 x 16 cells on a grid of `blocksPerSide`, missing blocks allowed
class CellIsland final: public LandIslandInterface
{
public:
	explicit CellIsland(uint16_t blocksPerSide)
	    : _blocksPerSide(blocksPerSide)
	    , _cells(static_cast<size_t>(blocksPerSide) * 16 * blocksPerSide * 16)
	    , _hasBlock(static_cast<size_t>(blocksPerSide) * blocksPerSide, false)
	{
	}

	void SetBlock(glm::u16vec2 block, bool present)
	{
		_hasBlock[static_cast<size_t>(block.x) * _blocksPerSide + block.y] = present;
	}
	lnd::LNDCell& Cell(glm::u16vec2 cell) { return _cells[static_cast<size_t>(cell.x) * _blocksPerSide * 16 + cell.y]; }

	/// the interpolated height is not modelled: a marker value so that the tests see when it is asked for
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return k_HeightMarker; }
	static constexpr float k_HeightMarker = 123.0f;
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

class PlacementCellsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto& island = static_cast<CellIsland&>(Locator::terrainSystem::emplace<CellIsland>(static_cast<uint16_t>(2)));
		island.SetBlock({0, 0}, true);
		_island = &island;
	}
	void TearDown() override { Locator::terrainSystem::reset(); }

	void SetCell(glm::u16vec2 cell, bool water, bool coast)
	{
		auto& c = _island->Cell(cell);
		c.altitude = 10;
		c.properties.hasWater = water ? 1 : 0;
		c.properties.coastLine = coast ? 1 : 0;
	}
	/// the middle of a map cell (10 m)
	static map_coords::MapCoords At(glm::u16vec2 cell)
	{
		return map_coords::FromMetres(
		    glm::vec2(static_cast<float>(cell.x) * 10.0f + 5.0f, static_cast<float>(cell.y) * 10.0f + 5.0f));
	}

	CellIsland* _island {nullptr};
};
} // namespace

TEST_F(PlacementCellsTest, LandIsBuildableForBoth)
{
	SetCell({3, 4}, false, false);
	EXPECT_TRUE(town_placement::PositionCellIsBuildable(At({3, 4}), false));
	EXPECT_TRUE(town_placement::PositionCellIsBuildable(At({3, 4}), true));
}

TEST_F(PlacementCellsTest, WaterIsRefusedByBoth)
{
	SetCell({3, 4}, true, false);
	EXPECT_FALSE(town_placement::PositionCellIsBuildable(At({3, 4}), false));
	EXPECT_FALSE(town_placement::PositionCellIsBuildable(At({3, 4}), true));
}

TEST_F(PlacementCellsTest, CoastIsRefusedOnlyByTheScaffoldsTest)
{
	// the coast-line flag on a land cell: the town placement accepts it, the scaffolds' test does not
	SetCell({3, 4}, false, true);
	EXPECT_TRUE(town_placement::PositionCellIsBuildable(At({3, 4}), false));
	EXPECT_FALSE(town_placement::PositionCellIsBuildable(At({3, 4}), true));
}

TEST_F(PlacementCellsTest, NoBlockIsWater)
{
	// a cell without a landscape block counts as water (sea_cells::IsWater)
	EXPECT_FALSE(town_placement::PositionCellIsBuildable(At({20, 20}), false));
}
