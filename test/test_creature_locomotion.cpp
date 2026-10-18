/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>
#include <numbers>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureLocomotion.h"

using namespace openblack;
using namespace openblack::creature_locomotion;

namespace
{
constexpr float k_Tolerance = 1e-4f;
constexpr float k_Pi = std::numbers::pi_v<float>;

float Degrees(float degrees)
{
	return degrees * k_Pi / 180.0f;
}

// A made-up species: a stand of 2 s, a walk of 2.4 s striding 70 mesh units, a run of 1 s striding 130
constexpr Cycle k_Stand {.durationMs = 2000.0f, .stride = 0.0f};
constexpr Cycle k_Walk {.durationMs = 2400.0f, .stride = 70.0f};
constexpr Cycle k_Run {.durationMs = 1000.0f, .stride = 130.0f};
} // namespace

TEST(CreatureLocomotion, SpeedsGrowWithSize)
{
	const auto one = SpeedsFor(1.0f);
	EXPECT_NEAR(one.walk, 8.0f * 1.125f, k_Tolerance);
	EXPECT_NEAR(one.run, 20.0f * 1.125f, k_Tolerance);
	const auto two = SpeedsFor(2.0f);
	EXPECT_NEAR(two.walk, 8.0f * 2.0f, k_Tolerance);
	// Sizes are kept within 0.05 and 4
	EXPECT_NEAR(SpeedsFor(10.0f).run, SpeedsFor(4.0f).run, k_Tolerance);
	EXPECT_NEAR(SpeedsFor(0.0f).walk, 8.0f * ((0.875f * 0.05f) + 0.25f), k_Tolerance);
}

TEST(CreatureLocomotion, TargetSpeedIsAFractionOfALittleOverRunning)
{
	EXPECT_NEAR(TargetSpeed(1.0f, 20.0f), 22.0f, k_Tolerance);
	EXPECT_NEAR(TargetSpeed(0.5f, 20.0f), 11.0f, k_Tolerance);
	EXPECT_NEAR(TargetSpeed(2.0f, 20.0f), 22.0f, k_Tolerance);
	EXPECT_NEAR(TargetSpeed(-1.0f, 20.0f), 0.0f, k_Tolerance);
}

TEST(CreatureLocomotion, ExhaustedCreaturesGoSlowly)
{
	EXPECT_NEAR(RequiredFraction(0.9f, 0.5f, 0.79f, false), 0.9f, k_Tolerance);
	EXPECT_NEAR(RequiredFraction(0.9f, 0.5f, 0.8f, false), 0.5f, k_Tolerance);
}

TEST(CreatureLocomotion, AnExhaustedCreatureStaysSlowAfterItRecovers)
{
	// Exhausted, the creature gets its slow fraction and keeps it
	auto kept = RequiredFraction(0.7f, 0.5f, 0.9f, false);
	EXPECT_NEAR(kept, 0.5f, k_Tolerance);
	// Rested again, it keeps the slow fraction until a new action sets a different speed
	kept = RequiredFraction(kept, 0.5f, 0.1f, false);
	EXPECT_NEAR(kept, 0.5f, k_Tolerance);
	EXPECT_NEAR(KeptFraction(kept, 0.7f), 0.5f, k_Tolerance);
}

TEST(CreatureLocomotion, AHeldCreatureDoesNotGetTheSlowFraction)
{
	EXPECT_NEAR(RequiredFraction(0.7f, 0.5f, 0.9f, true), 0.7f, k_Tolerance);
}

TEST(CreatureLocomotion, AWalkKeepsTheSpeedThatTheCreatureGotLast)
{
	// After a run, a walk that asks for no speed goes on at the run speed
	EXPECT_NEAR(KeptFraction(0.9f, 0.6f), 0.9f, k_Tolerance);
	// A pull on the leash gave it more than its run speed, and it keeps that too
	EXPECT_NEAR(KeptFraction(1.4f, 0.6f), 1.4f, k_Tolerance);
	// A creature that got no speed yet walks
	EXPECT_NEAR(KeptFraction(0.0f, 0.6f), 0.6f, k_Tolerance);
}

TEST(CreatureLocomotion, AcceleratesAtTwelveUnitsASecondEachSecond)
{
	EXPECT_NEAR(Accelerate(0.0f, 10.0f, 0.1f), 1.2f, k_Tolerance);
	EXPECT_NEAR(Accelerate(9.5f, 10.0f, 0.1f), 10.0f, k_Tolerance);
	// It never slows down towards a lower target by itself
	EXPECT_NEAR(Accelerate(15.0f, 10.0f, 0.1f), 15.0f, k_Tolerance);
	// From standing it takes nine turns to reach 10
	float speed = 0.0f;
	int turns = 0;
	while (speed < 10.0f)
	{
		speed = Accelerate(speed, 10.0f, 0.1f);
		++turns;
	}
	EXPECT_EQ(turns, 9);
}

TEST(CreatureLocomotion, BrakesToStopInTheDistanceLeft)
{
	EXPECT_NEAR(StopCap(6.0f), 12.0f, k_Tolerance);
	EXPECT_NEAR(StopCap(0.0f), k_MinSpeed, k_Tolerance);
	EXPECT_NEAR(CornerCap(1.0f, 2.0f), std::sqrt(24.0f + 24.0f), k_Tolerance);
	// Braking each turn to the cap stops it right at the end
	float remaining = 30.0f;
	float speed = 20.0f;
	while (remaining > 0.0f && speed > k_MinSpeed)
	{
		speed = std::min(speed, StopCap(remaining));
		remaining -= speed * 0.1f;
	}
	EXPECT_GT(remaining, -1.5f);
}

TEST(CreatureLocomotion, UphillIsSlowerDownhillALittleFaster)
{
	EXPECT_NEAR(SlopeFactor(10.0f, 10.0f, 5.0f), 1.0f, k_Tolerance);
	EXPECT_NEAR(SlopeFactor(12.5f, 10.0f, 5.0f), 0.7f, k_Tolerance);
	EXPECT_NEAR(SlopeFactor(100.0f, 10.0f, 5.0f), k_MinSlopeFactor, k_Tolerance);
	EXPECT_NEAR(SlopeFactor(0.0f, 10.0f, 5.0f), k_MaxSlopeFactor, k_Tolerance);
}

TEST(CreatureLocomotion, HeadingsFaceTheWayTheyPoint)
{
	for (const float heading : {0.0f, 1.0f, -2.0f, 3.0f})
	{
		EXPECT_NEAR(HeadingOf(DirectionOf(GameHeading(heading))).Radians(), heading, k_Tolerance);
	}
	// Heading 0 looks along -z, as the meshes do, and a quarter turn looks along +x
	EXPECT_NEAR(DirectionOf(GameHeading()).y, -1.0f, k_Tolerance);
	EXPECT_NEAR(DirectionOf(GameHeading(k_Pi / 2.0f)).x, 1.0f, k_Tolerance);
	// Halfway round the short way, through 180 degrees
	EXPECT_NEAR(std::abs(LerpHeading(GameHeading(Degrees(170.0f)), GameHeading(Degrees(-170.0f)), 500.0f, 1000.0f).Radians()),
	            k_Pi, k_Tolerance);
	EXPECT_NEAR(LerpHeading(GameHeading(0.0f), GameHeading(1.0f), 2000.0f, 1000.0f).Radians(), 1.0f, k_Tolerance);
}

TEST(CreatureLocomotion, SlowerThanWalkingBlendsStandAndWalk)
{
	const Speeds speeds {.walk = 9.0f, .run = 22.5f};
	const auto gait = BlendGait(4.5f, speeds, 0.45f, 0.2f, k_Stand, k_Walk, k_Run, 0.0f, 0.25f);
	EXPECT_EQ(gait.slots[0].animation, animations::k_Stand);
	EXPECT_EQ(gait.slots[1].animation, animations::k_Walk);
	EXPECT_NEAR(gait.slots[0].weight, 0.5f, k_Tolerance);
	EXPECT_NEAR(gait.slots[1].weight, 0.5f, k_Tolerance);
	// The stand follows the breath
	EXPECT_NEAR(gait.slots[0].timeMs, 500.0f, k_Tolerance);
	// The blend's stride is half the walk's: 0.5 * 70 * 0.2 = 7 units, so 0.45 units is 0.45 / 7 of the walk
	EXPECT_NEAR(gait.walkAdvanceMs, 2400.0f * 0.45f / 7.0f, 1e-2f);
	EXPECT_NEAR(gait.slots[1].timeMs, gait.walkAdvanceMs, 1e-2f);
}

TEST(CreatureLocomotion, FasterThanWalkingBlendsWalkAndRunInStep)
{
	const Speeds speeds {.walk = 10.0f, .run = 20.0f};
	const auto gait = BlendGait(15.0f, speeds, 1.5f, 0.1f, k_Stand, k_Walk, k_Run, 2000.0f, 0.0f);
	EXPECT_EQ(gait.slots[0].animation, animations::k_Run);
	EXPECT_EQ(gait.slots[1].animation, animations::k_Walk);
	EXPECT_NEAR(gait.slots[0].weight, 0.5f, k_Tolerance);
	EXPECT_NEAR(gait.slots[1].weight, 0.5f, k_Tolerance);
	// Stride (0.5 * 70 + 0.5 * 130) * 0.1 = 10 units, so 1.5 units is 15% of the walk, which loops
	EXPECT_NEAR(gait.walkAdvanceMs, 360.0f, 1e-2f);
	EXPECT_NEAR(gait.walkTimeMs, 2360.0f, 1e-2f);
	// The run keeps the walk's phase
	EXPECT_NEAR(gait.slots[0].timeMs / k_Run.durationMs, gait.slots[1].timeMs / k_Walk.durationMs, k_Tolerance);
	// Past running speed the weights stay within 0 and 1
	const auto flatOut = BlendGait(22.0f, speeds, 2.2f, 0.1f, k_Stand, k_Walk, k_Run, 0.0f, 0.0f);
	EXPECT_NEAR(flatOut.slots[0].weight, 1.0f, k_Tolerance);
	EXPECT_NEAR(flatOut.slots[1].weight, 0.0f, k_Tolerance);
}

TEST(CreatureLocomotion, StandingStillMovesNoFeet)
{
	const auto gait = BlendGait(0.0f, {.walk = 9.0f, .run = 22.0f}, 0.0f, 0.2f, k_Stand, k_Walk, k_Run, 300.0f, 0.5f);
	EXPECT_NEAR(gait.slots[0].weight, 1.0f, k_Tolerance);
	EXPECT_NEAR(gait.walkTimeMs, 300.0f, k_Tolerance);
	EXPECT_NEAR(gait.walkAdvanceMs, 0.0f, k_Tolerance);
}

TEST(CreatureLocomotion, AnglesPickTheirPairOfAnimations)
{
	const auto ahead = PairFor(animations::k_RightSpin, 0.0f);
	EXPECT_EQ(ahead.from, animations::k_RightSpin);
	EXPECT_NEAR(ahead.weight, 0.0f, k_Tolerance);
	const auto side = PairFor(animations::k_RightSpin, Degrees(45.0f));
	EXPECT_EQ(side.from, animations::k_RightSpin);
	EXPECT_EQ(side.to, animations::k_RightSpin + 1);
	EXPECT_NEAR(side.weight, 0.5f, k_Tolerance);
	const auto behind = PairFor(animations::k_RightSpin, Degrees(-135.0f));
	EXPECT_EQ(behind.from, animations::k_RightSpin + 1);
	EXPECT_EQ(behind.to, animations::k_RightSpin + 2);
	EXPECT_NEAR(behind.weight, 0.5f, k_Tolerance);
}

TEST(CreatureLocomotion, StartsByStepWalkOrTurn)
{
	const StartOptions steps {.stepStrides = std::array {46.0f, 42.0f, 40.0f}, .hasSpins = true, .moving = true};
	// Far enough for every step: it steps off, right for negative angles
	const auto step = ChooseStart(Degrees(-100.0f), 50.0f, steps);
	EXPECT_EQ(step.kind, Start::Kind::Step);
	EXPECT_EQ(step.side, Side::Right);
	ASSERT_TRUE(step.animations.has_value());
	EXPECT_EQ(step.animations->from, animations::k_RightStep + 1);
	const auto left = ChooseStart(Degrees(30.0f), 50.0f, steps);
	EXPECT_EQ(left.side, Side::Left);
	EXPECT_EQ(left.animations->from, animations::k_LeftStep);
	// Too close for a step: nearly facing it walks straight on, otherwise turns on the spot
	EXPECT_EQ(ChooseStart(Degrees(10.0f), 45.0f, steps).kind, Start::Kind::Walk);
	const auto turn = ChooseStart(Degrees(60.0f), 45.0f, steps);
	EXPECT_EQ(turn.kind, Start::Kind::Turn);
	EXPECT_EQ(turn.animations->from, animations::k_LeftSpin);
	// Without spins the turn has no animations
	EXPECT_FALSE(ChooseStart(Degrees(60.0f), 10.0f, {.stepStrides = std::nullopt, .hasSpins = false, .moving = true})
	                 .animations.has_value());
	// Only turning to face, it never steps
	EXPECT_EQ(ChooseStart(Degrees(100.0f), 500.0f, {.stepStrides = steps.stepStrides, .hasSpins = true, .moving = false}).kind,
	          Start::Kind::Turn);
}

TEST(CreatureLocomotion, ArrivalRings)
{
	const auto ring = MoveRing(50.0f, 0.0f, 1.0f);
	EXPECT_NEAR(ring.min, 0.0f, k_Tolerance);
	EXPECT_NEAR(ring.max, 1.0f, k_Tolerance);
	// Never asked to stop beyond where it already is, and never thinner than a hair
	const auto close = MoveRing(3.0f, 10.0f, 2.0f);
	EXPECT_NEAR(close.min, 2.95f, k_Tolerance);
	EXPECT_NEAR(close.max, 2.951f, k_Tolerance);
	const auto object = ObjectRing(5.0f, 2.0f, 3.0f);
	EXPECT_NEAR(object.min, 7.0f, k_Tolerance);
	EXPECT_NEAR(object.max, 10.0f, k_Tolerance);
	EXPECT_TRUE(Arrived(9.0f, object));
	EXPECT_FALSE(Arrived(10.5f, object));
	EXPECT_NEAR(TimeLimitMs(100.0f), 5200.0f, k_Tolerance);
}

namespace
{
/// A creature in the middle of map cell (100, 100)
constexpr map_coords::MapCoords k_InCell {.x = (100 << 16) | 0x8000, .z = (100 << 16) | 0x8000, .altitude = 2.0f};

RunAwayLand OpenLand(float ground = 7.0f)
{
	return {.cellClear = [](int32_t, int32_t) { return true; },
	        .ground = [ground](const map_coords::MapCoords&) { return ground; },
	        .canStand = [](glm::vec2) { return true; },
	        .nearestStand = [](glm::vec2) { return std::optional<glm::vec2>(); }};
}
} // namespace

TEST(CreatureLocomotion, RunsAwayToTheCornerOfTheCellItAimsAt)
{
	// The way from the threat to the cell corner (1000, 1000) is along x; 10 + 40 units on is cell 105
	const auto point = RunAwayPoint(k_InCell, {950.0f, 1000.0f}, 10, 40.0f, OpenLand());
	EXPECT_EQ(point, glm::vec3(1050.0f, 7.0f, 1000.0f));
	// A threat on the corner sends it along x too
	EXPECT_EQ(RunAwayPoint(k_InCell, {1000.0f, 1000.0f}, 0, 20.0f, OpenLand()), glm::vec3(1020.0f, 7.0f, 1000.0f));
}

TEST(CreatureLocomotion, RunsAwayToTheFirstClearCellOfTheSpiral)
{
	auto land = OpenLand();
	// The aimed cell is not clear; the spiral's first step goes one cell back along x
	land.cellClear = [](int32_t x, int32_t z) { return !(x == 105 && z == 100); };
	EXPECT_EQ(RunAwayPoint(k_InCell, {950.0f, 1000.0f}, 10, 40.0f, land), glm::vec3(1040.0f, 7.0f, 1000.0f));
	// With no clear cell it stays where it is, at its own height above the ground
	land.cellClear = [](int32_t, int32_t) { return false; };
	EXPECT_EQ(RunAwayPoint(k_InCell, {950.0f, 1000.0f}, 10, 40.0f, land), glm::vec3(1005.0f, 9.0f, 1005.0f));
}

TEST(CreatureLocomotion, RunsAwayToTheNearestPlaceToStandOnlyWhenNearEnough)
{
	auto land = OpenLand(3.0f);
	land.canStand = [](glm::vec2 at) { return at.x < 1045.0f; };
	land.nearestStand = [](glm::vec2) { return std::optional<glm::vec2>(glm::vec2(1040.0f, 1000.0f)); };
	EXPECT_EQ(RunAwayPoint(k_InCell, {950.0f, 1000.0f}, 10, 40.0f, land), glm::vec3(1040.0f, 0.0f, 1000.0f));
	// The search's point has no height, so high ground puts it out of reach
	land.ground = [](const map_coords::MapCoords&) { return 40.0f; };
	EXPECT_EQ(RunAwayPoint(k_InCell, {950.0f, 1000.0f}, 10, 40.0f, land), glm::vec3(1050.0f, 40.0f, 1000.0f));
}

TEST(CreatureLocomotion, RunsAwayWithNoPlaceToStandTestsWhatTheSearchLeft)
{
	auto land = OpenLand();
	std::vector<glm::vec2> tested;
	land.canStand = [&tested](glm::vec2 at) {
		tested.push_back(at);
		return false;
	};
	EXPECT_EQ(RunAwayPoint(k_InCell, {950.0f, 1000.0f}, 10, 40.0f, land), glm::vec3(1050.0f, 7.0f, 1000.0f));
	ASSERT_EQ(tested.size(), 2u);
	// The cell across as the bits of a float, and the distance along z
	EXPECT_EQ(std::bit_cast<uint32_t>(tested[1].x), 100u);
	EXPECT_EQ(tested[1].y, 0.0f);
}

TEST(CreatureLocomotion, ATurnOnTheSpotOfNoneUsesTheSpinsToTheRight)
{
	EXPECT_EQ(SpinSideOf(0.0f), Side::Right);
	EXPECT_EQ(SpinSideOf(-0.5f), Side::Right);
	EXPECT_EQ(SpinSideOf(0.5f), Side::Left);
}

TEST(CreatureLocomotion, TheSpinBlendMultipliesTheTurnByTheShareOfAQuarterTurn)
{
	// The game makes the turn positive, then multiplies by its float of 2 / pi, after it takes away a quarter turn
	// beyond a quarter turn. It does not divide by a quarter turn.
	constexpr float k_Share = std::bit_cast<float>(0x3F22F983u);
	constexpr float k_Quarter = std::bit_cast<float>(0x3FC90FDBu);
	for (const float angle : {0.0f, -0.3f, 0.3f, 1.2f, -1.5707963f, 1.5707964f, 2.5f, -3.1415927f, 3.1415927f})
	{
		const auto pair = SpinPairFor(animations::k_RightSpin, angle);
		const float turn = angle <= 0.0f ? std::bit_cast<float>(std::bit_cast<uint32_t>(angle) ^ 0x80000000u) : angle;
		const float expected = turn < k_Quarter ? turn * k_Share : (turn - k_Quarter) * k_Share;
		EXPECT_EQ(std::bit_cast<uint32_t>(pair.weight), std::bit_cast<uint32_t>(expected)) << angle;
		EXPECT_EQ(pair.from, turn < k_Quarter ? animations::k_RightSpin : animations::k_RightSpin + 1) << angle;
	}
	// A turn of +0 is made -0, as the game changes the sign of every turn of 0 or less
	EXPECT_EQ(std::bit_cast<uint32_t>(SpinPairFor(animations::k_RightSpin, 0.0f).weight), 0x80000000u);
}

TEST(CreatureLocomotion, ATurnWithoutSpinsClosesTheGapByTheShareOfTheTimeBeforeEachStep)
{
	// From 0 to 1 over 300 ms in steps of 100 ms: no change, then a third, then two thirds of what is left
	const GameHeading target(1.0f);
	auto heading = GameHeading(0.0f);
	heading = LerpHeading(heading, target, 0.0f, 300.0f);
	EXPECT_NEAR(heading.Radians(), 0.0f, k_Tolerance);
	heading = LerpHeading(heading, target, 100.0f, 300.0f);
	EXPECT_NEAR(heading.Radians(), 1.0f / 3.0f, k_Tolerance);
	heading = LerpHeading(heading, target, 200.0f, 300.0f);
	EXPECT_NEAR(heading.Radians(), (1.0f / 3.0f) + ((2.0f / 3.0f) * (2.0f / 3.0f)), k_Tolerance);
	EXPECT_NEAR(LerpHeading(heading, target, 300.0f, 300.0f).Radians(), 1.0f, k_Tolerance);
	// The step is the gap wrapped once, times the time, over the length, plus the heading, all as floats, wrapped once
	const GameHeading from(3.0f);
	const GameHeading to(-3.0f);
	const float gap = GameHeading::Wrap(to.Radians() - from.Radians());
	const float expected = GameHeading::Wrap(((gap * 100.0f) / 300.0f) + from.Radians());
	EXPECT_EQ(std::bit_cast<uint32_t>(LerpHeading(from, to, 100.0f, 300.0f).Radians()), std::bit_cast<uint32_t>(expected));
}
