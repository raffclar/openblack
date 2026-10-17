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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// How a creature points. It blends four animations: low to the left, low to the right, high to the left and high to
/// the right. How far to the side and how high the target is give the weights. The arm rises to the species' moment of
/// the point and holds there. When the point ends, the arm plays to the end of the animations.
namespace openblack::creature_pointing
{
/// The animations, low left, low right, high left and high right
inline constexpr std::array<size_t, 4> k_Animations {208, 209, 210, 211};
/// The creature points from this height for each unit of its size
inline constexpr float k_OriginHeightPerSize = 15.0f;
/// Across, the aim stops at this angle each way (radians)
inline constexpr float k_AcrossLimit = 0.5235987901687622f;
/// The aim goes 0 to 1 across this many radians, the two limits apart
inline constexpr float k_AcrossScale = 0.9549296498298645f;
/// Up and down, the aim stops at this sine of the angle each way
inline constexpr float k_RiseLimit = 0.5f;
/// A target nearer than this gives a level aim straight ahead
inline constexpr float k_TooNear = 0.1f;

/// The game's angle of a direction on the land: 0 along -z, a quarter turn along +x
[[nodiscard]] float AngleOf(glm::vec2 direction);
/// An angle brought into a half turn each way with one full turn, as the game does
[[nodiscard]] float Wrap(float angle);

/// Where the target is from the creature: the angle across from ahead, and the sine of the angle up
struct Aim
{
	float across {0.0f};
	float rise {0.0f};
};
/// The aim at a target from a point, for a creature that faces a way on the land. Each part stops at its limit.
[[nodiscard]] Aim AimAt(glm::vec3 origin, glm::vec2 facing, glm::vec3 target);

/// The creature turns to face the target before it points when the target is this far round from its head (radians)
inline constexpr float k_TurnLimit = 0.4712389409542084f;
/// How far round the target is from the way the creature faces, as seen from its head
[[nodiscard]] float TurnAngle(glm::vec3 head, glm::vec2 facing, glm::vec3 target);

/// Whether the creature points with its left arm, chosen as it starts to point. A creature that holds something
/// points with the free hand. A creature that did not turn first uses its left arm when the turn angle is more than 0.
/// After a turn, it uses its left arm at a target on the positive side of the aim.
[[nodiscard]] bool PointsWithLeft(float turnAngle, std::optional<bool> heldInLeft);
[[nodiscard]] bool PointsWithLeftAfterTurn(const Aim& aim, std::optional<bool> heldInLeft);

/// The weights of the four animations, low left, low right, high left and high right. The left arm turns the aim
/// over before the weights.
[[nodiscard]] std::array<float, 4> Weights(const Aim& aim, bool left);

/// The time in the animations after a step. While the creature points, the time moves on until it gets to the
/// moment of the point, and stays there. When the point ends, the time moves on freely.
[[nodiscard]] float AdvanceTime(float timeMs, float stepMs, float pointMs, bool ending);
} // namespace openblack::creature_pointing
