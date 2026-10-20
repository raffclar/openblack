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

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec3.hpp>

/// The camera shakes as a blast lands near it. Each shake has a place, a radius, a strength and a time in whole
/// milliseconds. Only the nearest shake counts, and only when the camera is in its radius. Its strength decreases
/// steadily to zero over its time. Each frame, the eye and the focus move by a new random distance up to that strength.
/// A shake moves them up or down only, or along all three axes when it is not vertical only.
namespace openblack::camera_shake
{

struct Shake
{
	glm::vec3 position {0.0f};
	float radius {0.0f};
	float strength {1.0f};
	int32_t milliseconds {0};
	int32_t millisecondsLeft {0};
	/// It jitters the camera up and down only, not along every axis
	bool verticalOnly {true};

	/// Gives the values that a saved game keeps to a visitor
	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self.position, self.radius, self.strength, self.milliseconds, self.millisecondsLeft, self.verticalOnly);
	}
};

/// The milliseconds of a script's shake: the seconds times 1000, rounded to the nearest whole number (a half goes to
/// the even number)
[[nodiscard]] int32_t ScriptMilliseconds(float seconds);
/// The milliseconds of a particle effect's shake: the seconds times 1000, with the fraction removed
[[nodiscard]] int32_t EffectMilliseconds(float seconds);
/// The shakes count down by the whole milliseconds of the real frame. A shake with no time left stops
void Advance(std::vector<Shake>& shakes, int32_t frameMilliseconds);
/// How hard the camera shakes where it is: by the nearest shake, when within its radius
[[nodiscard]] float Amplitude(std::span<const Shake> shakes, const glm::vec3& camera);
/// The nearest shake to the camera, which is the one that shakes it; none without shakes
[[nodiscard]] const Shake* Nearest(std::span<const Shake> shakes, const glm::vec3& camera);

/// How far the eye and what it looks at jump this frame
struct Offsets
{
	glm::vec3 eye {0.0f};
	glm::vec3 focus {0.0f};
};
/// random(a, b) draws a number between them; a shake along every axis draws z, then y, then x for the eye and the same
/// for what it looks at
[[nodiscard]] Offsets Jitter(float amplitude, bool verticalOnly, const std::function<float(float, float)>& random);

} // namespace openblack::camera_shake
