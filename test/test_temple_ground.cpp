/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <bit>

#include <gtest/gtest.h>

#include "3D/TempleGround.h"

namespace temple_ground = openblack::temple_ground;
namespace map_coords = openblack::map_coords;

TEST(TempleGround, TheShareIsTheGameFloat)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(temple_ground::k_SharePerMetre), 0x3CEA0EA1u);
}

TEST(TempleGround, CornersInTheFlatRadiusKeepNoShare)
{
	EXPECT_EQ(temple_ground::OwnShare(0, 0), 0.0f);
	EXPECT_EQ(temple_ground::OwnShare(3, 0), 0.0f);
	EXPECT_EQ(temple_ground::OwnShare(-2, 2), 0.0f);
}

TEST(TempleGround, TheShareGrowsInAStraightLineToTheEdge)
{
	// 40 m: five metres past the flat radius
	EXPECT_EQ(std::bit_cast<uint32_t>(temple_ground::OwnShare(4, 0)), 0x3E124925u);
	// sqrt(4100) m
	EXPECT_EQ(std::bit_cast<uint32_t>(temple_ground::OwnShare(-5, -4)), 0x3F5457C2u);
	// 70 m is not past the edge, and 35 shares of 1 / 35 make 1
	EXPECT_EQ(temple_ground::OwnShare(7, 0), 1.0f);
	EXPECT_EQ(temple_ground::OwnShare(7, 1), 1.0f);
	EXPECT_EQ(temple_ground::OwnShare(8, 8), 1.0f);
}

TEST(TempleGround, TheAltitudeIsTruncated)
{
	// A low corner near the coast of Land 1 goes up from 4 to 10, not 11
	EXPECT_EQ(temple_ground::LevelledAltitude(4, temple_ground::OwnShare(-5, -4), 44), 10);
	EXPECT_EQ(temple_ground::LevelledAltitude(19, temple_ground::OwnShare(-6, -3), 44), 21);
	EXPECT_EQ(temple_ground::LevelledAltitude(100, 0.5f, 50), 75);
	EXPECT_EQ(temple_ground::LevelledAltitude(7, 0.0f, 44), 44);
	EXPECT_EQ(temple_ground::LevelledAltitude(7, 1.0f, 44), 7);
}

TEST(TempleGround, TheCentreCornerIsTheMetresTimesATenthTruncated)
{
	const auto place = map_coords::MapCoords {.x = map_coords::ToFixed(1915.05f), .z = map_coords::ToFixed(2508.89f)};
	EXPECT_EQ(temple_ground::CentreCorner(place), glm::ivec2(191, 250));
}

TEST(TempleGround, AllTheCornersRoundTheCentreChangeInOrder)
{
	const glm::ivec2 centre {100, 200};
	const auto corners = temple_ground::Levelled(centre, [](glm::ivec2 corner) -> uint8_t {
		return corner == glm::ivec2(100, 200) ? 60 : static_cast<uint8_t>(corner.x - 80);
	});
	ASSERT_EQ(corners.size(), 17u * 17u);
	EXPECT_EQ(corners.front().corner, glm::ivec2(92, 192));
	EXPECT_EQ(corners[1].corner, glm::ivec2(92, 193));
	EXPECT_EQ(corners[17].corner, glm::ivec2(93, 192));
	EXPECT_EQ(corners.back().corner, glm::ivec2(108, 208));
	// A corner in the flat radius takes the centre altitude; a far corner keeps its own
	EXPECT_EQ(corners[8 * 17 + 9].altitude, 60);
	EXPECT_EQ(corners.front().altitude, 12);
}

TEST(TempleGround, LevelLandCanLoseAUnitWhereTheFloatsRoundDown)
{
	// The game's float sum of the two shares of 255 is just below 255 at some corners, so these go down to 254
	for (const auto altitude : {uint8_t {44}, uint8_t {100}, uint8_t {200}, uint8_t {255}})
	{
		const auto corners = temple_ground::Levelled({50, 50}, [altitude](glm::ivec2) { return altitude; });
		const auto changed = std::ranges::count_if(corners, [altitude](const auto& c) { return c.altitude != altitude; });
		EXPECT_EQ(changed, altitude == 255 ? 8 : 0) << static_cast<int>(altitude);
	}
	const auto corners = temple_ground::Levelled({50, 50}, [](glm::ivec2) { return uint8_t {255}; });
	EXPECT_EQ(corners[(8 - 4) * 17 + (8 - 1)].altitude, 254);
}
