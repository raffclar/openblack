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
} // namespace

TEST(CreatureLocomotion, SpeedsGrowWithSize)
{
	const auto one = SpeedsFor(1.0f);
	EXPECT_NEAR(one.walk, 8.0f * 1.125f, k_Tolerance);
	EXPECT_NEAR(one.run, 20.0f * 1.125f, k_Tolerance);
	const auto two = SpeedsFor(2.0f);
	EXPECT_NEAR(two.walk, 8.0f * 2.0f, k_Tolerance);
	// The speeds do not keep the size within 0.05 and 4
	EXPECT_EQ(SpeedsFor(10.0f).walk, 8.0f * ((10.0f * 0.5f * 1.75f) + 0.25f));
	EXPECT_EQ(SpeedsFor(0.0f).walk, 8.0f * 0.25f);
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

// While it waits for its route, the creature fidgets when the wait falls below zero. The wait counts down by the body's
// clip step. The first fidget is a look about with no draw. Each later fidget draws one of three from the game's random
// numbers, and the confused clip draws its side after that.
TEST(CreatureLocomotion, TheWaitForARouteFidgetsWithTheGamesDraws)
{
	std::vector<uint32_t> ranges;
	std::vector<uint32_t> numbers;
	const Random game = [&ranges, &numbers](uint32_t range) {
		ranges.push_back(range);
		const auto number = numbers.front();
		numbers.erase(numbers.begin());
		return number;
	};
	FidgetWait wait;
	// At zero it does not fidget yet
	EXPECT_EQ(WaitTurn(wait, k_FidgetMs, true, game).fidget, Fidget::None);
	EXPECT_EQ(WaitTurn(wait, 1.0f, true, game).fidget, Fidget::LookAbout);
	EXPECT_TRUE(ranges.empty());
	EXPECT_FLOAT_EQ(wait.ms, k_LookAboutFidgetMs);
	// The look about gives a longer wait. Then a puzzled face.
	numbers = {1};
	EXPECT_EQ(WaitTurn(wait, k_LookAboutFidgetMs + 1.0f, true, game).fidget, Fidget::Puzzled);
	EXPECT_FLOAT_EQ(wait.ms, k_FidgetMs);
	// Then the confused clip, on the left side
	numbers = {2, 1};
	const auto confused = WaitTurn(wait, k_FidgetMs + 1.0f, true, game);
	EXPECT_EQ(confused.fidget, Fidget::Confused);
	EXPECT_TRUE(confused.mirrored);
	EXPECT_EQ(ranges, (std::vector<uint32_t> {3, 3, 2}));
	// A body that plays something else plays no confused clip and draws no side
	numbers = {2};
	EXPECT_EQ(WaitTurn(wait, k_FidgetMs + 1.0f, false, game).fidget, Fidget::None);
	EXPECT_EQ(ranges.size(), 4u);
	EXPECT_FLOAT_EQ(wait.ms, k_FidgetMs);
}

// A stop at once in the turn that a turn on the spot ended shows the body at its new heading at once, not at the old
// heading without the spin
TEST(CreatureLocomotion, AStopAfterATurnShowsTheNewHeadingAtOnce)
{
	const DrawnTurn ended {.from = GameHeading(-1.5f), .to = GameHeading(-1.5f), .snaps = true};
	const auto stopped = DrawnAfterStop(ended, GameHeading(1.5f));
	EXPECT_EQ(stopped.from, GameHeading(1.5f));
	EXPECT_EQ(stopped.to, GameHeading(1.5f));
	EXPECT_FALSE(stopped.snaps);
	// Any other turn is drawn as it was
	const DrawnTurn walking {.from = GameHeading(0.2f), .to = GameHeading(0.3f), .snaps = false};
	const auto kept = DrawnAfterStop(walking, GameHeading(0.3f));
	EXPECT_EQ(kept.from, GameHeading(0.2f));
	EXPECT_EQ(kept.to, GameHeading(0.3f));
}

TEST(CreatureLocomotion, ATurnThatWaitsForTheBodyStartsOnlyWhenTheBodyIsAtRest)
{
	// The hand takes hold of a creature at rest: the turn starts now
	EXPECT_EQ(TurnThatStartsAtRest(GameHeading(0.5f), false), GameHeading(0.5f));
	// The body plays an action, such as the finish of a fight: the turn waits
	EXPECT_FALSE(TurnThatStartsAtRest(GameHeading(0.5f), true).has_value());
	// No turn waits
	EXPECT_FALSE(TurnThatStartsAtRest(std::nullopt, false).has_value());
	EXPECT_FALSE(TurnThatStartsAtRest(std::nullopt, true).has_value());
}
