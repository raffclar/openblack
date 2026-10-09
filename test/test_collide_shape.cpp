/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <vector>

#include <gtest/gtest.h>

#include "ECS/CollideShape.h"

using namespace openblack::ecs::collide;

namespace
{
constexpr glm::vec2 k_XAxis {1.0f, 0.0f};
constexpr glm::vec2 k_ZAxis {0.0f, 1.0f};
constexpr glm::ivec2 k_Cells {512, 512};
} // namespace

TEST(CollideShape, ANearSquareBoxIsOneCircleAsWideAsItsLongerHalfSide)
{
	const auto shape = ShapeOfBox({50.0f, 60.0f}, {4.0f, 5.5f}, k_XAxis, k_ZAxis);
	EXPECT_EQ(shape.bounds.centre, glm::vec2(50.0f, 60.0f));
	EXPECT_FLOAT_EQ(shape.bounds.radius, 5.5f);
	EXPECT_TRUE(shape.row.empty());
}

TEST(CollideShape, HalfSidesAreAtLeastAMetre)
{
	const auto shape = ShapeOfBox({50.0f, 60.0f}, {0.2f, 0.3f}, k_XAxis, k_ZAxis);
	EXPECT_FLOAT_EQ(shape.bounds.radius, 1.0f);
	EXPECT_TRUE(shape.row.empty());
}

TEST(CollideShape, ALongBoxIsARowOfCirclesAlongItsLongSide)
{
	// Ten by three: four circles of radius three, five metres apart, in a circle reaching the corners
	const auto shape = ShapeOfBox({50.0f, 60.0f}, {10.0f, 3.0f}, k_XAxis, k_ZAxis);
	EXPECT_FLOAT_EQ(shape.bounds.radius, std::sqrt(109.0f));
	EXPECT_FLOAT_EQ(shape.boundsSquared, 109.0f);
	ASSERT_EQ(shape.row.size(), 4u);
	const std::vector<float> xs {42.5f, 47.5f, 52.5f, 57.5f};
	for (size_t i = 0; i < xs.size(); ++i)
	{
		EXPECT_FLOAT_EQ(shape.row[i].centre.x, xs[i]);
		EXPECT_FLOAT_EQ(shape.row[i].centre.y, 60.0f);
		EXPECT_FLOAT_EQ(shape.row[i].radius, 3.0f);
	}
}

TEST(CollideShape, ARowTurnsWithItsBox)
{
	// Long along its own z, which points along the world's -x
	const auto shape = ShapeOfBox({50.0f, 60.0f}, {2.0f, 6.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f});
	ASSERT_EQ(shape.row.size(), 4u);
	EXPECT_FLOAT_EQ(shape.row.front().centre.x, 54.5f);
	EXPECT_FLOAT_EQ(shape.row.back().centre.x, 45.5f);
	EXPECT_FLOAT_EQ(shape.row.front().centre.y, 60.0f);
	EXPECT_FLOAT_EQ(shape.row.front().radius, 2.0f);
}

TEST(CollideShape, ACircleTouchesALongShapeOnlyWhereItsRowIs)
{
	const auto shape = ShapeOfBox({50.0f, 60.0f}, {10.0f, 3.0f}, k_XAxis, k_ZAxis);
	// Inside the bounding circle, off the row's side
	EXPECT_FALSE(Touches({.centre = {50.0f, 68.0f}, .radius = 1.0f}, shape));
	EXPECT_TRUE(Touches({.centre = {50.0f, 65.0f}, .radius = 3.0f}, shape));
	// Touching counts
	EXPECT_TRUE(Touches({.centre = {57.5f, 64.0f}, .radius = 1.0f}, shape));
	EXPECT_FALSE(Touches({.centre = {80.0f, 60.0f}, .radius = 1.0f}, shape));
}

TEST(CollideShape, ABuildingIsFiledInTheCellsItsShapeReaches)
{
	// A seven metre circle in the middle of cell (5, 5) reaches the middles of the four cells beside it, not those
	// at its corners
	const auto shape = ShapeOfBox({55.0f, 55.0f}, {7.0f, 7.0f}, k_XAxis, k_ZAxis);
	const std::vector<glm::ivec2> expected {{4, 5}, {5, 4}, {5, 5}, {5, 6}, {6, 5}};
	EXPECT_EQ(FootprintCells(shape, 10.0f, k_Cells), expected);
}

TEST(CollideShape, OnlyCellsWithinReachAreLookedAt)
{
	const auto shape = ShapeOfBox({55.0f, 55.0f}, {7.0f, 7.0f}, k_XAxis, k_ZAxis);
	const std::vector<glm::ivec2> expected {{5, 5}};
	EXPECT_EQ(FootprintCells(shape, 4.0f, k_Cells), expected);
}

TEST(CollideShape, CellsOffTheMapAreLeftOut)
{
	const auto shape = ShapeOfBox({55.0f, 55.0f}, {7.0f, 7.0f}, k_XAxis, k_ZAxis);
	const std::vector<glm::ivec2> expected {{4, 5}, {5, 4}, {5, 5}};
	EXPECT_EQ(FootprintCells(shape, 10.0f, {6, 6}), expected);
}

TEST(CollideShape, ABuildingTouchingNoCellIsFiledInTheMiddleOfItsRange)
{
	// Off the map's low corner the range starts at the first cell, which it doesn't reach
	const auto shape = ShapeOfBox({-30.0f, -30.0f}, {1.0f, 1.0f}, k_XAxis, k_ZAxis);
	const std::vector<glm::ivec2> expected {{0, 0}};
	EXPECT_EQ(FootprintCells(shape, 2.0f, k_Cells), expected);
	// Past the far edge the middle cell is off the map too: it is filed nowhere
	const auto beyond = ShapeOfBox({75.0f, 5.0f}, {1.0f, 1.0f}, k_XAxis, k_ZAxis);
	EXPECT_TRUE(FootprintCells(beyond, 2.0f, {4, 4}).empty());
}
