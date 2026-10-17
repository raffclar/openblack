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
#include <numeric>
#include <span>
#include <utility>
#include <vector>

#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

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
	// When the two lights are weak, the direction moves towards straight up. Thus it turns smoothly from the sun to
	// the moon, which stand on the two sides of the sky, and it does not jump.
	const float upWeight = std::max(k_UpBlend - total, 0.0f);
	towards.y += upWeight;
	above.y += upWeight;
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
	// The water scatters the green and the blue most, so its own colour is a dark green teal, lit by the sky and by the
	// sun or the moon. It is darker than the sky, so the water does not look like a mirror of it.
	constexpr glm::vec3 k_WaterTint {0.07f, 0.28f, 0.25f};
	const glm::vec3 lit = skyColour * 0.5f + light.colour * 0.5f;
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
	WaveState waves {.scale = 1.0f + storm * 3.5f, .storm = storm, .windAngle = windAngle, .swash = 0.25f + storm * 0.55f};
	// The waves that come in to the coast: the sum of the heights of the swell and of the two longest waves of the wind
	// sea, at the steepness that a long wind sea has (0.012 on a calm day to 0.025 in a storm)
	const auto table = WaveTable(waves);
	const float longSteepness = (0.012f + 0.013f * storm) * std::min(waves.scale, 1.0f);
	waves.swellHeight = table.at(0).z + table.at(1).z + (table.at(2).y + table.at(3).y) * longSteepness;
	return waves;
}

std::array<glm::vec4, underwater::k_WaveCount> underwater::WaveTable(const WaveState& waves)
{
	std::array<glm::vec4, k_WaveCount> table {};
	// The lengths of the waves stay the same in all weather, so a change of the weather never moves their crests. The
	// weather changes only how high each wave is. scale is 1 for the usual sea and 0 for no waves.
	const float on = std::min(waves.scale, 1.0f);
	// The swell from far away: 160 m and 115 m long, 1 m and 0.7 m high on a calm day, three times that in a storm
	const float swell = on * (1.0f + 2.0f * waves.storm);
	table.at(0) = {k_SwellAngle, Metres(160.0f), Metres(1.0f) * swell, 0.7f};
	table.at(1) = {k_SwellAngle + 0.15f, Metres(115.0f), Metres(0.7f) * swell, 3.9f};
	// The wind sea: 75 m long to 5 m, each 0.78 of the one before, directions spread about the wind's. The steepness
	// (height for each length) has a peak at the middle lengths, as a real wind sea's spectrum does: on a calm or breezy
	// day the waves of 8 m to 20 m are the high ones, which gives broad, rolling waves. The long waves of the wind sea
	// are low. A storm makes every wave higher and moves the peak to longer waves.
	constexpr std::array<float, 12> k_Spread {0.0f, 0.35f, -0.45f, 0.6f, -0.2f, 0.85f, -0.7f, 0.3f, -1.0f, 0.95f, -0.5f, 0.65f};
	constexpr std::array<float, 12> k_Phase {0.0f, 1.7f, 4.1f, 2.3f, 5.2f, 0.9f, 3.3f, 6.0f, 1.2f, 4.6f, 2.8f, 5.7f};
	const auto peak = [](float length, float middle) {
		const float far = std::log(length / middle) / k_WindSeaPeakWidth;
		return std::exp(-0.5f * far * far);
	};
	for (size_t i = 0; i < 12; ++i)
	{
		const float length = Metres(75.0f) * std::pow(0.78f, static_cast<float>(i));
		const float steepness = k_WindSeaBase + 0.012f * waves.storm +
		                        k_WindSeaPeak * peak(length, Metres(k_WindSeaPeakMetres)) +
		                        0.02f * waves.storm * peak(length, Metres(28.0f));
		float height = length * steepness * on;
		// The shortest chop is low, so that the waves read as broad and chunky; a storm makes it higher
		if (length < Metres(6.0f))
		{
			height *= 0.6f * (1.0f + waves.storm);
		}
		table.at(i + 2) = {waves.windAngle + k_Spread.at(i), length, height, k_Phase.at(i)};
	}
	return table;
}

float underwater::ShoreOmega(float storm)
{
	// A wave comes in every 8 seconds on a calm day and every 11 seconds in a storm
	const float period = 8.0f + 3.0f * std::clamp(storm, 0.0f, 1.0f);
	return 2.0f * std::numbers::pi_v<float> / period;
}

float underwater::AdvanceShorePhase(float phase, float storm, float seconds)
{
	constexpr float k_Range = 64.0f * 2.0f * std::numbers::pi_v<float>;
	const float next = phase + ShoreOmega(storm) * std::max(seconds, 0.0f);
	return next >= k_Range ? next - k_Range : next;
}

float underwater::Shoaling(float k, float depth)
{
	if (depth <= 0.0f)
	{
		return 0.0f;
	}
	// The wave number in this depth, from the dispersion of linear waves (an explicit approximation)
	const float deep = k * depth;
	if (deep > 6.0f)
	{
		return 1.0f;
	}
	const float kh = deep / std::sqrt(std::tanh(deep));
	// The speed of the wave's energy against its speed in deep water
	const float group = std::tanh(kh) * (1.0f + 2.0f * kh / std::sinh(2.0f * kh));
	return 1.0f / std::sqrt(std::max(group, 1e-4f));
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

float underwater::OpenSeaSize(glm::vec2 point, float seconds, float shoreDistance)
{
	// Out at sea the waves vary more: wide patches of bigger and smaller swell drift slowly over the sea, and the
	// waves grow with the distance from the coast
	const float far = glm::smoothstep(Metres(60.0f), Metres(500.0f), shoreDistance);
	const float patch = 0.5f + 0.5f * std::sin(point.x * 0.0021f + point.y * 0.0013f + seconds * 0.01f) *
	                               std::sin(point.y * 0.0017f - point.x * 0.0009f - seconds * 0.008f);
	return glm::mix(0.85f, 0.7f + 0.9f * patch, far) * (1.0f + 0.35f * far);
}

underwater::WaveVariation underwater::WaveVariationAt(glm::vec2 point, float seconds)
{
	// The warp: four slow waves of position, 70 m to 130 m long, that bend the crests and make the waves longer or
	// shorter from place to place
	constexpr float k_Tau = 2.0f * std::numbers::pi_v<float>;
	const float amplitude = Metres(k_WarpMetres);
	const float f1 = k_Tau / Metres(110.0f);
	const float f2 = k_Tau / Metres(130.0f);
	const float f3 = k_Tau / Metres(70.0f);
	const float f4 = k_Tau / Metres(85.0f);
	const glm::vec2 d1 {0.8f, 0.6f};
	const glm::vec2 d2 {-0.6f, 0.8f};
	const glm::vec2 d3 {0.28f, -0.96f};
	const glm::vec2 d4 {0.96f, 0.28f};
	const float a1 = f1 * glm::dot(d1, point) + 0.11f * seconds;
	const float a2 = f2 * glm::dot(d2, point) - 0.09f * seconds;
	const float a3 = f3 * glm::dot(d3, point) - 0.07f * seconds + 1.3f;
	const float a4 = f4 * glm::dot(d4, point) + 0.05f * seconds + 2.9f;
	WaveVariation variation {};
	variation.warp = amplitude * glm::vec2(std::sin(a1) + 0.5f * std::sin(a3), std::sin(a2) + 0.5f * std::sin(a4));
	const glm::vec2 dx = amplitude * (glm::vec2(f1 * d1.x * std::cos(a1), f2 * d2.x * std::cos(a2)) +
	                                  0.5f * glm::vec2(f3 * d3.x * std::cos(a3), f4 * d4.x * std::cos(a4)));
	const glm::vec2 dz = amplitude * (glm::vec2(f1 * d1.y * std::cos(a1), f2 * d2.y * std::cos(a2)) +
	                                  0.5f * glm::vec2(f3 * d3.y * std::cos(a3), f4 * d4.y * std::cos(a4)));
	variation.warpSlope = {dx, dz};
	// The groups: patches 60 m to 90 m across where the waves are higher or lower, which drift slowly
	const float g1 = k_Tau / Metres(90.0f);
	const float g2 = k_Tau / Metres(60.0f);
	const glm::vec2 e1 {0.92f, 0.39f};
	const glm::vec2 e2 {-0.31f, 0.95f};
	const float b1 = g1 * glm::dot(e1, point) + 0.04f * seconds;
	const float b2 = g2 * glm::dot(e2, point) - 0.03f * seconds;
	const float b3 = g2 * glm::dot(e1, point) - 0.05f * seconds + 2.1f;
	const float b4 = g1 * glm::dot(e2, point) + 0.035f * seconds + 0.7f;
	constexpr float k_Range = 0.5f * k_GroupRange;
	variation.groups = {1.0f + k_Range * std::sin(b1) * std::sin(b2), 1.0f + k_Range * std::sin(b3) * std::sin(b4)};
	const glm::vec2 even = k_Range * (std::cos(b1) * std::sin(b2) * g1 * e1 + std::sin(b1) * std::cos(b2) * g2 * e2);
	const glm::vec2 odd = k_Range * (std::cos(b3) * std::sin(b4) * g2 * e1 + std::sin(b3) * std::cos(b4) * g1 * e2);
	variation.groupSlope = {even.x, odd.x, even.y, odd.y};
	return variation;
}

underwater::WaveTilt underwater::WaveTiltAt(glm::vec2 point, float seconds)
{
	constexpr float k_Tau = 2.0f * std::numbers::pi_v<float>;
	const float h1 = k_Tau / Metres(260.0f);
	const float h2 = k_Tau / Metres(190.0f);
	const glm::vec2 q1 {0.6f, -0.8f};
	const glm::vec2 q2 {0.95f, 0.31f};
	const float c1 = h1 * glm::dot(q1, point) + 0.012f * seconds + 0.4f;
	const float c2 = h2 * glm::dot(q2, point) - 0.009f * seconds + 1.9f;
	return WaveTilt {.tilt = std::sin(c1) * std::sin(c2),
	                 .slope = std::cos(c1) * std::sin(c2) * h1 * q1 + std::sin(c1) * std::cos(c2) * h2 * q2};
}

namespace
{
/// One octave of the chop at a point of its grid, 0 in the trough to 1 on the crest
float ChopOctave(glm::vec2 q, float pointed)
{
	q += glm::vec2(underwater::Noise(q) * 2.0f - 1.0f);
	const glm::vec2 edge = glm::vec2(1.0f) - glm::abs(glm::sin(q));
	const glm::vec2 trough = glm::mix(edge, glm::abs(glm::cos(q)), edge);
	return std::pow(1.0f - std::pow(trough.x * trough.y, 0.6f), pointed);
}
} // namespace

float underwater::ChopAt(glm::vec2 point, float seconds, const WaveState& waves, int octaves)
{
	float amplitude = Metres(k_ChopMetres) * (1.0f + 2.0f * waves.storm) * std::min(waves.scale, 1.0f);
	if (amplitude <= 0.0f)
	{
		return 0.0f;
	}
	const glm::vec2 wind {std::cos(waves.windAngle), std::sin(waves.windAngle)};
	float k = 2.0f * std::numbers::pi_v<float> / Metres(k_ChopLengthMetres);
	glm::vec2 q = glm::vec2(glm::dot(point, wind), wind.x * point.y - wind.y * point.x) * k;
	float pointed = 4.0f;
	float height = 0.0f;
	for (int i = 0; i < std::min(octaves, k_ChopShadingOctaves); ++i)
	{
		const float drift = std::sqrt(k_Gravity * k) * seconds;
		const float pair = ChopOctave(q + glm::vec2(drift, 0.0f), pointed) + ChopOctave(q - glm::vec2(drift, 0.0f), pointed);
		height += (pair - 2.0f * (0.42f + 0.028f * static_cast<float>(i))) * amplitude;
		q = glm::vec2(1.6f * q.x + 1.2f * q.y, 1.6f * q.y - 1.2f * q.x) * 1.9f;
		k *= 3.8f;
		amplitude *= 0.22f;
		pointed = glm::mix(pointed, 1.0f, 0.2f);
	}
	return height;
}

float underwater::WhitecapShare(float envelope, float storm)
{
	const float start = 0.9f - 0.08f * storm;
	return glm::smoothstep(start, start + 0.12f, envelope);
}

float underwater::WhitecapCover(float storm)
{
	return 0.005f + 0.2f * std::pow(storm, 1.6f);
}

float underwater::CapSteepness(float height, float limit, float& rate)
{
	const float over = height / limit;
	const float share = 1.0f / (1.0f + over * over);
	rate = share * std::sqrt(share);
	return height * std::sqrt(share);
}

float underwater::TiltScale(float tilt, float length, float& rate)
{
	const float lengthLog = k_TiltStrength * std::log(std::max(length, 1e-3f) / Metres(k_TiltMetres));
	const float scale = std::exp(tilt * lengthLog);
	rate = scale * lengthLog;
	return scale;
}

underwater::WaveSample underwater::WavesAt(glm::vec2 point, float seconds, const WaveState& waves)
{
	return WavesAt(point, seconds, waves, std::nullopt, 1e6f, 1e6f);
}

underwater::WaveSample underwater::WavesInDepth(glm::vec2 point, float seconds, const WaveState& waves, float shoreDistance,
                                                float depth)
{
	WaveSample sample {.height = 0.0f, .slope = {0.0f, 0.0f}};
	const auto table = WaveTable(waves);
	const auto far = FarWaveTable(waves);
	auto sets = WaveSets(point, seconds, waves);
	// The sets are stronger far out at sea
	const float open = glm::smoothstep(Metres(60.0f), Metres(500.0f), shoreDistance);
	sets = glm::pow(glm::max(sets, glm::vec2(1e-3f)), glm::vec2(1.0f + 0.8f * open));
	const float size = OpenSeaSize(point, seconds, shoreDistance);
	// Each wave grows as the water gets shallow; all of them together break where they are too high for the depth
	std::array<float, k_WaveCount + k_FarWaveCount> grown {};
	float total = 0.0f;
	for (size_t i = 0; i < grown.size(); ++i)
	{
		const auto& wave = i < k_WaveCount ? table.at(i) : far.at(i - k_WaveCount);
		const bool swell = i < 2 || i >= k_WaveCount;
		const float k = 2.0f * std::numbers::pi_v<float> / wave.y;
		grown.at(i) = wave.z * (swell ? sets.y : sets.x) * size * Shoaling(k, depth);
		total += grown.at(i);
	}
	const float breaking = total > 0.0f ? std::min(1.0f, BreakingCrest(depth) / total) : 0.0f;
	const float sharp = k_CrestSharpness + 1.5f * waves.storm;
	// The wind sea is drawn at a warped point, with its waves in groups (see WaveVariationAt)
	const auto variation = WaveVariationAt(point, seconds);
	const auto tilt = WaveTiltAt(point, seconds);
	const glm::vec2 warped = point + variation.warp;
	glm::vec2 windSlope {0.0f};
	for (size_t i = 0; i < table.size(); ++i)
	{
		const auto& wave = table.at(i);
		const bool wind = i >= 2;
		const glm::vec2 along {std::cos(wave.x), std::sin(wave.x)};
		const float k = 2.0f * std::numbers::pi_v<float> / wave.y;
		const float omega = std::sqrt(k_Gravity * k);
		const bool even = (i % 2) == 0;
		float tiltRate = 0.0f;
		const float tiltScale = wind ? TiltScale(tilt.tilt, wave.y, tiltRate) : 1.0f;
		const float group = wind ? (even ? variation.groups.x : variation.groups.y) * tiltScale : 1.0f;
		// A soft cap keeps each wave under the steepness that the sea holds
		const float limit = glm::mix(k_SteepestWaveCalm, k_SteepestWaveStorm, waves.storm) / k;
		float capRate = 1.0f;
		const float amplitude = CapSteepness(grown.at(i) * breaking * group, limit, capRate);
		const float theta = k * glm::dot(along, wind ? warped : point) - omega * seconds + wave.w;
		const float s = 0.5f + 0.5f * std::sin(theta);
		const float shape = 2.0f * std::pow(s, sharp) - 1.0f;
		sample.height += amplitude * shape;
		const float rise = amplitude * sharp * std::pow(std::max(s, 1e-4f), sharp - 1.0f) * std::cos(theta) * k;
		if (wind)
		{
			// The crest envelope: the steepness on the crest, and for a short time after it
			constexpr float k_Tau = 2.0f * std::numbers::pi_v<float>;
			const float behind = std::numbers::pi_v<float> / 2.0f - theta;
			const float age = (behind - k_Tau * std::floor(behind / k_Tau)) / omega;
			sample.whitecap += amplitude * k * std::max(std::pow(s, sharp), std::exp(-age / k_WhitecapSeconds));
			windSlope += along * rise;
			// The height changes with the group too
			const glm::vec2 groupSlope = even ? glm::vec2(variation.groupSlope.x, variation.groupSlope.z)
			                                  : glm::vec2(variation.groupSlope.y, variation.groupSlope.w);
			const float plainGroup = even ? variation.groups.x : variation.groups.y;
			sample.slope +=
			    grown.at(i) * breaking * capRate * shape * (groupSlope * tiltScale + plainGroup * tiltRate * tilt.slope);
		}
		else
		{
			sample.slope += along * rise;
		}
	}
	// The slope at the warped point, turned back to the slope at the point itself
	sample.slope += glm::vec2(windSlope.x * (1.0f + variation.warpSlope.x) + windSlope.y * variation.warpSlope.y,
	                          windSlope.x * variation.warpSlope.z + windSlope.y * (1.0f + variation.warpSlope.w));
	// The long waves of the open sea are plain sine waves, which rise and fall with the swell's sets
	for (size_t j = 0; j < far.size(); ++j)
	{
		const auto& wave = far.at(j);
		const glm::vec2 along {std::cos(wave.x), std::sin(wave.x)};
		const float k = 2.0f * std::numbers::pi_v<float> / wave.y;
		const float omega = std::sqrt(k_Gravity * k);
		const float amplitude = grown.at(k_WaveCount + j) * breaking;
		const float theta = k * glm::dot(along, point) - omega * seconds + wave.w;
		sample.height += amplitude * std::sin(theta);
		sample.slope += along * (amplitude * k * std::cos(theta));
	}
	// The chop of the wind, cut by the depth as the other waves are; its slope from three heights
	constexpr float k_ChopStep = 0.02f;
	const float chop = ChopAt(point, seconds, waves, k_ChopGeometryOctaves);
	const float chopX = ChopAt(point + glm::vec2(k_ChopStep, 0.0f), seconds, waves, k_ChopGeometryOctaves);
	const float chopZ = ChopAt(point + glm::vec2(0.0f, k_ChopStep), seconds, waves, k_ChopGeometryOctaves);
	sample.height += chop * breaking;
	sample.slope += glm::vec2(chopX - chop, chopZ - chop) / k_ChopStep * breaking;
	return sample;
}

std::array<glm::vec4, underwater::k_FarWaveCount> underwater::FarWaveTable(const WaveState& waves)
{
	// A storm makes the long waves longer and much higher, so that the storm shows far out
	// Their lengths stay the same in all weather; a storm makes them six times higher
	const float higher = (1.0f + 5.0f * waves.storm) * std::min(waves.scale, 1.0f);
	return {{
	    {waves.windAngle + 0.2f, Metres(180.0f), Metres(0.4f) * higher, 2.1f},
	    {waves.windAngle - 0.35f, Metres(250.0f), Metres(0.5f) * higher, 4.9f},
	    {k_SwellAngle + 0.4f, Metres(340.0f), Metres(0.6f) * higher, 0.4f},
	}};
}

underwater::EnvironmentFaceAxes underwater::EnvironmentFaceAxesOf(uint8_t face)
{
	// The axes of a cube texture's faces, for a world with the left hand's axes
	constexpr std::array<EnvironmentFaceAxes, k_EnvironmentFaces> k_Axes {{
	    {.forward = {1.0f, 0.0f, 0.0f}, .up = {0.0f, 1.0f, 0.0f}},
	    {.forward = {-1.0f, 0.0f, 0.0f}, .up = {0.0f, 1.0f, 0.0f}},
	    {.forward = {0.0f, 1.0f, 0.0f}, .up = {0.0f, 0.0f, -1.0f}},
	    {.forward = {0.0f, -1.0f, 0.0f}, .up = {0.0f, 0.0f, 1.0f}},
	    {.forward = {0.0f, 0.0f, 1.0f}, .up = {0.0f, 1.0f, 0.0f}},
	    {.forward = {0.0f, 0.0f, -1.0f}, .up = {0.0f, 1.0f, 0.0f}},
	}};
	return k_Axes.at(face % k_EnvironmentFaces);
}

bool underwater::LightJumped(const FrameLight& before, const FrameLight& now)
{
	const float before2 = glm::dot(before.towardsLight, before.towardsLight);
	const float now2 = glm::dot(now.towardsLight, now.towardsLight);
	bool turned = false;
	if (before2 > 1e-8f && now2 > 1e-8f)
	{
		const float cosine = glm::dot(before.towardsLight, now.towardsLight) / std::sqrt(before2 * now2);
		turned = cosine < std::cos(k_LightJumpRadians);
	}
	const glm::vec3 change = glm::abs(now.skyColour - before.skyColour);
	const bool recoloured = std::max({change.x, change.y, change.z}) > k_SkyJumpChange;
	const bool moved = glm::distance(now.eye, before.eye) > k_EyeJumpUnits;
	return turned || recoloured || moved;
}

glm::vec3 underwater::EnvironmentCentre(glm::vec3 eye, float seaLevel)
{
	// A little over the water, so the waves near the middle do not cover the faces that look to the sides
	constexpr float k_OverTheWater = 1.5f;
	return {eye.x, seaLevel + k_OverTheWater, eye.z};
}

underwater::WaveFilter underwater::FilterWave(float length, float amplitude, float footprint)
{
	// The surface keeps the height of a wave that is longer than about eight pixels, and fades it out under four. The
	// normal keeps the slope of a wave that is longer than four pixels, and fades it out under two: a
	// shorter wave would flicker from one pixel to the next.
	const float pixel = std::max(footprint, 1e-4f);
	const auto share = [&](float from, float to) {
		const float x = std::clamp((length - from * pixel) / ((to - from) * pixel), 0.0f, 1.0f);
		return x * x * (3.0f - 2.0f * x);
	};
	const float height = share(4.0f, 8.0f);
	const float slope = share(2.0f, 4.0f);
	// The mean square slope of a sine wave is half of its steepness squared
	const float steepness = amplitude * 2.0f * std::numbers::pi_v<float> / std::max(length, 1e-3f);
	return {.height = height, .slope = slope, .variance = (1.0f - slope * slope) * 0.5f * steepness * steepness};
}

float underwater::FilteredRoughness(float variance)
{
	// The roughness of the water where every wave shows: the small ripples that no wave of the table has
	constexpr float k_CalmRoughness = 0.06f;
	return std::min(std::sqrt(k_CalmRoughness * k_CalmRoughness + std::max(variance, 0.0f)), 1.0f);
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

// A hash of a lattice point to 0 to 1, the same each time
float LatticeHash(int x, int y, int z)
{
	auto h = static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u ^ static_cast<uint32_t>(z) * 83492791u;
	h ^= h >> 13u;
	h *= 0x5bd1e995u;
	h ^= h >> 15u;
	return static_cast<float>(h & 0xffffffu) / static_cast<float>(0xffffffu);
}

int Wrap(int value, int period)
{
	return ((value % period) + period) % period;
}

// Smooth noise that repeats every `period` lattice cells
float TiledValueNoise(glm::vec3 p, int period)
{
	const glm::vec3 cell = glm::floor(p);
	const glm::vec3 f = p - cell;
	const glm::vec3 u = f * f * (3.0f - 2.0f * f);
	const auto x0 = static_cast<int>(cell.x);
	const auto y0 = static_cast<int>(cell.y);
	const auto z0 = static_cast<int>(cell.z);
	auto at = [period](int x, int y, int z) { return LatticeHash(Wrap(x, period), Wrap(y, period), Wrap(z, period)); };
	const float x00 = glm::mix(at(x0, y0, z0), at(x0 + 1, y0, z0), u.x);
	const float x10 = glm::mix(at(x0, y0 + 1, z0), at(x0 + 1, y0 + 1, z0), u.x);
	const float x01 = glm::mix(at(x0, y0, z0 + 1), at(x0 + 1, y0, z0 + 1), u.x);
	const float x11 = glm::mix(at(x0, y0 + 1, z0 + 1), at(x0 + 1, y0 + 1, z0 + 1), u.x);
	return glm::mix(glm::mix(x00, x10, u.y), glm::mix(x01, x11, u.y), u.z);
}

// Smooth noise in octaves, 0 to 1, that repeats over the unit cube
float TiledFbm(glm::vec3 unit, int period, int octaves)
{
	float sum = 0.0f;
	float weight = 0.5f;
	float total = 0.0f;
	for (int octave = 0; octave < octaves; ++octave)
	{
		sum += weight * TiledValueNoise(unit * static_cast<float>(period), period);
		total += weight;
		weight *= 0.5f;
		period *= 2;
	}
	return sum / total;
}

// Round cells: 1 at the middle of the nearest cell, falling to 0 at its edge, repeating over the unit cube
float TiledCells(glm::vec3 unit, int period)
{
	const glm::vec3 p = unit * static_cast<float>(period);
	const glm::vec3 cell = glm::floor(p);
	float nearest = 1e9f;
	for (int dz = -1; dz <= 1; ++dz)
	{
		for (int dy = -1; dy <= 1; ++dy)
		{
			for (int dx = -1; dx <= 1; ++dx)
			{
				const auto cx = static_cast<int>(cell.x) + dx;
				const auto cy = static_cast<int>(cell.y) + dy;
				const auto cz = static_cast<int>(cell.z) + dz;
				const int wx = Wrap(cx, period);
				const int wy = Wrap(cy, period);
				const int wz = Wrap(cz, period);
				const glm::vec3 jitter {LatticeHash(wx, wy, wz + 101), LatticeHash(wx + 37, wy, wz),
				                        LatticeHash(wx, wy + 59, wz)};
				const glm::vec3 centre = glm::vec3(cx, cy, cz) + jitter;
				nearest = std::min(nearest, glm::length(p - centre));
			}
		}
	}
	return std::clamp(1.0f - nearest, 0.0f, 1.0f);
}

uint8_t ToByte(float value)
{
	return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

} // namespace

std::vector<uint8_t> underwater::SprayNoiseVolume()
{
	constexpr int k_Side = k_SprayNoiseSide;
	std::vector<uint8_t> texels(static_cast<size_t>(k_Side) * k_Side * k_Side * 4);
	for (int z = 0; z < k_Side; ++z)
	{
		for (int y = 0; y < k_Side; ++y)
		{
			for (int x = 0; x < k_Side; ++x)
			{
				const glm::vec3 unit = (glm::vec3(x, y, z) + 0.5f) / static_cast<float>(k_Side);
				const float smooth = TiledFbm(unit, 4, 3);
				const float fine = TiledFbm(unit, 8, 2);
				// Stretch the smooth noise out about its middle, so it reaches from 0 to 1
				const float spread = std::clamp((smooth - 0.5f) * 2.2f + 0.5f, 0.0f, 1.0f);
				const float ridge = 1.0f - std::abs(2.0f * TiledFbm(unit + glm::vec3(0.37f), 4, 3) - 1.0f);
				const auto index = static_cast<size_t>((((z * k_Side) + y) * k_Side) + x) * 4;
				texels[index + 0] = ToByte(spread);
				texels[index + 1] = ToByte(TiledCells(unit, 4));
				texels[index + 2] = ToByte((fine - 0.5f) * 2.0f + 0.5f);
				texels[index + 3] = ToByte(ridge * ridge);
			}
		}
	}
	return texels;
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

glm::vec4 underwater::ShoreTexel(glm::vec4 shore, float cliff, float pond)
{
	if (pond > 0.0f)
	{
		// A pond keeps its share in the length of the direction, over 1
		const glm::vec2 direction {shore.y, shore.z};
		const float length = glm::length(direction);
		const auto unit = length > 1e-4f ? direction / length : glm::vec2(1.0f, 0.0f);
		const auto longer = unit * (1.0f + 0.5f * std::min(pond, 1.0f));
		return {shore.x, longer.x, longer.y, shore.w};
	}
	const float shorten = 1.0f - 0.5f * cliff;
	return {shore.x, shore.y * shorten, shore.z * shorten, shore.w};
}

float underwater::ShorePond(glm::vec4 shore)
{
	return std::clamp((glm::length(glm::vec2(shore.y, shore.z)) - 1.0f) * 2.0f, 0.0f, 1.0f);
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

underwater::CliffCrash underwater::CliffCrashAt(glm::vec2 foot, const WaveState& waves, float seaLevel, float cliff)
{
	constexpr float k_Turn = 2.0f * std::numbers::pi_v<float>;
	// The same steps as UwCliffCrash in underwater.sh: when the shore wave reaches the coast here, and which crash it is
	const float omega = ShoreOmega(waves.storm);
	const float theta = waves.shorePhase + Noise(foot * 0.008f) * 1.2f;
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

std::vector<underwater::Waterspout> underwater::WaterspoutsAt(float seconds, float storm, glm::vec2 centre,
                                                              const std::function<float(glm::vec2)>& seaDistance)
{
	std::vector<Waterspout> spouts;
	const float stormShare = glm::smoothstep(0.5f, 0.8f, storm);
	if (stormShare <= 0.0f)
	{
		return spouts;
	}
	for (size_t slot = 0; slot < k_MostWaterspouts; ++slot)
	{
		// Each slot starts a new waterspout once a life, the slots half a life apart
		const float shifted = seconds + static_cast<float>(slot) * k_WaterspoutSeconds * 0.5f;
		const float life = std::floor(shifted / k_WaterspoutSeconds);
		const float age = shifted - life * k_WaterspoutSeconds;
		const auto seed = static_cast<int>(life) * 7 + static_cast<int>(slot) * 131;
		// A few places to try, the first one far enough out to sea is the waterspout's
		for (int attempt = 0; attempt < 4; ++attempt)
		{
			const float angle = LatticeHash(seed, attempt, 3) * 6.2831853f;
			const float radius = 500.0f + 800.0f * LatticeHash(seed, attempt, 5);
			const glm::vec2 start = centre + glm::vec2(std::cos(angle), std::sin(angle)) * radius;
			const float heading = LatticeHash(seed, attempt, 7) * 6.2831853f;
			const glm::vec2 position = start + glm::vec2(std::cos(heading), std::sin(heading)) * (age * 1.5f);
			if (seaDistance(start) >= k_WaterspoutSeaRoom && seaDistance(position) >= k_WaterspoutSeaRoom)
			{
				const float grow = glm::smoothstep(0.0f, 8.0f, age) *
				                   (1.0f - glm::smoothstep(k_WaterspoutSeconds - 12.0f, k_WaterspoutSeconds, age));
				spouts.push_back({.position = position, .strength = grow * stormShare});
				break;
			}
		}
	}
	return spouts;
}

float underwater::SignificantWaveHeight(const WaveState& waves)
{
	const auto table = WaveTable(waves);
	const float squares = std::accumulate(table.begin(), table.end(), 0.0f,
	                                      [](float sum, const glm::vec4& wave) { return sum + wave.z * wave.z; });
	return 4.0f * std::sqrt(squares * 0.5f);
}

namespace
{
// The trough in front of a rogue wave is this share of its crest, so the height from crest to trough is 1.5 crests
constexpr float k_RogueTroughShare = 0.5f;

// One bump of the rogue wave's profile at a place along it: its value and its rise along the profile
struct Bump
{
	float value;
	float rise;
};
Bump RogueBump(float u, float middle, float width, float amount)
{
	const float s = (u - middle) / width;
	const float value = amount * std::exp(-s * s);
	return {.value = value, .rise = value * (-2.0f * s / width)};
}
} // namespace

std::optional<underwater::RogueWave> underwater::RogueWaveAt(float seconds, const WaveState& waves, glm::vec2 centre,
                                                             const std::function<float(glm::vec2)>& seaDistance,
                                                             bool firstCertain)
{
	const float stormShare = glm::smoothstep(0.55f, 0.85f, waves.storm);
	if (stormShare <= 0.0f || seconds < 0.0f)
	{
		return std::nullopt;
	}
	const float period = std::floor(seconds / k_RoguePeriod);
	const auto seed = static_cast<int>(period);
	const bool first = firstCertain && seed == 0;
	// The chance of a wave in a period grows with the storm, to about two periods in three in the worst storm
	if (!first && LatticeHash(seed, 11, 2) > 0.7f * stormShare)
	{
		return std::nullopt;
	}
	const float start = first ? 0.0f : LatticeHash(seed, 13, 4) * (k_RoguePeriod - k_RogueSeconds);
	const float age = seconds - period * k_RoguePeriod - start;
	if (age < 0.0f || age > k_RogueSeconds)
	{
		return std::nullopt;
	}
	const float length = 90.0f + 40.0f * LatticeHash(seed, 17, 6);
	// It travels at the speed of a deep water wave of its length, as the waves of the wave table do
	const float speed = 0.8f * std::sqrt(9.8f * length / (2.0f * std::numbers::pi_v<float>));
	const float halfWidth = 90.0f + 70.0f * LatticeHash(seed, 19, 8);
	const float travel = speed * k_RogueSeconds;
	// Places to try, the first ones nearest to the centre: the first one whose path stays out at sea is the wave's
	for (int attempt = 0; attempt < 16; ++attempt)
	{
		const float angle = LatticeHash(seed, attempt, 21) * 2.0f * std::numbers::pi_v<float>;
		const float radius = 120.0f + 330.0f * LatticeHash(seed, attempt, 23) + 100.0f * static_cast<float>(attempt);
		const glm::vec2 place = centre + glm::vec2(std::cos(angle), std::sin(angle)) * radius;
		const float heading = waves.windAngle + (LatticeHash(seed, attempt, 25) - 0.5f) * 0.8f;
		const glm::vec2 direction {std::cos(heading), std::sin(heading)};
		const glm::vec2 across {-direction.y, direction.x};
		const glm::vec2 begin = place - direction * (travel * 0.5f);
		const glm::vec2 end = place + direction * (travel * 0.5f);
		// The whole path, at the middle and the ends of the crest line, must stay out at sea
		const std::array<glm::vec2, 9> path {begin,
		                                     place,
		                                     end,
		                                     begin + across * halfWidth,
		                                     begin - across * halfWidth,
		                                     place + across * halfWidth,
		                                     place - across * halfWidth,
		                                     end + across * halfWidth,
		                                     end - across * halfWidth};
		if (!std::ranges::all_of(path, [&](glm::vec2 point) { return seaDistance(point) >= k_RogueSeaRoom; }))
		{
			continue;
		}
		const glm::vec2 position = begin + direction * (speed * age);
		// It grows as its group of waves comes together, holds, and dies away as the group spreads out again
		const float life =
		    glm::smoothstep(0.0f, 7.0f, age) * (1.0f - glm::smoothstep(k_RogueSeconds - 9.0f, k_RogueSeconds, age));
		const float room = glm::smoothstep(k_RogueShoreNear, k_RogueShoreFar, seaDistance(position));
		const float strength = life * stormShare * room;
		const float height = SignificantWaveHeight(waves) * (2.0f + LatticeHash(seed, 27, 10));
		return RogueWave {.position = position,
		                  .direction = direction,
		                  .crest = height / (1.0f + k_RogueTroughShare) * strength,
		                  .length = length,
		                  .halfWidth = halfWidth,
		                  .bend = 0.1f + 0.15f * LatticeHash(seed, 29, 12),
		                  .breaking = glm::smoothstep(0.55f, 0.9f, life) * stormShare * room,
		                  .strength = strength,
		                  .age = age,
		                  .seed = static_cast<float>(seed % 97)};
	}
	return std::nullopt;
}

underwater::WaveSample underwater::RogueWaveHeight(glm::vec2 point, const RogueWave& rogue, float shoreDistance, float depth)
{
	WaveSample sample {.height = 0.0f, .slope = {0.0f, 0.0f}};
	if (rogue.crest <= 0.0f)
	{
		return sample;
	}
	const glm::vec2 across {-rogue.direction.y, rogue.direction.x};
	const glm::vec2 offset = point - rogue.position;
	const float side = glm::dot(offset, across) / rogue.halfWidth;
	if (std::abs(side) > 2.0f)
	{
		return sample;
	}
	// The crest line bends back towards its ends
	const float u = (glm::dot(offset, rogue.direction) + rogue.bend * rogue.length * side * side) / rogue.length;
	if (u < -1.6f || u > 0.9f)
	{
		return sample;
	}
	// The crest, with a steep face in front and a long back, the deep trough in front of it, the trough behind it and
	// a smaller wave behind that
	const auto crest = RogueBump(u, 0.0f, u >= 0.0f ? 0.08f : 0.2f, 1.0f);
	const auto front = RogueBump(u, 0.3f, 0.14f, -k_RogueTroughShare);
	const auto back = RogueBump(u, -0.55f, 0.2f, -0.35f);
	const auto next = RogueBump(u, -1.0f, 0.14f, 0.3f);
	const float profile = crest.value + front.value + back.value + next.value;
	const float profileRise = crest.rise + front.rise + back.rise + next.rise;
	// The crest line fades out towards its ends
	const float ends = std::exp(-(side * side * side * side));
	const float endsRise = ends * (-4.0f * side * side * side / rogue.halfWidth);
	const float fade = glm::smoothstep(k_RogueShoreNear, k_RogueShoreFar, shoreDistance) * glm::smoothstep(3.0f, 15.0f, depth);
	const glm::vec2 uRise =
	    (rogue.direction + across * (rogue.bend * 2.0f * side / rogue.halfWidth * rogue.length)) / rogue.length;
	sample.height = rogue.crest * fade * profile * ends;
	sample.slope = rogue.crest * fade * (uRise * (profileRise * ends) + across * (profile * endsRise));
	return sample;
}

std::array<glm::vec4, 2> underwater::RogueUniform(const std::optional<RogueWave>& rogue)
{
	if (!rogue.has_value())
	{
		return {};
	}
	return {glm::vec4(rogue->position, std::atan2(rogue->direction.y, rogue->direction.x), rogue->crest),
	        glm::vec4(rogue->length, rogue->halfWidth, rogue->breaking, rogue->bend)};
}

underwater::WaveSample underwater::WavesAt(glm::vec2 point, float seconds, const WaveState& waves,
                                           const std::optional<RogueWave>& rogue, float shoreDistance, float depth)
{
	auto sample = WavesInDepth(point, seconds, waves, shoreDistance, depth);
	if (rogue.has_value())
	{
		const auto big = RogueWaveHeight(point, *rogue, shoreDistance, depth);
		sample.height += big.height;
		sample.slope += big.slope;
	}
	return sample;
}

underwater::SeaStrike underwater::TrackStrike(const SeaStrike& before, const std::optional<StrikeFlash>& flash, float seconds,
                                              float seaDistance)
{
	SeaStrike after = before;
	after.since = std::min(before.since + seconds, 1000.0f);
	if (!flash.has_value() || !flash->active)
	{
		after.flashTime = 1000.0f;
		return after;
	}
	const bool fresh = flash->time < before.flashTime || glm::distance(flash->position, before.position) > 1.0f;
	after.flashTime = flash->time;
	if (fresh)
	{
		after.position = flash->position;
		after.since = flash->time;
		after.strength = flash->strength;
		after.overSea = seaDistance >= k_StrikeSeaRoom;
	}
	return after;
}

float underwater::StrikeRingRadius(float since)
{
	return 2.0f + 30.0f * std::sqrt(std::max(since, 0.0f));
}

bool underwater::InQuality(Effect effect, Quality quality)
{
	switch (effect)
	{
	case Effect::Environment:
	case Effect::FoamVolume:
	case Effect::SprayMist:
	case Effect::Sediment:
	case Effect::Fog:
	case Effect::AmbientOcclusion:
		return quality == Quality::High;
	case Effect::Reflections:
	case Effect::Shafts:
	case Effect::Bubbles:
	case Effect::Filtering:
	case Effect::Motes:
	case Effect::Shadows:
	case Effect::Clouds:
		return quality != Quality::Low;
	default:
		return true;
	}
}

underwater::LightDetail underwater::HazeDetail(Quality quality)
{
	switch (quality)
	{
	case Quality::High:
		return LightDetail::Full;
	case Quality::Medium:
		return LightDetail::Reduced;
	default:
		return LightDetail::Basic;
	}
}

underwater::LightDetail underwater::LightingDetail(Quality quality)
{
	return HazeDetail(quality);
}

underwater::Quality underwater::NextQuality(Quality quality)
{
	return static_cast<Quality>((static_cast<uint8_t>(quality) + 1) % static_cast<uint8_t>(Quality::_Count));
}

std::array<glm::vec4, 2> underwater::ProfileUniform(const WaterProfile& profile)
{
	return {glm::vec4(profile.absorption, profile.reflection), glm::vec4(profile.scatter, profile.roughness)};
}
