/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the one wind of the experiment

#include "WindRules.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack;

wind::Wind wind::Target(glm::vec2 weatherWind, float storm)
{
	const float strength = glm::length(weatherWind);
	return {.angle = strength > k_TurnThreshold ? std::atan2(weatherWind.y, weatherWind.x) : k_CalmAngle,
	        .speed = k_CalmSpeed + strength * k_SpeedPerPercent,
	        .storm = std::clamp(storm, 0.0f, 1.0f)};
}

wind::Wind wind::Step(const Wind& wind, const Wind& target, float seconds)
{
	if (seconds <= 0.0f)
	{
		return wind;
	}
	Wind next = wind;
	const float turn = std::remainder(target.angle - wind.angle, 2.0f * std::numbers::pi_v<float>);
	next.angle = std::remainder(wind.angle + std::clamp(turn, -k_TurnRate * seconds, k_TurnRate * seconds),
	                            2.0f * std::numbers::pi_v<float>);
	const float ease = 1.0f - std::exp(-seconds / k_SpeedSeconds);
	next.speed += (target.speed - wind.speed) * ease;
	next.storm += (target.storm - wind.storm) * ease;
	return next;
}

glm::vec2 wind::Direction(const Wind& wind)
{
	return {std::cos(wind.angle), std::sin(wind.angle)};
}

float wind::Gust(const Wind& wind, glm::vec2 point, float seconds)
{
	// Three trains of gust fronts at other lengths, each moving downwind at about the wind's speed, and a slow swell
	const auto along = glm::dot(point, Direction(wind));
	const glm::vec2 side {-Direction(wind).y, Direction(wind).x};
	const float across = glm::dot(point, side);
	const float travel = along - wind.speed * seconds;
	const float fronts = 0.5f * std::sin(travel / 37.0f + std::sin(across / 61.0f)) +
	                     0.3f * std::sin(travel / 15.0f + across / 43.0f) + 0.2f * std::sin(seconds * 0.21f);
	const float strength = 0.18f + 0.22f * wind.storm;
	return std::clamp(1.0f + fronts * strength, 0.5f, 1.6f);
}

glm::vec2 wind::VelocityAt(const Wind& wind, glm::vec2 point, float seconds)
{
	return Direction(wind) * wind.speed * Gust(wind, point, seconds);
}
