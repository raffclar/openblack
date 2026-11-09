/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "3D/SnowCover.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/SnowSystem.h"

using namespace openblack;

namespace
{
std::vector<float> Grid(float depth = 0.0f)
{
	return std::vector<float>(snow_cover::k_Cells, depth);
}

float& Cell(std::vector<float>& grid, int x, int z)
{
	return grid.at((static_cast<size_t>(z) * snow_cover::k_GridSize) + static_cast<size_t>(x));
}

ecs::components::Storm SnowStorm(int8_t snow)
{
	ecs::components::Storm storm {};
	storm.effect.snow = snow;
	storm.currentStrength = 1.0f;
	storm.currentPosition = {400.0f, 300.0f, 400.0f};
	storm.currentInnerRadius = 80.0f;
	storm.outerRadius = 200.0f;
	return storm;
}
} // namespace

TEST(SnowCover, StormsSnowBySnowAndStrength)
{
	// Each storm lays its snow at 1.2 times its snow, cut to a whole number
	EXPECT_NEAR(snow_cover::StormSnowPerTurn(100, 1.0f), 0.36f, 1e-6f);
	EXPECT_NEAR(snow_cover::StormSnowPerTurn(100, 0.5f), 0.18f, 1e-6f);
	EXPECT_NEAR(snow_cover::StormSnowPerTurn(5, 0.5f), 0.009f, 1e-7f);
	EXPECT_FLOAT_EQ(snow_cover::StormSnowPerTurn(1, 0.5f), 0.0f);
	EXPECT_FLOAT_EQ(snow_cover::StormSnowPerTurn(0, 1.0f), 0.0f);
}

TEST(SnowCover, StormsSnowLessBeyondTheirInnerRadius)
{
	auto grid = Grid();
	// Centred on cell (10, 10), all of it within 2 cells, none from 5
	snow_cover::AddStorm(grid, {400.0f, 400.0f}, 80.0f, 200.0f, 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 10, 10), 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 12, 10), 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 13, 10), 12.0f / 21.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 14, 10), 5.0f / 21.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 15, 10), 0.0f);
}

TEST(SnowCover, StormsTakeSnowAwayNearTheirEdge)
{
	auto grid = Grid(1.0f);
	// 6 cells out, 2 in: 5 across and 3 down lies beyond what is left of the outer radius once the inner is taken off
	snow_cover::AddStorm(grid, {400.0f, 400.0f}, 80.0f, 240.0f, 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 15, 13), 1.0f - (2.0f / 32.0f));
	// And never below none
	auto bare = Grid();
	snow_cover::AddStorm(bare, {400.0f, 400.0f}, 80.0f, 240.0f, 1.0f);
	EXPECT_FLOAT_EQ(Cell(bare, 15, 13), 0.0f);
}

TEST(SnowCover, AStormWithNoEdgeTakesAnEdgeOfAHundredthOfACell)
{
	auto grid = Grid(255.0f);
	// The inner radius is 9.995 cells and the outer radius is not more, thus the outer radius becomes 10.005 cells. The
	// cell 10 across is in the edge, where the storm takes snow away.
	snow_cover::AddStorm(grid, {400.0f, 400.0f}, 399.8f, 399.8f, 0.1f);
	const float inner = 399.8f * 0.025f;
	const float outer = inner + 0.01f;
	const float laid = (((outer * outer) - 100.0f) - (inner * inner)) * (0.1f / ((outer * outer) - (inner * inner)));
	EXPECT_FLOAT_EQ(Cell(grid, 20, 10), 255.0f + laid);
	EXPECT_NEAR(Cell(grid, 20, 10), 205.1f, 0.5f);
	// A cell within the inner radius takes all of the snow
	EXPECT_FLOAT_EQ(Cell(grid, 19, 10), 255.0f);
}

TEST(SnowCover, SnowLiesNoDeeperThanItsMost)
{
	auto grid = Grid(254.5f);
	snow_cover::AddStorm(grid, {400.0f, 400.0f}, 80.0f, 200.0f, 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 10, 10), snow_cover::k_MaxDepth);
}

TEST(SnowCover, MeltsABandAtATime)
{
	auto grid = Grid(3.0f);
	snow_cover::Melting melting;
	snow_cover::Melt(grid, melting, 0.31f);
	// The second band of sixteen rows, then the third
	EXPECT_FLOAT_EQ(Cell(grid, 0, 15), 3.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 5, 16), 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 5, 31), 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 5, 32), 3.0f);
	snow_cover::Melt(grid, melting, 0.3f);
	EXPECT_FLOAT_EQ(Cell(grid, 5, 32), 1.0f);
	// Snow of 2 or less is gone
	melting.band = 0;
	melting.clock = 0.31f;
	snow_cover::Melt(grid, melting, 0.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 5, 16), 0.0f);
}

TEST(SnowCover, DepthBlendsBetweenCells)
{
	auto grid = Grid();
	Cell(grid, 1, 1) = 4.0f;
	Cell(grid, 2, 1) = 8.0f;
	EXPECT_FLOAT_EQ(snow_cover::DepthAt(grid, {60.0f, 40.0f}), 6.0f);
	EXPECT_FLOAT_EQ(snow_cover::DepthAt(grid, {-1.0f, 40.0f}), 0.0f);
	EXPECT_FLOAT_EQ(snow_cover::DepthAt(grid, {127.0f * 40.0f, 40.0f}), 0.0f);
}

TEST(SnowCover, ObjectsShowSnowBeyond20)
{
	EXPECT_EQ(snow_cover::ObjectLevel(10.0f, snow_cover::k_ObjectRate, 255), 0);
	EXPECT_EQ(snow_cover::ObjectLevel(120.0f, snow_cover::k_ObjectRate, 255), 99);
	// A field's crop, at a quarter
	EXPECT_EQ(snow_cover::ObjectLevel(120.0f, 64, 64), 25);
	EXPECT_EQ(snow_cover::ObjectLevel(255.0f, 64, 64), 58);
	// The more snow, the more of the texture shows
	EXPECT_EQ(snow_cover::ObjectThreshold(0), 250);
	EXPECT_EQ(snow_cover::ObjectThreshold(100), 150);
	EXPECT_EQ(snow_cover::ObjectThreshold(300), 0);
}

TEST(SnowCover, LandWhitensOverTheNoise)
{
	EXPECT_EQ(snow_cover::LandLevel(100.0f, 90), 1);
	EXPECT_EQ(snow_cover::LandLevel(90.0f, 100), 0);
	EXPECT_EQ(snow_cover::LandLevel(200.7f, 72), 16);
	EXPECT_EQ(snow_cover::LandLevel(255.0f, 0), 16);
	EXPECT_EQ(snow_cover::LandWhite(0), 14);
	EXPECT_EQ(snow_cover::LandWhite(2), 15);
	EXPECT_EQ(snow_cover::LandWhite(6), 14);
}

TEST(SnowCover, TexelDepthStepsInWholeParts)
{
	auto grid = Grid();
	Cell(grid, 2, 3) = 10.0f;
	Cell(grid, 3, 3) = 20.0f;
	Cell(grid, 2, 4) = 30.0f;
	Cell(grid, 3, 4) = 50.0f;
	EXPECT_EQ(snow_cover::TexelDepth(grid, {2, 3}, {0, 0}), 10 * 65536);
	EXPECT_EQ(snow_cover::TexelDepth(grid, {2, 3}, {32, 0}), 983040);
	EXPECT_EQ(snow_cover::TexelDepth(grid, {2, 3}, {0, 32}), 1310720);
	EXPECT_EQ(snow_cover::TexelDepth(grid, {2, 3}, {32, 32}), 1802240);
	// A step is the difference over 64, cut towards zero, so a small rise adds less than a blend
	Cell(grid, 6, 5) = 0.001f;
	EXPECT_EQ(snow_cover::TexelDepth(grid, {5, 5}, {63, 0}), 63);
	Cell(grid, 5, 5) = 0.001f;
	Cell(grid, 6, 5) = 0.0f;
	EXPECT_EQ(snow_cover::TexelDepth(grid, {5, 5}, {63, 0}), 2);
	// Corners off the grid have no snow
	Cell(grid, 127, 0) = 4.0f;
	EXPECT_EQ(snow_cover::TexelDepth(grid, {127, 0}, {32, 0}), 2 * 65536);
}

TEST(SnowCover, CellsTakeSnowOverTheLeast)
{
	auto grid = Grid(5.0f);
	EXPECT_FALSE(snow_cover::CellTakesSnow(grid, {10, 10}));
	Cell(grid, 11, 11) = 5.0001f;
	EXPECT_TRUE(snow_cover::CellTakesSnow(grid, {10, 10}));
	EXPECT_FALSE(snow_cover::CellTakesSnow(grid, {8, 8}));
}

TEST(SnowCover, TexelsTakeTheSnowInFourBits)
{
	const glm::u8vec3 colour {8, 4, 2};
	EXPECT_EQ(snow_cover::PaintTexel(colour, 0, 14), colour);
	EXPECT_EQ(snow_cover::PaintTexel(colour, 4, 14), glm::u8vec3(9, 6, 4));
	EXPECT_EQ(snow_cover::PaintTexel(colour, 15, 15), glm::u8vec3(14, 14, 14));
	EXPECT_EQ(snow_cover::PaintTexel(colour, 16, 14), glm::u8vec3(14, 14, 15));
}

TEST(SnowSystem, StormsThatSnowLayItAndItMelts)
{
	ecs::systems::SnowSystem snow;
	const auto revision = snow.GetRevision();
	const std::vector<ecs::components::Storm> storms {SnowStorm(100), SnowStorm(0)};
	snow.ProcessTurn(storms);
	EXPECT_NEAR(snow.GetDepth({400.0f, 400.0f}), 0.36f, 1e-6f);
	EXPECT_NE(snow.GetRevision(), revision);
	// A good while later, with no more snow, it has all melted
	for (int turn = 0; turn < 100; ++turn)
	{
		snow.ProcessTurn({});
	}
	EXPECT_FLOAT_EQ(snow.GetDepth({400.0f, 400.0f}), 0.0f);
}

TEST(SnowSystem, ResetClearsTheSnow)
{
	ecs::systems::SnowSystem snow;
	const std::vector<ecs::components::Storm> storms {SnowStorm(100)};
	snow.ProcessTurn(storms);
	snow.Reset();
	EXPECT_FLOAT_EQ(snow.GetDepth({400.0f, 400.0f}), 0.0f);
}

namespace
{
float Change(const snow_cover::BlockRepaint& repaint, int x, int z)
{
	return repaint.changes.at(snow_cover::BlockIndex({x, z}));
}
} // namespace

TEST(SnowRepaint, LaidSnowGoesToTheBlockAndToTheBlocksThatShareItsEdge)
{
	auto grid = Grid();
	snow_cover::BlockRepaint repaint;
	// One unit of snow on cell (4, 4) and its four neighbours, a corner of four blocks
	snow_cover::AddStorm(grid, repaint.changes, {160.0f, 160.0f}, 50.0f, 50.0f, 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 4, 4), 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 5, 5), 0.0f);
	EXPECT_FLOAT_EQ(Change(repaint, 1, 1), 3.0f);
	EXPECT_FLOAT_EQ(Change(repaint, 0, 1), 3.0f);
	EXPECT_FLOAT_EQ(Change(repaint, 0, 0), 3.0f);
	// The corner cell is not added to the block on its low z side
	EXPECT_FLOAT_EQ(Change(repaint, 1, 0), 2.0f);
	EXPECT_FLOAT_EQ(Change(repaint, 2, 1), 0.0f);
}

TEST(SnowRepaint, OnlyTheChangeThatTheDepthTakesIsAdded)
{
	auto grid = Grid(254.5f);
	snow_cover::BlockRepaint repaint;
	snow_cover::AddStorm(grid, repaint.changes, {80.0f, 80.0f}, 10.0f, 10.0f, 1.0f);
	EXPECT_FLOAT_EQ(Cell(grid, 2, 2), snow_cover::k_MaxDepth);
	EXPECT_FLOAT_EQ(Change(repaint, 0, 0), 0.5f);
}

TEST(SnowRepaint, BlocksThatChangedByMoreThan96AreListedLargestFirst)
{
	snow_cover::BlockRepaint repaint;
	repaint.changes.at(snow_cover::BlockIndex({0, 4})) = 96.0f;
	repaint.changes.at(snow_cover::BlockIndex({1, 4})) = -100.0f;
	repaint.changes.at(snow_cover::BlockIndex({2, 5})) = 200.0f;
	repaint.changes.at(snow_cover::BlockIndex({3, 7})) = 150.0f;
	repaint.changes.at(snow_cover::BlockIndex({3, 8})) = 500.0f;
	std::vector<uint16_t> paintNow;
	snow_cover::ListRepaints(repaint, 4, 4, paintNow);
	EXPECT_TRUE(paintNow.empty());
	ASSERT_EQ(repaint.count, 3);
	EXPECT_EQ(repaint.blocks[0], snow_cover::BlockIndex({2, 5}));
	EXPECT_EQ(repaint.blocks[1], snow_cover::BlockIndex({3, 7}));
	EXPECT_EQ(repaint.blocks[2], snow_cover::BlockIndex({1, 4}));
	EXPECT_FLOAT_EQ(repaint.sizes[2], 100.0f);
	EXPECT_FLOAT_EQ(repaint.sizes[3], 0.0f);
	// A listed block starts again from no change; the others keep theirs
	EXPECT_FLOAT_EQ(Change(repaint, 1, 4), 0.0f);
	EXPECT_FLOAT_EQ(Change(repaint, 0, 4), 96.0f);
	EXPECT_FLOAT_EQ(Change(repaint, 3, 8), 500.0f);
}

TEST(SnowRepaint, ALargerChangePushesTheBlockInItsPlaceOutOfAFullList)
{
	snow_cover::BlockRepaint repaint;
	for (int x = 0; x < 16; ++x)
	{
		repaint.changes.at(snow_cover::BlockIndex({x, 4})) = 1000.0f - static_cast<float>(x * 10);
	}
	std::vector<uint16_t> paintNow;
	snow_cover::ListRepaints(repaint, 4, 1, paintNow);
	ASSERT_EQ(repaint.count, 16);
	repaint.changes.at(snow_cover::BlockIndex({5, 5})) = 975.0f;
	repaint.changes.at(snow_cover::BlockIndex({6, 5})) = 100.0f;
	snow_cover::ListRepaints(repaint, 5, 1, paintNow);
	// 975 takes the place of 970, the fourth, and the list does not move. 100 finds no smaller size.
	ASSERT_EQ(paintNow.size(), 1U);
	EXPECT_EQ(paintNow[0], snow_cover::BlockIndex({3, 4}));
	EXPECT_EQ(repaint.count, 16);
	EXPECT_EQ(repaint.blocks[3], snow_cover::BlockIndex({5, 5}));
	EXPECT_FLOAT_EQ(repaint.sizes[3], 975.0f);
	EXPECT_EQ(repaint.blocks[4], snow_cover::BlockIndex({4, 4}));
	EXPECT_FLOAT_EQ(Change(repaint, 6, 5), 100.0f);
	// Then a change between two sizes takes the first smaller place, not the smallest
	repaint.changes.at(snow_cover::BlockIndex({7, 5})) = 955.0f;
	snow_cover::ListRepaints(repaint, 5, 1, paintNow);
	ASSERT_EQ(paintNow.size(), 2U);
	EXPECT_EQ(paintNow[1], snow_cover::BlockIndex({5, 4}));
	EXPECT_EQ(repaint.blocks[5], snow_cover::BlockIndex({7, 5}));
}

TEST(SnowRepaint, TheFirstBandPaintsTheListBeforeItListsItsOwnBlocks)
{
	snow_cover::BlockRepaint repaint;
	repaint.changes.at(snow_cover::BlockIndex({9, 12})) = 300.0f;
	repaint.changes.at(snow_cover::BlockIndex({2, 13})) = 200.0f;
	std::vector<uint16_t> paintNow;
	snow_cover::ListRepaints(repaint, 12, 4, paintNow);
	repaint.changes.at(snow_cover::BlockIndex({1, 0})) = 400.0f;
	snow_cover::ListRepaints(repaint, 0, 4, paintNow);
	ASSERT_EQ(paintNow.size(), 2U);
	EXPECT_EQ(paintNow[0], snow_cover::BlockIndex({9, 12}));
	EXPECT_EQ(paintNow[1], snow_cover::BlockIndex({2, 13}));
	ASSERT_EQ(repaint.count, 1);
	EXPECT_EQ(repaint.blocks[0], snow_cover::BlockIndex({1, 0}));
	EXPECT_FLOAT_EQ(repaint.sizes[1], 0.0f);
}

TEST(SnowRepaint, MeltedSnowComesOffTheOwnBlockAndTheBandIsListed)
{
	auto grid = Grid(3.0f);
	Cell(grid, 0, 16) = 1.5f;
	snow_cover::Melting melting;
	snow_cover::BlockRepaint repaint;
	repaint.changes.at(snow_cover::BlockIndex({0, 4})) = -70.0f;
	repaint.changes.at(snow_cover::BlockIndex({0, 8})) = -70.0f;
	std::vector<uint16_t> paintNow;
	// The second band melts: rows 16 to 31, the blocks of rows 4 to 7
	snow_cover::Melt(grid, melting, 0.31f, repaint, paintNow);
	EXPECT_TRUE(paintNow.empty());
	ASSERT_EQ(repaint.count, 1);
	EXPECT_EQ(repaint.blocks[0], snow_cover::BlockIndex({0, 4}));
	EXPECT_FLOAT_EQ(repaint.sizes[0], 70.0f + 30.0f + 1.5f);
	EXPECT_FLOAT_EQ(Change(repaint, 1, 4), -32.0f);
	EXPECT_FLOAT_EQ(Change(repaint, 0, 8), -70.0f);
	// The melt is not added to the blocks next to the cell
	EXPECT_FLOAT_EQ(Change(repaint, 0, 3), 0.0f);
}

TEST(SnowRepaint, ABlockTakesTheSnowAtItsFiveByFivePoints)
{
	auto grid = Grid();
	for (size_t i = 0; i < grid.size(); ++i)
	{
		grid[i] = static_cast<float>(i % 251);
	}
	std::vector<float> painted(static_cast<size_t>(snow_cover::k_PaintedSide) * snow_cover::k_PaintedSide, -1.0f);
	snow_cover::PaintBlockCorners(grid, {31, 2}, painted);
	const auto at = [&](int i, int j) {
		return painted.at((static_cast<size_t>((2 * 5) + j) * snow_cover::k_PaintedSide) + static_cast<size_t>((31 * 5) + i));
	};
	EXPECT_FLOAT_EQ(at(0, 0), Cell(grid, 124, 8));
	EXPECT_FLOAT_EQ(at(3, 4), Cell(grid, 127, 12));
	// The far edge of the grid has no snow
	EXPECT_FLOAT_EQ(at(4, 1), 0.0f);
	// The other blocks are not changed
	EXPECT_FLOAT_EQ(painted.at(0), -1.0f);
}

TEST(SnowRepaint, ABlockShowsTheSnowOfItsLastPaint)
{
	auto grid = Grid(10.0f);
	snow_cover::PaintedBlocks painted;
	const auto point = [&](glm::ivec2 block) {
		return painted
		    .GetPainted()[(static_cast<size_t>(block.y * 5) * snow_cover::k_PaintedSide) + static_cast<size_t>(block.x * 5)];
	};
	// A new land paints each block the first time it draws it
	const std::vector<glm::ivec2> drawn {{1, 1}, {2, 1}};
	EXPECT_TRUE(painted.PaintDrawn(drawn, grid));
	EXPECT_FLOAT_EQ(point({1, 1}), 10.0f);
	EXPECT_TRUE(painted.Waits(snow_cover::BlockIndex({3, 1})));
	// More snow shows only after a request, and only on a drawn block
	std::ranges::fill(grid, 50.0f);
	EXPECT_FALSE(painted.PaintDrawn(drawn, grid));
	EXPECT_FLOAT_EQ(point({1, 1}), 10.0f);
	painted.Request(snow_cover::BlockIndex({2, 1}));
	painted.Request(snow_cover::BlockIndex({5, 5}));
	EXPECT_TRUE(painted.PaintDrawn(drawn, grid));
	EXPECT_FLOAT_EQ(point({1, 1}), 10.0f);
	EXPECT_FLOAT_EQ(point({2, 1}), 50.0f);
	EXPECT_TRUE(painted.Waits(snow_cover::BlockIndex({5, 5})));
	EXPECT_FALSE(painted.Waits(snow_cover::BlockIndex({2, 1})));
}

TEST(SnowSystem, TheLandIsAskedToPaintItsBlocksForTheSnow)
{
	ecs::systems::SnowSystem snow;
	snow.Cover(50.0f);
	EXPECT_TRUE(snow.TakePaintRequests().all());
	EXPECT_TRUE(snow.TakePaintRequests().none());
	// The melt takes 2 from each cell of a band every three turns. A block of 16 cells changes by 32 at each melt of its
	// band, so it is listed at the fourth melt (128) and painted at the first band after that.
	bool asked = false;
	int turns = 0;
	while (!asked && turns < 200)
	{
		snow.ProcessTurn({});
		++turns;
		asked = snow.TakePaintRequests().any();
	}
	EXPECT_TRUE(asked);
	EXPECT_GT(turns, 3 * 24);
}

TEST(SnowSystem, AStormThatSnowsAsksForPaints)
{
	ecs::systems::SnowSystem snow;
	snow.Cover(40.0f);
	static_cast<void>(snow.TakePaintRequests());
	auto storm = SnowStorm(100);
	storm.currentPosition = {2560.0f, 300.0f, 2560.0f};
	storm.currentInnerRadius = 4000.0f;
	storm.outerRadius = 4000.0f;
	const std::vector<ecs::components::Storm> storms {storm};
	int asked = 0;
	for (int turn = 0; turn < 100; ++turn)
	{
		snow.ProcessTurn(storms);
		asked += static_cast<int>(snow.TakePaintRequests().count());
	}
	EXPECT_GT(asked, 0);
}
