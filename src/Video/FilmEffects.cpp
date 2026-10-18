/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FilmEffects.h"

#include <cmath>

#include <algorithm>

using namespace openblack;

namespace
{
/// The sine and cosine keep the processor's wider precision until the next operation rounds them
double Sine(float radians)
{
	return std::sin(static_cast<double>(radians));
}

double Cosine(float radians)
{
	return std::cos(static_cast<double>(radians));
}

/// A product of a wide value and a float, rounded to a float
float Times(double wide, float value)
{
	return static_cast<float>(wide * static_cast<double>(value));
}

/// The part of a value's size after its whole number, scaled to a quarter and moved to a start
float QuarterOf(float value, float start)
{
	const float size = std::abs(value);
	const auto whole = static_cast<float>(static_cast<int32_t>(size));
	const float part = size - whole;
	const float quarter = part * 0.25f;
	return quarter + start;
}

/// Each colour of the fans, without its alpha
constexpr std::array<uint32_t, film_effects::k_FanCount> k_FanColours {0x0020FF40u, 0x00FF40FFu, 0x004080FFu, 0x00FF4040u};
} // namespace

std::array<film_effects::Star, film_effects::k_StarCount> film_effects::MakeStars(const CrtRandom& random)
{
	std::array<Star, k_StarCount> stars {};
	for (auto& star : stars)
	{
		// A random brightness gives an orange colour: red twice the green, at most full, and blue a third of the green
		const auto brightness = static_cast<int32_t>(random(16.0f, 100.0f));
		const auto red = std::min(brightness + brightness, 255);
		star.rgb = (static_cast<uint32_t>(red) << 16u) | (static_cast<uint32_t>(brightness) << 8u) |
		           static_cast<uint32_t>(brightness / 3);
		star.screen.x = random(-0.1f, 1.1f);
		star.screen.y = random(-0.1f, 1.1f);
		star.depth = random(5.0f, 25.0f);
		// A star that starts higher becomes smaller faster
		const float below = 1.0f - star.screen.y;
		star.shrink = below + 1.0f;
		star.size = random(4.0f, 8.0f);
		star.spin = random(-2.0f, 2.0f);
		star.age = 0.0f;
	}
	return stars;
}

std::optional<film_effects::StarSprite> film_effects::StepStar(Star& star, int32_t frameMs, int32_t index)
{
	const float step = static_cast<float>(frameMs) * 0.0013f;
	// It fades by its age before this frame, and ages
	const float faded = star.age * 32.0f;
	const auto alpha = static_cast<int32_t>(255.0f - faded);
	star.age = step + star.age;
	if (alpha <= 0)
	{
		return std::nullopt;
	}

	StarSprite sprite;
	sprite.argb = star.rgb + (static_cast<uint32_t>(std::min(alpha, 255)) << 24u);
	// It becomes smaller with its new age, but never smaller than the least size
	const float shrunk = star.shrink * star.age;
	const float half = shrunk * 0.5f;
	const float left = star.size - half;
	const float grown = left + 1.0f;
	sprite.halfSize = std::max(grown * 0.75f, 1e-4f);
	const float turned = star.spin * star.age;
	sprite.roll = turned + static_cast<float>(index);
	sprite.frame = static_cast<int32_t>(star.age * 8.0f) & 0xF;
	const float down = static_cast<float>(k_Screen.y) * star.screen.y;
	const float across = static_cast<float>(k_Screen.x) * star.screen.x;
	sprite.pixel = {static_cast<int32_t>(across), static_cast<int32_t>(down)};
	sprite.depth = star.depth;

	// It falls towards the bottom of the screen, slower as it comes near
	const float speed = star.spin + 1.0f;
	const float rest = 1.0f - star.screen.y;
	const float fall = speed * rest;
	const float stepped = fall * step;
	const float moved = stepped * 0.1f;
	star.screen.y = moved + star.screen.y;
	return sprite;
}

glm::vec4 film_effects::SheetCell(int32_t frame)
{
	constexpr int32_t k_Columns = 8;
	constexpr float k_Size = 1.0f / 8.0f;
	return {static_cast<float>(frame % k_Columns) * k_Size, static_cast<float>(frame / k_Columns) * k_Size, k_Size, k_Size};
}

std::array<film_effects::Ray, film_effects::k_RayCount>
film_effects::MakeRays(const LocalFloatRandom& localFloat, const CrtRandom& random, const LocalRandom& local)
{
	std::array<Ray, k_RayCount> rays {};
	for (auto& ray : rays)
	{
		ray.length = localFloat(1.0f) + 0.5f;
		ray.angle = random(0.0f, 6.2831855f);
		ray.speed = random(2.0f, 20.0f);
		// Half of the rays turn the other way
		if (random(0.0f, 1.0f) < 0.5f)
		{
			ray.speed = -ray.speed;
		}
		ray.wobble = random(-0.8f, 0.8f);
		// Some rays are longer and some are shorter
		if (local(16) < 2)
		{
			ray.length = ray.length * 1.4f;
		}
		if (local(16) < 1)
		{
			ray.length = ray.length * 0.71428573f;
		}
	}
	return rays;
}

uint32_t film_effects::StepBurst(Burst& burst, int32_t frameMs)
{
	const float seconds = static_cast<float>(frameMs) * 0.001f;
	const float step = seconds * 0.7f;
	const float turned = step * 0.1f;
	burst.spin = turned + burst.spin;
	const auto alpha = static_cast<int32_t>(burst.spin * 255.0f);
	const float opened = step * 0.25f;
	burst.open = opened + burst.open;
	// It grows once it has opened half way
	if (burst.open > 0.5f)
	{
		const float grown = step * 5.0f;
		burst.grow = grown + burst.grow;
	}
	return static_cast<uint32_t>(std::min(alpha, 255));
}

std::array<film_effects::Fan, film_effects::k_FanCount> film_effects::Fans(const Burst& burst, uint32_t alpha)
{
	const auto colour = [alpha](size_t fan) { return k_FanColours.at(fan) | (alpha << 24u); };
	const float grow = burst.grow;
	const float squared = grow * grow;
	const float cubed = squared * grow;
	const float spin = burst.spin;
	return {{
	    {.radius = cubed * 10.0f, .time = spin, .argb = colour(0)},
	    {.radius = grow * 25.0f, .time = -spin, .argb = colour(1)},
	    {.radius = grow * 50.0f, .time = spin * -2.0f, .argb = colour(2)},
	    {.radius = squared * 75.0f, .time = spin + spin, .argb = colour(3)},
	}};
}

std::array<film_effects::FanCorner, film_effects::k_FanCorners> film_effects::FanCorners(const Burst& burst, const Fan& fan,
                                                                                         glm::ivec2 centre)
{
	// As the burst opens, the rays gather into bunches; once it is fully open, each ray is as long as it is
	const float openSquared = burst.open * burst.open;
	const float bunching = openSquared * 0.7853982f;
	float whole = 1.0f - openSquared;
	if (whole <= 0.0f)
	{
		whole = 0.0f;
	}
	else if (!(whole < 1.0f))
	{
		whole = 1.0f;
	}
	const float rest = 1.0f - whole;
	const float slide = fan.time * 0.4f;
	const float outerSlide = slide + 1.0f;
	const glm::vec2 middle {static_cast<float>(centre.x), static_cast<float>(centre.y)};
	const uint32_t clear = fan.argb & 0xFF000000u;

	std::array<FanCorner, k_FanCorners> corners {};
	for (size_t i = 0; i < k_RayCount; ++i)
	{
		const auto& ray = burst.rays.at(i);
		const float turned = fan.time * ray.speed;
		const float angle = turned + ray.angle;
		const float wobble = Times(Sine(angle), ray.wobble);
		const float length = wobble + ray.length;
		const float radius = length * fan.radius;

		const float place = static_cast<float>(i + 1) * bunching;
		const double bunch = std::abs(Sine(place));
		const auto bunchSquared = static_cast<float>(bunch * bunch);
		const float four = bunchSquared * 4.0f;
		const float bunched = four * radius;
		const float weighted = bunched * whole;
		const float kept = rest * radius;
		const float reach = weighted + kept;

		const float spread = static_cast<float>(i) * 0.09817477f;
		const float direction = spread + fan.time;
		const float across = Times(Sine(direction), reach) + middle.x;
		const float down = Times(Cosine(direction), reach) + middle.y;

		const float u = QuarterOf(static_cast<float>(i) * 0.015625f, 0.5f);
		corners.at(i * 2) = {.pixel = centre, .uv = {u, QuarterOf(slide, 0.25f)}, .argb = fan.argb};
		corners.at((i * 2) + 1) = {.pixel = {static_cast<int32_t>(across), static_cast<int32_t>(down)},
		                           .uv = {u, QuarterOf(outerSlide, 0.25f)},
		                           .argb = clear};
	}
	return corners;
}

std::array<uint16_t, film_effects::k_FanIndices> film_effects::FanTriangles()
{
	std::array<uint16_t, k_FanIndices> indices {};
	for (size_t i = 0; i < k_RayCount; ++i)
	{
		indices.at(i * 3) = static_cast<uint16_t>(i * 2);
		indices.at((i * 3) + 1) = static_cast<uint16_t>((i * 2) + 1);
		indices.at((i * 3) + 2) = static_cast<uint16_t>((((i + 1) & (k_RayCount - 1)) * 2) + 1);
	}
	return indices;
}

glm::ivec2 film_effects::PixelOf(glm::vec2 screen)
{
	const glm::vec2 half {static_cast<float>(k_Screen.x) * 0.5f, static_cast<float>(k_Screen.y) * 0.5f};
	const float across = (screen.x + 1.0f) * half.x;
	const float down = (1.0f - screen.y) * half.y;
	return {static_cast<int32_t>(across), static_cast<int32_t>(down)};
}

glm::vec3 film_effects::CameraPoint(glm::ivec2 pixel, float depth, glm::vec2 projectionScale)
{
	const glm::vec2 half {static_cast<float>(k_Screen.x) * 0.5f, static_cast<float>(k_Screen.y) * 0.5f};
	const float across = (static_cast<float>(pixel.x) - half.x) / half.x;
	const float up = (half.y - static_cast<float>(pixel.y)) / half.y;
	return {across * depth / projectionScale.x, up * depth / projectionScale.y, depth};
}
