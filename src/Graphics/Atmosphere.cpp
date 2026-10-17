/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the air's light

#include "Atmosphere.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/geometric.hpp>

using namespace openblack::graphics;
using namespace openblack::graphics::atmosphere;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
/// The steps along a ray to the top of the air, and along a ray through the sky
constexpr int k_TransmittanceSteps = 24;
constexpr int k_SkySteps = 24;
constexpr int k_HazeSteps = 16;
/// The directions round the horizon that its mean takes
constexpr int k_HorizonDirections = 8;

float Smoothstep(float low, float high, float x)
{
	const float t = std::clamp((x - low) / (high - low), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

float SafeSqrt(float x)
{
	return std::sqrt(std::max(x, 0.0f));
}

float DistanceToGround(float radius, float mu)
{
	return -radius * mu - SafeSqrt(radius * radius * (mu * mu - 1.0f) + k_GroundRadius * k_GroundRadius);
}

/// The light that the air scatters more than once, for each steradian, at a height with the light at mu
glm::vec3 MultipleScattering(float altitude, float mu, const Air& air)
{
	return k_MultipleShare * Transmittance(altitude, std::max(mu, 0.05f), air) * Smoothstep(-0.25f, 0.05f, mu) / (4.0f * k_Pi);
}

/// The light that one stretch of a ray scatters towards its start, and what the stretch lets through
struct Stretch
{
	glm::vec3 light;
	glm::vec3 through;
};

/// The light that the air at a point scatters towards the eye along `view`, over a stretch `length` kilometres long,
/// with its density times `density`
Stretch ScatterAt(glm::vec3 point, glm::vec3 view, glm::vec3 towardsLight, float length, float density, const Air& air)
{
	const float radius = glm::length(point);
	const float altitude = radius - k_GroundRadius;
	const glm::vec3 up = point / radius;
	const float lightMu = glm::dot(up, towardsLight);
	const auto medium = MediumAt(altitude, air);
	const float cosAngle = glm::dot(view, towardsLight);
	const glm::vec3 sun = Transmittance(altitude, lightMu, air);
	const glm::vec3 single = medium.rayleigh * RayleighPhase(cosAngle) + medium.mie * MiePhase(cosAngle, k_MieForward);
	const glm::vec3 scattered =
	    density * (single * sun + (medium.rayleigh + medium.mie) * MultipleScattering(altitude, lightMu, air));
	const glm::vec3 extinction = glm::max(density * medium.extinction, glm::vec3(1e-7f));
	const glm::vec3 through = glm::exp(-extinction * length);
	// The light of the stretch, as the stretch's own air dims it
	return {scattered * (1.0f - through) / extinction, through};
}
} // namespace

Air atmosphere::AirFor(float storm, float wetness)
{
	// Sea air holds salt: about twice the clean air's dust. Rain and a storm's spray add more.
	return {.turbidity = 2.0f + 2.0f * std::clamp(wetness, 0.0f, 1.0f) + 6.0f * std::clamp(storm, 0.0f, 1.0f)};
}

Medium atmosphere::MediumAt(float altitude, const Air& air)
{
	const float h = std::max(altitude, 0.0f);
	const float rayleighDensity = std::exp(-h / k_RayleighHeight);
	const float mieDensity = std::exp(-h / k_MieHeight) * air.turbidity;
	const float ozoneDensity = std::max(0.0f, 1.0f - std::abs(h - k_OzoneMiddle) / k_OzoneHalfWidth);
	Medium medium {};
	medium.rayleigh = k_RayleighScattering * rayleighDensity;
	medium.mie = k_MieScattering * mieDensity;
	medium.extinction = medium.rayleigh + glm::vec3(k_MieExtinction * mieDensity) + k_OzoneAbsorption * ozoneDensity;
	return medium;
}

float atmosphere::RayleighPhase(float cosAngle)
{
	return 3.0f / (16.0f * k_Pi) * (1.0f + cosAngle * cosAngle);
}

float atmosphere::MiePhase(float cosAngle, float forward)
{
	const float g2 = forward * forward;
	const float d = 1.0f + g2 - 2.0f * forward * cosAngle;
	return 3.0f / (8.0f * k_Pi) * (1.0f - g2) * (1.0f + cosAngle * cosAngle) / ((2.0f + g2) * d * std::sqrt(d));
}

float atmosphere::DistanceToTop(float radius, float mu)
{
	return -radius * mu + SafeSqrt(radius * radius * (mu * mu - 1.0f) + k_TopRadius * k_TopRadius);
}

bool atmosphere::HitsGround(float radius, float mu)
{
	return mu < 0.0f && radius * radius * (mu * mu - 1.0f) + k_GroundRadius * k_GroundRadius >= 0.0f;
}

glm::vec3 atmosphere::Transmittance(float altitude, float mu, const Air& air)
{
	const float radius = k_GroundRadius + std::max(altitude, 0.0f);
	if (HitsGround(radius, mu))
	{
		return glm::vec3(0.0f);
	}
	const float length = DistanceToTop(radius, mu);
	const float step = length / static_cast<float>(k_TransmittanceSteps);
	glm::vec3 depth {0.0f};
	for (int i = 0; i < k_TransmittanceSteps; ++i)
	{
		const float t = (static_cast<float>(i) + 0.5f) * step;
		const float r = std::sqrt(radius * radius + t * t + 2.0f * radius * mu * t);
		depth += MediumAt(r - k_GroundRadius, air).extinction * step;
	}
	return glm::exp(-depth);
}

glm::vec2 atmosphere::TransmittanceUv(float altitude, float mu)
{
	const float radius = k_GroundRadius + std::clamp(altitude, 0.0f, k_TopRadius - k_GroundRadius);
	const float horizon = std::sqrt(k_TopRadius * k_TopRadius - k_GroundRadius * k_GroundRadius);
	const float rho = SafeSqrt(radius * radius - k_GroundRadius * k_GroundRadius);
	const float d = DistanceToTop(radius, mu);
	const float dMin = k_TopRadius - radius;
	const float dMax = rho + horizon;
	return {(d - dMin) / (dMax - dMin), rho / horizon};
}

HeightAngle atmosphere::FromTransmittanceUv(glm::vec2 uv)
{
	const float horizon = std::sqrt(k_TopRadius * k_TopRadius - k_GroundRadius * k_GroundRadius);
	const float rho = horizon * uv.y;
	const float radius = std::sqrt(rho * rho + k_GroundRadius * k_GroundRadius);
	const float dMin = k_TopRadius - radius;
	const float dMax = rho + horizon;
	const float d = dMin + uv.x * (dMax - dMin);
	const float mu = d <= 0.0f ? 1.0f : (horizon * horizon - rho * rho - d * d) / (2.0f * radius * d);
	return {radius - k_GroundRadius, std::clamp(mu, -1.0f, 1.0f)};
}

glm::vec2 atmosphere::SkyViewUv(float viewMu, float lightCos, float altitude)
{
	const float radius = k_GroundRadius + std::max(altitude, 1e-3f);
	const float beta =
	    std::acos(std::clamp(std::sqrt(radius * radius - k_GroundRadius * k_GroundRadius) / radius, -1.0f, 1.0f));
	const float zenithHorizon = k_Pi - beta;
	const float viewAngle = std::acos(std::clamp(viewMu, -1.0f, 1.0f));
	float v = 0.0f;
	if (viewAngle < zenithHorizon)
	{
		const float coord = 1.0f - SafeSqrt(1.0f - viewAngle / zenithHorizon);
		v = coord * 0.5f;
	}
	else
	{
		const float coord = SafeSqrt((viewAngle - zenithHorizon) / beta);
		v = coord * 0.5f + 0.5f;
	}
	const float u = SafeSqrt(-std::clamp(lightCos, -1.0f, 1.0f) * 0.5f + 0.5f);
	return {u, v};
}

glm::vec2 atmosphere::FromSkyViewUv(glm::vec2 uv, float altitude)
{
	const float radius = k_GroundRadius + std::max(altitude, 1e-3f);
	const float beta =
	    std::acos(std::clamp(std::sqrt(radius * radius - k_GroundRadius * k_GroundRadius) / radius, -1.0f, 1.0f));
	const float zenithHorizon = k_Pi - beta;
	float viewAngle = 0.0f;
	if (uv.y < 0.5f)
	{
		const float coord = 1.0f - 2.0f * uv.y;
		viewAngle = zenithHorizon * (1.0f - coord * coord);
	}
	else
	{
		const float coord = uv.y * 2.0f - 1.0f;
		viewAngle = zenithHorizon + beta * coord * coord;
	}
	const float lightCos = -(uv.x * uv.x * 2.0f - 1.0f);
	return {std::cos(viewAngle), lightCos};
}

float atmosphere::AerialSliceDistance(float slice)
{
	return k_AerialNearest * std::pow(k_AerialFarthest / k_AerialNearest, (slice + 0.5f) / static_cast<float>(k_AerialSlices));
}

float atmosphere::AerialSliceAt(float distance)
{
	return std::log(std::max(distance, 1e-3f) / k_AerialNearest) / std::log(k_AerialFarthest / k_AerialNearest) *
	           static_cast<float>(k_AerialSlices) -
	       0.5f;
}

glm::vec3 atmosphere::SkyRadiance(glm::vec3 view, glm::vec3 towardsLight, float altitude, const Air& air)
{
	const float radius = k_GroundRadius + std::max(altitude, 1e-3f);
	const glm::vec3 eye {0.0f, radius, 0.0f};
	const float mu = view.y;
	const float length = HitsGround(radius, mu) ? DistanceToGround(radius, mu) : DistanceToTop(radius, mu);
	const float step = length / static_cast<float>(k_SkySteps);
	glm::vec3 light {0.0f};
	glm::vec3 through {1.0f};
	for (int i = 0; i < k_SkySteps; ++i)
	{
		const float t = (static_cast<float>(i) + 0.5f) * step;
		const auto stretch = ScatterAt(eye + view * t, view, towardsLight, step, 1.0f, air);
		light += through * stretch.light;
		through *= stretch.through;
	}
	return light;
}

float atmosphere::HazeScaleAt(float altitude)
{
	return 1.0f + (k_HazeScale - 1.0f) * std::exp(-std::max(altitude, 0.0f) / k_HazeLayerHeight);
}

Haze atmosphere::HazeAlong(glm::vec3 view, glm::vec3 towardsLight, float altitude, float distance, const Air& air)
{
	const float radius = k_GroundRadius + std::max(altitude, 1e-3f);
	const glm::vec3 eye {0.0f, radius, 0.0f};
	const float step = distance / k_UnitsPerKilometre / static_cast<float>(k_HazeSteps);
	Haze haze {glm::vec3(0.0f), glm::vec3(1.0f)};
	for (int i = 0; i < k_HazeSteps; ++i)
	{
		const float t = (static_cast<float>(i) + 0.5f) * step;
		auto point = eye + view * t;
		// The haze stays in the air: a point under the sea's level is taken at the level
		if (glm::length(point) < k_GroundRadius)
		{
			point = glm::normalize(point) * k_GroundRadius;
		}
		const auto stretch = ScatterAt(point, view, towardsLight, step, HazeScaleAt(glm::length(point) - k_GroundRadius), air);
		haze.light += haze.through * stretch.light;
		haze.through *= stretch.through;
	}
	return haze;
}

glm::vec3 atmosphere::LightAtGround(float elevationSine, const Air& air)
{
	return Transmittance(0.0f, std::max(elevationSine, 0.0f), air) * Smoothstep(-0.02f, 0.0f, elevationSine);
}

glm::vec3 atmosphere::HorizonRadiance(glm::vec3 towardsLight, float altitude, const Air& air)
{
	glm::vec3 sum {0.0f};
	for (int i = 0; i < k_HorizonDirections; ++i)
	{
		const float angle = 2.0f * k_Pi * (static_cast<float>(i) + 0.5f) / static_cast<float>(k_HorizonDirections);
		const glm::vec3 view = glm::normalize(glm::vec3(std::cos(angle), 0.02f, std::sin(angle)));
		sum += SkyRadiance(view, towardsLight, altitude, air);
	}
	return sum / static_cast<float>(k_HorizonDirections);
}

float atmosphere::Exposure(glm::vec3 horizon, glm::vec3 target)
{
	return Luminance(target) / std::max(Luminance(horizon), 1e-6f);
}

glm::vec3 atmosphere::ScreenScale(glm::vec3 horizon, glm::vec3 target, glm::vec3 tint, float originalShare)
{
	const glm::vec3 physical = tint * Exposure(horizon * tint, target);
	const glm::vec3 original = target / glm::max(horizon, glm::vec3(1e-6f));
	return glm::mix(physical, original, std::clamp(originalShare, 0.0f, 1.0f));
}

float atmosphere::Luminance(glm::vec3 colour)
{
	return glm::dot(colour, glm::vec3(0.2126f, 0.7152f, 0.0722f));
}

float atmosphere::AirSunElevation(float hour)
{
	const float h = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
	// Up from 6 to 18 on a sine; under the horizon through the night, lowest at midnight
	const float arc = std::sin(k_Pi * (h - 6.0f) / 12.0f);
	const float degrees = arc >= 0.0f ? k_NoonElevation * arc : 30.0f * arc;
	return std::sin(degrees * k_Pi / 180.0f);
}

glm::vec3 atmosphere::PlaceTint(std::span<const glm::vec3> colours, float share)
{
	if (colours.empty())
	{
		return glm::vec3(1.0f);
	}
	glm::vec3 mean {0.0f};
	for (const auto& colour : colours)
	{
		mean += colour;
	}
	mean /= static_cast<float>(colours.size());
	const float luminance = Luminance(mean);
	if (luminance <= 1e-4f)
	{
		return glm::vec3(1.0f);
	}
	return glm::mix(glm::vec3(1.0f), mean / luminance, std::clamp(share, 0.0f, 1.0f));
}

float atmosphere::AltitudeOf(float heightOverSea)
{
	return std::max(heightOverSea, 0.5f) / k_UnitsPerKilometre;
}
