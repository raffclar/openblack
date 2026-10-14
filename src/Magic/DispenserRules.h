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

#include <chrono>
#include <span>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The miracle dispensers' timing. A dispenser makes a one-shot miracle, a bubble with the miracle's seed inside,
// floating above itself. While the bubble is there, the dispenser waits. When the bubble goes, the dispenser counts
// the turns. After its period, it makes another bubble. It counts only while it is fully built and at its full life.
// Pure functions of a dispenser's timer.

namespace openblack::magic
{

/// A dispenser's bubble floats above it at this share of its height
inline constexpr float k_OrbHeightShare = 1.2f;
/// A bubble stays on its dispenser while the gap between their ground circles is this size or less, in metres
inline constexpr float k_OrbTouchGap = 0.001f;
/// The seed inside a bubble is drawn at this share of its size, and spins this many radians a second
inline constexpr float k_OrbSeedScale = 0.6f;
inline constexpr float k_OrbSeedSpin = 2.0f;
/// A bubble is added over what is behind it at this share of its colour
inline constexpr float k_OrbShare = 149.0f / 255.0f;

/// Every miracle a dispenser can give the player, in the tables' order. The creature spells that do nothing in the
/// game are left out.
[[nodiscard]] std::span<const MagicType> DispensableMiracles();

struct DispenserTimer
{
	/// Turns counted since the last bubble was taken
	uint32_t tick {0};
	/// Turns between a bubble being taken and the next, 0 for a dispenser that makes none
	uint32_t period {0};
	bool active {false};
};

/// What a dispenser does this turn
enum class DispenserStep : uint8_t
{
	/// Nothing: its bubble is there, it is not active, or its period has not passed
	Wait,
	/// Its bubble has been taken: it starts counting
	OrbTaken,
	/// It makes a new bubble. When it makes one, its count starts again.
	MakeOrb,
};

/// What a dispenser knows at the start of its turn
struct DispenserTurn
{
	/// It has a bubble of its own
	bool hasOrb {false};
	/// Its bubble is still in the world and still touches it
	bool orbStillThere {false};
	/// It has a miracle to give
	bool hasMagic {false};
	/// It is fully built and has all its life. A damaged dispenser makes nothing until its town mends it.
	bool whole {true};
};

/// One turn of a dispenser
[[nodiscard]] DispenserStep StepDispenser(DispenserTimer& timer, const DispenserTurn& turn);

/// A period in seconds as turns; none for none
[[nodiscard]] uint32_t PeriodTurns(float seconds, std::chrono::milliseconds turn);

/// The turn that points a model's up along a direction, its other axes kept level where they can be
[[nodiscard]] glm::mat3 FaceTowards(const glm::vec3& direction);

/// Where a dispenser floats its bubble: above it by its height times the share
[[nodiscard]] glm::vec3 OrbPosition(const glm::vec3& base, float height);
/// Whether two things touch: the distance between their positions on the ground, less the radii of their ground circles,
/// is the touch gap or less
[[nodiscard]] bool Touching(glm::vec2 first, float firstRadius, glm::vec2 second, float secondRadius);

} // namespace openblack::magic
