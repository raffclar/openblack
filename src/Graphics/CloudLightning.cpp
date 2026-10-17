/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CloudLightning.h"

#include <cmath>

#include <algorithm>
#include <numbers>
#include <ranges>

#include <glm/geometric.hpp>

using namespace openblack::graphics;

namespace
{

uint32_t Mix(uint32_t value)
{
	value ^= value >> 16u;
	value *= 0x7feb352du;
	value ^= value >> 15u;
	value *= 0x846ca68bu;
	value ^= value >> 16u;
	return value;
}

} // namespace

uint32_t cloud_lightning::SeedAt(glm::vec2 position)
{
	const auto x = static_cast<uint32_t>(static_cast<int32_t>(std::floor(position.x)));
	const auto z = static_cast<uint32_t>(static_cast<int32_t>(std::floor(position.y)));
	return Mix(x * 0x9e3779b9u ^ Mix(z + 0x632be5abu));
}

float cloud_lightning::RandomOf(uint32_t seed, uint32_t index)
{
	return static_cast<float>(Mix(seed ^ Mix(index + 0x1b873593u)) >> 8u) / static_cast<float>(1u << 24u);
}

float cloud_lightning::FlickerSeconds(uint32_t seed)
{
	return k_ShortestFlicker + (k_LongestFlicker - k_ShortestFlicker) * RandomOf(seed, 0);
}

uint32_t cloud_lightning::PulseCount(uint32_t seed)
{
	const auto spread = static_cast<float>(k_MostPulses - k_FewestPulses + 1);
	return std::min(k_FewestPulses + static_cast<uint32_t>(RandomOf(seed, 1) * spread), k_MostPulses);
}

float cloud_lightning::FlickerEnvelope(float seconds, uint32_t seed)
{
	const float duration = FlickerSeconds(seed);
	const float end = duration + 4.0f * k_PulseFade;
	if (seconds < 0.0f || seconds > end)
	{
		return 0.0f;
	}
	const uint32_t pulses = PulseCount(seed);
	float brightest = 0.0f;
	for (uint32_t i = 0; i < pulses; ++i)
	{
		// The first pulse is at the strike and is the brightest. The others come later and are weaker.
		const float share = static_cast<float>(i) / static_cast<float>(pulses - 1);
		const float start = i == 0 ? 0.0f : duration * share * (0.7f + 0.3f * RandomOf(seed, 2 + i));
		const float peak = i == 0 ? 1.0f : 0.35f + 0.55f * RandomOf(seed, 10 + i);
		const float after = seconds - start;
		if (after >= 0.0f)
		{
			const float rise = std::min(after / k_PulseRise, 1.0f);
			brightest = std::max(brightest, peak * rise * std::exp(-after / k_PulseFade));
		}
	}
	// The channel glows dimly between the pulses, and the glow fades out after the last pulse
	const float fadeOut = std::clamp((end - seconds) / (end - duration), 0.0f, 1.0f);
	const float glow = 0.12f * std::exp(-seconds / (duration * 0.5f)) * fadeOut;
	return std::clamp(std::max(brightest, glow), 0.0f, 1.0f);
}

glm::vec2 cloud_lightning::FlashPlace(const Strike& strike, uint32_t seed)
{
	const glm::vec2 offset = strike.position - strike.stormCentre;
	const float distance = glm::length(offset);
	if (distance <= strike.stormRadius)
	{
		return strike.position;
	}
	const float inside = strike.stormRadius * (0.45f + 0.35f * RandomOf(seed, 30));
	// With no direction, the flash takes a direction of its own
	const float angle = RandomOf(seed, 31) * 2.0f * std::numbers::pi_v<float>;
	const glm::vec2 towards = distance > 1e-3f ? offset / distance : glm::vec2 {std::cos(angle), std::sin(angle)};
	return strike.stormCentre + towards * inside;
}

cloud_lightning::FlashKind cloud_lightning::KindAt(float distance)
{
	return distance > k_SheetDistance ? FlashKind::Sheet : FlashKind::Bolt;
}

cloud_lightning::FlashLight cloud_lightning::LightFor(const FlashEvent& flash, const glm::vec3& eye, float cloudBottom,
                                                      float cloudDepth)
{
	// The channel starts near the bottom of the cloud over the strike, so that it lights the bottom brightly. It goes
	// across and up the cloud in a direction of its own.
	const float angle = RandomOf(flash.seed, 20) * 2.0f * std::numbers::pi_v<float>;
	const float length = k_ShortestChannel + (k_LongestChannel - k_ShortestChannel) * RandomOf(flash.seed, 21);
	const float rise = cloudDepth * (0.2f + 0.25f * RandomOf(flash.seed, 22));
	const glm::vec3 start {flash.place.x, cloudBottom + cloudDepth * 0.15f, flash.place.y};
	const glm::vec3 end = start + glm::vec3 {std::cos(angle) * length, rise, std::sin(angle) * length};
	const float distance = glm::distance(eye, (start + end) * 0.5f);
	const bool sheet = KindAt(distance) == FlashKind::Sheet;
	return {
	    .start = start,
	    .end = end,
	    .brightness = flash.strength * FlickerEnvelope(flash.since, flash.seed) * (sheet ? k_SheetShare : 1.0f),
	    .radius = sheet ? k_SheetRadius : k_BoltRadius,
	};
}

cloud_lightning::FlashEvents cloud_lightning::TrackFlashes(const FlashEvents& before, std::span<const Strike> strikes,
                                                           float seconds)
{
	FlashEvents after;
	for (auto flash : before.All())
	{
		flash.since += seconds;
		if (flash.since <= k_FlashMemory)
		{
			after.events.at(after.count++) = flash;
		}
	}
	for (const auto& strike : strikes)
	{
		auto known = std::ranges::find_if(
		    after.events.begin(), after.events.begin() + static_cast<std::ptrdiff_t>(after.count),
		    [&strike](const FlashEvent& flash) { return glm::distance(flash.position, strike.position) <= 1.0f; });
		const bool found = known != after.events.begin() + static_cast<std::ptrdiff_t>(after.count);
		if (found && strike.time >= known->strikeTime)
		{
			known->strikeTime = strike.time;
			continue;
		}
		const uint32_t seed = SeedAt(strike.position) ^ Mix(static_cast<uint32_t>(strike.strength * 1000.0f));
		const FlashEvent fresh {.position = strike.position,
		                        .place = FlashPlace(strike, seed),
		                        .since = strike.time,
		                        .strength = strike.strength,
		                        .seed = seed,
		                        .strikeTime = strike.time};
		if (found)
		{
			*known = fresh;
		}
		else if (after.count < k_MaxFlashes)
		{
			after.events.at(after.count++) = fresh;
		}
		else
		{
			auto oldest = std::ranges::max_element(after.events, {}, &FlashEvent::since);
			*oldest = fresh;
		}
	}
	return after;
}

std::array<glm::vec4, cloud_lightning::k_MaxFlashes * 2> cloud_lightning::Pack(std::span<const FlashLight> lights)
{
	std::array<glm::vec4, k_MaxFlashes * 2> packed {};
	const size_t count = std::min(lights.size(), k_MaxFlashes);
	for (size_t i = 0; i < count; ++i)
	{
		packed.at(i) = {lights[i].start, lights[i].brightness};
		packed.at(k_MaxFlashes + i) = {lights[i].end, lights[i].radius};
	}
	return packed;
}
