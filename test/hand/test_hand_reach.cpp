/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "Hand/HandReach.h"

namespace hand_reach = openblack::hand_reach;
namespace map_coords = openblack::map_coords;

namespace
{
map_coords::MapCoords At(float x, float z)
{
	return map_coords::FromMetres({x, z});
}
} // namespace

TEST(HandReach, TheHandsMiddleIsTheMiddleOfTheBoxRoundItsBonesLeavingOutTheRoot)
{
	const std::array<glm::vec3, 4> bones = {
	    {{100.0f, 100.0f, 100.0f}, {0.0f, 0.0f, 0.0f}, {2.0f, 4.0f, -2.0f}, {1.0f, 1.0f, 1.0f}}};
	const auto middle = hand_reach::Middle(bones);
	ASSERT_TRUE(middle.has_value());
	EXPECT_FLOAT_EQ(middle->x, 1.0f);
	EXPECT_FLOAT_EQ(middle->y, 2.0f);
	EXPECT_FLOAT_EQ(middle->z, -0.5f);
	// A root alone has no middle
	EXPECT_FALSE(hand_reach::Middle(std::span(bones).first(1)).has_value());
}

TEST(HandReach, TheCellsSearchedAreThoseWithinFiveMetresAcross)
{
	const auto cells = hand_reach::CellsAround(At(1234.5f, 87.0f));
	EXPECT_EQ(cells.first, glm::ivec2(122, 8));
	EXPECT_EQ(cells.last, glm::ivec2(123, 9));
	// Near the map's corner the first cells are off the map
	const auto corner = hand_reach::CellsAround(At(3.0f, 3.0f));
	EXPECT_EQ(corner.first, glm::ivec2(-1, -1));
	EXPECT_EQ(corner.last, glm::ivec2(0, 0));
}

TEST(HandReach, TheNearestWithinFiveMetresIsTakenIfNoFurtherThanTheCursorsLand)
{
	const auto hand = At(100.0f, 100.0f);
	const std::array<map_coords::MapCoords, 4> things = {
	    {At(104.9f, 100.0f), At(103.0f, 100.0f), At(100.0f, 103.0f), At(106.0f, 100.0f)}};
	// The first of the two as near is taken while the cursor's land is as far from the hand or further
	EXPECT_EQ(hand_reach::Nearest(hand, At(103.0f, 100.0f), things), std::optional<size_t>(1));
	EXPECT_EQ(hand_reach::Nearest(hand, At(100.0f, 140.0f), things), std::optional<size_t>(1));
	// Nearer the hand than anything, the cursor's land keeps the hand from reaching
	EXPECT_FALSE(hand_reach::Nearest(hand, At(102.0f, 100.0f), things).has_value());
	// Nothing within five metres
	const std::array<map_coords::MapCoords, 2> far = {{At(105.0f, 100.0f), At(100.0f, 94.0f)}};
	EXPECT_FALSE(hand_reach::Nearest(hand, At(100.0f, 140.0f), far).has_value());
}
