/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// Which things the scripts see as objects of the world. A miracle that a script casts on an object hits it, and a script's
/// delete with fade or with explode removes only an object.
namespace openblack::ecs::script_object
{

/// The parts of a thing that tell if it is an object
struct Parts
{
	/// It has a model in the world
	bool model {false};
	/// It is a hand
	bool hand {false};
	bool creature {false};
	bool field {false};
	/// It is a one-shot globe. The globe has no model part: its dome turns to face the camera
	bool globe {false};
	/// It is a miracle seed, in a hand or let go
	bool seed {false};
};

/// Whether a thing is an object: anything with a model but the hand, a creature, a field, a one-shot globe or a miracle
/// seed. A town, a flock or a running miracle has only a place and is not an object.
[[nodiscard]] constexpr bool IsObject(const Parts& parts)
{
	return (parts.model && !parts.hand) || parts.creature || parts.field || parts.globe || parts.seed;
}

} // namespace openblack::ecs::script_object
