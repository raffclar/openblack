/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <numbers>

#include <glm/vec2.hpp>

/// EXPERIMENT (not for merge): one wind for the clouds, the fog, the waves, the spray, the foam and the plants. The
/// original game has a wind in its weather only. The wind follows the weather at the eye, turns and changes speed
/// slowly, and has gusts that travel downwind.
namespace openblack::wind
{

/// The direction of a calm day's wind, in radians about the up axis from +x towards +z: three eighths of a turn round,
/// along the original game's track of the clouds
inline constexpr float k_CalmAngle = 0.75f * std::numbers::pi_v<float>;
/// The speed of a calm day's wind at the ground, in world units a second (about a metre a second each)
inline constexpr float k_CalmSpeed = 3.0f;
/// The speed that each percent of the weather's wind adds, in world units a second
inline constexpr float k_SpeedPerPercent = 0.2f;
/// The weather's wind turns the wind only when it is stronger than this, in percent
inline constexpr float k_TurnThreshold = 5.0f;
/// The most that the wind turns in a second, in radians
inline constexpr float k_TurnRate = 0.2f;
/// The time constant of a change of speed, in seconds
inline constexpr float k_SpeedSeconds = 6.0f;

/// The wind at a moment
struct Wind
{
	/// The direction it blows towards, in radians about the up axis from +x towards +z
	float angle {k_CalmAngle};
	/// Its mean speed at the ground, in world units a second
	float speed {k_CalmSpeed};
	/// How stormy it is, 0 calm to 1
	float storm {0.0f};
};

/// The wind that the weather asks for: its wind in percent along x and z, and how stormy it is
[[nodiscard]] Wind Target(glm::vec2 weatherWind, float storm);

/// One frame of the given seconds towards the wind that is asked for. The wind turns by at most k_TurnRate a second,
/// the shorter way round, and its speed and storm ease with k_SpeedSeconds.
[[nodiscard]] Wind Step(const Wind& wind, const Wind& target, float seconds);

/// The direction that the wind blows towards, a unit vector across the ground
[[nodiscard]] glm::vec2 Direction(const Wind& wind);

/// The share of the mean speed that blows at a point and time: gust fronts that travel downwind with the wind. About
/// 1 on average, from 0.5 to 1.6; the gusts are stronger in a storm.
[[nodiscard]] float Gust(const Wind& wind, glm::vec2 point, float seconds);

/// The wind's velocity at a point and time, with its gust, in world units a second
[[nodiscard]] glm::vec2 VelocityAt(const Wind& wind, glm::vec2 point, float seconds);

} // namespace openblack::wind
