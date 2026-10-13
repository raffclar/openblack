/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/LeashRope.h"
#include "Creature/LeashRules.h"

/// Snapping a tied leash: with the leash tied to something, the hand can take hold of its rope with the Action button.
/// While held, the rope is drawn as two, one from the creature's collar to the hand and one from what it is tied to to
/// the hand, and the hand pinches it. Let go, the leash's length becomes how far the hand is from what it is tied to, and
/// the creature is sent to the place on the land under the hand, as a tap on the land sends it.
namespace openblack::leash_snap
{

/// A point of the rope as seen on the screen: its pixel and its depth along the view, or none when it is behind the eye
struct ScreenPoint
{
	glm::vec2 pixel {0.0f};
	float depth {0.0f};
	bool inFront {false};
};

/// How wide the rope is to the hand's touch: each end of each segment reaches this many times the screen's width times
/// the rope's half width over its depth, in whole pixels
constexpr float k_TouchWidthScale = 2.0f;
/// Never less than this many pixels
constexpr int k_MinTouchPixels = 2;
/// Points nearer the eye than this are never touched
constexpr float k_MinDepth = 1.0f;
/// A segment shorter on the screen than this (the product of its length and the cursor's distance from its start, in
/// pixels) can only be touched at its ends
constexpr float k_MinSegmentSpan = 0.1f;

/// How far round each end of a segment the cursor touches it, in pixels
[[nodiscard]] int TouchPixels(float screenWidth, float halfWidth, float depth);

/// Whether the cursor touches the segment from a to b, reaching ra pixels round a and rb round b, and how far along it
/// as the game measures it: 0 within the disc round a, 1 within the disc round b, else how much the cursor's direction
/// from a lies along the segment. Between the ends the reach goes from ra to rb by that same share.
[[nodiscard]] std::optional<float> TouchAlong(glm::ivec2 cursor, glm::ivec2 a, int ra, glm::ivec2 b, int rb);

/// Where the cursor first touches the rope from its start, as a depth along the view, or none. points runs from the
/// rope's start to its end.
[[nodiscard]] std::optional<float> TouchDepth(std::span<const ScreenPoint> points, glm::ivec2 cursor, float screenWidth,
                                              float halfWidth);

/// What else the hand is over, which the rope must be in front of to be the thing under the hand
struct Under
{
	/// The thing picked under the cursor, and its depth; a creature always comes before the rope
	bool object {false};
	bool creature {false};
	float objectDepth {0.0f};
	/// The land under the cursor and its depth, none over the sky
	std::optional<float> landDepth;
};
/// Whether the rope touched at a depth is what the hand is over: a creature under the cursor always wins, another thing
/// only when it is behind the rope, and the land when it is behind the rope
[[nodiscard]] bool RopeIsUnder(float ropeDepth, const Under& under);

/// The leash's lengths once the hand lets go of its rope: as long as the hand is from what it is tied to, and never less
/// than twice its radius on the ground; half of that at rest
[[nodiscard]] creature_leash::Lengths LengthsOnLettingGo(const glm::vec3& hand, const glm::vec3& tied, float tiedRadius);

/// The rope from the collar to the held hand: the leash's own look, half as long again as the leash's rope
[[nodiscard]] creature_leash::Lengths CollarRopeLengths(const creature_leash::Lengths& leash);
/// The rope from what the leash is tied to to the held hand: always the same short lengths
constexpr creature_leash::Lengths k_TiedRopeLengths {.slack = 5.0f, .max = 2.0f};

/// What the hand does about the rope of a tied leash
enum class Hand : uint8_t
{
	/// Nothing
	Away,
	/// The hand is over the rope, its fingers open to pinch it
	Over,
	/// The hand holds the rope, pinched between its fingers
	Holding,
};
/// The hand's fingers-pinching pose is held still, as a share of its animation: open at its start over the rope, pinched
/// halfway through while holding it
[[nodiscard]] float PinchShare(Hand hand);

} // namespace openblack::leash_snap
