/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandShadow.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include "GroundShadow.h"

namespace openblack::graphics
{

namespace
{
/// The sun's height above the horizon at sunrise and sunset and at midday
constexpr float k_HorizonElevation = glm::radians(10.0f);
constexpr float k_MiddayElevation = glm::radians(60.0f);
/// Black & White's light sits at (-500000, 500000, -500000): the midday sun comes from that side
const glm::vec3 k_MiddaySunSide = glm::normalize(glm::vec3(-1.0f, 0.0f, -1.0f));
} // namespace

glm::vec3 HandShadow::SunlightDirection(float hours, const DayNightTimes& times)
{
	const auto sunrise = times.duskStart;
	const auto sunset = 24.0f - times.duskStart;
	const auto day = std::clamp((hours - sunrise) / std::max(sunset - sunrise, 0.001f), 0.0f, 1.0f);

	// Over the day the sun turns half way round the sky, through the midday side
	const auto turn = (day - 0.5f) * glm::pi<float>();
	const auto side = glm::vec3((k_MiddaySunSide.x * std::cos(turn)) - (k_MiddaySunSide.z * std::sin(turn)), 0.0f,
	                            (k_MiddaySunSide.x * std::sin(turn)) + (k_MiddaySunSide.z * std::cos(turn)));
	const auto elevation = k_HorizonElevation + ((k_MiddayElevation - k_HorizonElevation) * std::sin(day * glm::pi<float>()));
	const auto towardsSun = side * std::cos(elevation) + glm::vec3(0.0f, std::sin(elevation), 0.0f);
	return -glm::normalize(towardsSun);
}

float HandShadow::Daylight(float hours, const DayNightTimes& times)
{
	// The sky's times are those of the morning, the evening mirrors them around midday
	const auto morning = hours <= 12.0f ? hours : 24.0f - hours;
	return std::clamp((morning - times.duskStart) / std::max(times.dayFull - times.duskStart, 0.001f), 0.0f, 1.0f);
}

glm::vec3 HandShadow::LightPoint(const glm::vec3& groundBelow, float hours, const DayNightTimes& times)
{
	return groundBelow - (SunlightDirection(hours, times) * glm::length(GroundShadow::k_Sun));
}

} // namespace openblack::graphics
