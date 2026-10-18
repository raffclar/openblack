/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlaceLight.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/common.hpp>

namespace place_light = openblack::graphics::place_light;

namespace
{
constexpr float k_Degrees = 180.0f / std::numbers::pi_v<float>;
constexpr int k_LastStepKnot = place_light::k_Knots - 2;

float Smoothstep(float low, float high, float x)
{
	const float t = std::clamp((x - low) / (high - low), 0.0f, 1.0f);
	return t * t * (3.0f - (2.0f * t));
}
} // namespace

float place_light::KnotDegrees(int knot)
{
	const int clamped = std::clamp(knot, 0, k_Knots - 1);
	return clamped > k_LastStepKnot ? k_TopDegrees : k_LowestDegrees + (static_cast<float>(clamped) * k_StepDegrees);
}

place_light::KnotPlace place_light::PlaceOf(float sunHeight)
{
	const float degrees = std::asin(std::clamp(sunHeight, -1.0f, 1.0f)) * k_Degrees;
	const float lastStep = KnotDegrees(k_LastStepKnot);
	if (degrees >= lastStep)
	{
		return {.knot = k_LastStepKnot, .share = std::clamp((degrees - lastStep) / (k_TopDegrees - lastStep), 0.0f, 1.0f)};
	}
	const float along = std::max((degrees - k_LowestDegrees) / k_StepDegrees, 0.0f);
	const int knot = std::min(static_cast<int>(along), k_LastStepKnot - 1);
	return {.knot = knot, .share = std::clamp(along - static_cast<float>(knot), 0.0f, 1.0f)};
}

place_light::Knot place_light::KnotAt(float sunHeight, const Inputs& inputs)
{
	return {
	    .sunWater = inputs.sunColour * (inputs.sunStrength ? inputs.sunStrength(sunHeight) : 0.0f),
	    .glow = inputs.nightGlow ? inputs.nightGlow(sunHeight) : 0.0f,
	    .sky = inputs.skyColour ? inputs.skyColour(sunHeight) : glm::vec3(0.0f),
	    .airTint = inputs.airTint ? inputs.airTint(sunHeight) : glm::vec3(1.0f),
	    .airScale = glm::vec3(1.0f),
	};
}

place_light::Knot place_light::Interpolate(std::span<const Knot, k_Knots> knots, float sunHeight)
{
	const auto place = PlaceOf(sunHeight);
	const auto& a = knots[static_cast<size_t>(place.knot)];
	const auto& b = knots[static_cast<size_t>(place.knot + 1)];
	const float t = place.share;
	return {
	    .sunWater = glm::mix(a.sunWater, b.sunWater, t),
	    .glow = glm::mix(a.glow, b.glow, t),
	    .sky = glm::mix(a.sky, b.sky, t),
	    .airTint = glm::mix(a.airTint, b.airTint, t),
	    .airScale = glm::mix(a.airScale, b.airScale, t),
	};
}

std::array<glm::vec4, place_light::k_Knots * place_light::k_VectorsPerKnot>
place_light::Pack(std::span<const Knot, k_Knots> knots)
{
	std::array<glm::vec4, k_Knots * k_VectorsPerKnot> packed {};
	for (size_t i = 0; i < knots.size(); ++i)
	{
		const auto& knot = knots[i];
		const size_t at = i * k_VectorsPerKnot;
		packed[at] = glm::vec4(knot.sunWater, knot.glow);
		packed[at + 1] = glm::vec4(knot.sky, 0.0f);
		packed[at + 2] = glm::vec4(knot.airTint, 0.0f);
		packed[at + 3] = glm::vec4(knot.airScale, 0.0f);
	}
	return packed;
}

float place_light::IslandShare(float upY)
{
	return Smoothstep(std::cos(k_IslandFar), std::cos(k_IslandNear), upY);
}

float place_light::MoonStrengthAt(float fullStrength, float moonHeight, float glow)
{
	// The air takes more of the moon's light near the horizon, and none shows when the moon is down
	const float through = Smoothstep(-0.05f, 0.3f, moonHeight);
	return std::max(fullStrength * through, glow);
}
