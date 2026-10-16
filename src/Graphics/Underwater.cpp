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

float underwater::EyeDepth(float eyeHeight)
{
	return std::max(k_SeaLevel - eyeHeight, 0.0f);
}
