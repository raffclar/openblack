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

#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// How a creature draws a gesture in the air. It points with its arm, and the point moves along the gesture's shape on
/// a flat upright square in front of its head. A glowing chain follows its hand. A gesture as long as the circle takes
/// 30 turns. At the end of the shape, the arm holds still for eight more turns, then the point ends.
namespace openblack::creature_gesture
{
/// The highest gesture number that a creature can draw
inline constexpr uint32_t k_LastGesture = 23;
/// The gesture that a creature draws in k_ReferenceTurns turns: the circle
inline constexpr uint32_t k_ReferenceGesture = 4;
inline constexpr float k_ReferenceTurns = 30.0f;
/// The turns that the counter must pass at the end of the shape before the drawing is done, and at the start
inline constexpr uint32_t k_HoldTurns = 8;
/// The square is this many times the creature's radius in front of its head
inline constexpr float k_AheadRadii = 2.0f;
/// The side of the square is this many times the creature's radius
inline constexpr float k_SizePerRadius = 1.3f;

/// The point of a shape a share of the way along it, from 0 to 1. The shape is its points evenly along its length; the
/// point is on the straight line between the two points either side.
[[nodiscard]] glm::vec3 SamplePath(std::span<const glm::vec3> points, float share);

/// Where the creature points for a point of the shape. The shape's x goes to the creature's left side from a point
/// ahead of its head, and the shape's z goes up from the height of its head.
[[nodiscard]] glm::vec3 HandTarget(glm::vec3 head, glm::vec2 facing, float radius, glm::vec3 shapePoint);

/// The share of the shape that the creature draws in one turn, by the length of the shape and of the circle
[[nodiscard]] float SharePerTurn(float length, float referenceLength);

/// A drawing in progress: the share of the shape drawn, and the turns since the counter last went back to 0
struct Drawing
{
	float drawn {0.0f};
	uint32_t turns {0};
};

enum class Outcome : uint8_t
{
	/// It waits for its arm to point, before it starts the shape
	Waiting,
	/// It moves the point along the shape, or it holds still at its end
	Drawing,
	/// The shape is complete: the point ends
	Done,
	/// The creature cannot draw the gesture
	Failed,
};

/// One turn of a drawing. The counter goes up by one each turn. The drawing waits at its start until the arm points and
/// the counter is at k_HoldTurns or more. Each turn adds a share of the shape. The counter goes back to 0 until the
/// shape is complete. When the counter is more than k_HoldTurns, the drawing is done.
[[nodiscard]] Outcome Advance(Drawing& drawing, uint32_t gesture, bool armPoints, float sharePerTurn);
} // namespace openblack::creature_gesture
