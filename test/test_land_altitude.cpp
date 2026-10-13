/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
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
	[[nodiscard]] const graphics::FrameBuffer& GetFootprintFramebuffer() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::FrameBuffer& GetLandAlphaFramebuffer() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const override { return {}; }
	[[nodiscard]] glm::mat4 GetOrthoView() const override { return glm::mat4(1.0f); }
	[[nodiscard]] glm::mat4 GetOrthoProj() const override { return glm::mat4(1.0f); }
	[[nodiscard]] Extent2 GetExtent() const override { return {}; }
	uint8_t GetNoise(glm::u8vec2) override { return 0; }

private:
	std::array<lnd::LNDCell, 17 * 17> _cells {};
};

/// The height in the game's integer steps of 1/256 of a height unit
double Steps(double steps)
{
	return steps * static_cast<double>(LandIslandInterface::k_HeightUnit) / 256.0;
}
} // namespace

// The expected values come from the game's own height lookup. We calculated each value by hand.

TEST(LandAltitude, ACornerIsItsAltitudeInHeightUnits)
{
	const OneCellIsland island(40, 48, 56, 72, false);
	EXPECT_DOUBLE_EQ(island.GetAltitude(0, 0), Steps(10240));
	EXPECT_NEAR(island.GetAltitude(0, 0), 26.8, 1e-5);
}

TEST(LandAltitude, ACellWithoutTheSplitFlagIsCutFromCornerToCorner)
{
	const OneCellIsland island(40, 48, 56, 72, false);
	// Further along x than z, and further along z than x: two different planes
	EXPECT_DOUBLE_EQ(island.GetAltitude(0xC000, 0x4000), Steps(14336));
	EXPECT_DOUBLE_EQ(island.GetAltitude(0x4000, 0xC000), Steps(13312));
}

TEST(LandAltitude, ACellWithTheSplitFlagIsCutAcrossTheOtherCorners)
{
	const OneCellIsland island(40, 48, 56, 72, true);
	EXPECT_DOUBLE_EQ(island.GetAltitude(0x4000, 0x4000), Steps(11776));
	EXPECT_DOUBLE_EQ(island.GetAltitude(0xC000, 0xC000), Steps(15872));
}

TEST(LandAltitude, LowLandAtTheShoreIsFlattenedToTheSea)
{
	// A cell no higher than 4 counts its corners no higher than 3 as the sea's height
	const OneCellIsland shore(3, 4, 3, 4, false);
	EXPECT_DOUBLE_EQ(shore.GetAltitude(0x8000, 0x8000), Steps(512));
	// This does not occur when the caller asks for the land's own altitudes
	EXPECT_DOUBLE_EQ(shore.GetAltitude(0x8000, 0x8000, false), Steps(896));

	const OneCellIsland lowCorner(4, 3, 3, 8, false);
	EXPECT_DOUBLE_EQ(lowCorner.GetAltitude(0x8000, 0x8000), Steps(1536));

	// A cell of 5 or more keeps all its corners
	const OneCellIsland higher(5, 3, 3, 3, false);
	EXPECT_DOUBLE_EQ(higher.GetAltitude(0x8000, 0x8000), Steps(1024));
}

TEST(LandAltitude, OffTheMapOrOffTheBlocksIsAtSeaLevel)
{
	const OneCellIsland island(40, 48, 56, 72, false);
	EXPECT_DOUBLE_EQ(island.GetAltitude(-1, 0), 0.0);
	EXPECT_DOUBLE_EQ(island.GetAltitude(512 * k_Fixed, 0), 0.0);
	EXPECT_DOUBLE_EQ(island.GetAltitude(20 * k_Fixed, 0), 0.0);
}
