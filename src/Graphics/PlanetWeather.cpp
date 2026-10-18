/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlanetWeather.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <future>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/trigonometric.hpp>

#include "VolumetricClouds.h"

using namespace openblack::graphics;

namespace
{

constexpr uint32_t k_Seed = 0x51a7e3u;
/// The noise's lattice repeats after this many cells, far past the largest frequency, so no repeat shows
constexpr int32_t k_Period = 4096;

/// A turn by an angle round an axis of length 1
glm::mat3 Turn(glm::vec3 axis, float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	const float t = 1.0f - c;
	const glm::vec3 a = axis;
	return glm::mat3 {{t * a.x * a.x + c, t * a.x * a.y + s * a.z, t * a.x * a.z - s * a.y},
	                  {t * a.x * a.y - s * a.z, t * a.y * a.y + c, t * a.y * a.z + s * a.x},
	                  {t * a.x * a.z + s * a.y, t * a.y * a.z - s * a.x, t * a.z * a.z + c}};
}

/// Each octave of the noise turns on its own axes, so that the lattices of the octaves never line up
const std::array<glm::mat3, 8>& OctaveTurns()
{
	static const std::array<glm::mat3, 8> k_Turns = [] {
		std::array<glm::mat3, 8> turns {};
		for (size_t i = 0; i < turns.size(); ++i)
		{
			const auto f = static_cast<float>(i);
			const glm::vec3 axis =
			    glm::normalize(glm::vec3 {std::sin(f * 2.1f + 0.3f), std::cos(f * 1.3f + 0.7f), std::sin(f * 0.7f + 1.9f)});
			turns.at(i) = Turn(axis, 0.6f + f * 1.17f);
		}
		return turns;
	}();
	return k_Turns;
}

/// Each octave of the weather's noise is this much as strong as the one before: the small weather has much contrast
constexpr float k_Roughness = 0.62f;

/// Octaves of gradient noise at a point in space, from `frequency` up, each weaker by k_Roughness: about -0.5 to 0.5
float Octaves(glm::vec3 point, float frequency, int octaves, uint32_t seed)
{
	const auto& turns = OctaveTurns();
	float sum = 0.0f;
	float amplitude = 0.5f;
	float total = 0.0f;
	for (int i = 0; i < octaves; ++i)
	{
		const auto& turn = turns.at(static_cast<size_t>(i) % turns.size());
		sum +=
		    amplitude * volumetric_clouds::Perlin(turn * point * frequency + 17.0f, k_Period, seed + static_cast<uint32_t>(i));
		total += amplitude;
		frequency *= 2.03f;
		amplitude *= k_Roughness;
	}
	return sum / total;
}

float RandomOf(uint32_t index, uint32_t salt)
{
	return volumetric_clouds::Hash({static_cast<int32_t>(index), static_cast<int32_t>(salt), 7}, k_Seed);
}

/// A cyclone: its middle on the sphere, its radius as an angle, and its turn (1 against the clock in the north)
struct Cyclone
{
	glm::vec3 middle {0.0f, 1.0f, 0.0f};
	float radius {0.1f};
	float spin {1.0f};
	float strength {1.0f};
};

const std::array<Cyclone, planet_weather::k_Cyclones>& Cyclones()
{
	static const std::array<Cyclone, planet_weather::k_Cyclones> k_All = [] {
		std::array<Cyclone, planet_weather::k_Cyclones> all {};
		const glm::vec3 north = planet_weather::NorthPole();
		// An axis across the pole, from which the longitude counts
		const glm::vec3 east = glm::normalize(glm::cross(north, glm::vec3 {1.0f, 0.0f, 0.0f}));
		const glm::vec3 other = glm::cross(north, east);
		for (uint32_t i = 0; i < all.size(); ++i)
		{
			// Most cyclones are over the storm tracks of the middle latitudes, a few near the tropics
			const bool south = i % 2 == 1;
			const float latitude = glm::radians(28.0f + 37.0f * RandomOf(i, 1)) * (south ? -1.0f : 1.0f);
			const float longitude = 2.0f * std::numbers::pi_v<float> * RandomOf(i, 2);
			const glm::vec3 around = std::cos(longitude) * east + std::sin(longitude) * other;
			all.at(i) = {.middle = glm::normalize(std::cos(latitude) * around + std::sin(latitude) * north),
			             .radius = 0.06f + 0.09f * RandomOf(i, 3),
			             .spin = south ? -1.0f : 1.0f,
			             .strength = 0.6f + 0.4f * RandomOf(i, 4)};
		}
		return all;
	}();
	return k_All;
}

float Smooth(float low, float high, float x)
{
	const float t = std::clamp((x - low) / (high - low), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

} // namespace

glm::vec3 planet_weather::NorthPole()
{
	// At the island the pole is 90 degrees less the latitude from straight up, towards the north (+z)
	const float tilt = glm::radians(90.0f - k_IslandLatitude);
	return {0.0f, std::cos(tilt), std::sin(tilt)};
}

float planet_weather::LatitudeOf(glm::vec3 direction)
{
	return glm::degrees(std::asin(std::clamp(glm::dot(direction, NorthPole()), -1.0f, 1.0f)));
}

float planet_weather::LatitudeBands(float latitude)
{
	const float away = std::abs(latitude);
	const auto bump = [](float x, float middle, float width) {
		const float d = (x - middle) / width;
		return std::exp(-d * d);
	};
	// The calms of the equator, the dry subtropics, the storm tracks and the polar air
	return 0.18f * bump(away, 0.0f, 7.0f) - 0.25f * bump(away, 24.0f, 9.0f) + 0.16f * bump(away, 52.0f, 12.0f) +
	       0.05f * bump(away, 80.0f, 10.0f);
}

planet_weather::Weather planet_weather::WeatherAt(glm::vec3 direction)
{
	const glm::vec3 d = glm::normalize(direction);
	// The cyclones turn the air round their middles: the noise that the cover reads turns with them, into spirals
	glm::vec3 q = d;
	float arms = 0.0f;
	for (const auto& cyclone : Cyclones())
	{
		const float angle = std::acos(std::clamp(glm::dot(q, cyclone.middle), -1.0f, 1.0f));
		const float r = angle / cyclone.radius;
		if (r > 3.0f)
		{
			continue;
		}
		const float swirl = cyclone.spin * 2.2f * std::exp(-r * r * 0.5f);
		q = Turn(cyclone.middle, swirl) * q;
		// The spiral arms of cloud round a clear eye
		const glm::vec3 east = glm::normalize(glm::cross(NorthPole(), cyclone.middle));
		const glm::vec3 north = glm::cross(cyclone.middle, east);
		const float theta = std::atan2(glm::dot(q, north), glm::dot(q, east));
		const float spiral = 0.5f + 0.5f * std::cos(2.0f * cyclone.spin * theta - 5.0f * std::log(r + 0.08f));
		const float reach = 1.0f - Smooth(0.6f, 2.6f, r);
		const float eye = Smooth(0.05f, 0.2f, r);
		arms = std::max(arms, cyclone.strength * reach * eye * (0.35f + 0.65f * spiral));
	}
	// The noise bends itself, so that the weather has no round blobs
	const glm::vec3 bend {Octaves(q, 2.5f, 3, k_Seed + 100), Octaves(q, 2.5f, 3, k_Seed + 200),
	                      Octaves(q, 2.5f, 3, k_Seed + 300)};
	const glm::vec3 bent = q + bend * 0.35f;
	const float noise = Octaves(bent, 2.0f, 7, k_Seed);
	// Long fronts: narrow bands where a slow noise crosses its middle
	const float slow = Octaves(bent, 1.6f, 3, k_Seed + 400);
	const float fronts =
	    std::exp(-slow * slow * 900.0f) * Smooth(0.0f, 0.3f, std::abs(Octaves(bent, 4.0f, 2, k_Seed + 500)) + 0.1f);
	const float latitude = LatitudeOf(d);
	Weather weather;
	weather.front = std::clamp(std::max(arms, fronts * 0.8f), 0.0f, 1.0f);
	weather.cover = std::clamp(0.5f + 2.6f * noise + LatitudeBands(latitude) + 0.45f * weather.front, 0.0f, 1.0f);
	const float tropics = std::exp(-(latitude / 14.0f) * (latitude / 14.0f));
	weather.type =
	    std::clamp(0.5f + 0.8f * Octaves(bent, 5.0f, 3, k_Seed + 600) + 0.25f * tropics + 0.2f * weather.front, 0.0f, 1.0f);
	weather.pattern = Smooth(-0.08f, 0.08f, Octaves(q, 45.0f, 2, k_Seed + 700));
	return weather;
}

glm::vec3 planet_weather::CubeDirection(uint32_t face, glm::vec2 uv)
{
	const float s = uv.x * 2.0f - 1.0f;
	const float t = uv.y * 2.0f - 1.0f;
	glm::vec3 d {0.0f};
	switch (face)
	{
	case 0:
		d = {1.0f, -t, -s};
		break;
	case 1:
		d = {-1.0f, -t, s};
		break;
	case 2:
		d = {s, 1.0f, t};
		break;
	case 3:
		d = {s, -1.0f, -t};
		break;
	case 4:
		d = {s, -t, 1.0f};
		break;
	default:
		d = {-s, -t, -1.0f};
		break;
	}
	return glm::normalize(d);
}

std::vector<uint8_t> planet_weather::CubeTexels(uint32_t side)
{
	const size_t faceSize = static_cast<size_t>(side) * side * 4;
	std::vector<uint8_t> texels(faceSize * 6);
	const auto toByte = [](float value) { return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); };
	std::array<std::future<void>, 6> faces;
	for (uint32_t face = 0; face < 6; ++face)
	{
		faces.at(face) = std::async(std::launch::async, [&, face] {
			for (uint32_t y = 0; y < side; ++y)
			{
				for (uint32_t x = 0; x < side; ++x)
				{
					const glm::vec2 uv {(static_cast<float>(x) + 0.5f) / static_cast<float>(side),
					                    (static_cast<float>(y) + 0.5f) / static_cast<float>(side)};
					const auto weather = WeatherAt(CubeDirection(face, uv));
					const size_t at = faceSize * face + (static_cast<size_t>(y) * side + x) * 4;
					texels[at + 0] = toByte(weather.cover);
					texels[at + 1] = toByte(weather.type);
					texels[at + 2] = toByte(weather.front);
					texels[at + 3] = toByte(weather.pattern);
				}
			}
		});
	}
	for (auto& face : faces)
	{
		face.wait();
	}
	return texels;
}

glm::vec3 planet_weather::DirectionAt(glm::vec2 fromMiddle, float radius)
{
	return glm::normalize(glm::vec3 {fromMiddle.x, radius, fromMiddle.y});
}
