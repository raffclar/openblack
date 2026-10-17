/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the light that the land and the sky give to the shade

#include "GlobalIllumination.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

using namespace openblack::graphics;
using namespace openblack::graphics::global_illumination;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
/// The harmonics' constants: the mean's and the first band's
constexpr float k_Y0 = 0.282095f;
constexpr float k_Y1 = 0.488603f;
} // namespace

std::array<float, 4> global_illumination::ShBasis(glm::vec3 direction)
{
	return {k_Y0, k_Y1 * direction.y, k_Y1 * direction.z, k_Y1 * direction.x};
}

void global_illumination::ShAdd(ShL1& sh, glm::vec3 direction, glm::vec3 radiance, float solidAngle)
{
	const auto basis = ShBasis(direction);
	for (size_t i = 0; i < basis.size(); ++i)
	{
		sh.c.at(i) += radiance * basis.at(i) * solidAngle;
	}
}

glm::vec3 global_illumination::ShIrradiance(const ShL1& sh, glm::vec3 normal)
{
	// The light of a cosine lobe: pi for the mean, two thirds of pi for the first band
	constexpr float k_A0 = k_Pi;
	constexpr float k_A1 = 2.0f * k_Pi / 3.0f;
	const glm::vec3 irradiance =
	    k_A0 * k_Y0 * sh.c[0] + k_A1 * k_Y1 * (sh.c[1] * normal.y + sh.c[2] * normal.z + sh.c[3] * normal.x);
	return glm::max(irradiance, glm::vec3(0.0f));
}

std::vector<glm::vec3> global_illumination::RayDirections(uint16_t count)
{
	std::vector<glm::vec3> directions;
	directions.reserve(count);
	const float golden = k_Pi * (3.0f - std::sqrt(5.0f));
	for (uint16_t i = 0; i < count; ++i)
	{
		const float y = 1.0f - (2.0f * static_cast<float>(i) + 1.0f) / static_cast<float>(count);
		const float r = std::sqrt(std::max(1.0f - y * y, 0.0f));
		const float angle = golden * static_cast<float>(i);
		directions.emplace_back(r * std::cos(angle), y, r * std::sin(angle));
	}
	return directions;
}

ShL1 global_illumination::SkySh(glm::vec3 towardsLight, float altitude, const atmosphere::Air& air, uint16_t samples)
{
	ShL1 sh;
	const float solidAngle = 4.0f * k_Pi / static_cast<float>(std::max<uint16_t>(samples, 1));
	for (const auto& direction : RayDirections(samples))
	{
		if (direction.y <= 0.0f)
		{
			continue;
		}
		ShAdd(sh, direction, atmosphere::SkyRadiance(direction, towardsLight, altitude, air), solidAngle);
	}
	return sh;
}

glm::vec2 global_illumination::ProbeCentre(uint16_t column, uint16_t row)
{
	return {(static_cast<float>(column) + 0.5f) * k_ProbeSpacing, (static_cast<float>(row) + 0.5f) * k_ProbeSpacing};
}

void ProbeSweep::Start()
{
	_nextRow = 0;
}

std::optional<ProbeSweep::Band> ProbeSweep::Next()
{
	if (!_nextRow.has_value())
	{
		return std::nullopt;
	}
	const uint16_t first = *_nextRow;
	const auto rows = static_cast<uint16_t>(std::min<int>(k_RowsPerFrame, k_ProbesPerSide - first));
	const auto next = static_cast<uint16_t>(first + rows);
	if (next >= k_ProbesPerSide)
	{
		_nextRow.reset();
	}
	else
	{
		_nextRow = next;
	}
	return Band {.firstRow = first, .rows = rows};
}

float global_illumination::ProbeBlend(float seconds)
{
	const float t = std::clamp(seconds / k_BlendSeconds, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

glm::vec3 global_illumination::ShadeTint(glm::vec3 light, glm::vec3 openSky)
{
	const float reference = std::max(atmosphere::Luminance(openSky), 1e-5f);
	const glm::vec3 ratio = glm::max(light / reference, glm::vec3(0.0f));
	const float grey = atmosphere::Luminance(ratio);
	const glm::vec3 tint = glm::mix(glm::vec3(grey), ratio, k_ColourShare);
	const float held = std::clamp(grey, k_DarkestShade, k_BrightestShade);
	return grey > 1e-5f ? tint * (held / grey) : glm::vec3(k_DarkestShade);
}
