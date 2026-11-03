/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandBrush.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

namespace openblack::editor::land_brush
{

float Weight(float distance, float innerRadius, float outerRadius)
{
	if (distance <= innerRadius)
	{
		return distance <= outerRadius ? 1.0f : 0.0f;
	}
	if (distance >= outerRadius)
	{
		return 0.0f;
	}
	return (outerRadius - distance) / (outerRadius - innerRadius);
}

uint8_t Brushed(uint8_t altitude, int step, float weight)
{
	const auto change = static_cast<int>(std::lround(static_cast<float>(step) * weight));
	return static_cast<uint8_t>(std::clamp(static_cast<int>(altitude) + change, 0, 255));
}

std::vector<glm::u16vec2> CornersInReach(glm::vec2 centre, float outerRadius)
{
	std::vector<glm::u16vec2> corners;
	const auto radius = std::clamp(outerRadius, 0.0f, k_MostRadius);
	const auto first = glm::max(glm::ivec2(glm::ceil((centre - radius) / k_CornerSpacing)), glm::ivec2(0));
	const auto last = glm::min(glm::ivec2(glm::floor((centre + radius) / k_CornerSpacing)), glm::ivec2(k_MapCorners - 1));
	for (int x = first.x; x <= last.x; ++x)
	{
		for (int z = first.y; z <= last.y; ++z)
		{
			const glm::vec2 corner {static_cast<float>(x) * k_CornerSpacing, static_cast<float>(z) * k_CornerSpacing};
			if (glm::distance(corner, centre) <= radius)
			{
				corners.emplace_back(static_cast<uint16_t>(x), static_cast<uint16_t>(z));
			}
		}
	}
	return corners;
}

std::vector<AltitudeChange> Stroke(const Brush& brush, glm::vec2 centre, const AltitudeAt& altitudeAt)
{
	std::vector<AltitudeChange> changes;
	if (brush.mode == Mode::Paint)
	{
		return changes;
	}
	const auto step = brush.mode == Mode::Raise ? brush.step : -brush.step;
	for (const auto corner : CornersInReach(centre, brush.outerRadius))
	{
		const auto altitude = altitudeAt(corner);
		if (!altitude.has_value())
		{
			continue;
		}
		const auto distance = glm::distance(glm::vec2(corner) * k_CornerSpacing, centre);
		const auto brushed = Brushed(*altitude, step, Weight(distance, brush.innerRadius, brush.outerRadius));
		if (brushed != *altitude)
		{
			changes.push_back({.corner = corner, .altitude = brushed});
		}
	}
	return changes;
}

std::vector<CountryChange> Paint(const Brush& brush, glm::vec2 centre, int countries, const CountryAt& countryAt)
{
	std::vector<CountryChange> changes;
	if (brush.mode != Mode::Paint || brush.country >= std::min(countries, k_MostCountries))
	{
		return changes;
	}
	for (const auto corner : CornersInReach(centre, brush.outerRadius))
	{
		const auto country = countryAt(corner);
		if (country.has_value() && *country != brush.country)
		{
			changes.push_back({.corner = corner, .country = brush.country});
		}
	}
	return changes;
}

bool MovedFarEnough(glm::vec2 lastStroke, glm::vec2 now)
{
	return glm::distance(lastStroke, now) >= k_CornerSpacing;
}

} // namespace openblack::editor::land_brush
