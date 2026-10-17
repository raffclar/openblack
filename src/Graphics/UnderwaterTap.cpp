/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the splash where the hand taps the water

#include "UnderwaterTap.h"

#include <cmath>

#include <algorithm>
#include <iterator>
#include <numbers>
#include <vector>

#include <glm/common.hpp>

namespace openblack::graphics::underwater::tap
{

namespace
{
constexpr float k_Metre = streams::k_UnitsPerMetre;
constexpr float k_G = streams::k_Gravity;

/// The fraction of a number
float Fraction(float value)
{
	return value - std::floor(value);
}
} // namespace

TapSplash SplashFor(float strength, float calm, glm::vec3 seed)
{
	const float hard = std::clamp(strength, k_SoftestTap, k_HardestTap);
	const float still = std::clamp(calm, 0.0f, 1.0f);
	TapSplash splash;
	splash.sheetRadius = (1.6f + 0.6f * hard) * k_Metre;
	splash.sheetHeight = 2.4f * hard * (1.0f - 0.5f * still) * k_Metre;
	splash.dropSpeed = (4.0f + 2.5f * hard) * (1.0f - 0.35f * still) * k_Metre;
	const float many = (30.0f + 50.0f * (hard - k_SoftestTap)) * (1.0f - 0.5f * still);
	splash.drops = static_cast<uint32_t>(std::clamp(many, 12.0f, static_cast<float>(k_DropParticles)));
	splash.spray = std::clamp(0.6f * hard * (1.0f - 0.7f * still), 0.0f, 1.0f);
	splash.ringDepth = 0.06f * hard * (1.0f - 0.3f * still) * k_Metre;
	splash.ringRadius = splash.sheetRadius * 0.35f;
	// The splash shows until its last drop is down and its spray has gone
	const float flight = 2.0f * splash.dropSpeed / k_G;
	splash.seconds = std::max(flight + 0.8f, 2.6f);
	splash.seed = glm::fract(seed);
	return splash;
}

float StrengthForHandSpeed(float speed)
{
	// A still hand taps softly; a hand that moves at 10 metres a second or more taps as hard as it can
	const float share = std::clamp(speed / (10.0f * k_Metre), 0.0f, 1.0f);
	return glm::mix(0.8f, k_HardestTap, share);
}

Drop DropOf(const TapSplash& splash, uint32_t index)
{
	// The numbers of each drop come from four sequences that spread evenly, the same in the shader
	const auto i = static_cast<float>(index);
	const float h1 = Fraction(i * 0.6180340f + splash.seed.x);
	const float h2 = Fraction(i * 0.7548777f + splash.seed.y);
	const float h3 = Fraction(i * 0.5698403f + splash.seed.z);
	const float h4 = Fraction(i * 0.4142136f + splash.seed.x + splash.seed.y);
	const float angle = h1 * 2.0f * std::numbers::pi_v<float>;
	const glm::vec2 out {std::cos(angle), std::sin(angle)};
	const float rise = std::sqrt(2.0f * k_G * std::max(splash.sheetHeight, 0.01f));
	const float apex = rise / k_G;
	Drop drop;
	drop.start = out * splash.sheetRadius * (0.7f + 0.5f * h3);
	const float outward = splash.dropSpeed * (0.15f + 0.35f * h4);
	drop.velocity = {out.x * outward, splash.dropSpeed * (0.5f + 0.5f * h2), out.y * outward};
	drop.delay = 0.12f * h2 * apex;
	drop.size = (0.025f + 0.045f * h3) * k_Metre;
	drop.shown = index < splash.drops;
	return drop;
}

Landing LandingOf(const Drop& drop)
{
	const float flight = 2.0f * drop.velocity.y / k_G;
	return {.position = drop.start + glm::vec2(drop.velocity.x, drop.velocity.z) * flight,
	        .seconds = drop.delay + flight,
	        .size = drop.size};
}

std::vector<Landing> RingLandings(const TapSplash& splash)
{
	std::vector<Drop> drops;
	for (uint32_t i = 0; i < splash.drops && i < k_DropParticles; ++i)
	{
		drops.push_back(DropOf(splash, i));
	}
	std::ranges::sort(drops, [](const Drop& a, const Drop& b) { return a.size > b.size; });
	if (drops.size() > k_MostDropRings)
	{
		drops.resize(k_MostDropRings);
	}
	std::vector<Landing> landings;
	landings.reserve(drops.size());
	std::ranges::transform(drops, std::back_inserter(landings), LandingOf);
	std::ranges::sort(landings, [](const Landing& a, const Landing& b) { return a.seconds < b.seconds; });
	return landings;
}

float DropRingDepth(float size)
{
	return std::clamp(size * 1.8f, 0.02f, 0.25f);
}

} // namespace openblack::graphics::underwater::tap
