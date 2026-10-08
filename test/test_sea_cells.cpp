/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The sea cell predicates (ECS/SeaCells: IsWater, IsLand, IsDryLand, IsCoastal, CollideLandscape, GetSurfaceType) on
// a small hand-made island and, when the original data is there, on Land1.lnd (OPENBLACK_LAND1_LND, or the default
// install path below).

#include <cstdio>
#include <cstdlib>

#include <filesystem>
#include <stdexcept>
#include <vector>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "ECS/SeaCells.h"

using namespace openblack;
namespace sea = openblack::ecs::sea_cells;

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

lnd::LNDCell MakeCell(uint8_t altitude, bool water, bool coast)
{
	lnd::LNDCell cell {};
	cell.altitude = altitude;
	cell.properties.hasWater = water ? 1 : 0;
	cell.properties.coastLine = coast ? 1 : 0;
	return cell;
}

std::filesystem::path Land1Path()
{
	if (const char* env = std::getenv("OPENBLACK_LAND1_LND"))
	{
		return env;
	}
	// the game folder every data-backed test reads; without it the test that needs Land1 skips
	if (const char* game = std::getenv("OPENBLACK_GAME_PATH"); game != nullptr && *game != '\0')
	{
		return std::filesystem::path(game) / "Data" / "Landscape" / "Land1.lnd";
	}
	return {};
}
} // namespace

TEST(SeaCells, CellOfLikeMapCoords)
{
	// trunc(x * 6553.6) >> 16: rounded down, negative -> the unsigned high word (off the map)
	EXPECT_EQ(sea::CellOf(glm::vec3(1464.0f, 0.0f, 2016.0f)), glm::ivec2(146, 201));
	EXPECT_EQ(sea::CellOf(glm::vec3(1469.9f, 0.0f, 2019.9f)), glm::ivec2(146, 201));
	EXPECT_EQ(sea::CellOf(glm::vec3(0.05f, 0.0f, 9.99f)), glm::ivec2(0, 0));
	EXPECT_GE(sea::CellOf(glm::vec3(-0.05f, 0.0f, 0.0f)).x, 512);
	// rounded to the nearest
	EXPECT_EQ(sea::RoundedCellOf(glm::vec3(1466.0f, 0.0f, 2014.0f)), glm::ivec2(147, 201));
	EXPECT_EQ(sea::RoundedCellOf(glm::vec3(-4.0f, 0.0f, 0.0f)), glm::ivec2(0, 0));
	EXPECT_EQ(sea::RoundedCellOf(glm::vec3(-6.0f, 0.0f, 0.0f)), glm::ivec2(-1, 0));
}

TEST(SeaCells, PredicatesOnHandMadeIsland)
{
	CellIsland island(2); // 32 x 32 cells
	island.SetBlock({0, 0}, true);
	island.SetBlock({1, 0}, true);
	island.SetBlock({0, 1}, true);
	// block (1, 1) missing
	island.Cell({1, 1}) = MakeCell(0, true, false);   // open sea
	island.Cell({2, 1}) = MakeCell(3, true, false);   // shallow water, altitude 3
	island.Cell({3, 1}) = MakeCell(5, true, false);   // a raised lake: water but dry land by altitude
	island.Cell({4, 1}) = MakeCell(2, false, true);   // beach without the water bit, on the coast line
	island.Cell({5, 1}) = MakeCell(40, false, false); // inland
	island.Cell({6, 1}) = MakeCell(9, true, true);    // water and coast: not coastal

	struct Expect
	{
		glm::ivec2 cell;
		bool water, land, dry, coastal;
		uint32_t collide;
	};
	const Expect expects[] = {
	    {{1, 1}, true, false, false, false, sea::k_CollideWater},   {{2, 1}, true, false, false, false, sea::k_CollideWater},
	    {{3, 1}, true, false, true, false, sea::k_CollideWater},    {{4, 1}, false, true, false, true, sea::k_CollideLand},
	    {{5, 1}, false, true, true, false, sea::k_CollideLand},     {{6, 1}, true, false, true, false, sea::k_CollideWater},
	    {{20, 20}, true, false, false, false, sea::k_CollideWater}, // no block: water, not an edge
	    {{-1, 0}, true, false, false, false, sea::k_CollideEdge},   {{0, 32}, true, false, false, false, sea::k_CollideEdge},
	};
	for (const auto& e : expects)
	{
		SCOPED_TRACE(testing::Message() << "cell " << e.cell.x << "," << e.cell.y);
		EXPECT_EQ(sea::IsWater(island, e.cell), e.water);
		EXPECT_EQ(sea::IsLand(island, e.cell), e.land);
		EXPECT_EQ(sea::IsDryLand(island, e.cell), e.dry);
		EXPECT_EQ(sea::IsCoastal(island, e.cell), e.coastal);
		EXPECT_EQ(sea::CollideLandscape(island, e.cell), e.collide);
	}
	// GET_LAND_HEIGHT: -10 at altitude 0, no block or off 0..511, else the land height
	EXPECT_EQ(sea::ScriptLandHeight(island, glm::vec3(15.0f, 0.0f, 15.0f)), -10.0f); // open sea, altitude 0
	island.Cell({7, 1}) = MakeCell(0, false, false);                                 // land bit but altitude 0
	EXPECT_EQ(sea::ScriptLandHeight(island, glm::vec3(75.0f, 0.0f, 15.0f)), -10.0f);
	EXPECT_EQ(sea::ScriptLandHeight(island, glm::vec3(25.0f, 0.0f, 15.0f)), CellIsland::k_HeightMarker); // altitude 3
	EXPECT_EQ(sea::ScriptLandHeight(island, glm::vec3(205.0f, 0.0f, 205.0f)), -10.0f);                   // no block
	// (int)(x * 0.1) truncates towards 0: x = -5 is cell 0 (altitude 0 here -> -10), x = -15 is off the map
	island.Cell({0, 1}) = MakeCell(6, false, false);
	EXPECT_EQ(sea::ScriptLandHeight(island, glm::vec3(-5.0f, 0.0f, 15.0f)), CellIsland::k_HeightMarker);
	EXPECT_EQ(sea::ScriptLandHeight(island, glm::vec3(-15.0f, 0.0f, 15.0f)), -10.0f);
	// GetSurfaceType: 6 no cell, 7 water, else the material's (3 without countries)
	EXPECT_EQ(sea::GetSurfaceType(island, glm::vec3(205.0f, 0.0f, 205.0f)), 6);
	EXPECT_EQ(sea::GetSurfaceType(island, glm::vec3(-1.0f, 0.0f, 5.0f)), 6);
	EXPECT_EQ(sea::GetSurfaceType(island, glm::vec3(15.0f, 0.0f, 15.0f)), 7);
	EXPECT_EQ(sea::GetSurfaceType(island, glm::vec3(55.0f, 0.0f, 15.0f)), 3);
}

/// The whole-map checks of Land1TestPoints on the hand-made island: every cell, with or without a block, and the
/// cells just off the map, is exactly one of land or water, and a coastal cell is always land
TEST(SeaCells, Land1TestPointsSynthetic)
{
	CellIsland island(2); // 32 x 32 cells
	island.SetBlock({0, 0}, true);
	island.SetBlock({1, 0}, true);
	island.SetBlock({0, 1}, true);
	// block (1, 1) missing
	island.Cell({1, 1}) = MakeCell(0, true, false);   // open sea
	island.Cell({2, 1}) = MakeCell(1, true, false);   // shore water at altitude 1
	island.Cell({3, 1}) = MakeCell(2, false, true);   // beach on the coast line
	island.Cell({4, 1}) = MakeCell(44, false, false); // inland
	island.Cell({5, 1}) = MakeCell(9, true, true);    // water on the coast line

	// the shore: water, not dry land
	EXPECT_EQ(sea::AltitudeAt(island, {2, 1}), 1);
	EXPECT_TRUE(sea::IsWater(island, {2, 1}));
	EXPECT_FALSE(sea::IsDryLand(island, {2, 1}));
	// inland: land, dry, and it collides as land
	EXPECT_EQ(sea::AltitudeAt(island, {4, 1}), 44);
	EXPECT_TRUE(sea::IsLand(island, {4, 1}));
	EXPECT_TRUE(sea::IsDryLand(island, {4, 1}));
	EXPECT_EQ(sea::CollideLandscape(island, {4, 1}), sea::k_CollideLand);
	// just past the last cell: water, not dry, the map's edge
	const glm::ivec2 off(32, 5);
	EXPECT_TRUE(sea::IsWater(island, off));
	EXPECT_FALSE(sea::IsDryLand(island, off));
	EXPECT_EQ(sea::CollideLandscape(island, off), sea::k_CollideEdge);

	int coastal = 0;
	int noBlock = 0;
	for (int x = -1; x <= 32; ++x)
	{
		for (int z = -1; z <= 32; ++z)
		{
			const glm::ivec2 c(x, z);
			SCOPED_TRACE(testing::Message() << "cell " << x << "," << z);
			ASSERT_NE(sea::IsLand(island, c), sea::IsWater(island, c));
			if (sea::IsCoastal(island, c))
			{
				ASSERT_TRUE(sea::IsLand(island, c));
				++coastal;
			}
			if (sea::CellAt(island, c) == nullptr)
			{
				++noBlock;
			}
		}
	}
	// only the beach is coastal: the water on the coast line is not
	EXPECT_EQ(coastal, 1);
	// the 16 x 16 cells of the missing block and the ring of 4 x 33 cells around the map
	EXPECT_EQ(noBlock, 16 * 16 + 4 * 33);
}

// Integration test: needs the original game data (OPENBLACK_LAND1_LND); skipped without it
TEST(SeaCells, Land1TestPoints)
{
	const auto path = Land1Path();
	if (!std::filesystem::exists(path))
	{
		GTEST_SKIP() << "no Land1.lnd at " << path.string();
	}
	lnd::LNDFile file;
	ASSERT_EQ(file.Open(path), lnd::LNDResult::Success);
	CellIsland island(32);
	for (const auto& block : file.GetBlocks())
	{
		if (block.blockX >= 32 || block.blockZ >= 32)
		{
			continue;
		}
		island.SetBlock(glm::u16vec2(block.blockX, block.blockZ), true);
		for (uint16_t x = 0; x < 16; ++x)
		{
			for (uint16_t z = 0; z < 16; ++z)
			{
				island.Cell(glm::u16vec2(block.blockX * 16 + x, block.blockZ * 16 + z)) = block.cells[x * 17u + z];
			}
		}
	}
	// open sea (146, 201) altitude 0 with water; shore (148, 201) altitude 1 with water; dry land (178, 271)
	// altitude 44
	const auto open = sea::CellOf(glm::vec3(1464.0f, 0.0f, 2016.0f));
	EXPECT_EQ(sea::AltitudeAt(island, open), 0);
	EXPECT_TRUE(sea::IsWater(island, open));
	EXPECT_FALSE(sea::IsLand(island, open));
	EXPECT_FALSE(sea::IsDryLand(island, open));
	EXPECT_EQ(sea::CollideLandscape(island, open), sea::k_CollideWater);
	EXPECT_EQ(sea::GetSurfaceType(island, glm::vec3(1464.0f, 0.0f, 2016.0f)), 7);

	const auto shore = sea::CellOf(glm::vec3(1485.0f, 0.0f, 2015.0f));
	EXPECT_EQ(sea::AltitudeAt(island, shore), 1);
	EXPECT_TRUE(sea::IsWater(island, shore));
	EXPECT_FALSE(sea::IsDryLand(island, shore));

	const auto dry = sea::CellOf(glm::vec3(1788.4f, 0.0f, 2710.0f));
	EXPECT_EQ(sea::AltitudeAt(island, dry), 44);
	// GET_LAND_HEIGHT: -10 on the open sea and the shore cell is land height (altitude 1); the dry point asks for it too
	// (28.917 in the game, checked with OPENBLACK_TEST_SEA)
	EXPECT_EQ(sea::ScriptLandHeight(island, glm::vec3(1464.0f, 0.0f, 2016.0f)), -10.0f);
	EXPECT_EQ(sea::ScriptLandHeight(island, glm::vec3(1485.0f, 0.0f, 2015.0f)), CellIsland::k_HeightMarker);
	EXPECT_EQ(sea::ScriptLandHeight(island, glm::vec3(1788.4f, 0.0f, 2710.0f)), CellIsland::k_HeightMarker);
	EXPECT_TRUE(sea::IsLand(island, dry));
	EXPECT_TRUE(sea::IsDryLand(island, dry));
	EXPECT_EQ(sea::CollideLandscape(island, dry), sea::k_CollideLand);

	const glm::ivec2 off(512, 100);
	EXPECT_TRUE(sea::IsWater(island, off));
	EXPECT_FALSE(sea::IsDryLand(island, off));
	EXPECT_EQ(sea::CollideLandscape(island, off), sea::k_CollideEdge);

	// Whole map: IsLand is !IsWater, IsCoastal only on land; count the kinds of low cells for the log
	int lowLand = 0;
	int highWater = 0;
	int coastal = 0;
	int noBlock = 0;
	glm::ivec2 firstAltitude3(-1);      // the "shore" branch of the water hit (ring + brown dust, ground sound)
	glm::ivec2 firstShallowLanding(-1); // water and altitude 2-3: a gentle release "lands" there and the villager drowns
	for (int x = 0; x < 512; ++x)
	{
		for (int z = 0; z < 512; ++z)
		{
			const glm::ivec2 c(x, z);
			ASSERT_NE(sea::IsLand(island, c), sea::IsWater(island, c));
			if (sea::IsCoastal(island, c))
			{
				ASSERT_TRUE(sea::IsLand(island, c));
				++coastal;
			}
			if (sea::CellAt(island, c) == nullptr)
			{
				++noBlock;
				continue;
			}
			const auto altitude = sea::AltitudeAt(island, c);
			if (altitude == 3 && firstAltitude3.x < 0)
			{
				firstAltitude3 = c;
			}
			if (sea::IsWater(island, c) && altitude > 1 && firstShallowLanding.x < 0)
			{
				firstShallowLanding = c;
			}
			lowLand += sea::IsLand(island, c) && altitude < 4 ? 1 : 0;
			highWater += sea::IsWater(island, c) && altitude >= 4 ? 1 : 0;
		}
	}
	std::printf("Land1: %d land cells under altitude 4, %d water cells at altitude 4 or more, %d coastal, %d without a block; "
	            "first altitude 3 cell (%d, %d), water %d; first water cell over altitude 1 (%d, %d)\n",
	            lowLand, highWater, coastal, noBlock, firstAltitude3.x, firstAltitude3.y,
	            firstAltitude3.x >= 0 ? static_cast<int>(sea::IsWater(island, firstAltitude3)) : -1, firstShallowLanding.x,
	            firstShallowLanding.y);
}
