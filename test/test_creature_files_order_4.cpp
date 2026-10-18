/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureArena.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureObstacles.h"
#include "Creature/CreatureThrow.h"

// The expected bits come from the game's instructions, worked step by step in single precision
// (scratchpad cfo4/bits.py of the creature-files-order-4 lane)

using namespace openblack;

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

float Quantised(float metres)
{
	return map_coords::Quantise(metres);
}
} // namespace

// Backing away to throw: the way and its length with z first, twice the height as a sum, and the share of the way
TEST(CreatureFilesOrder4, TheThrowStandBacksOffInTheGamesOrder)
{
	const glm::vec2 creature {Quantised(1000.37f), Quantised(2000.91f)};
	const glm::vec2 target {Quantised(1010.13f), Quantised(2004.77f)};
	ASSERT_EQ(Bits(creature.x), 0x447a17aeu);
	ASSERT_EQ(Bits(target.y), 0x44fa98a4u);
	const auto stand = creature_throw::WhereToThrowFrom(creature, target, 15.3f, 4.1f, 2.7f);
	ASSERT_TRUE(stand.has_value());
	ASSERT_EQ(stand->kind, creature_throw::ThrowStand::Kind::BackOff);
	EXPECT_EQ(Bits(stand->point.x), 0x4473d679u);
	EXPECT_EQ(Bits(stand->point.y), 0x44f8e07eu);
	EXPECT_EQ(Bits(stand->maxDistance), 0x41f4cccdu);
}

// Practising a throw: three heights added to the creature's map position, made a map position again by 65536 over 10
TEST(CreatureFilesOrder4, ThePracticeThrowGoesThroughMapUnits)
{
	const auto agenda = creature_mind::PracticeThrow(std::nullopt, {100.3f, 200.7f}, 10.123f);
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_EQ(Bits(agenda.back().order.point.x), 0x4302ab40u);
	EXPECT_EQ(Bits(agenda.back().order.point.y), 0x436711a2u);
}

// A worship site's ball: the products of z, y and x added in turn, then the place; the turn rows carry the scale
TEST(CreatureFilesOrder4, AWorshipSiteBallAddsItsPartsInTheGamesOrder)
{
	const auto spheres = creature_cast_moves::WorshipSiteSpheres({101.3f, 33.7f, 205.9f}, 0.0f, 1.37f);
	EXPECT_EQ(Bits(spheres[0].centre.x), 0x42d14ebau);
	EXPECT_EQ(Bits(spheres[0].centre.y), 0x420e78d5u);
	EXPECT_EQ(Bits(spheres[0].centre.z), 0x431f15f2u);
	EXPECT_EQ(Bits(spheres[0].radius), 0x3fe8f5c2u);
}

// An arena's cell test meets a worship site's ball by its distance with its height
TEST(CreatureFilesOrder4, AnArenaMeetsABallByItsHeight)
{
	creature_arena::ThingInCell thing {.kind = creature_arena::ThingKind::Fixed, .height = 1000.0f};
	thing.circles.push_back({.centre = {15.0f, 5.0f}, .radius = 2.0f, .height = 0.0f});
	EXPECT_TRUE(creature_arena::KeepsArenaOff(thing, 1.0f, {1, 0}));
	// High above the cell, the same ball is out of reach
	thing.circles.back().height = 30.0f;
	EXPECT_FALSE(creature_arena::KeepsArenaOff(thing, 1.0f, {1, 0}));
}

// The look while it goes keeps its point as a map position
TEST(CreatureFilesOrder4, TheLookWhileGoingIsAtAMapPosition)
{
	const creature_look::WhileGoingInfo info {
	    .sweepHalfAngle = 1.5f,
	    .sweepAngleStep = 0.1f,
	    .sweepNearest = 15.0f,
	    .sweepFurthest = 95.0f,
	    .sweepDistanceStep = 10.0f,
	    .placeRange = 25.0f,
	};
	auto state = creature_look::StartWhileGoing(info);
	const creature_look::WhileGoingView view {.position = {0.0f, 0.0f}, .place = {0.0f, 24.9f}, .turnsPerSecond = 10.0f};
	const auto point = creature_look::LookWhileGoing(state, info, view);
	ASSERT_TRUE(point.has_value());
	EXPECT_EQ(Bits(point->at.y), 0x41c73300u);
}

// A thing that does not move: the middle of its height over the ground where it stands
TEST(CreatureFilesOrder4, AThingIsAimedAtTheMiddleOfItsHeight)
{
	const creature_throw::ThrowTarget target {.mapPosition = {65536, 131072}, .height = 7.0f};
	const auto place = creature_throw::PlaceIn(target, 3.0f, [](int32_t, int32_t) { return 2.0f; });
	EXPECT_EQ(place, glm::vec3(10.0f, 5.5f, 20.0f));
}

// A walking villager goes on by its step for each whole turn in the time; one in a final state stays
TEST(CreatureFilesOrder4, AWalkingVillagerIsLedByWholeTurns)
{
	creature_throw::ThrowTarget target {.kind = creature_throw::ThrowTarget::Kind::Villager,
	                                    .mapPosition = {65536, 65536},
	                                    .height = 0.0f,
	                                    .step = {6554, -6554},
	                                    .walking = true};
	const auto ground = [](int32_t, int32_t) { return 0.0f; };
	// 0.29 s is two whole turns
	const auto led = creature_throw::PlaceIn(target, 0.29f, ground);
	const float step = (6554.0f * 10.0f) / 65536.0f;
	EXPECT_EQ(Bits(led.x), Bits((2.0f * step) + 10.0f));
	EXPECT_EQ(Bits(led.z), Bits((2.0f * -step) + 10.0f));
	target.walking = false;
	EXPECT_EQ(creature_throw::PlaceIn(target, 0.29f, ground), glm::vec3(10.0f, 0.0f, 10.0f));
}

// A thing that leans keeps its three angles for the middle of its box
TEST(CreatureFilesOrder4, ALeaningThingKeepsItsAngles)
{
	const auto rows = creature_obstacles::LeaningPlacement(0.3f, 0.2f, -0.1f, 1.0f);
	const glm::mat3 rotation(rows[0], rows[1], rows[2]);
	EXPECT_NEAR(creature_obstacles::YAngleOf(rotation), 0.3f, 1e-6f);
}

// Deviation 79, case 726: right on the thing's centre the way has no length, so there is no stand and the move fails
TEST(CreatureFilesOrder4, ThrowingFromTheThingsCentreHasNoStand)
{
	const glm::vec2 centre {Quantised(1000.37f), Quantised(2000.91f)};
	EXPECT_FALSE(creature_throw::WhereToThrowFrom(centre, centre, 15.3f, 4.1f, 2.7f).has_value());
	EXPECT_FALSE(creature_throw::WhereToThrowFrom(centre, centre, 15.3f, 0.0f, 0.0f).has_value());
}

// Deviation 79, case 727: a point that it may stand on is kept
TEST(CreatureFilesOrder4, TheBackOffPointIsKeptWhereItCanStand)
{
	const auto land = creature_route::WalkableLand::OfCells({});
	const auto point = creature_throw::BackOffPoint(land, {1000.0f, 2000.0f});
	ASSERT_TRUE(point.has_value());
	EXPECT_EQ(*point, glm::vec2(1000.0f, 2000.0f));
}

// Deviation 79, case 727: a land with no place to stand gives no point, so the move fails
TEST(CreatureFilesOrder4, TheBackOffPointFailsWhenTheSearchFindsNoPlace)
{
	const auto land = creature_route::WalkableLand::OfCells(std::vector<creature_route::Ground>(
	    static_cast<size_t>(creature_route::k_CellsPerSide) * creature_route::k_CellsPerSide, creature_route::Ground::Blocked));
	EXPECT_FALSE(creature_throw::BackOffPoint(land, {1000.0f, 2000.0f}).has_value());
}

// Deviation 79, case 727: a place found too far away gives no point either
TEST(CreatureFilesOrder4, TheBackOffPointFailsWhenTheNearestPlaceIsTooFar)
{
	// A blocked square of cells about 100 units across: the nearest place to stand is more than the reach away
	std::vector<creature_route::Ground> cells(
	    static_cast<size_t>(creature_route::k_CellsPerSide) * creature_route::k_CellsPerSide, creature_route::Ground::Open);
	for (int32_t z = 150; z < 170; ++z)
	{
		for (int32_t x = 150; x < 170; ++x)
		{
			cells[static_cast<size_t>(z) * creature_route::k_CellsPerSide + x] = creature_route::Ground::Blocked;
		}
	}
	const auto land = creature_route::WalkableLand::OfCells(std::move(cells));
	EXPECT_FALSE(creature_throw::BackOffPoint(land, {1600.0f, 1600.0f}).has_value());
}
