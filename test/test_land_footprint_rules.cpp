/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <map>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Common/X87.h"
#include "Common/X87Angle.h"
#include "ECS/LandFootprintRules.h"

namespace land_footprints = openblack::ecs::land_footprints;
namespace x87 = openblack::x87;

namespace
{
/// The lists of a few cells, empty elsewhere
class FakeCells
{
public:
	void Put(glm::ivec2 cell, std::vector<entt::entity> things) { _lists[{cell.x, cell.y}] = std::move(things); }
	[[nodiscard]] land_footprints::CellList List() const
	{
		return [this](glm::ivec2 cell) {
			const auto found = _lists.find({cell.x, cell.y});
			return found != _lists.end() ? std::span<const entt::entity>(found->second) : std::span<const entt::entity> {};
		};
	}

private:
	std::map<std::pair<int, int>, std::vector<entt::entity>> _lists;
};

entt::entity Thing(uint32_t number)
{
	return static_cast<entt::entity>(number);
}

bool AllHaveFootprints(entt::entity /*unused*/)
{
	return true;
}
} // namespace

TEST(LandFootprintRules, TheBlockTakesThingsFromFourCellsBeyondEachSide)
{
	FakeCells fixed;
	FakeCells mobile;
	// Block (1, 1) covers cells 16 to 31; the cells 12 to 36 count
	fixed.Put({12, 36}, {Thing(1)});
	fixed.Put({11, 20}, {Thing(2)});
	fixed.Put({20, 37}, {Thing(3)});
	fixed.Put({36, 12}, {Thing(4)});
	const auto things = land_footprints::ThingsOverBlock({1, 1}, fixed.List(), mobile.List(), AllHaveFootprints);
	EXPECT_EQ(things, (std::vector {Thing(1), Thing(4)}));
}

TEST(LandFootprintRules, CellsAreTakenRowByRowAlongXThenAlongZ)
{
	FakeCells fixed;
	FakeCells mobile;
	fixed.Put({5, 3}, {Thing(1)});
	fixed.Put({4, 9}, {Thing(2)});
	fixed.Put({5, 0}, {Thing(3)});
	const auto things = land_footprints::ThingsOverBlock({0, 0}, fixed.List(), mobile.List(), AllHaveFootprints);
	EXPECT_EQ(things, (std::vector {Thing(2), Thing(3), Thing(1)}));
}

TEST(LandFootprintRules, InACellTheThingsThatStayPutComeFirstAndEachThingComesOnce)
{
	FakeCells fixed;
	FakeCells mobile;
	fixed.Put({2, 2}, {Thing(1), Thing(2)});
	mobile.Put({2, 2}, {Thing(3)});
	// A building in two cells is painted where it is found first
	fixed.Put({2, 3}, {Thing(2), Thing(4)});
	const auto things = land_footprints::ThingsOverBlock({0, 0}, fixed.List(), mobile.List(), AllHaveFootprints);
	EXPECT_EQ(things, (std::vector {Thing(1), Thing(2), Thing(3), Thing(4)}));
}

TEST(LandFootprintRules, ThingsWithoutFootprintsAreLeftOut)
{
	FakeCells fixed;
	FakeCells mobile;
	fixed.Put({2, 2}, {Thing(1), Thing(2), Thing(3)});
	const auto things = land_footprints::ThingsOverBlock({0, 0}, fixed.List(), mobile.List(),
	                                                     [](entt::entity entity) { return entity != Thing(2); });
	EXPECT_EQ(things, (std::vector {Thing(1), Thing(3)}));
}

TEST(LandFootprintRules, TheLastBlocksTakeNoCellsBeyondTheMap)
{
	FakeCells fixed;
	FakeCells mobile;
	fixed.Put({511, 511}, {Thing(1)});
	fixed.Put({507, 507}, {Thing(2)});
	const auto things = land_footprints::ThingsOverBlock({31, 31}, fixed.List(), mobile.List(), AllHaveFootprints);
	EXPECT_EQ(things, (std::vector {Thing(2), Thing(1)}));
}

TEST(LandFootprintRules, AThingsFootprintTakesTheRowsOfItsTurn)
{
	const glm::mat3 turn {glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(4.0f, 5.0f, 6.0f), glm::vec3(7.0f, 8.0f, 9.0f)};
	const auto placement = land_footprints::PlacementOf({10.0f, 20.0f, 30.0f}, turn);
	EXPECT_EQ(placement.xFromX, 1.0f);
	EXPECT_EQ(placement.zFromX, 3.0f);
	EXPECT_EQ(placement.xFromZ, 7.0f);
	EXPECT_EQ(placement.zFromZ, 9.0f);
	EXPECT_EQ(placement.origin, glm::vec2(10.0f, 30.0f));
}

TEST(LandFootprintRules, AScaledUprightTurnRoundsEachProductOneTime)
{
	EXPECT_EQ(x87::GameUprightTurnScaled(0.0f, 1.5f), glm::mat3(1.5f));
	const float angle = 0.7f;
	const auto turn = x87::GameUprightTurnScaled(angle, 1.3f);
	EXPECT_EQ(turn[0].x, x87::Mul(x87::Cos(angle), 1.3f));
	EXPECT_EQ(turn[0].z, x87::Mul(x87::Sin(angle), 1.3f));
	EXPECT_EQ(turn[2].x, -turn[0].z);
	EXPECT_EQ(turn[2].z, turn[0].x);
	EXPECT_EQ(turn[1].y, 1.3f);
	// With no scale it is the game's upright turn
	EXPECT_EQ(x87::GameUprightTurnScaled(angle, 1.0f), x87::GameUprightTurn(angle));
}

TEST(LandFootprintRules, ARiverStretchesItsMeshesToTheNextPoint)
{
	// 30 units along x: no turn, and the thirtieth the game keeps makes 30 units a stretch of 1
	const auto alongX = land_footprints::RiverStretch({100.0f, 0.0f, 200.0f}, {130.0f, 0.0f, 200.0f});
	EXPECT_EQ(alongX.xFromX, 1.0f);
	EXPECT_EQ(alongX.zFromX, 0.0f);
	EXPECT_EQ(alongX.xFromZ, 0.0f);
	EXPECT_EQ(alongX.zFromZ, 1.0f);
	EXPECT_EQ(alongX.origin, glm::vec2(100.0f, 200.0f));
	// 60 units along z, with a rise: the mesh's x points along z, stretched by the distance over 30
	const auto alongZ = land_footprints::RiverStretch({0.0f, 0.0f, 0.0f}, {0.0f, 8.0f, 60.0f});
	const float stretch = std::sqrt((60.0f * 60.0f + 8.0f * 8.0f) + 0.0f) * 0.0333333351f;
	EXPECT_EQ(alongZ.zFromX, stretch * x87::RoundToFloat(x87::Sin(x87::RoundToFloat(x87::Atan2(60.0, 0.0)))));
	EXPECT_EQ(alongZ.xFromZ, -1.0f);
	EXPECT_NEAR(alongZ.xFromX, 0.0f, 1e-6f);
}

TEST(LandFootprintRules, ARiverTurnedBackwardsTakesAnAngleFromZeroToTwoPi)
{
	// Along -z the angle is -pi/2, which the game takes to 3 pi/2 with its float two pi
	const auto placement = land_footprints::RiverStretch({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -30.0f});
	const float angle = x87::Add(x87::Atan2(-30.0, 0.0), 6.2831854820251465f);
	EXPECT_EQ(placement.zFromX, x87::GameUprightTurn(angle)[0].z * 1.0f);
	EXPECT_NEAR(placement.zFromX, -1.0f, 1e-6f);
}

TEST(LandFootprintRules, BoundsAreTheLowestAndHighestCorners)
{
	const std::vector<openblack::footprint_paint::Triangle> triangles {
	    {.corners = {glm::vec2(-1.0f, 2.0f), glm::vec2(3.0f, -4.0f), glm::vec2(0.5f, 0.5f)}, .texels = {}},
	};
	const auto [low, high] = land_footprints::GroundBounds(triangles, {.origin = {100.0f, 50.0f}});
	EXPECT_EQ(low, glm::vec2(99.0f, 46.0f));
	EXPECT_EQ(high, glm::vec2(103.0f, 52.0f));
}

TEST(LandFootprintRules, ARiverReachesTheBlocksItsBoundsTouch)
{
	const std::pair bounds {glm::vec2(150.0f, 10.0f), glm::vec2(170.0f, 20.0f)};
	EXPECT_TRUE(land_footprints::ReachesBlock(bounds, {0.0f, 0.0f}));
	EXPECT_TRUE(land_footprints::ReachesBlock(bounds, {160.0f, 0.0f}));
	EXPECT_FALSE(land_footprints::ReachesBlock(bounds, {320.0f, 0.0f}));
	EXPECT_FALSE(land_footprints::ReachesBlock(bounds, {0.0f, 160.0f}));
	// An edge that only touches counts
	EXPECT_TRUE(land_footprints::ReachesBlock({glm::vec2(320.0f, 0.0f), glm::vec2(330.0f, 5.0f)}, {160.0f, 0.0f}));
}

TEST(LandFootprintRules, RiversArePaintedFirstRiverFirstLastStretchFirst)
{
	const std::vector<size_t> points {3, 2, 1};
	const auto order = land_footprints::RiverPrintOrder(points);
	using Print = land_footprints::RiverPrint;
	const std::vector<Print> expected {
	    {.river = 0, .from = 1, .channel = false}, {.river = 0, .from = 1, .channel = true},
	    {.river = 0, .from = 0, .channel = false}, {.river = 0, .from = 0, .channel = true},
	    {.river = 1, .from = 0, .channel = false}, {.river = 1, .from = 0, .channel = true},
	};
	EXPECT_EQ(order, expected);
}
