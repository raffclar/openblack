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

/// The values that a creature's body gets when the game makes it and keeps for its life. The mind file keeps them with
/// the body, and a body that loads a mind file takes them from the file.
namespace openblack::creature_born
{

struct BornWith
{
	/// The direction of the creature when the game made it. This is always 0 for a new creature, and the game does not
	/// read it.
	uint16_t facing {0};
	/// The slow, walk and run speeds of its species when the game made it. Its slow speed has no other use. A script
	/// can get the walk speed and the run speed of a creature.
	float slowSpeed {0.0f};
	float walkSpeed {0.0f};
	float runSpeed {0.0f};

	bool operator==(const BornWith&) const = default;
};

/// The values of a new creature of a species with these speeds
[[nodiscard]] constexpr BornWith FromSpecies(float slowSpeed, float walkSpeed, float runSpeed)
{
	return {.facing = 0, .slowSpeed = slowSpeed, .walkSpeed = walkSpeed, .runSpeed = runSpeed};
}

} // namespace openblack::creature_born
