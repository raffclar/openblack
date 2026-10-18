/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the Milky Way and the faint nebulae

#include "Galaxy.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>

using namespace openblack::graphics;

namespace
{
constexpr float k_Degrees = 180.0f / std::numbers::pi_v<float>;

/// A bell curve of height 1 with the spread `spread`
[[nodiscard]] float Bell(float x, float spread)
{
	return std::exp(-(x * x) / (2.0f * spread * spread));
}

/// An oval bell of height 1 round a point, with the half sizes `size` in degrees
[[nodiscard]] float Oval(glm::vec2 at, glm::vec2 middle, glm::vec2 size)
{
	const glm::vec2 d = (at - middle) / size;
	return std::exp(-glm::dot(d, d));
}

[[nodiscard]] float SmoothStep(float edge0, float edge1, float x)
{
	const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	return t * t * (3.0f - (2.0f * t));
}
} // namespace

glm::vec3 galaxy::GalacticDirection(glm::vec3 celestial)
{
	return k_SkyToGalaxy * celestial;
}

glm::vec2 galaxy::LongitudeLatitude(glm::vec3 galactic)
{
	return {std::atan2(galactic.y, galactic.x) * k_Degrees, std::asin(std::clamp(galactic.z, -1.0f, 1.0f)) * k_Degrees};
}

float galaxy::BandBrightness(glm::vec2 longitudeLatitude)
{
	const float l = longitudeLatitude.x;
	const float b = longitudeLatitude.y;
	const float towardsMiddle = 0.5f + (0.5f * std::cos(l / k_Degrees));
	// The band is brightest towards the middle and faint opposite it, with the bright star clouds of Cygnus, Carina,
	// Scutum and Norma
	const float along = 0.22f + (0.78f * towardsMiddle * towardsMiddle) + (0.30f * Bell(l - 75.0f, 12.0f)) +
	                    (0.25f * Bell(l + 68.0f, 14.0f)) + (0.25f * Bell(l - 27.0f, 5.0f)) + (0.15f * Bell(l + 30.0f, 8.0f));
	const float width = 4.0f + (3.0f * towardsMiddle);
	const float disc = along * ((0.7f * std::exp(-std::abs(b) / width)) + (0.3f * std::exp(-std::abs(b) / (2.5f * width))));
	const float bulge = 1.1f * Oval(longitudeLatitude, {0.0f, -1.0f}, {8.5f, 6.0f});
	return disc + bulge;
}

float galaxy::DustDepth(glm::vec2 longitudeLatitude)
{
	const float l = longitudeLatitude.x;
	const float b = longitudeLatitude.y;
	const float lane = 1.3f * Bell(b, 1.2f) * (0.35f + (0.65f * Bell(l, 50.0f)));
	// The Great Rift goes from Cygnus to Ophiuchus, and moves north of the band as it comes nearer the middle
	const float riftLatitude = 0.8f + (5.5f * (1.0f - SmoothStep(0.0f, 45.0f, l)));
	const float rift = 1.1f * Bell(b - riftLatitude, 2.0f) * SmoothStep(-8.0f, 2.0f, l) * (1.0f - SmoothStep(78.0f, 92.0f, l));
	const float pipe = 1.4f * Oval(longitudeLatitude, {0.5f, 5.5f}, {3.5f, 1.4f});
	const float rhoOphiuchi = 0.9f * Oval(longitudeLatitude, {-6.5f, 16.5f}, {3.5f, 2.5f});
	const float coalsack = 2.2f * Oval(longitudeLatitude, {-58.5f, -1.0f}, {2.6f, 2.3f});
	const float northernCoalsack = 1.0f * Oval(longitudeLatitude, {82.0f, 1.0f}, {5.0f, 2.2f});
	const float taurus = 0.5f * Oval(longitudeLatitude, {172.0f, -15.0f}, {6.0f, 4.0f});
	return lane + rift + pipe + rhoOphiuchi + coalsack + northernCoalsack + taurus;
}

float galaxy::StarDensity(glm::vec3 celestial)
{
	const glm::vec2 lb = LongitudeLatitude(GalacticDirection(celestial));
	return 1.0f + (1.5f * BandBrightness(lb) * std::exp(-0.7f * DustDepth(lb)));
}

float galaxy::Visibility(float skyLuminance)
{
	return 1.0f - SmoothStep(k_FullVisibilityLuminance, k_NoVisibilityLuminance, skyLuminance);
}

float galaxy::MoonVisibility(float moonBrightness, float moonSine)
{
	return 1.0f - (k_MoonWash * moonBrightness * SmoothStep(-0.05f, 0.25f, moonSine));
}
