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
#include <chrono>
#include <limits>
#include <numbers>

#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/geometric.hpp>

#include "Graphics/Moon.h"

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

/// The direction towards a body in the sky from its hour angle west of the south and its angle north of the equator,
/// both in radians, at the latitude of the world
glm::vec3 TowardsInSky(float hourAngle, float declination)
{
	constexpr float k_Radians = k_Pi / 180.0f;
	const float latitude = k_LatitudeDegrees * k_Radians;
	// The direction in the frame of the ground: east, north and up
	const float east = -std::cos(declination) * std::sin(hourAngle);
	const float north =
	    (std::cos(latitude) * std::sin(declination)) - (std::sin(latitude) * std::cos(declination) * std::cos(hourAngle));
	const float up =
	    (std::sin(latitude) * std::sin(declination)) + (std::cos(latitude) * std::cos(declination) * std::cos(hourAngle));
	return glm::normalize((east * k_East) + (north * k_North) + glm::vec3(0.0f, up, 0.0f));
}

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

glm::vec3 atmosphere::DayMultipleScattering(float altitude, float lightMu, const Air& air)
{
	return k_MultipleShare * Transmittance(altitude, std::max(lightMu, 0.05f), air) * Smoothstep(-0.25f, 0.05f, lightMu) /
	       (4.0f * k_Pi);
}

glm::vec3 atmosphere::MultipleScattering(float altitude, float lightMu, const Air& air)
{
	const float day = Smoothstep(k_MultipleDayLow, k_MultipleDayHigh, lightMu);
	if (air.multiple == nullptr || day >= 1.0f)
	{
		return DayMultipleScattering(altitude, lightMu, air);
	}
	const glm::vec3 table = air.multiple->At(altitude, lightMu);
	return day <= 0.0f ? table : glm::mix(table, DayMultipleScattering(altitude, lightMu, air), day);
}

glm::vec3 atmosphere::MultipleScatteringAt(float altitude, float lightMu, const Air& air, int directions, int steps,
                                           const ThroughFunction& through)
{
	const float radius = k_GroundRadius + std::clamp(altitude, 1e-3f, k_TopRadius - k_GroundRadius - 1e-3f);
	const glm::vec3 eye {0.0f, radius, 0.0f};
	const float mu = std::clamp(lightMu, -1.0f, 1.0f);
	const glm::vec3 light {std::sqrt(1.0f - (mu * mu)), mu, 0.0f};
	// The directions are on a Fibonacci spiral, so that they cover the ball evenly
	const float golden = k_Pi * (3.0f - std::sqrt(5.0f));
	glm::vec3 second {0.0f};
	glm::vec3 share {0.0f};
	for (int i = 0; i < directions; ++i)
	{
		const float y = 1.0f - (2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(directions));
		const float across = std::sqrt(std::max(1.0f - (y * y), 0.0f));
		const float angle = golden * static_cast<float>(i);
		const glm::vec3 direction {across * std::cos(angle), y, across * std::sin(angle)};
		const bool ground = HitsGround(radius, y);
		const float length = ground ? DistanceToGround(radius, y) : DistanceToTop(radius, y);
		const float step = length / static_cast<float>(steps);
		glm::vec3 throughput {1.0f};
		for (int k = 0; k < steps; ++k)
		{
			const glm::vec3 point = eye + (direction * ((static_cast<float>(k) + 0.5f) * step));
			const float r = glm::length(point);
			const float h = r - k_GroundRadius;
			const auto medium = MediumAt(h, air);
			const glm::vec3 scattering = medium.rayleigh + glm::vec3(medium.mie);
			const glm::vec3 extinction = glm::max(medium.extinction, glm::vec3(1e-7f));
			const glm::vec3 dimmed = glm::exp(-extinction * step);
			const glm::vec3 part = throughput * (1.0f - dimmed) / extinction;
			second += part * scattering * through(h, glm::dot(point / r, light)) / (4.0f * k_Pi);
			share += part * scattering;
			throughput *= dimmed;
		}
		// The ground sends back some of the sun's light that reaches it
		if (ground)
		{
			const glm::vec3 normal = glm::normalize(eye + (direction * length));
			const float lit = glm::dot(normal, light);
			second += throughput * through(0.0f, lit) * std::max(lit, 0.0f) * k_GroundAlbedo / k_Pi;
		}
	}
	second /= static_cast<float>(directions);
	share /= static_cast<float>(directions);
	// Each scattering after the second gives the same share of the one before it: a series
	return second / glm::max(1.0f - share, glm::vec3(1e-3f));
}

HeightAngle atmosphere::FromMultipleTexel(int column, int row, uint16_t width, uint16_t height)
{
	const float mu = -1.0f + (2.0f * static_cast<float>(column) / static_cast<float>(width - 1));
	const float altitude = (k_TopRadius - k_GroundRadius) * static_cast<float>(row) / static_cast<float>(height - 1);
	return {altitude, mu};
}

glm::vec3 atmosphere::MultipleTable::At(float altitude, float lightMu) const
{
	if (width < 2 || height < 2 || texels.size() < static_cast<size_t>(width) * height)
	{
		return glm::vec3(0.0f);
	}
	const float x = std::clamp((lightMu + 1.0f) * 0.5f, 0.0f, 1.0f) * static_cast<float>(width - 1);
	const float y = std::clamp(altitude / (k_TopRadius - k_GroundRadius), 0.0f, 1.0f) * static_cast<float>(height - 1);
	const int x0 = std::min(static_cast<int>(x), width - 2);
	const int y0 = std::min(static_cast<int>(y), height - 2);
	const float fx = x - static_cast<float>(x0);
	const float fy = y - static_cast<float>(y0);
	const auto texel = [this](int column, int row) { return texels[static_cast<size_t>(row) * width + column]; };
	const glm::vec3 low = glm::mix(texel(x0, y0), texel(x0 + 1, y0), fx);
	const glm::vec3 high = glm::mix(texel(x0, y0 + 1), texel(x0 + 1, y0 + 1), fx);
	return glm::mix(low, high, fy);
}

std::shared_ptr<const MultipleTable> atmosphere::MakeMultipleTable(const Air& air)
{
	// A small table of the light that gets through, in the same layout as the texture of the graphics card
	constexpr int k_Across = 64;
	constexpr int k_Down = 32;
	std::vector<glm::vec3> through(static_cast<size_t>(k_Across) * k_Down);
	for (int row = 0; row < k_Down; ++row)
	{
		for (int column = 0; column < k_Across; ++column)
		{
			const glm::vec2 uv {static_cast<float>(column) / (k_Across - 1), static_cast<float>(row) / (k_Down - 1)};
			const auto at = FromTransmittanceUv(uv);
			through[static_cast<size_t>(row) * k_Across + column] = Transmittance(at.altitude, at.mu, air);
		}
	}
	const auto lookUp = [&through](float altitude, float mu) {
		const float radius = k_GroundRadius + std::max(altitude, 0.0f);
		if (HitsGround(radius, mu))
		{
			return glm::vec3(0.0f);
		}
		const glm::vec2 uv = glm::clamp(TransmittanceUv(altitude, mu), glm::vec2(0.0f), glm::vec2(1.0f));
		const float x = uv.x * (k_Across - 1);
		const float y = uv.y * (k_Down - 1);
		const int x0 = std::min(static_cast<int>(x), k_Across - 2);
		const int y0 = std::min(static_cast<int>(y), k_Down - 2);
		const float fx = x - static_cast<float>(x0);
		const float fy = y - static_cast<float>(y0);
		const auto texel = [&through](int column, int row) { return through[static_cast<size_t>(row) * k_Across + column]; };
		return glm::mix(glm::mix(texel(x0, y0), texel(x0 + 1, y0), fx), glm::mix(texel(x0, y0 + 1), texel(x0 + 1, y0 + 1), fx),
		                fy);
	};
	auto table = std::make_shared<MultipleTable>();
	table->width = k_MultipleCpuWidth;
	table->height = k_MultipleCpuHeight;
	table->texels.resize(static_cast<size_t>(table->width) * table->height);
	for (int row = 0; row < table->height; ++row)
	{
		for (int column = 0; column < table->width; ++column)
		{
			const auto at = FromMultipleTexel(column, row, table->width, table->height);
			table->texels[static_cast<size_t>(row) * table->width + column] =
			    MultipleScatteringAt(at.altitude, at.mu, air, k_MultipleCpuDirections, k_MultipleCpuSteps, lookUp);
		}
	}
	return table;
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

glm::vec3 atmosphere::HorizonRadiance(glm::vec3 towardsLight, float altitude, const Air& air, float horizonY)
{
	glm::vec3 sum {0.0f};
	for (int i = 0; i < k_HorizonDirections; ++i)
	{
		const float angle = 2.0f * k_Pi * (static_cast<float>(i) + 0.5f) / static_cast<float>(k_HorizonDirections);
		const glm::vec3 view = glm::normalize(glm::vec3(std::cos(angle), horizonY + 0.02f, std::sin(angle)));
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

float atmosphere::SkyColourKept(float sunHeight)
{
	// The blue hour: the sky gets back most of its colour as the sun goes down to 6 degrees under the horizon, keeps it
	// to 9 degrees, and loses it again by 17 degrees, when the night starts
	const float blueHour = Smoothstep(0.0f, -0.1f, sunHeight) * Smoothstep(-0.29f, -0.16f, sunHeight);
	return 0.25f + 0.75f * std::max(Smoothstep(0.0f, 0.35f, sunHeight), k_BlueHourColour * blueHour);
}

glm::vec3 atmosphere::DimSkyColour(glm::vec3 colour, float sunHeight)
{
	return glm::mix(glm::vec3(Luminance(colour)), colour, SkyColourKept(sunHeight));
}

glm::vec3 atmosphere::OnScreen(glm::vec3 linear)
{
	constexpr float k_Gamma = 2.2f;
	return glm::pow(glm::max(linear, glm::vec3(0.0f)), glm::vec3(1.0f / k_Gamma));
}

glm::vec3 atmosphere::CloudSkyLight(glm::vec3 original, glm::vec3 airAbove, float sunHeight)
{
	const glm::vec3 air = airAbove * (Luminance(original) / std::max(Luminance(airAbove), 1e-4f));
	return glm::mix(original, air, 1.0f - Smoothstep(0.1f, 0.35f, sunHeight));
}

glm::vec3 atmosphere::SunTowards(float hour)
{
	constexpr float k_Radians = k_Pi / 180.0f;
	const float h = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
	return TowardsInSky((h - 12.0f) * 15.0f * k_Radians, k_DeclinationDegrees * k_Radians);
}

double atmosphere::MoonMonthFraction(int64_t unixTime, int32_t passedDays, float hour)
{
	const int64_t wholeDays = (unixTime / moon::k_SecondsPerDay) - moon::k_NewMoonDay + passedDays;
	const double days = static_cast<double>(wholeDays) + (static_cast<double>(std::clamp(hour, 0.0f, 24.0f)) / 24.0);
	const double months = days * moon::k_MonthsPerDay;
	return months - std::floor(months);
}

float atmosphere::MoonPhase(double monthFraction)
{
	return static_cast<float>((1.0 - monthFraction) * 2.0 * std::numbers::pi);
}

glm::vec3 atmosphere::MoonTowards(float hour, double monthFraction)
{
	constexpr float k_Radians = k_Pi / 180.0f;
	// The sun is at the start of its yearly path on the day of the equinox. The moon is east of it along the path by
	// its age. Its angles on the sky come from its place along the path and the tilt of the path to the equator.
	const auto along = static_cast<float>(monthFraction * 2.0 * std::numbers::pi);
	const float tilt = k_AxisTiltDegrees * k_Radians;
	const float eastOfSun = std::atan2(std::cos(tilt) * std::sin(along), std::cos(along));
	const float declination = std::asin(std::sin(tilt) * std::sin(along));
	const float h = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
	return TowardsInSky(((h - 12.0f) * 15.0f * k_Radians) - eastOfSun, declination);
}

float atmosphere::MoonThroughAir(float elevationSine)
{
	if (elevationSine <= 0.0f)
	{
		return 0.0f;
	}
	// The length of the way through the air, in units of the way straight up (Kasten and Young). The clear air takes
	// a fifth of the light for each unit of that length.
	const float degrees = std::asin(std::clamp(elevationSine, 0.0f, 1.0f)) * 180.0f / k_Pi;
	const float airMass = 1.0f / (elevationSine + (0.50572f * std::pow(degrees + 6.07995f, -1.6364f)));
	return std::exp(-0.2f * (airMass - 1.0f));
}

float atmosphere::NightGlow(float sunElevationSine)
{
	return k_NightGlow * AirMoonShare(sunElevationSine);
}

glm::vec3 atmosphere::AirMoonTowards(glm::vec3 sunTowards, glm::vec3 moonTowards)
{
	const glm::vec3 moonUnit = glm::normalize(moonTowards);
	const float up = Smoothstep(-0.05f, 0.1f, moonUnit.y);
	return glm::normalize(glm::mix(-glm::normalize(sunTowards), moonUnit, up));
}

int32_t atmosphere::DaysStarted(float earlierHour, float hour)
{
	constexpr float k_Late = 18.0f;
	constexpr float k_Early = 6.0f;
	if (earlierHour < 0.0f)
	{
		return 0;
	}
	if (earlierHour >= k_Late && hour < k_Early)
	{
		return 1;
	}
	if (earlierHour < k_Early && hour >= k_Late)
	{
		return -1;
	}
	return 0;
}

float atmosphere::SunStrength(float elevationSine)
{
	const float degrees = std::asin(std::clamp(elevationSine, -1.0f, 1.0f)) * 180.0f / k_Pi;
	return 255.0f * std::clamp((degrees + 1.0f) / 5.0f, 0.0f, 1.0f);
}

float atmosphere::SkyTypeFromSun(float elevationSine)
{
	const float degrees = std::asin(std::clamp(elevationSine, -1.0f, 1.0f)) * 180.0f / k_Pi;
	if (degrees < 0.0f)
	{
		return std::clamp(1.0f + (degrees / 18.0f), 0.0f, 1.0f);
	}
	return 1.0f + std::clamp((degrees - 5.0f) / 5.0f, 0.0f, 1.0f);
}

float atmosphere::AirMoonShare(float elevationSine)
{
	const float degrees = std::asin(std::clamp(elevationSine, -1.0f, 1.0f)) * 180.0f / k_Pi;
	const float t = std::clamp((-degrees - 2.0f) / 6.0f, 0.0f, 1.0f);
	return t * t * (3.0f - (2.0f * t));
}

glm::vec3 atmosphere::AirLightTowards(glm::vec3 sun, glm::vec3 moon, float moonShare)
{
	const auto angles = [](glm::vec3 towards) {
		const glm::vec3 unit = glm::normalize(towards);
		float round = std::atan2(glm::dot(unit, k_East), glm::dot(unit, k_North));
		if (round < 0.0f)
		{
			round += 2.0f * k_Pi;
		}
		return glm::vec2(round, std::asin(std::clamp(unit.y, -1.0f, 1.0f)));
	};
	constexpr float k_Dip = 10.0f * k_Pi / 180.0f;
	const float share = std::clamp(moonShare, 0.0f, 1.0f);
	glm::vec2 angle = glm::mix(angles(sun), angles(moon), share);
	angle.y -= k_Dip * std::sin(k_Pi * share);
	const float flat = std::cos(angle.y);
	return (flat * ((std::sin(angle.x) * k_East) + (std::cos(angle.x) * k_North))) + glm::vec3(0.0f, std::sin(angle.y), 0.0f);
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

float atmosphere::HeightOverMapBall(glm::vec3 eye, float seaLevel, float mapSize)
{
	const glm::dvec2 across = glm::dvec2(eye.x, eye.z) - glm::dvec2(0.5 * mapSize);
	const double height = static_cast<double>(eye.y) - seaLevel;
	const double radius = k_WorldRadius;
	const double squared = glm::dot(across, across) + height * (height + 2.0 * radius);
	// distance - R, with no loss of the small terms
	return static_cast<float>(squared / (std::sqrt(glm::dot(across, across) + (height + radius) * (height + radius)) + radius));
}

namespace
{
// The share of k_CapOnMapAbove from which a cap round an eye with no island in sight moves back to the map
constexpr float k_OrbitCapFrom = 0.9f;

// How much the cap follows the eye: all of it low down, none from high up, where it is on the map's middle
float CapFollowsEye(glm::vec2 eye, float heightOverSea, float mapSize)
{
	const float overBall = atmosphere::HeightOverMapBall({eye.x, heightOverSea, eye.y}, 0.0f, mapSize);
	return 1.0f - Smoothstep(atmosphere::k_CapFollowsEyeBelow, atmosphere::k_CapOnMapAbove, std::max(heightOverSea, overBall));
}
} // namespace

float atmosphere::IslandOutOfView(glm::vec2 eye, float heightOverSea, float mapSize)
{
	const glm::dvec2 mapMiddle {0.5 * mapSize};
	const double halfDiagonal = 0.5 * std::numbers::sqrt2 * mapSize;
	const double across = std::max(glm::length(glm::dvec2(eye) - mapMiddle) - halfDiagonal, 0.0);
	// The distance to the horizon from the eye, and from the island's highest ground on the other side of it
	const double radius = k_WorldRadius;
	const double height = std::max(static_cast<double>(heightOverSea), 0.0);
	const double sight = std::sqrt(height * (2.0 * radius + height)) + std::sqrt(k_IslandTop * (2.0 * radius + k_IslandTop));
	const auto out = static_cast<float>(std::clamp((across - sight) / (0.25 * sight), 0.0, 1.0));
	// Under the orbit view the camera is over its own patch of sea. From the orbit view the cap stays on the map.
	const float overBall = HeightOverMapBall({eye.x, heightOverSea, eye.y}, 0.0f, mapSize);
	const float underOrbit =
	    1.0f - Smoothstep(k_OrbitCapFrom * k_CapOnMapAbove, k_CapOnMapAbove, std::max(heightOverSea, overBall));
	return out * out * (3.0f - 2.0f * out) * underOrbit;
}

atmosphere::WorldCap atmosphere::MapCap(glm::vec2 eye, float heightOverSea, float mapSize, bool islandFar)
{
	const glm::vec2 mapMiddle {0.5f * mapSize};
	const float followsEye = CapFollowsEye(eye, heightOverSea, mapSize);
	glm::vec2 middle = glm::mix(mapMiddle, eye, followsEye);
	const glm::vec2 far {std::max(middle.x, mapSize - middle.x), std::max(middle.y, mapSize - middle.y)};
	float radius = glm::length(far);
	// With no island in sight the cap is small round the eye
	const float out = islandFar ? 1.0f : IslandOutOfView(eye, heightOverSea, mapSize);
	middle = glm::mix(middle, eye, out);
	radius = glm::mix(radius, k_SeaCapRadius, out);
	return {.eyeFromMiddle = eye - middle, .radius = radius};
}

atmosphere::WorldSection atmosphere::SectionToward(const WorldCap& cap, glm::vec2 across)
{
	const float b = glm::dot(cap.eyeFromMiddle, across);
	const float side = std::max(glm::dot(cap.eyeFromMiddle, cap.eyeFromMiddle) - (b * b), 0.0f);
	if (side < cap.radius * cap.radius)
	{
		const float half = std::sqrt((cap.radius * cap.radius) - side);
		return {.chordFrom = -b - half, .chordTo = -b + half, .pastSquared = 0.0f};
	}
	const float past = std::sqrt(side) - cap.radius;
	return {.chordFrom = -b, .chordTo = -b, .pastSquared = past * past};
}

namespace
{
// The direction on the ground of a ray, or, for a ray straight up or down, the direction towards the cap
glm::vec2 AcrossOf(const atmosphere::WorldCap& cap, glm::vec2 direction)
{
	const float length = glm::length(direction);
	if (length > 1e-6f)
	{
		return direction / length;
	}
	const float fromMiddle = glm::length(cap.eyeFromMiddle);
	return fromMiddle > 1e-3f ? -cap.eyeFromMiddle / fromMiddle : glm::vec2(1.0f, 0.0f);
}

// Where a ray meets a circle in its upright plane, the circle's middle `middle` units along the plane and the world's
// radius under the sea, with `c` the square of the eye's distance from the middle less the square of the radius
std::optional<glm::vec2> CircleSpan(float middle, float c, float across, float up, float heightOverSea)
{
	const float b = (-across * middle) + (up * (heightOverSea + atmosphere::k_WorldRadius));
	const float disc = (b * b) - c;
	if (disc < 0.0f)
	{
		return std::nullopt;
	}
	const float s = std::sqrt(disc);
	// The roots of t^2 + 2 b t + c, kept exact for a small c
	const float q = b < 0.0f ? -b + s : -b - s;
	const glm::vec2 roots = std::abs(q) > 1e-20f ? glm::vec2(c / q, q) : glm::vec2(-b);
	return glm::vec2(std::min(roots.x, roots.y), std::max(roots.x, roots.y));
}

// Where a ray from the eye is inside the world with `lift` added to its height: the slab over the cap's chord with a
// circle at each end in the ray's upright plane. The shape is convex, so the span joins the spans of its parts.
std::optional<glm::vec2> WorldSpan(const atmosphere::WorldCap& cap, float heightOverSea, glm::vec3 direction, float lift)
{
	constexpr float k_Radius = atmosphere::k_WorldRadius;
	const glm::vec2 flat {direction.x, direction.z};
	const float across = glm::length(flat);
	const auto section = atmosphere::SectionToward(cap, AcrossOf(cap, flat));
	const float c0 = ((heightOverSea - lift) * (heightOverSea + lift + (2.0f * k_Radius))) + section.pastSquared;
	std::optional<glm::vec2> span;
	const auto join = [&span](glm::vec2 part) {
		span = span.has_value() ? glm::vec2(std::min(span->x, part.x), std::max(span->y, part.y)) : part;
	};
	for (const float middle : {section.chordFrom, section.chordTo})
	{
		if (const auto part = CircleSpan(middle, (middle * middle) + c0, across, direction.y, heightOverSea))
		{
			join(*part);
		}
	}
	if (section.chordTo > section.chordFrom)
	{
		glm::vec2 slab {-1e30f, 1e30f};
		if (across > 1e-6f)
		{
			slab = {section.chordFrom / across, section.chordTo / across};
		}
		else if (section.chordFrom > 0.0f || section.chordTo < 0.0f)
		{
			slab = {1.0f, -1.0f};
		}
		const float top = heightOverSea - lift;
		const float bottom = heightOverSea + lift + (2.0f * k_Radius);
		if (direction.y < 0.0f)
		{
			slab = {std::max(slab.x, top / -direction.y), std::min(slab.y, bottom / -direction.y)};
		}
		else if (direction.y > 0.0f)
		{
			slab = {std::max(slab.x, -bottom / direction.y), std::min(slab.y, -top / direction.y)};
		}
		else if (top > 0.0f)
		{
			slab = {1.0f, -1.0f};
		}
		if (slab.x < slab.y)
		{
			join(slab);
		}
	}
	return span;
}
} // namespace

float atmosphere::WorldDrop(const WorldCap& cap, glm::vec2 fromEye)
{
	const auto section = SectionToward(cap, AcrossOf(cap, fromEye));
	const float along = glm::length(fromEye);
	const float past = along > section.chordTo ? along - section.chordTo : std::max(section.chordFrom - along, 0.0f);
	const float square = (past * past) + section.pastSquared;
	if (square >= k_WorldRadius * k_WorldRadius)
	{
		return std::numeric_limits<float>::max();
	}
	return square / (k_WorldRadius + std::sqrt((k_WorldRadius * k_WorldRadius) - square));
}

std::optional<float> atmosphere::CurvedSeaDistance(const WorldCap& cap, float heightOverSea, glm::vec3 direction)
{
	const auto span = WorldSpan(cap, heightOverSea, direction, 0.0f);
	if (!span.has_value() || span->x >= span->y || span->x <= 0.0f)
	{
		return std::nullopt;
	}
	return span->x;
}

float atmosphere::WorldHorizonY(const WorldCap& cap, float heightOverSea)
{
	const float fromMiddle = glm::length(cap.eyeFromMiddle);
	const float chord = fromMiddle < cap.radius ? std::sqrt((cap.radius * cap.radius) - (fromMiddle * fromMiddle)) : 0.0f;
	const float past = std::max(fromMiddle - cap.radius, 0.0f);
	const float circle = std::sqrt(std::max((k_WorldRadius * k_WorldRadius) - (past * past), 0.0f));
	const float up = std::max(heightOverSea, 0.0f) + k_WorldRadius;
	const float toMiddle = std::sqrt((chord * chord) + (up * up));
	const float down = std::atan2(up, chord) - std::asin(std::min(circle / toMiddle, 1.0f));
	return -std::sin(down);
}

atmosphere::AirFrame atmosphere::AirFrameFor(const WorldCap& cap, float heightOverSea)
{
	const glm::vec3 fromMiddle {cap.eyeFromMiddle.x, heightOverSea + k_WorldRadius, cap.eyeFromMiddle.y};
	const float distance = glm::length(fromMiddle);
	const float across = glm::dot(cap.eyeFromMiddle, cap.eyeFromMiddle);
	// distance - R, with no loss of the small terms
	const float height = (across + (heightOverSea * (heightOverSea + (2.0f * k_WorldRadius)))) / (distance + k_WorldRadius);
	return {.up = fromMiddle / distance, .height = height};
}

float atmosphere::EyeOverWorld(glm::vec3 eye, float seaLevel, float mapSize, bool islandFar)
{
	const float heightOverSea = eye.y - seaLevel;
	return AirFrameFor(MapCap({eye.x, eye.z}, heightOverSea, mapSize, islandFar), heightOverSea).height;
}

glm::vec3 atmosphere::TurnToUp(glm::vec3 up, glm::vec3 direction)
{
	// Turn about the line up x (0, 1, 0) by the angle between them: v cos + k x v + k (k . v) / (1 + cos)
	const glm::vec3 axis {-up.z, 0.0f, up.x};
	if (glm::dot(axis, axis) < 1e-12f || up.y <= -1.0f + 1e-6f)
	{
		return direction;
	}
	return (direction * up.y) + glm::cross(axis, direction) + (axis * (glm::dot(axis, direction) / (1.0f + up.y)));
}

glm::vec3 atmosphere::MagneticPole()
{
	const float angle = k_PoleFromIslandDegrees * k_Pi / 180.0f;
	return (std::cos(angle) * glm::vec3(0.0f, 1.0f, 0.0f)) + (std::sin(angle) * k_North);
}

float atmosphere::MagneticLatitude(glm::vec3 up)
{
	return std::asin(std::clamp(glm::dot(glm::normalize(up), MagneticPole()), -1.0f, 1.0f)) * 180.0f / k_Pi;
}

float atmosphere::AuroraStrength(int night)
{
	// A hash of the night's number, then most nights pushed low
	auto bits = static_cast<uint32_t>(night) * 2654435761u + 0x9E3779B9u;
	bits ^= bits >> 15u;
	bits *= 2246822519u;
	bits ^= bits >> 13u;
	const float random = static_cast<float>(bits & 0xFFFFu) / 65535.0f;
	return std::pow(random, 1.8f);
}

float atmosphere::AuroraTonight(int night, std::optional<float> chosen)
{
	return chosen.has_value() ? std::clamp(*chosen, 0.0f, 1.0f) : AuroraStrength(night);
}

int atmosphere::DayOfYear(int64_t unixTime, int32_t passedDays)
{
	using namespace std::chrono;
	const auto day = sys_days {} + days {(unixTime / moon::k_SecondsPerDay) + passedDays};
	const year_month_day date {day};
	return static_cast<int>((day - sys_days {date.year() / January / 1}).count());
}

float atmosphere::NoctilucentChance(int dayOfYear)
{
	if (dayOfYear <= k_NoctilucentSeasonStart || dayOfYear >= k_NoctilucentSeasonEnd)
	{
		return 0.0f;
	}
	const float through = static_cast<float>(dayOfYear - k_NoctilucentSeasonStart) /
	                      static_cast<float>(k_NoctilucentSeasonEnd - k_NoctilucentSeasonStart);
	const float shape = std::sin(k_Pi * through);
	return k_NoctilucentPeakChance * shape * shape;
}

float atmosphere::NoctilucentStrength(int64_t night, int dayOfYear)
{
	// Two hashes of the night's number: one for whether the clouds form and one for how strong they are
	const auto hash = [night](uint32_t salt) {
		auto bits = (static_cast<uint32_t>(night) * 2246822519u) ^ salt;
		bits ^= bits >> 16u;
		bits *= 2654435761u;
		bits ^= bits >> 13u;
		bits *= 3266489917u;
		bits ^= bits >> 16u;
		return static_cast<float>(bits & 0xFFFFu) / 65535.0f;
	};
	if (hash(0x5BD1E995u) >= NoctilucentChance(dayOfYear))
	{
		return 0.0f;
	}
	return 0.5f + (0.5f * hash(0x27D4EB2Fu));
}

glm::vec3 atmosphere::LightEye(glm::vec3 eye, float seaLevel)
{
	constexpr glm::vec2 k_MapMiddle {2560.0f, 2560.0f};
	glm::vec2 across = glm::vec2(eye.x, eye.z) - k_MapMiddle;
	const float distance = glm::length(across);
	if (distance > k_LightEyeReach)
	{
		across *= k_LightEyeReach / distance;
	}
	return {k_MapMiddle.x + across.x, std::min(eye.y, seaLevel + k_LightEyeHeight), k_MapMiddle.y + across.y};
}
