/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The falling spell film's effects: the glows on the copy's hands, the stars and the burst of light

#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include "Video/FilmCreature.h"
#include "Video/FilmEffects.h"

using namespace openblack;
using creature_hand_glow::Place;

namespace
{
/// A random number from the middle of each range
float Middle(float a, float b)
{
	return (a + b) * 0.5f;
}
} // namespace

TEST(FilmEffects, TheHandGlowsRiseAndFallByTheFilmsTime)
{
	EXPECT_FALSE(film_creature::GlowPower(Place::LeftHand, 16349).has_value());
	// A level of 0 leaves the glow as it is
	EXPECT_FALSE(film_creature::GlowPower(Place::LeftHand, 16350).has_value());
	EXPECT_FLOAT_EQ(*film_creature::GlowPower(Place::LeftHand, 16850), 2.0f);
	EXPECT_FLOAT_EQ(*film_creature::GlowPower(Place::LeftHand, 17350), 3.0f);
	EXPECT_FLOAT_EQ(*film_creature::GlowPower(Place::LeftHand, 18350), 2.0f);
	EXPECT_FALSE(film_creature::GlowPower(Place::LeftHand, 19350).has_value());
	EXPECT_FALSE(film_creature::GlowPower(Place::LeftHand, 31650).has_value());
	EXPECT_FLOAT_EQ(*film_creature::GlowPower(Place::LeftHand, 32150), 2.0f);
	EXPECT_FALSE(film_creature::GlowPower(Place::LeftHand, 32650).has_value());

	EXPECT_FLOAT_EQ(*film_creature::GlowPower(Place::RightHand, 17700), 2.0f);
	EXPECT_FLOAT_EQ(*film_creature::GlowPower(Place::RightHand, 37000), 2.0f);
	EXPECT_FALSE(film_creature::GlowPower(Place::Belly, 17000).has_value());
}

TEST(FilmEffects, TheStarsStartOrangeAndFallFadeAndShrink)
{
	auto stars = film_effects::MakeStars(Middle);
	auto& star = stars.at(3);
	// Brightness 58: red twice it, blue a third of it
	EXPECT_EQ(star.rgb, 0x743A13u);
	EXPECT_FLOAT_EQ(star.screen.x, 0.5f);
	EXPECT_FLOAT_EQ(star.screen.y, 0.5f);
	EXPECT_FLOAT_EQ(star.depth, 15.0f);
	EXPECT_FLOAT_EQ(star.shrink, 1.5f);
	EXPECT_FLOAT_EQ(star.size, 6.0f);

	const auto sprite = film_effects::StepStar(star, 100, 3);
	ASSERT_TRUE(sprite.has_value());
	EXPECT_EQ(sprite->argb, 0xFF743A13u);
	EXPECT_EQ(sprite->pixel, glm::ivec2(400, 300));
	EXPECT_FLOAT_EQ(sprite->depth, 15.0f);
	const float age = 100.0f * 0.0013f;
	EXPECT_FLOAT_EQ(star.age, age);
	EXPECT_FLOAT_EQ(sprite->halfSize, ((6.0f - ((1.5f * age) * 0.5f)) + 1.0f) * 0.75f);
	EXPECT_FLOAT_EQ(sprite->roll, 3.0f);
	EXPECT_EQ(sprite->frame, 1);
	// It falls after it is drawn
	EXPECT_FLOAT_EQ(star.screen.y, 0.5f + ((1.0f * 0.5f) * age * 0.1f));

	// An old star has faded and is not drawn
	star.age = 8.0f;
	EXPECT_FALSE(film_effects::StepStar(star, 16, 3).has_value());
	EXPECT_GT(star.age, 8.0f);

	EXPECT_EQ(film_effects::SheetCell(9), glm::vec4(0.125f, 0.125f, 0.125f, 0.125f));
}

TEST(FilmEffects, TheRaysTurnOpenAndGrow)
{
	const auto rays = film_effects::MakeRays([](float x) { return x * 0.5f; }, Middle, [](int32_t) { return 1u; });
	EXPECT_FLOAT_EQ(rays.at(0).length, 1.0f * 1.4f);
	EXPECT_FLOAT_EQ(rays.at(0).angle, 3.1415927f);
	// The middle of 0 to 1 is not less than a half, so the ray keeps its way round
	EXPECT_FLOAT_EQ(rays.at(0).speed, 11.0f);
	EXPECT_FLOAT_EQ(rays.at(0).wobble, 0.0f);

	film_effects::Burst burst;
	EXPECT_EQ(film_effects::StepBurst(burst, 1000), 17u);
	EXPECT_FLOAT_EQ(burst.open, 0.175f);
	EXPECT_FLOAT_EQ(burst.grow, 1.0f);
	for (int i = 0; i < 3; ++i)
	{
		static_cast<void>(film_effects::StepBurst(burst, 1000));
	}
	EXPECT_GT(burst.grow, 1.0f);

	burst.grow = 2.0f;
	burst.spin = 0.5f;
	const auto fans = film_effects::Fans(burst, 100);
	EXPECT_FLOAT_EQ(fans.at(0).radius, 80.0f);
	EXPECT_FLOAT_EQ(fans.at(1).radius, 50.0f);
	EXPECT_FLOAT_EQ(fans.at(2).radius, 100.0f);
	EXPECT_FLOAT_EQ(fans.at(3).radius, 300.0f);
	EXPECT_FLOAT_EQ(fans.at(1).time, -0.5f);
	EXPECT_FLOAT_EQ(fans.at(2).time, -1.0f);
	EXPECT_FLOAT_EQ(fans.at(3).time, 1.0f);
	EXPECT_EQ(fans.at(0).argb, 0x6420FF40u);
	EXPECT_EQ(fans.at(3).argb, 0x64FF4040u);
}

TEST(FilmEffects, AFanHasACornerAtTheMiddleAndOneAtTheEndOfEachRay)
{
	film_effects::Burst burst;
	for (auto& ray : burst.rays)
	{
		ray = {.length = 1.0f, .angle = 0.0f, .speed = 0.0f, .wobble = 0.0f};
	}
	// Not open, the rays have no length
	const film_effects::Fan fan {.radius = 10.0f, .time = 0.0f, .argb = 0x80FF0000u};
	const auto closed = film_effects::FanCorners(burst, fan, {100, 200});
	EXPECT_EQ(closed.at(1).pixel, glm::ivec2(100, 200));

	// Fully open, each ray is as long as the fan's radius
	burst.open = 1.0f;
	const auto corners = film_effects::FanCorners(burst, fan, {100, 200});
	EXPECT_EQ(corners.at(0).pixel, glm::ivec2(100, 200));
	EXPECT_EQ(corners.at(0).argb, 0x80FF0000u);
	EXPECT_EQ(corners.at(1).pixel, glm::ivec2(100, 210));
	EXPECT_EQ(corners.at(1).argb, 0x80000000u);
	EXPECT_FLOAT_EQ(corners.at(0).uv.x, 0.5f);
	EXPECT_FLOAT_EQ(corners.at(0).uv.y, 0.25f);
	EXPECT_FLOAT_EQ(corners.at(1).uv.y, 0.25f);
	EXPECT_FLOAT_EQ(corners.at(2).uv.x, 0.5f + (0.015625f * 0.25f));

	const auto triangles = film_effects::FanTriangles();
	EXPECT_EQ(triangles.at(0), 0);
	EXPECT_EQ(triangles.at(1), 1);
	EXPECT_EQ(triangles.at(2), 3);
	EXPECT_EQ(triangles.at(189), 126);
	EXPECT_EQ(triangles.at(190), 127);
	EXPECT_EQ(triangles.at(191), 1);
}

TEST(FilmEffects, ThePixelsOfTheDefaultScreenGoToAndFromTheCamera)
{
	EXPECT_EQ(film_effects::PixelOf({0.0f, 0.0f}), glm::ivec2(400, 300));
	EXPECT_EQ(film_effects::PixelOf({-1.0f, 1.0f}), glm::ivec2(0, 0));
	EXPECT_EQ(film_effects::CameraPoint({400, 300}, 10.0f, {1.0f, 1.0f}), glm::vec3(0.0f, 0.0f, 10.0f));
	const auto right = film_effects::CameraPoint({800, 0}, 10.0f, {2.0f, 1.0f});
	EXPECT_FLOAT_EQ(right.x, 5.0f);
	EXPECT_FLOAT_EQ(right.y, 10.0f);
}
