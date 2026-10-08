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

/// The lightning flash at the camera that the landscape light table reads every frame. It tints the
/// whole frame through the table: the land, its reflection, the sea (table[255]), the rings created in that frame and
/// the distance haze. The other input of the table, the overcast at the camera, has a single source:
/// Clouds::WeatherOvercastAtCamera (the overcast byte of weather::atmos::GetWeatherSmooth(camera) x 0.01).
/// The flash itself is the weather's: weather::LightningFlashAtCamera (ECS/Weather/LightningFlash, the storms' flash
/// objects, updated every frame by weather::UpdateFrame).
namespace openblack::sky_weather
{

/// 0, or trunc(clamp(flash, 0, 1) * 255) when the camera is inside the nearest live storm
/// (dXZ^2 < ((inner + outer) / 2)^2). flash = f1 * intensity (f1: s = 1 - age, 0.1 between 0.2 and 0.5 s, gone at
/// 0.8 s; intensity 0.5 fork / 1.0 sheet lightning).
/// weather::LightningFlashAtCamera at the camera's position (0 without a camera).
[[nodiscard]] uint8_t LightningFlash();

} // namespace openblack::sky_weather
