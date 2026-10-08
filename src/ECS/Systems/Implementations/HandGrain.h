/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>

#include <glm/vec3.hpp>

// The hand's grain state (hand state 8: the hand holds a spell seed). It raises and tilts the hand while a sprinkle
// miracle (food, wood, water) pours from it: the sprinkle starts it, the hand's game turn steps it, the holding state
// reads it. Wiki: docs/bw1-notes/miracles.md, "Food and wood".

namespace openblack::ecs::systems::hand_grain
{

/// The key points (0, 0), (0.2, 1), (0.8, 1), (1, 0) and the second derivatives that the Numerical Recipes spline
/// gives them as a natural spline: 1e30 is passed for both end slopes, and the raise peaks at 1.61 x HeightToRaise in
/// the middle
struct Spline
{
	std::array<float, 4> x {0.0f, 0.2f, 0.8f, 1.0f};
	std::array<float, 4> y {0.0f, 1.0f, 1.0f, 0.0f};
	std::array<float, 4> y2 {};
};
/// The second derivatives (an end slope above 0.99e30: natural end)
[[nodiscard]] Spline BuildSpline(float yp1, float ypn);
/// The evaluation (bisection, then the cubic; t itself when two key points share x)
[[nodiscard]] float Evaluate(const Spline& spline, float t);

/// The raise starts at t = 0 from where the hand is now (kept as the start position)
void Start(bool clampHand, float totalTime, float heightToRaise, float angleToRaise, bool loop);
/// Off, no clamp, height and tilt 0
void Stop();

/// First thing in the hand's game turn (dt = turn ms x 0.001): the last values are kept for the interpolation,
/// t += dt / TotalTime (past 1: back to 0 when looping, else Stop), height = v x HeightToRaise, tilt = v x
/// AngleToRaise
void GameTurnUpdate(float dt);

/// The height and the tilt between the last two turns (by the fraction of the turn drawn)
[[nodiscard]] float Height();
[[nodiscard]] float Tilt();
/// With ClampHand the hand's required position is the one it had when it started
[[nodiscard]] std::optional<glm::vec3> ClampedPosition();
[[nodiscard]] bool Active();
/// The raw state after the last turn (for the traces and the unit test): t, height, tilt
[[nodiscard]] glm::vec3 Debug();

/// The hand enters (everything reset) or leaves (off) the seed-holding state
void SetHoldingSeed(bool holding);

/// A land is loaded
void Reset();

} // namespace openblack::ecs::systems::hand_grain
