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

#include <glm/vec3.hpp>

#include "WeatherInfo.h"

// The engine's atmosphere: a 128 x 128 grid of 40 m cells (8 bytes each) caching the weather that the registered storms
// make at each cell's corner. A cell is recomputed on demand when its stamp is not the current frame (advanced once per
// game turn by UpdateGame); outside the grid the ambient weather is used.

namespace openblack::weather::atmos
{
constexpr int32_t k_GridSize = 128;
constexpr float k_CellSize = 40.0f;

/// On a new land: every storm deleted, the grid and the ambient weather zeroed, the frame back to 1
void Reset();

/// The cell of (x, z) (recomputed first if stale and `recalc`), then the height: above 50 m the temperature drops by
/// 0.075 per metre, -11 above 200 m
[[nodiscard]] WeatherInfo GetWeather(const glm::vec3& point, bool recalc = true);
/// The four cells around the point, bilinear in 1/256 steps; above 200 m it goes towards the ambient weather by
/// (height - 200) / 4 / 256, then the same height drop
[[nodiscard]] WeatherInfo GetWeatherSmooth(const glm::vec3& point, bool recalc = true);

/// Once a game turn (the visual time of day, 0.1 s): the storms' update, the snow cover (not ported) and the next
/// frame (at 256 every stamp is cleared and the frame is 1). The sun position it also computes (4000,
/// cos(t x pi/12) x 1100 - 150, sin x 800) is for the landscape lighting, not used here.
void UpdateGame(float visualTime, float seconds);

/// The ambient weather: all 0 in a game (only the internet weather and the save file write it)
[[nodiscard]] const WeatherInfo& Ambient();
void SetAmbient(const WeatherInfo& ambient);
/// The current frame stamp
[[nodiscard]] uint8_t Frame();
} // namespace openblack::weather::atmos
