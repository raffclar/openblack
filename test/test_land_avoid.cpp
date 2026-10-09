/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "3D/LandAvoid.h"

using namespace openblack;
using namespace openblack::land_avoid;

namespace
{
constexpr int32_t k_Side = 8;
} // namespace

TEST(LandAvoid, AnIslandIsReachedAndTheSeaAroundItAvoided)
{
	// An island of corners 2..5 at altitude 10 in a sea of altitude 0
	const auto altitude = [](glm::ivec2 corner) -> uint8_t {
		return corner.x >= 2 && corner.x <= 5 && corner.y >= 2 && corner.y <= 5 ? 10 : 0;
	};
	const auto map = Build(k_Side, altitude, [](glm::ivec2) { return false; });
	// Cells with any corner on the island are walkable (a rise of 6.7 is not too steep) and reached
	EXPECT_EQ(map.At({1, 1}), k_Land);
	EXPECT_EQ(map.At({3, 3}), k_Land);
	EXPECT_EQ(map.At({5, 5}), k_Land);
	// Cells with all four corners at sea level next to it are avoided, those further away out of reach
	EXPECT_EQ(map.At({0, 3}), k_Avoid);
	EXPECT_EQ(map.At({6, 3}), k_Avoid);
	EXPECT_EQ(map.At({7, 7}), k_Unreachable);
	EXPECT_EQ(map.At({0, 0}), k_Unreachable);
	// Off the map is out of reach
	EXPECT_EQ(map.At({-1, 0}), k_Unreachable);
	EXPECT_EQ(map.At({0, k_Side}), k_Unreachable);
}

TEST(LandAvoid, SteepCellsBlock)
{
	// Flat land at 20 with a cliff: corners from x = 4 on are at 40, a rise of 13.4 across cell 3. Off the map the
	// corners are at 0, so the last column and row are too steep as well.
	const auto altitude = [](glm::ivec2 corner) -> uint8_t { return corner.x >= 4 ? 40 : 20; };
	const auto map = Build(k_Side, altitude, [](glm::ivec2) { return false; });
	EXPECT_EQ(map.At({3, 0}), k_Avoid);
	EXPECT_EQ(map.At({3, 5}), k_Avoid);
	// The walk starts from the end of the last run a blocked cell ends: the east side, before the last column, in the
	// last row that has land
	EXPECT_EQ(map.Seed(), glm::ivec2(k_Side - 2, k_Side - 2));
	EXPECT_EQ(map.At({5, 4}), k_Land);
	EXPECT_EQ(map.At({4, 0}), k_Land);
	EXPECT_EQ(map.At({k_Side - 1, 3}), k_Avoid);
	// The west side is flat but not reached from the east
	EXPECT_EQ(map.At({0, 0}), k_Unreachable);
	EXPECT_EQ(map.At({2, 4}), k_Unreachable);
}

TEST(LandAvoid, ReachedWaterIsWater)
{
	const auto altitude = [](glm::ivec2) -> uint8_t { return 20; };
	const auto water = [](glm::ivec2 cell) { return cell.x == 1 && cell.y == 1; };
	const auto map = Build(k_Side, altitude, water);
	EXPECT_EQ(map.Seed(), glm::ivec2(k_Side - 2, k_Side - 2));
	EXPECT_EQ(map.At({1, 1}), k_Water);
	EXPECT_EQ(map.At({2, 1}), k_Land);
}
