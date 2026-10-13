/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "3D/Rain.h"

using namespace openblack;

namespace
{
/// Hands out its numbers in turn and keeps the ranges it was asked for
struct FakeRandom
{
	std::vector<float> values;
	std::vector<std::pair<float, float>> asked;
	size_t next {0};

	rain::Random Get()
	{
		return [this](float a, float b) {
			asked.emplace_back(a, b);
			return values.at(next++ % values.size());
		};
	}
};
} // namespace

TEST(Rain, PlacedInTheGamesOrder)
{
	FakeRandom random {.values = {10.0f, -20.0f, 3.0f, -4.0f, 0.25f, 0.15f, 0.5f}};
	const auto streak = rain::Place(random.Get());
	EXPECT_FLOAT_EQ(streak.x, 5.0f);
	EXPECT_FLOAT_EQ(streak.z, -10.0f);
	EXPECT_FLOAT_EQ(streak.dx, 3.0f);
	EXPECT_FLOAT_EQ(streak.dz, -4.0f);
	EXPECT_FLOAT_EQ(streak.scroll, 0.25f);
	EXPECT_FLOAT_EQ(streak.speed, 0.15f);
	EXPECT_FLOAT_EQ(streak.phase, 0.5f);
	ASSERT_EQ(random.asked.size(), 7u);
	EXPECT_EQ(random.asked[0], std::make_pair(-160.0f, 160.0f));
	EXPECT_EQ(random.asked[2], std::make_pair(-15.0f, 15.0f));
	EXPECT_EQ(random.asked[5], std::make_pair(0.1f, 0.2f));
}

TEST(Rain, StepsAndStartsAgain)
{
	FakeRandom random {.values = {1.0f, 2.0f, 30.0f, 40.0f}};
	std::array<rain::Streak, 2> streaks {};
	streaks[0] = {.phase = 0.5f, .x = 7.0f, .scroll = 0.9f, .speed = 0.2f};
	streaks[1] = {.phase = 0.9f, .x = 7.0f, .scroll = 0.1f, .speed = 0.1f};
	rain::Step(streaks, 0.1f, 10.0f, random.Get());
	// 0.9 + 10 x 0.2 x 0.1 wraps to 0.1; its life moves on by 0.24
	EXPECT_NEAR(streaks[0].scroll, 0.1f, 1e-5f);
	EXPECT_NEAR(streaks[0].phase, 0.74f, 1e-5f);
	EXPECT_FLOAT_EQ(streaks[0].x, 7.0f);
	// The second's life ends: it starts again with a new slant and place
	EXPECT_NEAR(streaks[1].phase, 0.14f, 1e-5f);
	EXPECT_FLOAT_EQ(streaks[1].dx, 1.0f);
	EXPECT_FLOAT_EQ(streaks[1].dz, 2.0f);
	EXPECT_FLOAT_EQ(streaks[1].x, 15.0f);
	EXPECT_FLOAT_EQ(streaks[1].z, 20.0f);
}

TEST(Rain, FollowsTheNearestStorm)
{
	const auto step = rain::Follow({}, rain::Fall {.height = 500.0f, .speed = 2.0f});
	EXPECT_FLOAT_EQ(step.height, 160.0f + (340.0f * 0.3f));
	EXPECT_FLOAT_EQ(step.speed, 1.3f);
	// Back towards the calm air without one, within bounds
	EXPECT_EQ(rain::Follow({.height = 10.0f, .speed = 0.0f}, std::nullopt), (rain::Fall {.height = 55.0f, .speed = 0.3f}));
	EXPECT_EQ(rain::Follow({.height = 10.0f, .speed = 0.0f}, rain::Fall {.height = 0.0f, .speed = 0.0f}),
	          (rain::Fall {.height = 40.0f, .speed = 0.3f}));
	EXPECT_EQ(rain::Follow({.height = 2000.0f, .speed = 20.0f}, rain::Fall {.height = 2000.0f, .speed = 20.0f}),
	          (rain::Fall {.height = 640.0f, .speed = 5.0f}));
}

TEST(Rain, TilesThinOutWithDistance)
{
	// Too little rain, or too far
	EXPECT_FALSE(rain::TileOf({0.0f, 0.0f}, 5, {0.0f, 0.0f}).has_value());
	EXPECT_FALSE(rain::TileOf({0.0f, 0.0f}, 100, {401.0f, 0.0f}).has_value());
	// Near: all the streaks, 88 of 100 opaque, the top a fifth of it
	const auto near = rain::TileOf({0.0f, 0.0f}, 100, {50.0f, 0.0f});
	ASSERT_TRUE(near.has_value());
	EXPECT_EQ(near->streaks, 128);
	EXPECT_EQ(near->alpha, 88);
	EXPECT_EQ(near->alphaTop, 14);
	// Halfway out of the thinning, half of each
	const auto far = rain::TileOf({0.0f, 0.0f}, 100, {250.0f, 0.0f});
	ASSERT_TRUE(far.has_value());
	EXPECT_EQ(far->streaks, 64);
	EXPECT_EQ(far->alpha, 44);
	EXPECT_EQ(far->alphaTop, 3);
}

TEST(Rain, StreaksFadeInAndOut)
{
	EXPECT_EQ(rain::StreakAlpha(200, 0.025f), 100);
	EXPECT_EQ(rain::StreakAlpha(200, 0.5f), 200);
	EXPECT_EQ(rain::StreakAlpha(200, 0.975f), 99);
	const auto ends = rain::Ends({.x = 1.0f, .z = 2.0f, .dx = 3.0f, .dz = -4.0f}, 160.0f);
	EXPECT_EQ(ends[0], glm::vec3(1.0f, -50.0f, 2.0f));
	EXPECT_EQ(ends[1], glm::vec3(4.0f, 160.0f, -2.0f));
}

TEST(Rain, AWeakRainShowsNoStreaks)
{
	// A rain of 6 percent has a strength of 5, which is too weak for streaks
	EXPECT_EQ(rain::Strength(6), 5);
	EXPECT_FALSE(rain::TileOf({0.0f, 0.0f}, 6, {0.0f, 0.0f}).has_value());
	EXPECT_EQ(rain::Strength(7), 6);
	EXPECT_TRUE(rain::TileOf({0.0f, 0.0f}, 7, {0.0f, 0.0f}).has_value());
	EXPECT_EQ(rain::Strength(1000), 255);
}

TEST(Rain, OnlyAHeavyRainSplashes)
{
	// A rain of 51 percent has a strength of 44, and a rain of 52 percent a strength of 45
	EXPECT_FALSE(rain::SplashAlpha(rain::Strength(51)).has_value());
	ASSERT_TRUE(rain::SplashAlpha(rain::Strength(52)).has_value());
	EXPECT_EQ(*rain::SplashAlpha(45), 130);
	EXPECT_EQ(*rain::SplashAlpha(88), 255);
	EXPECT_EQ(*rain::SplashAlpha(255), 255);
}

TEST(Rain, SplashesInTheGamesOrder)
{
	FakeRandom random {.values = {10.0f, 150.0f, 0.2f}};
	std::vector<glm::vec2> heightsAsked;
	const rain::HeightAt heightAt = [&heightsAsked](glm::vec2 point) {
		heightsAsked.push_back(point);
		return 7.0f;
	};
	const auto ring = rain::Splash({240.0f, 400.0f}, 0x82102030u, random.Get(), heightAt);
	// The z comes first, then the x, then the size
	ASSERT_EQ(random.asked.size(), 3u);
	EXPECT_EQ(random.asked[0], std::make_pair(0.0f, 160.0f));
	EXPECT_EQ(random.asked[1], std::make_pair(0.0f, 160.0f));
	EXPECT_EQ(random.asked[2], std::make_pair(0.0f, 0.3f));
	EXPECT_FLOAT_EQ(ring.position.z, 400.0f + 10.0f - 80.0f);
	EXPECT_FLOAT_EQ(ring.position.x, 240.0f + 150.0f - 80.0f);
	// One unit above the ground under it
	ASSERT_EQ(heightsAsked.size(), 1u);
	EXPECT_EQ(heightsAsked[0], glm::vec2(ring.position.x, ring.position.z));
	EXPECT_FLOAT_EQ(ring.position.y, 8.0f);
	// A round ring that grows at the usual rate, unturned
	EXPECT_FLOAT_EQ(ring.growth, 0.7f);
	EXPECT_EQ(ring.age, 0u);
	EXPECT_FLOAT_EQ(ring.angle, 0.0f);
	EXPECT_FLOAT_EQ(ring.aspect, 1.0f);
	EXPECT_FLOAT_EQ(ring.rate, 1.0f);
	EXPECT_EQ(ring.cell, 0x30);
	EXPECT_EQ(ring.argb, 0x82102030u);
}

TEST(Rain, FarBlocksComeFirst)
{
	const glm::vec3 eye {0.0f, 30.0f, 0.0f};
	EXPECT_FLOAT_EQ(rain::DrawKey({30.0f, 40.0f}, 30.0f, eye), 2500.0f);
	EXPECT_GT(rain::DrawKey({0.0f, 300.0f}, 0.0f, eye), rain::DrawKey({0.0f, 100.0f}, 0.0f, eye));
}
