/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the one wind of the experiment

#include <cmath>

#include <numbers>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "ECS/WindRules.h"

namespace wind = openblack::wind;

TEST(Wind, TheWeatherSetsTheTarget)
{
	// A calm weather keeps the calm direction and speed
	const auto calm = wind::Target({0.0f, 0.0f}, 0.0f);
	EXPECT_FLOAT_EQ(calm.angle, wind::k_CalmAngle);
	EXPECT_FLOAT_EQ(calm.speed, wind::k_CalmSpeed);
	// A strong wind along +z blows that way, faster
	const auto strong = wind::Target({0.0f, 80.0f}, 0.7f);
	EXPECT_NEAR(strong.angle, std::numbers::pi_v<float> * 0.5f, 1e-5f);
	EXPECT_NEAR(strong.speed, wind::k_CalmSpeed + 80.0f * wind::k_SpeedPerPercent, 1e-4f);
	EXPECT_FLOAT_EQ(strong.storm, 0.7f);
}

TEST(Wind, TheWindTurnsSlowlyTheShortWay)
{
	wind::Wind now {.angle = 3.0f, .speed = 3.0f, .storm = 0.0f};
	// The target is just across the half turn: the short way goes up through pi
	const wind::Wind target {.angle = -3.0f, .speed = 10.0f, .storm = 1.0f};
	const auto next = wind::Step(now, target, 1.0f);
	const float turned = std::remainder(next.angle - now.angle, 2.0f * std::numbers::pi_v<float>);
	EXPECT_NEAR(turned, wind::k_TurnRate, 1e-4f);
	// The speed does not jump
	EXPECT_GT(next.speed, now.speed);
	EXPECT_LT(next.speed, 5.0f);
	// After long enough, it is there
	for (int i = 0; i < 600; ++i)
	{
		now = wind::Step(now, target, 0.1f);
	}
	EXPECT_NEAR(std::remainder(now.angle - target.angle, 2.0f * std::numbers::pi_v<float>), 0.0f, 1e-3f);
	EXPECT_NEAR(now.speed, 10.0f, 0.01f);
}

TEST(Wind, TheGustsAverageToTheMeanAndStayInRange)
{
	const wind::Wind calm {};
	double sum = 0.0;
	int count = 0;
	for (int x = 0; x < 50; ++x)
	{
		for (int t = 0; t < 40; ++t)
		{
			const float gust = wind::Gust(calm, {static_cast<float>(x) * 13.0f, 7.0f}, static_cast<float>(t) * 0.7f);
			EXPECT_GE(gust, 0.5f);
			EXPECT_LE(gust, 1.6f);
			sum += gust;
			++count;
		}
	}
	EXPECT_NEAR(sum / count, 1.0, 0.05);
	// The velocity points downwind
	const auto velocity = wind::VelocityAt(calm, {0.0f, 0.0f}, 0.0f);
	EXPECT_GT(glm::dot(glm::normalize(velocity), wind::Direction(calm)), 0.999f);
}

TEST(Wind, TheGustsTravelDownwind)
{
	const wind::Wind steady {.angle = 0.0f, .speed = 5.0f, .storm = 0.0f};
	// A gust front at a point now is at a point downwind later; the slow swell aside
	const float swellNow = 0.2f * 0.18f * std::sin(0.0f);
	const float swellLater = 0.2f * 0.18f * std::sin(0.21f * 10.0f);
	const float now = wind::Gust(steady, {100.0f, 0.0f}, 0.0f) - swellNow;
	const float later = wind::Gust(steady, {150.0f, 0.0f}, 10.0f) - swellLater;
	EXPECT_NEAR(now, later, 1e-3f);
}
