/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <glm/vec3.hpp>

namespace openblack
{

/// Zoomer: moves a value to a destination in a given time with a quartic that starts from the current value and speed
/// and ends with the destination speed and no acceleration (wiki: docs/bw1-notes/engine-math.md, "Zoomer (LH3DLib)").
/// Every operation is a float one in the original's order (24-bit precision).
struct Zoomer
{
	float value = 0.0f; ///< The current value
	float destination = 0.0f;
	float destinationSpeed = 0.0f;
	float speed = 0.0f; ///< The current speed
	// The original's second time field is only ever zeroed: not kept
	float time = 0.0f; ///< The time since the move started
	float duration = 0.0f;
	float startValue = 0.0f;
	float startSpeed = 0.0f;
	float c2 = 0.0f; ///< The coefficient of t^2 / 2
	float c3 = 0.0f; ///< Of t^3 / 6
	float c4 = 0.0f; ///< Of t^4 / 24

	/// Value = destination = start value = p, everything else 0
	void SetPosition(float position);
	/// Below 0.001 s (or NaN) SetPosition(target); otherwise the start is the current value and speed, and
	/// (c4, c3, c2) = (r1, r2, 0) M^-1 with M = rows (C, B, A), (B, A, T), (A, T, 1), A = (T T) 0.5,
	/// B = (A T) 0.33333334, C = (A A) 0.16666667, inverted by affine::Inverse (its |det| >= 1e-10 clamp makes a step
	/// of less than 0.0493 s barely move and then jump: det = -T^6 / 144), r1 = (target - start) - T start speed,
	/// r2 = target speed - start speed
	void SetDestinationWithSpeedAndTime(float target, float targetSpeed, float seconds);
	/// t = dt + time; at t >= duration the destination and its speed;
	/// otherwise speed = ((t c2 + a c3) + b c4) + start speed and value = ((((C c4) + b c3) + a c2) + t start speed) +
	/// start value, a = (t t) 0.5, b = (t a) 0.33333334, C = (a a) 0.16666667
	void Update(float seconds);
	/// (openblack) time < duration
	[[nodiscard]] bool IsMoving() const { return time < duration; }
};

/// Zoomer3: one Zoomer per axis, x, y, z
struct Zoomer3
{
	std::array<Zoomer, 3> axis;

	/// SetPosition on each axis
	void SetPosition(const glm::vec3& position);
	/// SetDestinationWithSpeedAndTime with destination speed 0 on the three axes
	void SetDestinationWithTime(const glm::vec3& target, float seconds);
	/// Update on x, y, z
	void Update(float seconds);
	[[nodiscard]] glm::vec3 GetCurrentValue() const;
	[[nodiscard]] glm::vec3 GetDestination() const;
	[[nodiscard]] glm::vec3 GetSpeed() const;
	[[nodiscard]] glm::vec3 GetStartValue() const;
	[[nodiscard]] glm::vec3 GetStartSpeed() const;
	[[nodiscard]] glm::vec3 GetDestinationSpeed() const;
};

} // namespace openblack
