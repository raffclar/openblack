/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <numbers>
#include <span>

#include <gtest/gtest.h>

#include "ECS/WallHugRules.h"

using namespace openblack::ecs::wall_hug;

namespace
{
/// A villager's usual walk: 4600 whole map units a turn
constexpr float k_Speed = 4600.0f * 10.0f / 65536.0f * k_TurnsPerSecond;

/// Made-up circles are placed well inside the map, a hundred metres from its corner
constexpr glm::vec2 k_Middle {100.0f, 100.0f};

CircleSweepInput Around(glm::vec2 position, glm::vec2 goal, bool clockwise, std::span<const BlockingCircle> blockers = {})
{
	return {
	    .position = ToWhole(k_Middle + position),
	    .goal = ToWhole(k_Middle + goal),
	    .centre = k_Middle,
	    .radius = 5.0f,
	    .clockwise = clockwise,
	    .wholeSpeed = WholeSpeed(k_Speed),
	    .blockers = blockers,
	};
}

glm::vec2 OnCircle(float degrees, float radius = 5.0f)
{
	const float radians = degrees * std::numbers::pi_v<float> / 180.0f;
	return glm::vec2(std::cos(radians), std::sin(radians)) * radius;
}
} // namespace

TEST(WallHugRules, AWalkerGoesATenthOfItsSpeedEachTurn)
{
	EXPECT_EQ(WholeSpeed(k_Speed), 4600);
	EXPECT_FLOAT_EQ(StepMetres(k_Speed), 0.701904296875f);
	// One metre a second is 655 whole units a turn, about a tenth of a metre
	EXPECT_EQ(WholeSpeed(1.0f), 655);
	EXPECT_NEAR(StepMetres(1.0f), 0.1f, 0.0001f);
	EXPECT_EQ(WholeSpeed(-1.0f), 0);
	EXPECT_EQ(WholeSpeed(1000.0f), k_MaxWholeSpeed);
}

TEST(WallHugRules, TheWalkIsInWholeMapUnitsAtGameAngles)
{
	// At 4600 whole units a turn the step is a sixteenth of the speed (287) times the sine table, over 4096: 4592 along
	// x, and 2384 by 3924 at game angle 334 (about 59 degrees)
	EXPECT_EQ(StepAlong(0, 4600), glm::ivec2(4592, 0));
	EXPECT_EQ(StepAlong(334, 4600), glm::ivec2(2384, 3924));
	EXPECT_EQ(StepAlong(0x400, 4600), glm::ivec2(-4592, 0));
	// Going round a 5 m circle it turns 0.14 radians, 45 game angles, and one more
	EXPECT_EQ(OrbitTurn(4600, 5.0f), 46);
	EXPECT_EQ(OrbitTurn(4600, 50.0f), 5);
	// Within its step of a point: strictly nearer than its speed in whole units
	EXPECT_TRUE(WithinStep({0, 0}, {4599, 0}, 4600));
	EXPECT_FALSE(WithinStep({0, 0}, {4600, 0}, 4600));
	// Map positions and metres: a metre is 6553.6 whole units, truncated going in
	EXPECT_EQ(ToWhole({1.0f, 10.0f}), glm::ivec2(6553, 65536));
	EXPECT_FLOAT_EQ(ToPoint({65536, 32768}).x, 10.0f);
	EXPECT_FLOAT_EQ(ToPoint({65536, 32768}).y, 5.0f);
	EXPECT_FLOAT_EQ(MetresDistanceSq({0, 0}, {65536, 65536}), 200.0f);
}

TEST(WallHugRules, AWalkerGoesRoundTheWayTheCircleLiesFromItsStep)
{
	// Heading along +x, with x to the right and z up: the circle's middle off to its right (-z) takes it round
	// clockwise, off to its left anticlockwise
	EXPECT_TRUE(GoesRoundClockwise({0, 0}, {65536, -65536}, {4592, 0}));
	EXPECT_FALSE(GoesRoundClockwise({0, 0}, {65536, 65536}, {4592, 0}));
}

TEST(WallHugRules, AGoalInsideTheCircleEndsTheWalkRoundAtItsBearing)
{
	// The goal is a quarter turn ahead going anticlockwise, three quarters going clockwise; a turn walks 0.7 m of the
	// arc and the look ahead counts it at two thirds of that
	const auto anticlockwise = SweepCircle(Around(OnCircle(0.0f), {0.0f, 2.0f}, false));
	EXPECT_EQ(anticlockwise.outcome, CircleSweep::Outcome::Hug);
	EXPECT_FALSE(anticlockwise.hugged.has_value());
	EXPECT_EQ(anticlockwise.turnsToObstacle, 7);
	ASSERT_TRUE(anticlockwise.heading.has_value());

	const auto clockwise = SweepCircle(Around(OnCircle(0.0f), {0.0f, 2.0f}, true));
	EXPECT_EQ(clockwise.outcome, CircleSweep::Outcome::Hug);
	EXPECT_EQ(clockwise.turnsToObstacle, 22);
}

TEST(WallHugRules, AtTheGoalsBearingTheWalkerStepsStraightIn)
{
	const auto sweep = SweepCircle(Around(OnCircle(89.5f), {0.0f, 2.0f}, false));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::StepThrough);
	EXPECT_FALSE(sweep.heading.has_value());
}

TEST(WallHugRules, WithTheGoalOutsideAndNothingInTheWayTheWalkerKeepsHugging)
{
	const auto sweep = SweepCircle(Around(OnCircle(0.0f), {20.0f, 20.0f}, false));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::Hug);
	EXPECT_EQ(sweep.turnsToObstacle, k_NoObstacleInReach);
	ASSERT_TRUE(sweep.heading.has_value());
	// On the circle, a tenth of a radius out from 0.9 of it: it faces a quarter turn (512 game angles) round from the
	// bearing to the middle (1024, back along -x), anticlockwise here, less 64 game angles for each radius beyond 0.9
	// of it, truncated: 1024 - 512 + 6
	EXPECT_EQ(*sweep.heading, 518);
}

TEST(WallHugRules, AnOverlappingCircleAheadEndsTheWalkRound)
{
	// The neighbour cuts the hugged circle at a bearing of about 37 degrees; going clockwise from 90 the walker has 53
	// degrees to go
	const std::array<BlockingCircle, 1> neighbour {
	    {{.centre = k_Middle + glm::vec2(8.0f, 0.0f), .radius = 5.0f, .landscape = false, .landscapeOrFence = false}}};
	const auto sweep = SweepCircle(Around(OnCircle(90.0f), {20.0f, 0.0f}, true, neighbour));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::Hug);
	EXPECT_FALSE(sweep.hugged.has_value());
	EXPECT_EQ(sweep.turnsToObstacle, 4);
}

TEST(WallHugRules, AtTheOverlapTheWalkerIsHandedOverToTheNeighbour)
{
	const std::array<BlockingCircle, 1> neighbour {
	    {{.centre = k_Middle + glm::vec2(8.0f, 0.0f), .radius = 5.0f, .landscape = false, .landscapeOrFence = false}}};
	const auto sweep = SweepCircle(Around(OnCircle(37.5f), {20.0f, 0.0f}, true, neighbour));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::Hug);
	ASSERT_TRUE(sweep.hugged.has_value());
	EXPECT_EQ(*sweep.hugged, 0U);
	EXPECT_FALSE(sweep.needsLookahead);
}

TEST(WallHugRules, WaterNeverCutsShortTheWalkRoundToAGoalInside)
{
	const std::array<BlockingCircle, 1> water {{{.centre = k_Middle + glm::vec2(6.0f, 6.0f),
	                                             .radius = k_LandscapeBlockerRadius,
	                                             .landscape = true,
	                                             .landscapeOrFence = true}}};
	const auto sweep = SweepCircle(Around(OnCircle(0.0f), {0.0f, 2.0f}, false, water));
	EXPECT_EQ(sweep.outcome, CircleSweep::Outcome::Hug);
	EXPECT_FALSE(sweep.hugged.has_value());
	EXPECT_EQ(sweep.turnsToObstacle, 7);
}

TEST(WallHugRules, AWalkerLeavesTheCircleOnlyNearerItsGoalAndWithTheGoalAhead)
{
	const glm::ivec2 goal = ToWhole(k_Middle);
	const auto at = [](float x) { return ToWhole(k_Middle + glm::vec2(x, 0.0f)); };
	// 10 m from the goal on starting round: 1279 hundred-and-twenty-eighths
	const auto entry = EntryDistance(at(-10.0f), goal);
	EXPECT_EQ(entry, 1279U);
	EXPECT_EQ(EntryDistance(goal, goal), 0U);
	// Nearer, with the goal ahead and off to the side away from an anticlockwise turn: going anticlockwise it leaves
	EXPECT_TRUE(LeavesCircle(at(-5.0f), goal, {3277, 3277}, entry, false));
	EXPECT_FALSE(LeavesCircle(at(-5.0f), goal, {3277, 3277}, entry, true));
	// The goal behind it
	EXPECT_FALSE(LeavesCircle(at(-5.0f), goal, {-3277, 3277}, entry, false));
	// No nearer than on starting round
	EXPECT_FALSE(LeavesCircle(at(-11.0f), goal, {3277, 3277}, entry, false));
}

namespace
{
/// A walker at 1000 m, 1000 m stepping 5 m a turn along x
const glm::ivec2 k_Walker = ToWhole({1000.0f, 1000.0f});
constexpr glm::ivec2 k_FiveMetresAlongX {32768, 0};

BlockingCircle Thing(glm::vec2 offset, float radius)
{
	return {.centre = glm::vec2(1000.0f, 1000.0f) + offset, .radius = radius, .landscape = false, .landscapeOrFence = false};
}
} // namespace

TEST(WallHugRules, HeadingStraightOnTheWalkerHeadsForTheNearestCircleOnItsLine)
{
	// The far one comes first in the cell, the near one is reached first: its edge 38 m on, after 7 whole turns
	const std::array<BlockingCircle, 3> circles {Thing({60.0f, 0.0f}, 2.0f), Thing({0.0f, 30.0f}, 2.0f),
	                                             Thing({40.0f, 1.0f}, 2.0f)};
	const auto scan = ScanLine(k_Walker, k_FiveMetresAlongX, circles);
	ASSERT_TRUE(scan.circle.has_value());
	EXPECT_EQ(*scan.circle, 2U);
	EXPECT_EQ(scan.turnsToObstacle, 7);
}

TEST(WallHugRules, HeadingStraightOnNothingOnTheLineIsNothingInReach)
{
	const std::array<BlockingCircle, 2> circles {Thing({0.0f, 30.0f}, 2.0f), Thing({-20.0f, 0.0f}, 2.0f)};
	const auto scan = ScanLine(k_Walker, k_FiveMetresAlongX, circles);
	EXPECT_FALSE(scan.circle.has_value());
	EXPECT_EQ(scan.turnsToObstacle, k_NoObstacleInReach);
	// Nor is a circle more than 255 turns away
	const std::array<BlockingCircle, 1> far {Thing({1300.0f, 0.0f}, 2.0f)};
	EXPECT_FALSE(ScanLine(k_Walker, k_FiveMetresAlongX, far).circle.has_value());
}

TEST(WallHugRules, HeadingStraightOnACircleTheWalkerIsWellInsideDoesNotCount)
{
	// Five metres inside the first: it is passed over for the one ahead
	const std::array<BlockingCircle, 2> circles {Thing({0.0f, 0.0f}, 5.0f), Thing({20.0f, 0.0f}, 2.0f)};
	const auto scan = ScanLine(k_Walker, k_FiveMetresAlongX, circles);
	ASSERT_TRUE(scan.circle.has_value());
	EXPECT_EQ(*scan.circle, 1U);
	EXPECT_EQ(scan.turnsToObstacle, 3);
	// Just inside the edge of one, a tenth of a metre, it meets it at once
	const std::array<BlockingCircle, 1> edge {Thing({2.1f, 0.0f}, 2.2f)};
	const auto atOnce = ScanLine(k_Walker, k_FiveMetresAlongX, edge);
	ASSERT_TRUE(atOnce.circle.has_value());
	EXPECT_EQ(atOnce.turnsToObstacle, 0);
}

namespace
{
ThingOnMap Placed(ThingShape shape, glm::vec3 boxHalfSize, bool fence = false)
{
	return {.shape = shape,
	        .position = {1000.0f, 0.0f, 1000.0f},
	        .rotation = glm::mat3(1.0f),
	        .scale = 1.0f,
	        .boxCentre = glm::vec3(0.0f),
	        .boxHalfSize = boxHalfSize,
	        .fence = fence};
}
} // namespace

TEST(WallHugRules, ATreeIsASmallCircleRoundItsTrunkWhateverItsSize)
{
	const auto circles = CirclesOf(Placed(ThingShape::Trunk, {6.0f, 10.0f, 6.0f}));
	ASSERT_EQ(circles.size(), 1U);
	EXPECT_FLOAT_EQ(circles[0].radius, k_TreeTrunkRadius);
	EXPECT_NEAR(circles[0].centre.x, 1000.0f, 0.001f);
	EXPECT_NEAR(circles[0].centre.y, 1000.0f, 0.001f);
	EXPECT_TRUE(CirclesOf(Placed(ThingShape::None, {6.0f, 10.0f, 6.0f})).empty());
}

TEST(WallHugRules, ALongModelBlocksWithEachCircleOfItsRowAndASquareOneWithOne)
{
	const auto square = CirclesOf(Placed(ThingShape::ModelBox, {4.0f, 3.0f, 5.0f}));
	ASSERT_EQ(square.size(), 1U);
	EXPECT_FLOAT_EQ(square[0].radius, 5.0f);
	// Five times as long as wide: a row of six circles as wide as the box, not its bounding circle
	const auto fence = CirclesOf(Placed(ThingShape::ModelBox, {10.0f, 1.0f, 2.0f}, true));
	ASSERT_EQ(fence.size(), 6U);
	for (const auto& circle : fence)
	{
		EXPECT_FLOAT_EQ(circle.radius, 2.0f);
		EXPECT_TRUE(circle.landscapeOrFence);
		EXPECT_FALSE(circle.landscape);
	}
	EXPECT_NEAR(fence.front().centre.x, 1000.0f - 10.0f + 20.0f / 12.0f, 0.01f);
}

TEST(WallHugRules, ATempleIsAWideCircleAndSevenSpokesWhateverItsModel)
{
	const auto circles = TempleRingCircles({1000.0f, 2000.0f}, 0.0f);
	// One over its middle, two bent spokes of six and five straight ones of four
	ASSERT_EQ(circles.size(), 33U);
	EXPECT_FLOAT_EQ(circles[0].radius, 21.5f * 0.7f);
	EXPECT_FLOAT_EQ(circles[0].centre.x, 1000.0f);
	EXPECT_FLOAT_EQ(circles[0].centre.y, 2000.0f);
	for (size_t i = 1; i < circles.size(); ++i)
	{
		EXPECT_NEAR(circles[i].radius, 21.5f * 0.11f * 1.4f, 1e-5f);
		EXPECT_FALSE(circles[i].landscapeOrFence);
	}
	const auto expectAt = [&circles](size_t i, double distance, double angle) {
		EXPECT_NEAR(circles[i].centre.x, 1000.0 + distance * std::cos(angle), 0.001) << i;
		EXPECT_NEAR(circles[i].centre.y, 2000.0 + distance * std::sin(angle), 0.001) << i;
	};
	const double first = 3.83;
	const double seventh = 2.0 * std::numbers::pi / 7.0;
	// The first spoke: three straight out, then three bent round one way, each further out
	expectAt(1, 15.91, first);
	expectAt(3, 24.51, first);
	expectAt(4, 28.81, first + 0.06);
	expectAt(5, 30.96, first + 0.16);
	expectAt(6, 32.465, first + 0.27);
	// The second bends the other way, towards the first
	expectAt(12, 32.465, first + seventh - 0.27);
	// The others are four circles straight out
	expectAt(13, 15.91, first + 2.0 * seventh);
	expectAt(32, 28.81, first + 6.0 * seventh);
}

TEST(WallHugRules, ATempleTurnsItsSpokesWithItsModel)
{
	const float angle = 1.0f;
	ThingOnMap temple = Placed(ThingShape::TempleRing, glm::vec3(0.0f));
	temple.rotation = glm::mat3(glm::vec3(std::cos(angle), 0.0f, std::sin(angle)), glm::vec3(0.0f, 1.0f, 0.0f),
	                            glm::vec3(-std::sin(angle), 0.0f, std::cos(angle)));
	const auto circles = CirclesOf(temple);
	const auto expected = TempleRingCircles({1000.0f, 1000.0f}, angle);
	ASSERT_EQ(circles.size(), expected.size());
	for (size_t i = 0; i < circles.size(); ++i)
	{
		EXPECT_NEAR(circles[i].centre.x, expected[i].centre.x, 0.001f);
		EXPECT_NEAR(circles[i].centre.y, expected[i].centre.y, 0.001f);
	}
}

TEST(WallHugRules, OnlyTheFencesModelsAreFences)
{
	EXPECT_TRUE(IsFenceModel(openblack::MeshId::BuildingAmericanFence));
	EXPECT_TRUE(IsFenceModel(openblack::MeshId::BuildingCelticFenceShort));
	EXPECT_TRUE(IsFenceModel(openblack::MeshId::BuildingCelticFenceTall));
	EXPECT_FALSE(IsFenceModel(openblack::MeshId::BuildingCeltic4));
}
