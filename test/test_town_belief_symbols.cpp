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

#include <gtest/gtest.h>

#include "Particles/TownBeliefMaths.h"

using namespace openblack::particles::town_belief;

namespace
{
/// The numbers of the game's own effect file
Params FileParams()
{
	return {.radiusAt0 = 0.3f,
	        .radiusAt1 = 0.6f,
	        .speedAt0 = 1.0f,
	        .speedAt1 = 2.0f,
	        .scaleAt0 = 0.5f,
	        .scaleAt1 = 1.0f,
	        .heightAt1 = 2.0f,
	        .heightPerLevel = 2.5f,
	        .useFixedScale = true,
	        .fixedScale = 0.01f,
	        .timeBetweenFightsAt0 = 10.0f,
	        .timeBetweenFightsAt1 = 3.0f,
	        .fightDurationAt0 = 1.5f,
	        .fightDurationAt1 = 3.0f,
	        .speedUpDuringFight = 2.0f,
	        .fightFracWhenEmitting = 0.2f,
	        .angleSpeedPhaseSpeed = 0.2f};
}

/// Always the middle of the range asked for
float Middle(float min, float max)
{
	return (min + max) * 0.5f;
}
} // namespace

TEST(TownBeliefSymbols, BeliefIsShownBetweenNoneAndAll)
{
	EXPECT_FLOAT_EQ(Shown(-0.5f), 0.0f);
	EXPECT_FLOAT_EQ(Shown(0.4f), 0.4f);
	EXPECT_FLOAT_EQ(Shown(9.2f), 1.0f);
}

TEST(TownBeliefSymbols, RankCountsThePlayersBelievedInMoreTiesGoingToTheLater)
{
	std::array<float, k_PlayerCount> beliefs {};
	beliefs[0] = 0.1f;
	beliefs[7] = 0.3f;
	EXPECT_EQ(Rank(beliefs, 7), 0);
	EXPECT_EQ(Rank(beliefs, 0), 1);
	// Six players believed in as little as player two (nothing): all later ones rank above, and players one and eight
	EXPECT_EQ(Rank(beliefs, 1), 7);
	// Over all, belief is capped at one: two players over it tie, the later first
	beliefs[0] = 4.0f;
	beliefs[7] = 2.0f;
	EXPECT_EQ(Rank(beliefs, 7), 0);
	EXPECT_EQ(Rank(beliefs, 0), 1);
}

TEST(TownBeliefSymbols, TheWayRoundGrowsWithTheCameraDistanceUpToTenTimes)
{
	const auto params = FileParams();
	EXPECT_FLOAT_EQ(DistanceScale(params, 40.0f), 1.0f);
	EXPECT_FLOAT_EQ(DistanceScale(params, 350.0f), 3.5f);
	EXPECT_FLOAT_EQ(DistanceScale(params, 5000.0f), 10.0f);
	auto unfixed = params;
	unfixed.useFixedScale = false;
	EXPECT_FLOAT_EQ(DistanceScale(unfixed, 5000.0f), 1.0f);
}

TEST(TownBeliefSymbols, TheMostBelievedSitsStillOverTheTotemSizedByItsBelief)
{
	const auto params = FileParams();
	Symbol symbol {.around = 1.0f, .tilt = 0.5f};
	const auto look = Step(symbol, params, 0.6f, 0, 0.1f, 1.0f, Middle);
	EXPECT_FLOAT_EQ(look.scale, 0.5f + 0.5f * 0.6f);
	EXPECT_FLOAT_EQ(look.offset.x, 0.0f);
	EXPECT_FLOAT_EQ(look.offset.y, 0.0f);
	EXPECT_FLOAT_EQ(look.offset.z, 0.0f);
	EXPECT_FLOAT_EQ(symbol.around, 1.0f);
	EXPECT_FLOAT_EQ(look.fight, 0.0f);
}

TEST(TownBeliefSymbols, ASymbolOfNoBeliefIsNotSeen)
{
	Symbol symbol;
	EXPECT_FLOAT_EQ(Step(symbol, FileParams(), 0.0f, 0, 0.1f, 1.0f, Middle).scale, 0.0f);
	EXPECT_FLOAT_EQ(Step(symbol, FileParams(), 0.0f, 3, 0.1f, 1.0f, Middle).scale, 0.0f);
}

TEST(TownBeliefSymbols, AnotherCirclesALevelUpForEachPlayerBelievedInMore)
{
	const auto params = FileParams();
	Symbol symbol;
	const float dt = 0.1f;
	const auto look = Step(symbol, params, 0.5f, 2, dt, 2.0f, Middle);
	const float radius = 0.45f;
	const float speed = 1.5f / radius;
	EXPECT_FLOAT_EQ(symbol.around, speed * dt);
	EXPECT_FLOAT_EQ(symbol.sway, dt * 0.2f);
	const float swayed = (std::cos(dt * 0.2f) + 1.0f) * 0.25f + 0.5f;
	EXPECT_NEAR(symbol.tilt, swayed * speed * dt * k_TiltShare, 1e-6f);
	EXPECT_NEAR(look.offset.x, std::cos(symbol.around) * std::cos(symbol.tilt) * radius * 2.0f, 1e-5f);
	EXPECT_NEAR(look.offset.y, (2.0f * 2.5f + std::sin(symbol.tilt) * radius) * 2.0f, 1e-5f);
	EXPECT_NEAR(look.offset.z, std::sin(symbol.around) * std::cos(symbol.tilt) * radius * 2.0f, 1e-5f);
}

TEST(TownBeliefSymbols, TheSecondFightsTheFirstNowAndThenComingDownToItsLevel)
{
	const auto params = FileParams();
	Symbol symbol;
	// The first step sets the clock: the wait and the length, each the usual times the random share
	auto look = Step(symbol, params, 0.0f, 1, 0.1f, 1.0f, Middle);
	EXPECT_FLOAT_EQ(symbol.untilFight, 10.0f);
	EXPECT_FLOAT_EQ(symbol.fightLength, 1.5f);
	EXPECT_FLOAT_EQ(look.fight, 0.0f);
	// Waiting out the next fight
	symbol.untilFight = 0.05f;
	look = Step(symbol, params, 0.0f, 1, 0.1f, 1.0f, Middle);
	EXPECT_FLOAT_EQ(look.fight, 0.0f);
	EXPECT_LE(symbol.untilFight, 0.0f);
	// Halfway into the fight it is at its height, down at the first's level
	symbol.intoFight = 0.65f;
	look = Step(symbol, params, 0.0f, 1, 0.1f, 1.0f, Middle);
	EXPECT_NEAR(look.fight, 1.0f - std::pow(0.75f / 1.5f * 2.0f - 1.0f, 2.0f), 1e-5f);
	EXPECT_NEAR(look.fight, 1.0f, 1e-5f);
	EXPECT_NEAR(look.offset.y, std::sin(symbol.tilt) * 0.3f, 1e-5f);
	EXPECT_TRUE(Sparking(params, look.fight));
	EXPECT_FALSE(Sparking(params, 0.2f));
	// The sparks swell again over the fight's swell: strongest on its way up and down, gone at its height
	EXPECT_EQ(SparkAlpha(0.5f), 255);
	EXPECT_EQ(SparkAlpha(look.fight), 0);
	EXPECT_EQ(SparkAlpha(0.0f), 0);
}

TEST(TownBeliefSymbols, AFightingSymbolCirclesFaster)
{
	const auto params = FileParams();
	Symbol calm {.untilFight = 5.0f};
	Symbol fighting {.untilFight = 0.0f, .intoFight = 0.65f, .fightLength = 1.5f};
	const auto calmLook = Step(calm, params, 0.0f, 1, 0.1f, 1.0f, Middle);
	EXPECT_FLOAT_EQ(calmLook.fight, 0.0f);
	const auto look = Step(fighting, params, 0.0f, 1, 0.1f, 1.0f, Middle);
	EXPECT_NEAR(fighting.around, calm.around * ((2.0f - 1.0f) * look.fight + 1.0f), 1e-5f);
}
