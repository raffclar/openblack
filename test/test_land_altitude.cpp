/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <stdexcept>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"

using namespace openblack;

namespace
{
constexpr int32_t k_Fixed = 0x10000;

/// One block of cells at the corner of the map. Only the first cell has heights.
class OneCellIsland final: public LandIslandInterface
{
public:
	/// The heights at the four corners of the first cell: x and z, x and z + 1, x + 1 and z, x + 1 and z + 1
	OneCellIsland(uint8_t a00, uint8_t a01, uint8_t a10, uint8_t a11, bool split)
	{
		_cells.at(0).altitude = a00;
		_cells.at(1).altitude = a01;
		_cells.at(17).altitude = a10;
		_cells.at(18).altitude = a11;
		_cells.at(0).properties.split = split ? 1 : 0;
	}

	[[nodiscard]] float GetHeightAt(glm::vec2) const override { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const override { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& coordinates) const override { return *FindCell(coordinates); }
	[[nodiscard]] const lnd::LNDCell* FindCell(const glm::u16vec2& coordinates) const override
	{
		if (coordinates.x >= 16 || coordinates.y >= 16)
		{
			return nullptr;
		}
		return &_cells.at((coordinates.x * 17u) + coordinates.y);
	}
	void DumpTextures() const override {}
	void DumpMaps() const override {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() override { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const override { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const override { throw std::logic_error("no countries"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::Texture2D& GetLuminosityMap() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::Texture2D& GetCellColourMap() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::Texture2D& GetBlockTextures() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const override { return {}; }
	[[nodiscard]] glm::mat4 GetOrthoView() const override { return glm::mat4(1.0f); }
	[[nodiscard]] glm::mat4 GetOrthoProj() const override { return glm::mat4(1.0f); }
	[[nodiscard]] Extent2 GetExtent() const override { return {}; }
	uint8_t GetNoise(glm::u8vec2) override { return 0; }

	/// Gives the island a height field with these heights in units at the four corners of the first cell
	void SetFieldCorners(uint16_t u00, uint16_t u01, uint16_t u10, uint16_t u11)
	{
		std::vector<uint16_t> units(static_cast<size_t>(k_MapCornersPerSide) * k_MapCornersPerSide, 0);
		units.at(0) = u00;
		units.at(1) = u01;
		units.at(k_MapCornersPerSide) = u10;
		units.at(k_MapCornersPerSide + 1) = u11;
		SetHeightField(landgen::HeightCurve::WithTop(60.0f, 1100.0f), std::move(units));
	}
	void SetCorner(uint8_t altitude) { SetFieldAltitude(0, 0, altitude); }

private:
	std::array<lnd::LNDCell, 17 * 17> _cells {};
};

/// The height in the game's integer steps of 1/256 of a height unit: the steps times the height unit round to a
/// float, and the division by 256 is exact
float Steps(int32_t steps)
{
	return static_cast<float>(steps) * LandIslandInterface::k_HeightUnit / 256.0f;
}
} // namespace

// The expected values come from the game's own height lookup. We calculated each value by hand.

TEST(LandAltitude, ACornerIsItsAltitudeInHeightUnits)
{
	const OneCellIsland island(40, 48, 56, 72, false);
	EXPECT_EQ(island.GetAltitude(0, 0), Steps(10240));
	EXPECT_EQ(std::bit_cast<uint32_t>(island.GetAltitude(0, 0)), 0x41D66667u);
}

TEST(LandAltitude, ACellWithoutTheSplitFlagIsCutFromCornerToCorner)
{
	const OneCellIsland island(40, 48, 56, 72, false);
	// Further along x than z, and further along z than x: two different planes
	EXPECT_EQ(island.GetAltitude(0xC000, 0x4000), Steps(14336));
	EXPECT_EQ(island.GetAltitude(0x4000, 0xC000), Steps(13312));
}

TEST(LandAltitude, ACellWithTheSplitFlagIsCutAcrossTheOtherCorners)
{
	const OneCellIsland island(40, 48, 56, 72, true);
	EXPECT_EQ(island.GetAltitude(0x4000, 0x4000), Steps(11776));
	EXPECT_EQ(island.GetAltitude(0xC000, 0xC000), Steps(15872));
}

TEST(LandAltitude, LowLandAtTheShoreIsFlattenedToTheSea)
{
	// A cell no higher than 4 counts its corners no higher than 3 as the sea's height
	const OneCellIsland shore(3, 4, 3, 4, false);
	EXPECT_EQ(shore.GetAltitude(0x8000, 0x8000), Steps(512));
	// This does not occur when the caller asks for the land's own altitudes
	EXPECT_EQ(shore.GetAltitude(0x8000, 0x8000, false), Steps(896));

	const OneCellIsland lowCorner(4, 3, 3, 8, false);
	EXPECT_EQ(lowCorner.GetAltitude(0x8000, 0x8000), Steps(1536));

	// A cell of 5 or more keeps all its corners
	const OneCellIsland higher(5, 3, 3, 3, false);
	EXPECT_EQ(higher.GetAltitude(0x8000, 0x8000), Steps(1024));
}

TEST(LandAltitude, OffTheMapOrOffTheBlocksIsAtSeaLevel)
{
	const OneCellIsland island(40, 48, 56, 72, false);
	EXPECT_EQ(island.GetAltitude(-1, 0), 0.0f);
	EXPECT_EQ(island.GetAltitude(512 * k_Fixed, 0), 0.0f);
	EXPECT_EQ(island.GetAltitude(20 * k_Fixed, 0), 0.0f);
}

TEST(LandAltitude, AScriptGetsTheHeightOfTheLandInMetres)
{
	const OneCellIsland island(40, 48, 56, 72, false);
	// The script's point in metres is the map position that the land interpolates
	EXPECT_EQ(island.GetScriptLandHeight({7.5f, 99.0f, 2.5f}),
	          island.GetAltitude(LandIslandInterface::ToMapCoords(7.5f), LandIslandInterface::ToMapCoords(2.5f)));
	EXPECT_EQ(island.GetScriptLandHeight({0.0f, 0.0f, 0.0f}), Steps(10240));
}

TEST(LandAltitude, AScriptGetsMinusTenWhereThereIsNoLand)
{
	const OneCellIsland island(40, 48, 56, 72, false);
	// Off the map
	EXPECT_EQ(island.GetScriptLandHeight({-10.0f, 0.0f, 0.0f}), -10.0f);
	EXPECT_EQ(island.GetScriptLandHeight({5120.0f, 0.0f, 0.0f}), -10.0f);
	// A cell with no block
	EXPECT_EQ(island.GetScriptLandHeight({200.0f, 0.0f, 0.0f}), -10.0f);
	// A cell whose first corner is at altitude 0, although its other corners are high
	const OneCellIsland seaBed(0, 48, 56, 72, false);
	EXPECT_EQ(seaBed.GetScriptLandHeight({5.0f, 0.0f, 5.0f}), -10.0f);
	EXPECT_NE(seaBed.GetAltitude(LandIslandInterface::ToMapCoords(5.0f), LandIslandInterface::ToMapCoords(5.0f)), 0.0f);
}

TEST(LandAltitude, AScriptFindsTheCellWithATruncatedTenth)
{
	const OneCellIsland island(40, 48, 56, 72, false);
	// A small negative coordinate truncates to cell 0, so the script gets the height of the land, not minus ten. The
	// map position is off the map, thus the height is 0.
	EXPECT_EQ(island.GetScriptLandHeight({-5.0f, 0.0f, 0.0f}), 0.0f);
}

TEST(LandHeightField, TheFieldGivesTheHeightAboveTheAltitudeByte)
{
	OneCellIsland island(200, 200, 200, 200, false);
	EXPECT_FALSE(island.HasHeightField());
	EXPECT_EQ(island.GetAltitude(0, 0), Steps(200 * 256));
	EXPECT_EQ(island.HighestHeight(), 255.0f * LandIslandInterface::k_HeightUnit);
	island.SetFieldCorners(1500, 1500, 1500, 1500);
	EXPECT_TRUE(island.HasHeightField());
	EXPECT_EQ(island.CornerUnits({0, 0}), 1500);
	EXPECT_EQ(island.GetAltitude(0, 0), Steps(1500 * 256));
	EXPECT_EQ(island.GetAltitude(k_Fixed / 2, k_Fixed / 2), Steps(1500 * 256));
	EXPECT_NEAR(island.HighestHeight(), 1100.0f, 1.0f);
}

TEST(LandHeightField, TheFieldSlopesAcrossTheCellAsTheAltitudesDo)
{
	OneCellIsland island(0, 0, 0, 0, false);
	island.SetFieldCorners(1000, 1000, 2000, 2000);
	// Half way along x, the height is half way between the corners, in the same integer steps as the altitudes
	EXPECT_EQ(island.GetAltitude(k_Fixed / 2, 0), Steps(1500 * 256));
}

TEST(LandHeightField, AChangedAltitudeSetsTheFieldThroughTheCurve)
{
	OneCellIsland island(0, 0, 0, 0, false);
	island.SetFieldCorners(1000, 1000, 1000, 1000);
	island.SetCorner(40);
	EXPECT_EQ(island.CornerUnits({0, 0}), 40);
	island.SetCorner(255);
	EXPECT_EQ(island.CornerUnits({0, 0}), island.GetHeightCurve().WholeUnitsOf(255.0f));
}
