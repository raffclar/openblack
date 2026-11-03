/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "ECS/FishPuzzleRules.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// A draw that always gives the middle of its range, and counts the draws
struct MiddleRandom
{
	int* draws;
	float operator()(float low, float high) const
	{
		++*draws;
		return (low + high) * 0.5f;
	}
};

constexpr glm::vec3 k_Middle {2497.875f, 0.0f, 3628.25f};

/// Puts all the fish of both shoals at a point, so that they stay near it for a while
void PutAllFishAt(fish_puzzle::FishNet& puzzle, glm::vec3 point)
{
	for (auto& shoal : puzzle.shoals)
	{
		shoal.centre = point;
		shoal.target = point;
		shoal.retargetSeconds = 100.0f;
		for (auto& fish : shoal.fish)
		{
			fish.position = point;
			fish.speed = 0.0f;
			fish.turnRate = 0.0f;
		}
	}
}
} // namespace

TEST(FishPuzzleRules, TheShoalsStartEastAndWestOfTheRingAndNorthOfIt)
{
	int draws = 0;
	const auto puzzle = fish_puzzle::Make(k_Middle, MiddleRandom {&draws});
	EXPECT_EQ(puzzle.shoals.at(0).centre, k_Middle + glm::vec3(12.0f, 0.0f, 12.0f));
	EXPECT_EQ(puzzle.shoals.at(1).centre, k_Middle + glm::vec3(-12.0f, 0.0f, 12.0f));
	EXPECT_EQ(puzzle.net.middle, k_Middle);
	EXPECT_FALSE(puzzle.net.caught);
	EXPECT_FLOAT_EQ(puzzle.net.closing, 1.0f);
	// Each fish takes ten draws, the fish of the east shoal first
	EXPECT_EQ(draws, 2 * 15 * 10);
	for (const auto& shoal : puzzle.shoals)
	{
		EXPECT_FLOAT_EQ(shoal.fullness, 1.0f);
		EXPECT_EQ(shoal.target, shoal.centre);
	}
}

TEST(FishPuzzleRules, OnlyTheFishInTheRingAcrossTheSeaCount)
{
	int draws = 0;
	auto puzzle = fish_puzzle::Make(k_Middle, MiddleRandom {&draws});
	auto& shoal = puzzle.shoals.at(0);
	for (auto& fish : shoal.fish)
	{
		fish.position = k_Middle + glm::vec3(11.0f, 0.0f, 0.0f);
	}
	EXPECT_EQ(fish_puzzle::FishInNet(shoal, puzzle.net), 0u);
	shoal.fish.at(0).position = k_Middle + glm::vec3(10.9f, -50.0f, 0.0f);
	EXPECT_EQ(fish_puzzle::FishInNet(shoal, puzzle.net), 1u);
	// A fish that does not show does not count
	shoal.fullness = 0.0f;
	EXPECT_EQ(fish_puzzle::FishInNet(shoal, puzzle.net), 0u);
}

TEST(FishPuzzleRules, TheFishAreCaughtWhenAllStayInTheRingForHalfASecond)
{
	int draws = 0;
	auto puzzle = fish_puzzle::Make(k_Middle, MiddleRandom {&draws});
	PutAllFishAt(puzzle, k_Middle);
	for (int turn = 0; turn < 4; ++turn)
	{
		EXPECT_TRUE(fish_puzzle::StepTurn(puzzle, 100, std::nullopt, MiddleRandom {&draws}).empty());
		EXPECT_FALSE(puzzle.net.caught);
	}
	EXPECT_EQ(puzzle.net.heldMilliseconds, 400u);
	const auto rings = fish_puzzle::StepTurn(puzzle, 100, std::nullopt, MiddleRandom {&draws});
	EXPECT_TRUE(puzzle.net.caught);
	// Each fish of both shoals makes a ring on the water
	EXPECT_EQ(rings.size(), 30u);
}

TEST(FishPuzzleRules, AFishOutOfTheRingStartsTheWaitAgain)
{
	int draws = 0;
	auto puzzle = fish_puzzle::Make(k_Middle, MiddleRandom {&draws});
	PutAllFishAt(puzzle, k_Middle);
	(void)fish_puzzle::StepTurn(puzzle, 100, std::nullopt, MiddleRandom {&draws});
	(void)fish_puzzle::StepTurn(puzzle, 100, std::nullopt, MiddleRandom {&draws});
	EXPECT_EQ(puzzle.net.heldMilliseconds, 200u);
	puzzle.shoals.at(1).fish.at(14).position = k_Middle + glm::vec3(0.0f, 0.0f, 20.0f);
	(void)fish_puzzle::StepTurn(puzzle, 100, std::nullopt, MiddleRandom {&draws});
	EXPECT_EQ(puzzle.net.heldMilliseconds, 0u);
	EXPECT_FALSE(puzzle.net.caught);
}

TEST(FishPuzzleRules, CaughtFishDoNotSwimAndTheFloatsCloseIn)
{
	int draws = 0;
	auto puzzle = fish_puzzle::Make(k_Middle, MiddleRandom {&draws});
	PutAllFishAt(puzzle, k_Middle);
	puzzle.net.caught = true;
	for (auto& shoal : puzzle.shoals)
	{
		for (auto& fish : shoal.fish)
		{
			fish.speed = 1.0f;
		}
	}
	const auto before = puzzle.shoals.at(0).fish.at(0).position;
	draws = 0;
	EXPECT_TRUE(fish_puzzle::StepTurn(puzzle, 100, k_Middle, MiddleRandom {&draws}).empty());
	EXPECT_EQ(puzzle.shoals.at(0).fish.at(0).position, before);
	EXPECT_EQ(draws, 0);
	// Two steps of the ring in the turn, each closing by two times its seconds
	EXPECT_NEAR(puzzle.net.closing, 0.6f, 1e-6f);
	EXPECT_NEAR(puzzle.net.bobTime, 0.4f, 1e-6f);
	(void)fish_puzzle::StepTurn(puzzle, 100, std::nullopt, MiddleRandom {&draws});
	(void)fish_puzzle::StepTurn(puzzle, 100, std::nullopt, MiddleRandom {&draws});
	EXPECT_FLOAT_EQ(puzzle.net.closing, 0.0f);
}

TEST(FishPuzzleRules, TheFloatsStandInARingThatClosesToOneMetre)
{
	fish_puzzle::Net net {.middle = k_Middle};
	auto points = fish_puzzle::FloatPoints(net);
	ASSERT_EQ(points.size(), 7u);
	for (const auto& point : points)
	{
		EXPECT_NEAR(glm::distance(glm::vec2(point.x, point.z), glm::vec2(k_Middle.x, k_Middle.z)), 11.0f, 1e-3f);
		EXPECT_LE(std::abs(point.y - k_Middle.y), 0.5f);
	}
	// The first float is east of the middle, and bobs by the cosine of the time
	EXPECT_NEAR(points.at(0).x, k_Middle.x + 11.0f, 1e-3f);
	EXPECT_NEAR(points.at(0).y, 0.5f, 1e-6f);
	net.closing = 0.0f;
	points = fish_puzzle::FloatPoints(net);
	for (const auto& point : points)
	{
		EXPECT_NEAR(glm::distance(glm::vec2(point.x, point.z), glm::vec2(k_Middle.x, k_Middle.z)), 1.0f, 1e-3f);
	}
}

TEST(FishPuzzleRules, AScareSendsTheShoalsOffAndTheNearFishDartAway)
{
	int draws = 0;
	auto puzzle = fish_puzzle::Make(k_Middle, MiddleRandom {&draws});
	const auto& fish = puzzle.shoals.at(0).fish.at(0);
	const auto scare = fish.position + glm::vec3(-1.0f, 0.0f, 0.0f);
	(void)fish_puzzle::StepTurn(puzzle, 100, scare, MiddleRandom {&draws});
	EXPECT_GT(fish.panic, 1.8f);
	EXPECT_FLOAT_EQ(puzzle.shoals.at(0).retargetSeconds, 2.0f);
}
