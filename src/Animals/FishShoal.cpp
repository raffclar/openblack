/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishShoal.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>

namespace openblack::fish_shoal
{
namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
constexpr float k_TwoPi = 6.2831855f;
/// A fish's turning speed is its swimming speed times this, give or take a tenth
constexpr float k_TurnPerSpeed = 0.6283185f;
/// Its picture runs this many frames a second for each unit of its speed
constexpr float k_FramesPerSpeed = 25.0f;
/// A panicking fish swims this many times its speed while it still has a second or more of panic
constexpr float k_PanicSpeedFactor = 4.0f;
/// A new point is as many seconds away as half its distance from the last
constexpr float k_RetargetSecondsPerMetre = 0.5f;

/// The truncation the game converts with
[[nodiscard]] int32_t Truncate(float value)
{
	return static_cast<int32_t>(value);
}
} // namespace

std::optional<glm::vec2> FindCentre(glm::vec2 farm, const std::function<bool(glm::vec2)>& isOpenSea)
{
	// How many rings running each direction has been open sea on
	std::array<uint8_t, k_RingDirections> runs {};
	for (float radius = k_RingStep; radius < k_RingLimit; radius += k_RingStep)
	{
		for (uint32_t direction = 0; direction < k_RingDirections; ++direction)
		{
			const float angle = static_cast<float>(direction) * k_TwoPi * (1.0f / static_cast<float>(k_RingDirections));
			const glm::vec2 point = farm + glm::vec2(std::cos(angle), std::sin(angle)) * radius;
			if (!isOpenSea(point))
			{
				runs.at(direction) = 0;
				continue;
			}
			if (++runs.at(direction) == 2)
			{
				return point;
			}
		}
	}
	return std::nullopt;
}

Fish MakeFish(glm::vec3 centre, const Random& random)
{
	Fish fish;
	fish.size = std::max(random(0.8f, 1.2f), 0.0001f);
	// The first frame and turn are drawn but given way to by the first swim
	fish.frame = static_cast<uint32_t>(Truncate(random(0.0f, 15.5f))) & 63u;
	[[maybe_unused]] const float firstTurn = random(0.0f, k_Pi);
	fish.phase = random(0.0f, 15.0f);
	const float z = random(-5.0f, 5.0f);
	const float y = random(-1.0f, 0.0f);
	const float x = random(-5.0f, 5.0f);
	fish.position = centre + glm::vec3(x, y, z);
	fish.heading = random(-k_Pi, k_Pi);
	fish.speed = random(0.5f, 1.5f);
	fish.turnRate = (random(-0.1f, 0.1f) + 1.0f) * fish.speed * k_TurnPerSpeed;
	fish.panic = 0.0f;
	return fish;
}

Shoal MakeShoal(glm::vec3 centre, const Random& random)
{
	Shoal shoal {.centre = centre, .target = centre, .retargetSeconds = 0.0f, .fullness = 1.0f};
	for (auto& fish : shoal.fish)
	{
		fish = MakeFish(centre, random);
	}
	return shoal;
}

uint32_t ShownCount(float fullness)
{
	return static_cast<uint32_t>(
	    std::clamp(Truncate(fullness * static_cast<float>(k_FishCount)), 0, static_cast<int32_t>(k_FishCount)));
}

std::optional<uint8_t> AlphaAt(float distanceSquared)
{
	if (distanceSquared > k_ShownDistanceSquared)
	{
		return std::nullopt;
	}
	if (distanceSquared <= k_OpaqueDistanceSquared)
	{
		return static_cast<uint8_t>(255);
	}
	// Faded right out well before the shoal stops showing, the opacity goes negative and wraps round as a byte
	const float fade = 1.0f - (distanceSquared - k_OpaqueDistanceSquared) * k_FadePerDistanceSquared;
	return static_cast<uint8_t>(static_cast<uint32_t>(Truncate(fade * 255.0f)) & 0xFFu);
}

void StepFish(Fish& fish, glm::vec3 target, float seconds)
{
	float factor = 1.0f;
	if (fish.panic != 0.0f)
	{
		fish.panic -= seconds;
		if (fish.panic < 0.0f)
		{
			fish.panic = 0.0f;
		}
		factor = fish.panic < 1.0f ? 1.0f + 3.0f * fish.panic : k_PanicSpeedFactor;
	}
	const float step = seconds < k_LongestStep ? seconds : k_LongestStep;

	// Its picture runs with its speed through sixteen frames
	fish.phase += step * fish.speed * k_FramesPerSpeed;
	fish.frame = ((static_cast<uint32_t>(Truncate(fish.phase)) & (k_FrameCount - 1)) + k_FirstFrame) & 63u;
	fish.phase -= static_cast<float>(Truncate(fish.phase * (1.0f / static_cast<float>(k_FrameCount - 1)))) *
	              static_cast<float>(k_FrameCount - 1);

	// It swims on the way it faces, never up or down
	const float cosine = std::cos(fish.heading);
	const float sine = std::sin(fish.heading);
	fish.position.x += cosine * fish.speed * step * factor;
	fish.position.z += sine * fish.speed * step * factor;

	// It turns towards the point, by the side the point is on
	const float side = sine * (target.x - fish.position.x) - (target.z - fish.position.z) * cosine;
	const float turn = side < 0.0f ? 1.0f : -1.0f;
	fish.heading += turn * fish.turnRate * step;
	fish.heading -= static_cast<float>(Truncate(fish.heading / k_TwoPi)) * k_TwoPi;
	if (fish.heading > k_Pi)
	{
		fish.heading -= k_TwoPi;
	}
}

void Step(Shoal& shoal, float seconds, std::optional<glm::vec3> scare, const Random& random)
{
	// When its time is up the point moves to somewhere else within its square about the centre, and stays there as long
	// as half the way it moved
	shoal.retargetSeconds -= seconds;
	if (shoal.retargetSeconds < 0.0f)
	{
		const float z = random(-k_WanderRadius, k_WanderRadius);
		const float x = random(-k_WanderRadius, k_WanderRadius);
		const glm::vec3 next = shoal.centre + glm::vec3(x, 0.0f, z);
		shoal.retargetSeconds = glm::length(shoal.target - next) * k_RetargetSecondsPerMetre;
		shoal.target = next;
	}
	if (scare.has_value())
	{
		const float angle = random(0.0f, k_TwoPi);
		shoal.target = shoal.centre + glm::vec3(std::cos(angle), 0.0f, std::sin(angle)) * k_ScaredTargetDistance;
	}
	const auto shown = ShownCount(shoal.fullness);
	for (uint32_t i = 0; i < shown; ++i)
	{
		auto& fish = shoal.fish.at(i);
		if (scare.has_value())
		{
			const auto away = fish.position - *scare;
			if (glm::dot(away, away) < k_ScareReachSquared)
			{
				// It darts straight away from what scared it
				fish.panic = k_PanicSeconds;
				shoal.retargetSeconds = k_PanicSeconds;
				fish.heading = std::atan2(away.z, away.x);
			}
		}
		StepFish(fish, shoal.target, seconds);
	}
}

bool HasShownFishNear(const Shoal& shoal, glm::vec2 point)
{
	const auto shown = ShownCount(shoal.fullness);
	return std::any_of(shoal.fish.begin(), shoal.fish.begin() + shown, [point](const Fish& fish) {
		const glm::vec2 offset = glm::vec2(fish.position.x, fish.position.z) - point;
		return glm::dot(offset, offset) < k_HandReachSquared;
	});
}

std::array<glm::vec3, 4> Corners(const Fish& fish)
{
	const float cosine = std::cos(fish.heading);
	const float sine = std::sin(fish.heading);
	// Along the way it swims, and across it
	const glm::vec3 along = glm::vec3(cosine, 0.0f, sine) * fish.size;
	const glm::vec3 across = glm::vec3(-sine, 0.0f, cosine) * fish.size;
	return {
	    fish.position - along - across,
	    fish.position + along - across,
	    fish.position + along + across,
	    fish.position - along + across,
	};
}

std::array<glm::vec2, 4> AtlasCorners(uint32_t frame)
{
	constexpr float k_Cell = 1.0f / static_cast<float>(k_AtlasColumns);
	const glm::vec2 origin =
	    glm::vec2(static_cast<float>(frame % k_AtlasColumns), static_cast<float>(frame / k_AtlasColumns)) * k_Cell;
	return {
	    origin,
	    origin + glm::vec2(k_Cell, 0.0f),
	    origin + glm::vec2(k_Cell, k_Cell),
	    origin + glm::vec2(0.0f, k_Cell),
	};
}

} // namespace openblack::fish_shoal
