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

#include <optional>

#include <glm/vec3.hpp>

/// How a creature's body sways: things thrown at it and the leash dragging it kick its lower or upper body, which
/// swings back on a damped spring and is drawn leaning by four lean animations played on top of its pose
namespace openblack::creature_sway
{
/// The lean animations in the creature spec file: the upper and lower body leaning sideways and front to back
inline constexpr uint32_t k_UpperSide = 194;
inline constexpr uint32_t k_UpperFrontBack = 195;
inline constexpr uint32_t k_LowerSide = 196;
inline constexpr uint32_t k_LowerFrontBack = 197;

/// A kick is taken by the upper body when it lands higher above the land than this many times the creature's size
inline constexpr float k_UpperHeightPerSize = 9.0f;
/// No kick leaves the body swinging faster than this
inline constexpr float k_MaxKickSpeed = 8.0f;
/// Within the spring the body never swings faster than this
inline constexpr float k_MaxSwingSpeed = 6.0f;
/// The spring's stiffness and damping, each for half the creature's mass
inline constexpr float k_Stiffness = -30.0f;
inline constexpr float k_Damping = 5.0f;
/// Each game turn is stepped in this many parts
inline constexpr int k_Substeps = 10;
/// A game turn lasts this many milliseconds at the usual pace
inline constexpr int32_t k_TurnMilliseconds = 100;
/// The sway settles, and stops being stepped and drawn, once every offset and speed is this small
inline constexpr float k_SettledLength = 0.05f;
/// A lean this small isn't drawn
inline constexpr float k_DrawnLean = 0.0001f;

/// The body's two swinging parts and what drives them
struct Sway
{
	glm::vec3 lowerVelocity {0.0f};
	glm::vec3 upperVelocity {0.0f};
	glm::vec3 lowerOffset {0.0f};
	glm::vec3 upperOffset {0.0f};
	/// Something kicked it and it has not settled yet
	bool active {false};
	/// The seconds that the creature lived in its last game turn, which turn a kick's force into speed
	float turnSeconds {0.0f};
	/// While the creature turns on the spot, the heading that the turn has reached. The sway leans along this heading.
	std::optional<float> turnHeading;
};

/// The seconds that a creature lives in a game turn at a pace. A frozen creature has a pace below 1. The
/// milliseconds are whole, cut towards 0.
[[nodiscard]] float TurnSeconds(float pace);

/// What the body does, as far as a kick is concerned
struct BodyFacts
{
	/// It faints, lies or gets up in a fight
	bool downInFight {false};
	/// It plays a start, loop and end, as it sits, sleeps, faints or lies dead
	bool playsStartLoopEnd {false};
	/// It turns on the spot, to face a point or before it walks
	bool turnsOnTheSpot {false};
};
/// Whether a kick moves the body. A body that plays a start, loop and end, turns on the spot or is down in a fight
/// does not sway.
[[nodiscard]] constexpr bool TakesKick(const BodyFacts& facts)
{
	return !facts.downInFight && !facts.playsStartLoopEnd && !facts.turnsOnTheSpot;
}

/// The heading that the body faces part of the way through a turn on the spot: the start heading, plus the turn
/// to the target heading times the time over the duration of the turn
[[nodiscard]] float TurnHeading(float heading, float targetHeading, float timeMs, float durationMs);

/// A force kicks the body at a point: the upper body when the point is higher above the land than its split height,
/// else the lower. The kick is only across the land, by the force over the last game turn for half the creature's
/// mass, no faster than the limit of the kick.
void Kick(Sway& sway, glm::vec3 force, float heightAboveLand, float splitHeight, float mass);

/// One game turn of the spring: the turn has a number of steps for each part. Each step keeps the speed of the part
/// within the limit. The spring stops when the two parts settle.
void Step(Sway& sway, float seconds, float mass);

/// How far through a lean animation to draw a lean along one of the creature's axes, by its duration in milliseconds
[[nodiscard]] uint32_t LeanTime(float lean, uint32_t duration);
} // namespace openblack::creature_sway
