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

/// The rule that turns a creature's head. Each frame the head turns towards a target or back to straight ahead. What the
/// body does decides if the head can turn at all: many actions keep the head straight.
namespace openblack::creature_head_look
{
/// What the body does, as far as the head is concerned
enum class Pose : uint8_t
{
	/// It stands and breathes
	Standing,
	/// It walks, waits for its route, fidgets while it waits, or steps to start a walk
	Moving,
	/// It turns on the spot before it starts to walk
	TurningToWalk,
	/// It turns on the spot to face a point, and goes nowhere
	TurningToFace,
	/// It starts the sit, sits, or ends the sit
	SittingDown,
	Sitting,
	GettingUp,
	/// It points at something
	Pointing,
	/// It waits for a thing that flies at it, or steps across to catch it
	WaitingToCatch,
	/// It stands in its fight stance
	FightStance,
	/// Every other action: emotes, statics, dances, fight moves, sleep, faints and falls
	Other,
};

/// The clip that sets when a point allows the head to turn
constexpr uint32_t k_PointTimingClip = 210;

/// Whether the body lets the head turn to its target. A creature that points lets it turn only before the point
/// reaches a fifth of the clip, and after seven tenths. The point clip's length and time are in whole milliseconds.
[[nodiscard]] bool AllowsLook(Pose pose, int32_t pointTimeMs = 0, int32_t pointDurationMs = 0);

/// What the head turns towards this frame
enum class Aim : uint8_t
{
	/// Straight ahead
	Ahead,
	/// The point that the creature looks at
	LookPoint,
	/// The head of the opponent in a fight
	OpponentHead,
	/// The thing that the creature waits to catch
	Thing,
};

struct AimFacts
{
	Pose pose {Pose::Standing};
	/// The creature has a point to look at
	bool hasLookPoint {false};
	/// A fight opponent, and a thing that flies at it to catch
	bool hasOpponent {false};
	bool hasThing {false};
	int32_t pointTimeMs {0};
	int32_t pointDurationMs {0};
};

/// The head's target. A thing to catch becomes the look point. In the fight stance the head always looks at the
/// opponent's head. Otherwise the head looks at the look point only when the body allows it.
[[nodiscard]] Aim AimOf(const AimFacts& facts);

/// How far to turn the head, in radians: yaw to the left of ahead, pitch up
struct Angles
{
	float yaw {0.0f};
	float pitch {0.0f};
};
/// The angles that turn the head from the eyes towards a target, given the heading that the body faces. The head turns
/// sideways only when the direction to the target is not on a map axis.
[[nodiscard]] Angles AnglesTowards(const glm::vec3& eyes, float facing, const glm::vec3& target);

/// How fast the head speeds up as it turns, in radians a second each second: the species' rate, slower for bigger
/// creatures as their animations are
[[nodiscard]] float Acceleration(float size, float speciesRate);

/// The head turns with the sitting head clips during the start, the loop and the end of the sit
[[nodiscard]] bool UsesSittingClips(Pose pose);
/// The head clips play when the head is turned, or when the creature has a point to look at
[[nodiscard]] bool PlaysHeadClip(float angle, bool hasLookPoint);
} // namespace openblack::creature_head_look
