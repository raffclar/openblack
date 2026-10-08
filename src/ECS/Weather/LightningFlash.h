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

#include "Storms.h"

// The lightning's light flash of the registered storms and what the camera sees of it. Wiki:
// docs/bw1-notes/miracles.md, "The flash and the clouds of the registered storms".

namespace openblack::weather
{
namespace flash
{
/// (pos, radius, intensity): the intensity, age f1 f3 0, active, the point and the radius
void Start(storms::Storm::Flash& flash, const glm::vec3& position, float radius, float intensity);
/// Every weather update: age += seconds; an active flash past 0.8 s goes off
void Age(storms::Storm::Flash& flash, float seconds);
/// Every frame for each storm not marked for deletion: active -> s = 1 - age, f1 = s, f3 = s^3, both 0.1 while
/// 0.2 < age < 0.5, then x intensity; off -> 0, 0. Returns f3, the strength of the land light stamp (a 64 x 64 radial
/// bitmap, mode 1), when active.
float Frame(storms::Storm::Flash& flash);

/// The flash's bitmap (64 x 64, built once for all flashes): at d = |(x, y)|, x, y in -32..31, (int)((32 - d) x 9)
/// capped at 255 inside d < 32, 0 outside; the three channels equal
[[nodiscard]] uint8_t BitmapTexel(int x, int y);

/// The storm nearest the camera in x, z (descriptor position, squared distance, the first of equals; only those not
/// marked for deletion), "inside" when its d^2 < ((inner + outer) / 2)^2 of the descriptor's radii — a flag that stays
/// set when a later, nearer storm is outside (the original never clears it) — and then the result = inside ?
/// (int)(clamp(nearest flash f1, 0, 1) x 255) : 0
[[nodiscard]] uint8_t AtCamera(const glm::vec3& camera);
/// Every frame (weather::UpdateFrame, the first part of the atmosphere's 3D update): Frame of every storm not marked
void UpdateFrame();
} // namespace flash

/// The last frame's flash of the nearest storm at the camera, 0..255 (for the sky and the water: 3D/SkyWeather and
/// LandLightTable::Build read it)
[[nodiscard]] uint8_t LightningFlashAtCamera(const glm::vec3& camera);
} // namespace openblack::weather
