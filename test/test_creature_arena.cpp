/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureArena.h"

using namespace openblack;
using namespace openblack::creature_arena;

namespace
{
/// A point in metres as a position in map units
glm::ivec2 Fixed(float x, float z)
{
	return {map_coords::ToFixed(x), map_coords::ToFixed(z)};
}
} // namespace

TEST(CreatureArena, ReuseDistanceGrowsWithSizeAndRunningShare)
{
	// Size 1: (0.875 + 0.25) * 20 = 22.5, times 1.1 and ten times that
	EXPECT_NEAR(ReuseDistance(1.0f, 1.0f), 247.5f, 1e-3f);
	EXPECT_NEAR(ReuseDistance(0.5f, 1.0f), 123.75f, 1e-3f);
	// The running share is kept within 0 and 1
	EXPECT_NEAR(ReuseDistance(2.0f, 1.0f), 247.5f, 1e-3f);
	EXPECT_FLOAT_EQ(ReuseDistance(-1.0f, 1.0f), 0.0f);
	// Size 2: (1.75 + 0.25) * 20 = 40
	EXPECT_NEAR(ReuseDistance(1.0f, 2.0f), 440.0f, 1e-3f);
}

TEST(CreatureArena, NearestArenaWithinTheDistance)
{
	const std::vector<glm::ivec2> arenas {Fixed(100.0f, 0.0f), Fixed(30.0f, 0.0f), Fixed(0.0f, 50.0f)};
	EXPECT_EQ(Nearest(arenas, Fixed(0.0f, 0.0f), 200.0f), 1u);
	// Only those strictly nearer than the distance
	EXPECT_EQ(Nearest(arenas, Fixed(0.0f, 0.0f), 20.0f), std::nullopt);
	EXPECT_EQ(Nearest({}, Fixed(0.0f, 0.0f), 200.0f), std::nullopt);
	// Of two as near, the first in the list
	const std::vector<glm::ivec2> even {Fixed(40.0f, 0.0f), Fixed(-40.0f, 0.0f)};
	EXPECT_EQ(Nearest(even, Fixed(0.0f, 0.0f), 200.0f), 0u);
}

TEST(CreatureArena, ClearPlaceOnOpenLandIsNextToTheStart)
{
	const auto start = Fixed(1005.0f, 2005.0f);
	const auto found = FindClearPlace(start, 60.0f, k_SearchRange, [](glm::ivec2) { return true; });
	ASSERT_TRUE(found.has_value());
	// The squares' middles are a cell's corner plus half the diameter: 1000 and 1010 are both 5 from 1005, and the first
	// found is the lower one
	EXPECT_NEAR(map_coords::ToMetres(found->x), 1000.0f, 0.01f);
	EXPECT_NEAR(map_coords::ToMetres(found->y), 2000.0f, 0.01f);
}

TEST(CreatureArena, ClearPlaceAvoidsBlockedCells)
{
	const auto start = Fixed(1005.0f, 1005.0f);
	// Everything west of cell 105 is blocked: the circle of 40 (four cells) must start at cell 105 or later
	const auto usable = [](glm::ivec2 cell) { return cell.x >= 105; };
	const auto found = FindClearPlace(start, 40.0f, k_SearchRange, usable);
	ASSERT_TRUE(found.has_value());
	EXPECT_NEAR(map_coords::ToMetres(found->x), 1050.0f + 20.0f, 0.01f);
	EXPECT_NEAR(map_coords::ToMetres(found->y), 1000.0f, 0.01f);
}

TEST(CreatureArena, ClearPlaceIgnoresTheCornersOutsideTheCircle)
{
	const auto start = Fixed(1005.0f, 1005.0f);
	// A circle of 40 covers four cells a side, and its corner cells' middles lie outside it: the nearest square, cells 98
	// to 101, is taken although its corners are blocked
	const auto corners = [](glm::ivec2 cell) {
		const bool corner = (cell.x == 98 || cell.x == 101) && (cell.y == 98 || cell.y == 101);
		return !corner;
	};
	const auto found = FindClearPlace(start, 40.0f, k_SearchRange, corners);
	ASSERT_TRUE(found.has_value());
	EXPECT_NEAR(map_coords::ToMetres(found->x), 1000.0f, 0.01f);
	EXPECT_NEAR(map_coords::ToMetres(found->y), 1000.0f, 0.01f);
	// A cell inside the circle blocked moves it on
	const auto middle = [](glm::ivec2 cell) { return cell != glm::ivec2(99, 99); };
	const auto moved = FindClearPlace(start, 40.0f, k_SearchRange, middle);
	ASSERT_TRUE(moved.has_value());
	EXPECT_NE(*moved, *found);
}

TEST(CreatureArena, NoClearPlaceAtAll)
{
	EXPECT_EQ(FindClearPlace(Fixed(1000.0f, 1000.0f), 40.0f, k_SearchRange, [](glm::ivec2) { return false; }), std::nullopt);
}

TEST(CreatureArena, SmallerArenasAreTriedDownToSixTenths)
{
	const auto never = [](glm::ivec2) { return false; };
	EXPECT_EQ(Place(Fixed(1000.0f, 1000.0f), 50.0f, never), std::nullopt);

	// Room for a circle of 60 (six cells) only: a radius of 50 fits only at six tenths
	const auto start = Fixed(1005.0f, 1005.0f);
	const auto room = [](glm::ivec2 cell) { return cell.x >= 100 && cell.x < 106 && cell.y >= 100 && cell.y < 106; };
	const auto placed = Place(start, 50.0f, room);
	ASSERT_TRUE(placed.has_value());
	EXPECT_NEAR(placed->scale, 0.6f, 1e-5f);
	EXPECT_NEAR(placed->radius, 30.0f, 1e-4f);
	EXPECT_NEAR(map_coords::ToMetres(placed->centre.x), 1030.0f, 0.01f);
	EXPECT_NEAR(map_coords::ToMetres(placed->centre.y), 1030.0f, 0.01f);
}

TEST(CreatureArena, RingPointsCloseOnTheLand)
{
	const auto points = RingPoints({100.0f, 200.0f}, 30.0f, [](glm::vec2 at) { return at.x * 0.1f; });
	ASSERT_EQ(points.size(), k_RingPoints);
	EXPECT_NEAR(points.front().x, 130.0f, 1e-4f);
	EXPECT_NEAR(points.front().z, 200.0f, 1e-4f);
	EXPECT_NEAR(points.front().y, 13.0f, 1e-4f);
	// The last closes the ring on the first
	EXPECT_NEAR(points.back().x, points.front().x, 1e-3f);
	EXPECT_NEAR(points.back().z, points.front().z, 1e-3f);
	// A quarter of the way round (of 31 steps) it is on the far side of z
	const float angle = 8.0f * 2.0f * std::numbers::pi_v<float> / 31.0f;
	EXPECT_NEAR(points.at(8).x, 100.0f + (30.0f * std::cos(angle)), 1e-3f);
	EXPECT_NEAR(points.at(8).z, 200.0f + (30.0f * std::sin(angle)), 1e-3f);
}

TEST(CreatureArena, ATempleAlwaysKeepsAnArenaOffItsCells)
{
	const ThingInCell temple {.kind = ThingKind::Temple, .height = 1.0f, .circles = {}};
	EXPECT_TRUE(KeepsArenaOff(temple, 1.0f, {10, 10}));
}

TEST(CreatureArena, LivingThingsAndThingsWithoutAModelAreNeverInTheWay)
{
	// A circle right on the cell's middle (105, 105)
	const std::vector<GroundCircle> onTheMiddle {{.centre = {105.0f, 105.0f}, .radius = 1.0f}};
	EXPECT_FALSE(
	    KeepsArenaOff({.kind = ThingKind::Fixed, .living = true, .height = 100.0f, .circles = onTheMiddle}, 1.0f, {10, 10}));
	EXPECT_FALSE(
	    KeepsArenaOff({.kind = ThingKind::Fixed, .hasModel = false, .height = 100.0f, .circles = onTheMiddle}, 1.0f, {10, 10}));
	EXPECT_TRUE(KeepsArenaOff({.kind = ThingKind::Fixed, .height = 1.0f, .circles = onTheMiddle}, 1.0f, {10, 10}));
}

TEST(CreatureArena, BuildingsAndThingsThatCanBeThrownAreInTheWayOnlyWhenTallerThanFifteenTimesTheCreature)
{
	const std::vector<GroundCircle> onTheMiddle {{.centre = {105.0f, 105.0f}, .radius = 1.0f}};
	for (const auto kind : {ThingKind::Building, ThingKind::Movable})
	{
		EXPECT_FALSE(KeepsArenaOff({.kind = kind, .height = 15.0f, .circles = onTheMiddle}, 1.0f, {10, 10}));
		EXPECT_TRUE(KeepsArenaOff({.kind = kind, .height = 15.5f, .circles = onTheMiddle}, 1.0f, {10, 10}));
		// A smaller creature finds the same thing taller
		EXPECT_TRUE(KeepsArenaOff({.kind = kind, .height = 8.0f, .circles = onTheMiddle}, 0.5f, {10, 10}));
	}
	// Storage pits and town centres are looked at however low
	EXPECT_TRUE(
	    KeepsArenaOff({.kind = ThingKind::StoragePitOrTownCentre, .height = 1.0f, .circles = onTheMiddle}, 1.0f, {10, 10}));
}

TEST(CreatureArena, AThingIsInTheWayWhenACircleComesWithinFiveMetresOfTheCellsMiddle)
{
	// 7 m east of the middle with a radius of 2: 5 m clear, not in the way; a little bigger and it is
	EXPECT_FALSE(KeepsArenaOff(
	    {.kind = ThingKind::Fixed, .height = 1.0f, .circles = {{.centre = {112.0f, 105.0f}, .radius = 2.0f}}}, 1.0f, {10, 10}));
	EXPECT_TRUE(KeepsArenaOff(
	    {.kind = ThingKind::Fixed, .height = 1.0f, .circles = {{.centre = {112.0f, 105.0f}, .radius = 2.1f}}}, 1.0f, {10, 10}));
	// Any one circle of a row is enough
	EXPECT_TRUE(
	    KeepsArenaOff({.kind = ThingKind::Fixed,
	                   .height = 1.0f,
	                   .circles = {{.centre = {200.0f, 200.0f}, .radius = 1.0f}, {.centre = {106.0f, 106.0f}, .radius = 1.0f}}},
	                  1.0f, {10, 10}));
	EXPECT_FALSE(KeepsArenaOff({.kind = ThingKind::Fixed, .height = 1.0f, .circles = {}}, 1.0f, {10, 10}));
}
