/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <numbers>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureSway.h"

using namespace openblack;
using namespace openblack::creature_sway;

TEST(CreatureSway, AKickHighOnTheBodySwaysTheUpperBodyAcrossTheLand)
{
	Sway sway;
	sway.turnSeconds = 0.05f;
	Kick(sway, glm::vec3(1000.0f, 500.0f, 0.0f), 10.0f, 9.0f, 1000.0f);
	EXPECT_TRUE(sway.active);
	EXPECT_EQ(sway.lowerVelocity, glm::vec3(0.0f));
	// The force over the game turn for half the mass, and nothing upwards
	EXPECT_FLOAT_EQ(sway.upperVelocity.x, 0.1f);
	EXPECT_FLOAT_EQ(sway.upperVelocity.y, 0.0f);
}

TEST(CreatureSway, AKickLowOnTheBodySwaysTheLowerBodyNoFasterThanItsLimit)
{
	Sway sway;
	sway.turnSeconds = 1.0f;
	Kick(sway, glm::vec3(0.0f, 0.0f, 1e6f), 2.0f, 9.0f, 100.0f);
	EXPECT_FLOAT_EQ(glm::length(sway.lowerVelocity), k_MaxKickSpeed);
	EXPECT_EQ(sway.upperVelocity, glm::vec3(0.0f));
}

TEST(CreatureSway, TheSpringSwingsBackAndSettles)
{
	Sway sway;
	sway.turnSeconds = 0.1f;
	Kick(sway, glm::vec3(2e5f, 0.0f, 0.0f), 0.0f, 9.0f, 1000.0f);
	bool wentOut = false;
	bool cameBack = false;
	for (int turn = 0; turn < 400 && sway.active; ++turn)
	{
		Step(sway, 0.1f, 1000.0f);
		wentOut = wentOut || sway.lowerOffset.x > 0.05f;
		cameBack = cameBack || (wentOut && sway.lowerOffset.x < 0.0f);
	}
	EXPECT_TRUE(wentOut);
	EXPECT_TRUE(cameBack);
	EXPECT_FALSE(sway.active);
}

TEST(CreatureSway, ASettledBodyIsNotStepped)
{
	Sway sway;
	sway.lowerOffset = glm::vec3(1.0f, 0.0f, 0.0f);
	Step(sway, 0.1f, 1000.0f);
	EXPECT_EQ(sway.lowerOffset, glm::vec3(1.0f, 0.0f, 0.0f));
}

TEST(CreatureSway, ALeanPicksHowFarThroughItsAnimation)
{
	EXPECT_EQ(LeanTime(0.0f, 1000), 500u);
	EXPECT_EQ(LeanTime(-3.0f, 1000), 0u);
	EXPECT_EQ(LeanTime(1.0f, 1000), 1000u);
	EXPECT_EQ(LeanTime(0.5f, 1000), 750u);
}

TEST(CreatureSway, AGameTurnLastsATenthOfASecondAtTheUsualPaceInWholeMilliseconds)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(TurnSeconds(1.0f)), 0x3DCCCCCDu);
	// A frozen creature lives fewer whole milliseconds of the turn: 55.5 becomes 55
	EXPECT_EQ(TurnSeconds(0.555f), 55.0f * 0.001f);
	EXPECT_EQ(TurnSeconds(0.0f), 0.0f);
}

TEST(CreatureSway, ATurnOfTheSpringHasTheBitsOfTheGame)
{
	// The speed is above its limit at the start, so the first step scales it down. The bits come from the game's own
	// code (the first row of test/oracle/data/creature_sway_spring.txt, made by tools/oracle/cases/sway_ribbon_cases.py).
	Sway sway;
	sway.active = true;
	sway.lowerVelocity = {5.0f, 3.0f, 2.0f};
	sway.lowerOffset = {0.1f, 0.2f, 0.3f};
	Step(sway, TurnSeconds(1.0f), 1000.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(sway.lowerVelocity.x), 0x400DF70Du);
	EXPECT_EQ(std::bit_cast<uint32_t>(sway.lowerVelocity.y), 0x3F815D0Fu);
	EXPECT_EQ(std::bit_cast<uint32_t>(sway.lowerVelocity.z), 0x3E95C1E0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(sway.lowerOffset.x), 0x3EE09945u);
	EXPECT_EQ(std::bit_cast<uint32_t>(sway.lowerOffset.y), 0x3EC47D79u);
	EXPECT_EQ(std::bit_cast<uint32_t>(sway.lowerOffset.z), 0x3ECC7B8Cu);
}

TEST(CreatureSway, ABodyThatPlaysAStartLoopAndEndTurnsOnTheSpotOrIsDownInAFightTakesNoKick)
{
	EXPECT_TRUE(TakesKick({}));
	EXPECT_FALSE(TakesKick({.playsStartLoopEnd = true}));
	EXPECT_FALSE(TakesKick({.turnsOnTheSpot = true}));
	EXPECT_FALSE(TakesKick({.downInFight = true}));
}

TEST(CreatureSway, ATurnOnTheSpotLeansAlongTheHeadingThatItHasReached)
{
	using openblack::GameHeading;
	EXPECT_FLOAT_EQ(TurnHeading(GameHeading(0.0f), GameHeading(1.0f), 250.0f, 1000.0f).Radians(), 0.25f);
	// The turn goes the short way round
	const float pi = std::numbers::pi_v<float>;
	EXPECT_NEAR(TurnHeading(GameHeading(0.9f * pi), GameHeading(-0.9f * pi), 500.0f, 1000.0f).Radians(), pi, 1e-5f);
}
