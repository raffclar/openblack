/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the moon that looks like the real moon

#include "RealMoon.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/geometric.hpp>

#include "Graphics/Atmosphere.h"

namespace openblack::graphics::real_moon
{

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
} // namespace

float Radius(float distance)
{
	return distance * std::tan(0.5f * k_ApparentDegrees * k_Pi / 180.0f);
}

glm::vec3 SkyNorthPole()
{
	const float latitude = atmosphere::k_LatitudeDegrees * k_Pi / 180.0f;
	return (std::cos(latitude) * atmosphere::k_North) + glm::vec3(0.0f, std::sin(latitude), 0.0f);
}

Face FaceAxes(glm::vec3 towardsMoon)
{
	const glm::vec3 towards = glm::normalize(towardsMoon);
	const glm::vec3 toEye = -towards;
	const glm::vec3 pole = SkyNorthPole();
	glm::vec3 north = pole - (toEye * glm::dot(pole, toEye));
	// At the pole of the sky, any direction across the face is north; take the world's up
	if (glm::length(north) < 1e-4f)
	{
		north = glm::vec3(0.0f, 1.0f, 0.0f) - (toEye * toEye.y);
	}
	north = glm::normalize(north);
	// The moon's east is on the eye's right when its north is up: on the west side of the sky
	const glm::vec3 east = glm::normalize(glm::cross(north, towards));
	return {.east = east, .north = north, .toEye = toEye};
}

glm::vec2 MapPoint(glm::vec3 normal, const Face& face)
{
	const glm::vec3 unit = glm::normalize(normal);
	const float longitude = std::atan2(glm::dot(unit, face.east), glm::dot(unit, face.toEye));
	const float latitude = std::asin(std::clamp(glm::dot(unit, face.north), -1.0f, 1.0f));
	return {0.5f + (longitude / (2.0f * k_Pi)), 0.5f - (latitude / k_Pi)};
}

float SurfaceLight(float towardsSun, float towardsEye)
{
	if (towardsSun <= 0.0f || towardsEye <= 0.0f)
	{
		return 0.0f;
	}
	return 2.0f * towardsSun / (towardsSun + towardsEye);
}

float EarthLight(float sunMoonCosine)
{
	return k_Earthshine * 0.5f * (1.0f + std::clamp(sunMoonCosine, -1.0f, 1.0f));
}

glm::vec3 AirTint(float elevationSine)
{
	// The length of the way through the air, in lengths of the way straight up (Kasten and Young), down to the
	// horizon and a little under it
	const float sine = std::clamp(elevationSine, -0.03f, 1.0f);
	const float degrees = std::asin(sine) * 180.0f / k_Pi;
	const float airMass = 1.0f / (std::max(sine, 0.0f) + (0.50572f * std::pow(degrees + 6.07995f, -1.6364f)));
	const glm::vec3 left = glm::exp(-k_Extinction * (airMass - 1.0f));
	// The eye adapts to the dim light: the colour stays, and the brightness changes less than the light
	const float luminance = atmosphere::Luminance(left);
	if (luminance <= 0.0f)
	{
		return glm::vec3(0.0f);
	}
	return left * (std::pow(luminance, k_EyeResponse) / luminance);
}

float HaloStrength(float elevationSine, float moonLight)
{
	const float high = std::clamp(elevationSine, 0.0f, 1.0f);
	return k_Halo * std::max(moonLight, 0.0f) * (1.0f + (2.5f * (1.0f - high)));
}

float Cover(float skyType)
{
	return k_NightCover * std::clamp(1.0f - (skyType / 0.5f), 0.0f, 1.0f);
}

} // namespace openblack::graphics::real_moon
