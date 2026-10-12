/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <limits>

#include <gtest/gtest.h>

#include "Help/TurnCheer.h"

using namespace openblack::help::turn_cheer;

namespace
{
/// Random numbers that always come out the same, counting the draws
struct FixedDraws
{
	float chance {0.0f};
	uint32_t pick {0};
	int floatDraws {0};
	int wholeDraws {0};

	Random Make()
	{
		return {
		    .upTo =
		        [this](float x) {
			        ++floatDraws;
			        return chance * x;
		        },
		    .below =
		        [this](int32_t) {
			        ++wholeDraws;
			        return pick;
		        },
		};
	}
};

Cheering Started()
{
	Cheering cheering;
	Start(cheering);
	return cheering;
}
} // namespace

TEST(TurnCheer, SpeedMovesTwoFifthsOfTheWayToTheFramesSpeed)
{
	// A tenth of a radian in a tenth of a second is a radian a second
	EXPECT_FLOAT_EQ(SmoothSpeed(0.0f, 0.1f, 0.1f), 0.4f);
	EXPECT_FLOAT_EQ(SmoothSpeed(1.0f, 0.0f, 0.1f), 0.6f);
	EXPECT_FLOAT_EQ(SmoothSpeed(5.0f, 0.5f, 0.1f), 5.0f);
}

TEST(TurnCheer, NothingWhileStopped)
{
	Cheering cheering;
	FixedDraws draws;
	EXPECT_FALSE(ProcessTurn(cheering, k_FullSpeed, 500, draws.Make()).has_value());
	Start(cheering);
	Stop(cheering);
	EXPECT_FALSE(ProcessTurn(cheering, k_FullSpeed, 500, draws.Make()).has_value());
	EXPECT_EQ(draws.floatDraws, 0);
}

TEST(TurnCheer, FlatOutEarnsTheBestPraiseEveryHundredTurns)
{
	auto cheering = Started();
	FixedDraws draws;
	// The turns count from the start: the best praise waits for more than a hundred
	EXPECT_FALSE(ProcessTurn(cheering, 10.0f, 100, draws.Make()).has_value());
	EXPECT_EQ(ProcessTurn(cheering, 10.0f, 101, draws.Make()), k_BestPraise);
	// Flat out but within the wait, nothing at all: no cheers either
	EXPECT_FALSE(ProcessTurn(cheering, -k_FullSpeed, 150, draws.Make()).has_value());
	EXPECT_FALSE(ProcessTurn(cheering, k_FullSpeed, 201, draws.Make()).has_value());
	EXPECT_EQ(ProcessTurn(cheering, -k_FullSpeed, 202, draws.Make()), k_BestPraise);
	EXPECT_EQ(draws.floatDraws, 0);
}

TEST(TurnCheer, ASpeedThatIsntANumberCountsAsFlatOut)
{
	auto cheering = Started();
	FixedDraws draws;
	EXPECT_EQ(ProcessTurn(cheering, std::numeric_limits<float>::quiet_NaN(), 101, draws.Make()), k_BestPraise);
}

TEST(TurnCheer, SlowerTurningCheersByChanceAfterTwentyTurns)
{
	auto cheering = Started();
	FixedDraws draws {.chance = 0.4f, .pick = 13};
	const float halfSpeed = k_FullSpeed * 0.5f;
	// Too soon after the start: no draw
	EXPECT_FALSE(ProcessTurn(cheering, halfSpeed, 20, draws.Make()).has_value());
	EXPECT_EQ(draws.floatDraws, 0);
	// A chance under the share of full speed cheers, picking among the cheers
	EXPECT_EQ(ProcessTurn(cheering, halfSpeed, 21, draws.Make()), k_FirstCheer + 13);
	EXPECT_EQ(draws.wholeDraws, 1);
	// Then waits again
	EXPECT_FALSE(ProcessTurn(cheering, halfSpeed, 41, draws.Make()).has_value());
	// Turning the other way counts the same
	EXPECT_EQ(ProcessTurn(cheering, -halfSpeed, 42, draws.Make()), k_FirstCheer + 13);
	// A chance not under the share, nothing, and the wait isn't started again
	draws.chance = 0.5f;
	EXPECT_FALSE(ProcessTurn(cheering, halfSpeed, 63, draws.Make()).has_value());
	EXPECT_EQ(cheering.lastCheerTurn, 42u);
}

TEST(TurnCheer, StandingStillDrawsNothing)
{
	auto cheering = Started();
	FixedDraws draws;
	EXPECT_FALSE(ProcessTurn(cheering, 0.0f, 500, draws.Make()).has_value());
	EXPECT_EQ(draws.floatDraws, 0);
}

TEST(TurnCheer, StartingAgainForgetsTheTurns)
{
	auto cheering = Started();
	FixedDraws draws;
	EXPECT_EQ(ProcessTurn(cheering, k_FullSpeed, 1000, draws.Make()), k_BestPraise);
	Start(cheering);
	EXPECT_EQ(cheering.lastBestPraiseTurn, 0u);
	EXPECT_EQ(ProcessTurn(cheering, k_FullSpeed, 1001, draws.Make()), k_BestPraise);
}
