/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Rain.h"

#include <cmath>

#include <algorithm>

namespace openblack::rain
{

namespace
{
/// The half width of the square a streak stands in, about the block's centre
constexpr float k_Spread = 160.0f;
constexpr float k_Slant = 15.0f;
/// How far through its life a streak goes in a second
constexpr float k_LifeRate = 2.4f;
/// How far the rain's fall moves towards a storm's in a frame, and its bounds
constexpr float k_FollowRate = 0.3f;
constexpr float k_LowestHeight = 40.0f;
constexpr float k_HighestHeight = 640.0f;
constexpr float k_SlowestFall = 0.3f;
constexpr float k_FastestFall = 5.0f;
/// The rain's strength of 100 for each percent
constexpr int32_t k_Opacity = 88;
/// Rain is drawn up to this far from the camera, thinning out from the nearer distance
constexpr float k_Farthest = 400.0f;
constexpr float k_Thinning = 100.0f;
/// The streaks fade in and out over this much of their life
constexpr float k_Fade = 0.05f;

/// The length of a side of a land block
constexpr float k_BlockSide = 160.0f;
/// A splash is this far above the ground, and its half width grows to its size plus a random part of the variation
constexpr float k_SplashHeight = 1.0f;
constexpr float k_SplashGrowth = 0.5f;
constexpr float k_SplashGrowthVariation = 0.3f;
constexpr uint8_t k_SplashCell = 0x30;

/// Drops what is left of a number past its whole part
float Fraction(float value)
{
	return value - static_cast<float>(static_cast<int32_t>(value));
}
} // namespace

Streak Place(const Random& random)
{
	Streak streak;
	streak.x = random(-k_Spread, k_Spread) * 0.5f;
	streak.z = random(-k_Spread, k_Spread) * 0.5f;
	streak.dx = random(-k_Slant, k_Slant);
	streak.dz = random(-k_Slant, k_Slant);
	streak.scroll = random(0.0f, 1.0f);
	streak.speed = random(0.1f, 0.2f);
	streak.phase = random(0.0f, 1.0f);
	return streak;
}

void Step(std::span<Streak> streaks, float seconds, float fallSpeed, const Random& random)
{
	const float life = seconds * k_LifeRate;
	for (auto& streak : streaks)
	{
		streak.scroll += fallSpeed * streak.speed * seconds;
		if (streak.scroll > 1.0f)
		{
			streak.scroll = Fraction(streak.scroll);
		}
		streak.phase += life;
		if (streak.phase > 1.0f)
		{
			// A new life somewhere else
			streak.phase = Fraction(streak.phase);
			streak.dx = random(-k_Slant, k_Slant);
			streak.dz = random(-k_Slant, k_Slant);
			streak.x = random(-k_Spread, k_Spread) * 0.5f;
			streak.z = random(-k_Spread, k_Spread) * 0.5f;
		}
	}
}

Fall Follow(Fall current, std::optional<Fall> storm)
{
	const auto target = storm.value_or(Fall {});
	current.height += (target.height - current.height) * k_FollowRate;
	current.speed += (target.speed - current.speed) * k_FollowRate;
	current.speed = !(current.speed > k_SlowestFall) ? k_SlowestFall : std::min(current.speed, k_FastestFall);
	current.height = !(current.height > k_LowestHeight) ? k_LowestHeight : std::min(current.height, k_HighestHeight);
	return current;
}

std::optional<Tile> TileOf(glm::vec2 centre, int32_t wettest, glm::vec2 camera)
{
	if (wettest <= k_LeastRain)
	{
		return std::nullopt;
	}
	Tile tile {.centre = centre, .streaks = static_cast<int32_t>(k_Streaks), .alpha = Strength(wettest)};
	// A weak rain shows no streaks
	if (tile.alpha <= k_LeastRain)
	{
		return std::nullopt;
	}
	const auto offset = camera - centre;
	const float distance = std::sqrt((offset.x * offset.x) + (offset.y * offset.y));
	if (!(distance <= k_Farthest))
	{
		return std::nullopt;
	}
	if (distance > k_Thinning)
	{
		const float left = 1.0f - ((distance - k_Thinning) / (k_Farthest - k_Thinning));
		tile.alpha = static_cast<int32_t>(static_cast<float>(tile.alpha) * left);
		tile.streaks = static_cast<int32_t>(static_cast<float>(tile.streaks) * left);
	}
	if (tile.streaks <= 0)
	{
		return std::nullopt;
	}
	// The top is fainter, and fainter still further away
	const auto share = distance / k_Farthest;
	const auto far = (share + share) + 1.0f;
	tile.alphaTop = static_cast<int32_t>(static_cast<float>(tile.alpha) / (far * 5.0f));
	return tile;
}

int32_t StreakAlpha(int32_t alpha, float phase)
{
	if (phase < k_Fade)
	{
		return static_cast<int32_t>(static_cast<float>(alpha) * phase * 20.0f);
	}
	if (phase > 1.0f - k_Fade)
	{
		return static_cast<int32_t>(static_cast<float>(alpha) * ((1.0f - phase) * 20.0f));
	}
	return alpha;
}

int32_t Strength(int32_t wettest)
{
	return std::min(wettest * k_Opacity / 100, 255);
}

float DrawKey(glm::vec2 centre, float ground, glm::vec3 eye)
{
	const glm::vec3 offset = glm::vec3(centre.x, ground, centre.y) - eye;
	return (offset.x * offset.x) + (offset.y * offset.y) + (offset.z * offset.z);
}

std::optional<uint8_t> SplashAlpha(int32_t strength)
{
	if (strength <= k_LeastSplashStrength)
	{
		return std::nullopt;
	}
	return static_cast<uint8_t>(std::clamp(strength * 255 / k_Opacity, 0, 255));
}

water_rings::Ring Splash(glm::vec2 centre, uint32_t argb, const Random& random, const HeightAt& heightAt)
{
	// The point is anywhere on the block
	const float z = random(0.0f, k_BlockSide) + centre.y - (k_BlockSide * 0.5f);
	const float x = random(0.0f, k_BlockSide) + centre.x - (k_BlockSide * 0.5f);
	const float y = heightAt({x, z}) + k_SplashHeight;
	return {
	    .position = {x, y, z},
	    .growth = random(0.0f, k_SplashGrowthVariation) + k_SplashGrowth,
	    .cell = k_SplashCell,
	    .argb = argb,
	};
}

std::array<glm::vec3, 2> Ends(const Streak& streak, float height)
{
	return {glm::vec3(streak.x, k_Bottom, streak.z), glm::vec3(streak.x + streak.dx, height, streak.z + streak.dz)};
}

} // namespace openblack::rain
