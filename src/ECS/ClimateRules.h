/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <glm/vec2.hpp>

/// The formulas of the climates and of the storms that they make, with the float steps of the game
namespace openblack::ecs::climate_rules
{

/// The raining days of a climate as a share of rain, which the climate compares with the limits of its season
[[nodiscard]] float RainedFor(int32_t rainingDays);

/// The wind of a climate (x, z) along its angle in radians. Usually the wind is the sum of the two limits of the season.
/// When the desire to rain is at half way or very near it, the wind is two times the lower limit.
[[nodiscard]] glm::vec2 Wind(float minimum, float maximum, float angle, float rainDesire);

/// How cloudy a new storm is, from 0 to 1, by its temperature: 1 at 30 degrees and less above and below it
[[nodiscard]] float StormCloud(int8_t temperature);

/// A share from 0 to 1 in whole percent, cut towards zero
[[nodiscard]] int8_t Percent(float share);

/// The snow percentage of a new storm with a temperature of 0 or more: all snow at 0, less snow as it gets warmer
[[nodiscard]] int8_t StormSnow(int8_t temperature);

/// The cell at a distance in cells and an angle in radians from the middle cell of a climate, cut towards zero
[[nodiscard]] glm::ivec2 CellNearMiddle(float angle, float distance, uint16_t middleX, uint16_t middleZ);

} // namespace openblack::ecs::climate_rules
