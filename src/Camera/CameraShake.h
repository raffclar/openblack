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

#include <vector>

#include <glm/vec3.hpp>

/// The camera shakes: a list of shake checkers, newest first.
///
/// - SHAKE_CAMERA -> StartCameraShake -> Create.
/// - Once a drawn frame (outside the citadel and not in a play back) the drawn position and focus go through Adjust
///   before they become the drawn camera: the shake moves the drawn camera only, never the camera's zoomers, so it
///   does not build up.
/// - At the start of each frame Tick counts the shakes down by the frame time and frees the spent ones.
/// - Other creators, not ported: a camera mode's force field (100, 1.0, 400 ms) and two sound/effect callers.
namespace openblack::camera_shake
{

/// Seconds to ms
constexpr float k_MsPerSecond = 1000.0f;

/// One camera shake
struct Checker
{
	float maxDistance = 0.0f; ///< The camera shakes only within it of `point`
	glm::vec3 point {0.0f};
	float amplitude = 0.0f;
	int32_t totalMs = 0;
	int32_t remainingMs = 0;
	bool yOnly = false;
};

/// A new shake at the head of the list: max distance, point, amplitude, total = remaining = ms, y only
void Create(float maxDistance, const glm::vec3& point, float amplitude, int32_t ms, bool yOnly);

/// Create(radius, point, amplitude, seconds x 1000 rounded, not y only). (inferred) rounded to nearest, the default
/// FPU rounding
void StartCameraShake(const glm::vec3& point, float radius, float amplitude, float seconds);

/// Nothing without shakes or with shakes switched off (never switched off: always on here, inferred). The shake
/// nearest to `lastDrawn` (the camera drawn the frame before; the first of equal ones, i.e. the newest) moves the
/// camera when that distance is below its max distance: a = remaining / total x amplitude, then
/// game_random::crt::Random(-a, a) on y of both with "y only" (position first), else on the position's z, y, x and
/// the target's z, y, x in that order. No fall-off with the distance
void Adjust(const glm::vec3& lastDrawn, glm::vec3& position, glm::vec3& target);

/// At the start of a frame, with shakes on: each shake's remaining ms -= the frame's wall ms; <= 0 frees it
void Tick(uint32_t frameMs);

/// Not original: no shakes (a new game, the tests); the original's list only empties by Tick
void Reset();

/// The list, newest first (tests)
[[nodiscard]] const std::vector<Checker>& Checkers();

} // namespace openblack::camera_shake
