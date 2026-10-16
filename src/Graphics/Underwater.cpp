/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Underwater.h"

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <array>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/geometric.hpp>

using namespace openblack::graphics;

float underwater::PhaseBrightness(float phase)
{
	return 0.5f * (1.0f - std::cos(phase));
}

glm::vec3 underwater::RefractIntoWater(glm::vec3 towards)
{
	const float length = glm::length(towards);
	if (length <= 0.0f)
	{
		return {0.0f, 1.0f, 0.0f};
	}
	towards /= length;
	// Snell's law at a flat surface: the part along the surface gets smaller by the refractive index, and the light
	// keeps its upward direction. A light below the horizon still comes in at the critical angle.
	auto along = glm::vec3(towards.x, 0.0f, towards.z) / k_RefractiveIndex;
	const float alongSquared = glm::dot(along, along);
	float up = std::sqrt(std::max(0.0f, 1.0f - alongSquared));
	up = std::max(up, k_LowestRefractedUp);
	return glm::normalize(glm::vec3(along.x, up, along.z));
}

underwater::WaterLight underwater::BlendLights(const std::optional<SkyLight>& sun, const std::optional<SkyLight>& moon)
{
	const float sunWeight = sun.has_value() ? std::clamp(sun->strength, 0.0f, 1.0f) : 0.0f;
	const float moonWeight = moon.has_value() ? std::clamp(moon->strength, 0.0f, 1.0f) * k_MoonLight : 0.0f;
	const float total = sunWeight + moonWeight;
	WaterLight light;
	if (total <= 0.0f)
	{
		return light;
	}
	glm::vec3 towards {0.0f};
	glm::vec3 above {0.0f};
	const auto unit = [](const glm::vec3& v) {
		return glm::length(v) > 0.0f ? glm::normalize(v) : glm::vec3(0.0f, 1.0f, 0.0f);
	};
	if (sunWeight > 0.0f)
	{
		towards += RefractIntoWater(sun->towards) * sunWeight;
		above += unit(sun->towards) * sunWeight;
		light.colour += sun->colour * sunWeight;
	}
	if (moonWeight > 0.0f)
	{
		towards += RefractIntoWater(moon->towards) * moonWeight;
		above += unit(moon->towards) * moonWeight;
		light.colour += moon->colour * moonWeight;
	}
	light.towards = unit(towards);
	light.aboveTowards = unit(above);
	light.moonShare = moonWeight / total;
	return light;
}

glm::vec3 underwater::Transmittance(float distance)
{
	return glm::exp(-k_Absorption * std::max(distance, 0.0f));
}

glm::vec3 underwater::AmbientAtDepth(const glm::vec3& surfaceAmbient, float depth)
{
	// The light comes down from the surface, so it goes through the depth once. Part of it comes back from the light
	// that the water scatters, so less is lost than on a straight path.
	return surfaceAmbient * Transmittance(std::max(depth, 0.0f) * 0.6f);
}

glm::vec3 underwater::ScatterColour(const glm::vec3& skyColour, const WaterLight& light)
{
	// The water scatters the blue and the green most, so its own colour is a deep blue green, lit by the sky and by the
	// sun or the moon
	constexpr glm::vec3 k_WaterTint {0.10f, 0.42f, 0.55f};
	const glm::vec3 lit = skyColour * 0.55f + light.colour * 0.6f;
	return lit * k_WaterTint;
}

underwater::TourPose underwater::TourAt(float seconds)
{
	struct Key
	{
		float time;
		TourPose pose;
	};
	static constexpr std::array<Key, 7> k_Keys {{
	    {0.0f, {{1486.0f, 27.0f, 1973.0f}, {1441.0f, 0.0f, 2081.0f}}},
	    {4.0f, {{1478.0f, 6.0f, 1990.0f}, {1455.0f, -4.0f, 2050.0f}}},
	    {7.5f, {{1472.0f, -3.0f, 2000.0f}, {1458.0f, -8.0f, 2040.0f}}},
	    {12.0f, {{1465.0f, -8.0f, 2015.0f}, {1440.0f, -14.0f, 2060.0f}}},
	    {16.5f, {{1455.0f, -18.0f, 2040.0f}, {1430.0f, -4.0f, 2090.0f}}},
	    {21.0f, {{1470.0f, -5.0f, 2000.0f}, {1500.0f, -3.0f, 2060.0f}}},
	    {26.0f, {{1480.0f, 5.0f, 1985.0f}, {1455.0f, -2.0f, 2050.0f}}},
	}};
	const float time = std::clamp(seconds, 0.0f, k_TourSeconds);
	size_t next = 1;
	while (next + 1 < k_Keys.size() && k_Keys.at(next).time < time)
	{
		++next;
	}
	const auto& from = k_Keys.at(next - 1);
	const auto& to = k_Keys.at(next);
	auto share = (time - from.time) / (to.time - from.time);
	// Eased at each key, so the camera does not jerk
	share = share * share * (3.0f - 2.0f * share);
	return {glm::mix(from.pose.eye, to.pose.eye, share), glm::mix(from.pose.focus, to.pose.focus, share)};
}

glm::vec2 underwater::FarTourGround(float seconds)
{
	const float share = std::clamp(seconds / k_FarTourSeconds, 0.0f, 1.0f);
	// Out to the west, swinging a little from side to side
	const float x = 1450.0f - share * 1350.0f;
	const float z = 2000.0f + 90.0f * std::sin(share * 6.2831f);
	return {x, z};
}

float underwater::TideLevel(float scriptHour, float moonPhase)
{
	// 1 at a full moon, 0 at a new moon; the spring tides are at both
	const float full = PhaseBrightness(moonPhase);
	const float spring = std::abs(std::cos(moonPhase));
	const float high = -k_NeapHighDrop * (1.0f - full);
	const float low = -k_LowestTide * (k_NeapLowShare + (1.0f - k_NeapLowShare) * spring);
	// High at midnight and midday, low at six
	const float rise = 0.5f + 0.5f * std::cos(scriptHour * 2.0f * std::numbers::pi_v<float> / 12.0f);
	return glm::mix(low, high, rise);
}

underwater::WaveState underwater::WavesFor(float storm, float windAngle)
{
	storm = std::clamp(storm, 0.0f, 1.0f);
	return {.scale = 1.0f + storm * 3.5f, .storm = storm, .windAngle = windAngle, .swash = 0.25f + storm * 0.55f};
}

std::array<glm::vec4, underwater::k_WaveCount> underwater::WaveTable(const WaveState& waves)
{
	std::array<glm::vec4, k_WaveCount> table {};
	// A storm makes the wind sea's waves shorter and higher, and its short chop much higher
	const float shorter = 1.0f - 0.35f * waves.storm;
	// The swell: two long waves from far away, a little higher in a storm
	table.at(0) = {k_SwellAngle, 120.0f, 0.22f * (1.0f + 0.5f * waves.storm), 0.7f};
	table.at(1) = {k_SwellAngle + 0.15f, 92.0f, 0.14f * (1.0f + 0.5f * waves.storm), 3.9f};
	// The wind sea: lengths falling by a share each, directions spread about the wind's, wider for the shorter waves
	constexpr std::array<float, 12> k_Spread {0.0f, 0.35f, -0.45f, 0.6f, -0.2f, 0.85f, -0.7f, 0.3f, -1.0f, 0.95f, -0.5f, 0.65f};
	constexpr std::array<float, 12> k_Phase {0.0f, 1.7f, 4.1f, 2.3f, 5.2f, 0.9f, 3.3f, 6.0f, 1.2f, 4.6f, 2.8f, 5.7f};
	for (size_t i = 0; i < 12; ++i)
	{
		const float length = 70.0f * std::pow(0.78f, static_cast<float>(i));
		// The spectrum: height grows a little slower than length
		float height = 0.0042f * std::pow(length, 0.9f) * waves.scale;
		if (length < 12.0f)
		{
			height *= 1.0f + waves.storm * 2.5f;
		}
		table.at(i + 2) = {waves.windAngle + k_Spread.at(i), length * shorter, height, k_Phase.at(i)};
	}
	return table;
}

glm::vec2 underwater::WaveSets(glm::vec2 point, float seconds, const WaveState& waves)
{
	// A group of bigger waves comes in every eight waves of 55 units, at the group's speed, half the waves' own
	const glm::vec2 wind {std::cos(waves.windAngle), std::sin(waves.windAngle)};
	const float kWind = 2.0f * std::numbers::pi_v<float> / 440.0f;
	const float speedWind = 0.5f * std::sqrt(9.8f * 55.0f / (2.0f * std::numbers::pi_v<float>)) * 0.8f;
	const float sets = 0.5f + 0.5f * std::sin(kWind * glm::dot(wind, point) - kWind * speedWind * seconds);
	// Calmer patches that drift slowly over the sea
	const float patches = 0.75f + 0.25f * std::sin(point.x * 0.004f + point.y * 0.0017f + seconds * 0.05f) *
	                                  std::sin(point.y * 0.0031f - point.x * 0.0012f - seconds * 0.04f);
	// The swell's sets: every seven of its waves
	const glm::vec2 swell {std::cos(k_SwellAngle), std::sin(k_SwellAngle)};
	const float kSwell = 2.0f * std::numbers::pi_v<float> / 840.0f;
	const float speedSwell = 0.5f * std::sqrt(9.8f * 120.0f / (2.0f * std::numbers::pi_v<float>)) * 0.8f;
	const float swellSets = 0.5f + 0.5f * std::sin(kSwell * glm::dot(swell, point) - kSwell * speedSwell * seconds + 1.3f);
	return {(0.45f + 1.1f * sets * sets) * patches, 0.5f + 1.0f * swellSets};
}

underwater::WaveSample underwater::WavesAt(glm::vec2 point, float seconds, const WaveState& waves)
{
	WaveSample sample {.height = 0.0f, .slope = {0.0f, 0.0f}};
	const auto table = WaveTable(waves);
	const auto sets = WaveSets(point, seconds, waves);
	const float sharp = 1.0f + 1.5f * waves.storm;
	for (size_t i = 0; i < table.size(); ++i)
	{
		const auto& wave = table.at(i);
		const glm::vec2 along {std::cos(wave.x), std::sin(wave.x)};
		const float k = 2.0f * std::numbers::pi_v<float> / wave.y;
		const float omega = std::sqrt(9.8f * k) * 0.8f;
		const float amplitude = wave.z * (i < 2 ? sets.y : sets.x);
		const float theta = k * glm::dot(along, point) - omega * seconds + wave.w;
		const float s = 0.5f + 0.5f * std::sin(theta);
		sample.height += amplitude * (2.0f * std::pow(s, sharp) - 1.0f);
		const float rise = amplitude * sharp * std::pow(std::max(s, 1e-4f), sharp - 1.0f) * std::cos(theta) * k;
		sample.slope += along * rise;
	}
	return sample;
}

underwater::CurrentState underwater::CurrentFor(float tideRate, float windAngle, float storm)
{
	storm = std::clamp(storm, 0.0f, 1.0f);
	// The tide flows in towards the land as it rises and out as it falls, along one way over the whole map
	const glm::vec2 tideWay {0.6f, 0.8f};
	const glm::vec2 wind {std::cos(windAngle), std::sin(windAngle)};
	return {.flow = tideWay * (tideRate * 40.0f) + wind * (0.05f + 0.25f * storm),
	        .swirl = 1.0f + 3.0f * storm,
	        .gust = 0.4f + 1.6f * storm};
}

namespace
{
struct Swirl
{
	glm::vec2 k;
	float omega;
	float amplitude;
	float phase;
};
/// The swirls: the flow is the curl of a sum of these, so the water has no sinks. The shaders have the same table.
constexpr std::array<Swirl, 3> k_Swirls {{
    {{0.031f, 0.012f}, 0.15f, 4.0f, 0.0f},
    {{-0.018f, 0.047f}, 0.22f, 2.4f, 2.1f},
    {{0.062f, -0.051f}, 0.31f, 1.2f, 4.4f},
}};
/// A gust front passes every this many units along the wind, at this speed
constexpr float k_GustSpacing = 300.0f;
constexpr float k_GustSpeed = 6.0f;
} // namespace

glm::vec2 underwater::CurrentAt(glm::vec3 point, float seconds, const CurrentState& current, const WaveState& waves,
                                float seaLevel)
{
	const glm::vec2 p {point.x, point.z};
	glm::vec2 flow = current.flow;
	for (const auto& swirl : k_Swirls)
	{
		const float theta = glm::dot(swirl.k, p) + swirl.omega * seconds + swirl.phase;
		// The curl of amplitude * sin(theta) across the ground
		flow += glm::vec2(swirl.k.y, -swirl.k.x) * (swirl.amplitude * current.swirl * std::cos(theta));
	}
	const glm::vec2 wind {std::cos(waves.windAngle), std::sin(waves.windAngle)};
	const float front = glm::fract((glm::dot(p, wind) - k_GustSpeed * seconds) / k_GustSpacing);
	const float gust = glm::smoothstep(0.0f, 0.06f, front) * (1.0f - glm::smoothstep(0.06f, 0.22f, front));
	flow += wind * (gust * current.gust);
	// The waves move the water near the surface to and fro along the longest swell
	const float k = 2.0f * std::numbers::pi_v<float> / (61.0f * (1.0f - 0.35f * waves.storm));
	const float omega = std::sqrt(9.8f * k) * 0.8f;
	const float theta = k * glm::dot(wind, p) - omega * seconds;
	const float nearSurface = std::exp(std::min(point.y - seaLevel, 0.0f) * k);
	flow += wind * (0.30f * waves.scale * omega * std::sin(theta) * nearSurface);
	return flow;
}

float underwater::Storminess(float rain, float overcast, glm::vec2 wind)
{
	const float wet = std::clamp(rain / 100.0f, 0.0f, 1.0f);
	const float cloud = std::clamp((overcast - 40.0f) / 60.0f, 0.0f, 1.0f);
	const float blow = std::clamp(glm::length(wind) / 100.0f, 0.0f, 1.0f);
	return std::clamp(wet * 0.45f + cloud * 0.25f + blow * 0.5f, 0.0f, 1.0f);
}

float underwater::EaseTowards(float value, float target, float rate, float seconds)
{
	const float step = rate * std::max(seconds, 0.0f);
	return target < value ? std::max(target, value - step) : std::min(target, value + step);
}

float underwater::EyeDepth(float eyeHeight, float seaLevel)
{
	return std::max(seaLevel - eyeHeight, 0.0f);
}

namespace
{
// The fraction of a value, as the shaders' fract
float Fract(float value)
{
	return value - std::floor(value);
}

// The hash of the shaders (UwHash in underwater.sh)
float NoiseHash(glm::vec2 p)
{
	glm::vec3 q {Fract(p.x * 0.1031f), Fract(p.y * 0.1031f), Fract(p.x * 0.1031f)};
	const float add = glm::dot(q, glm::vec3(q.y, q.z, q.x) + 33.33f);
	q += add;
	return Fract((q.x + q.y) * q.z);
}
} // namespace

float underwater::Noise(glm::vec2 point)
{
	const glm::vec2 i = glm::floor(point);
	const glm::vec2 f = point - i;
	const glm::vec2 u = f * f * (3.0f - 2.0f * f);
	const float a = NoiseHash(i);
	const float b = NoiseHash(i + glm::vec2(1.0f, 0.0f));
	const float c = NoiseHash(i + glm::vec2(0.0f, 1.0f));
	const float d = NoiseHash(i + glm::vec2(1.0f, 1.0f));
	return glm::mix(glm::mix(a, b, u.x), glm::mix(c, d, u.x), u.y);
}

float underwater::Hash(glm::vec2 point)
{
	return NoiseHash(point);
}

glm::vec4 underwater::ShoreTexel(glm::vec4 shore, float cliff)
{
	const float shorten = 1.0f - 0.5f * cliff;
	return {shore.x, shore.y * shorten, shore.z * shorten, shore.w};
}

glm::vec4 underwater::ShoreAt(std::span<const glm::vec4> shore, std::span<const float> cliff, glm::ivec2 size, float spacing,
                              glm::vec2 point)
{
	if (size.x <= 0 || size.y <= 0 || shore.size() < static_cast<size_t>(size.x) * static_cast<size_t>(size.y))
	{
		return {1000.0f, 0.0f, 0.0f, 0.0f};
	}
	const glm::vec2 cell = glm::clamp(point / spacing, glm::vec2(0.0f), glm::vec2(size - 1));
	const glm::ivec2 low = glm::min(glm::ivec2(glm::floor(cell)), size - 2);
	const glm::vec2 share = cell - glm::vec2(low);
	const auto at = [&](int x, int z) {
		const auto i = static_cast<size_t>(z) * static_cast<size_t>(size.x) + static_cast<size_t>(x);
		return ShoreTexel(shore[i], i < cliff.size() ? cliff[i] : 0.0f);
	};
	const auto nearRow = glm::mix(at(low.x, low.y), at(low.x + 1, low.y), share.x);
	const auto farRow = glm::mix(at(low.x, low.y + 1), at(low.x + 1, low.y + 1), share.x);
	return glm::mix(nearRow, farRow, share.y);
}

float underwater::ShoreCliff(glm::vec4 shore)
{
	return std::clamp((1.0f - glm::length(glm::vec2(shore.y, shore.z))) * 2.0f, 0.0f, 1.0f);
}

underwater::CliffCrash underwater::CliffCrashAt(glm::vec2 foot, float seconds, const WaveState& waves, float seaLevel,
                                                float cliff)
{
	constexpr float k_Turn = 2.0f * std::numbers::pi_v<float>;
	// The same steps as UwCliffCrash in underwater.sh: when the shore wave reaches the coast here, and which crash it is
	const float omega = glm::mix(1.1f, 1.5f, waves.storm);
	const float theta = omega * seconds + Noise(foot * 0.008f) * 1.2f;
	const float period = k_Turn / omega;
	const float number = std::floor(theta / k_Turn);
	// Each crash has its own strength, the sets make some crashes stronger, and some parts of a cliff take more
	const float own = 0.6f + 0.8f * NoiseHash({number, 17.0f});
	const float sets = 0.55f + 0.45f * std::sin(number * 0.9f);
	const float along = 0.6f + 0.8f * Noise(foot * 0.012f + glm::vec2(3.0f, 7.0f));
	const float strength = own * sets * along;
	const float tide = std::clamp((seaLevel + k_LowestTide) / k_LowestTide, 0.0f, 1.0f);
	return {
	    .number = number,
	    .since = Fract(theta / k_Turn) * period,
	    .power = waves.swash * cliff * (0.7f + 0.3f * tide) * strength / 0.55f,
	    .seed = number * 3.7f + glm::dot(foot, glm::vec2(0.0173f, 0.0291f)),
	};
}

bool underwater::CrashStarts(const CliffCrash& before, const CliffCrash& now)
{
	return now.number != before.number;
}

std::vector<size_t> underwater::HeardCliffFeet(std::span<const glm::vec4> feet, glm::vec2 eye, size_t count, float spacing,
                                               float range)
{
	std::vector<std::pair<float, size_t>> inRange;
	for (size_t i = 0; i < feet.size(); ++i)
	{
		const float distance = glm::distance(glm::vec2(feet[i]), eye);
		if (distance <= range)
		{
			inRange.emplace_back(distance, i);
		}
	}
	std::ranges::sort(inRange);
	std::vector<size_t> heard;
	for (const auto& [distance, index] : inRange)
	{
		if (heard.size() >= count)
		{
			break;
		}
		const glm::vec2 foot {feet[index]};
		const bool apart =
		    std::ranges::none_of(heard, [&](size_t other) { return glm::distance(glm::vec2(feet[other]), foot) < spacing; });
		if (apart)
		{
			heard.push_back(index);
		}
	}
	return heard;
}
