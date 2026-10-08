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

namespace openblack::ecs::components
{

/// A game thing the scripts know about: the original's script slot (511 of them, taken when a script adds the thing)
/// and the script bits of the thing's flag word. The logic is in ECS/ScriptHeld.h.
struct ScriptHeld
{
	/// How many script variables hold it (the count stops at 255)
	uint8_t references {0};
	/// The slot was taken by a CREATE-type command; the finders (CALL, GET_...) take it as not created
	bool createdByScript {false};
	/// A script holds it (in script)
	bool inScript {false};
	/// Set with the first reference of a thing the script created (controlled by script). Not prey unless the hunter is
	/// in a script, never reacts, its corpse never times out, a flock with it is a script flock.
	bool controlledByScript {false};
	/// false: only the flag bits, no script slot (controlled by script on a thing no script took): Process leaves it
	/// alone
	bool hasSlot {true};
};

/// Cannot be eaten and survives an attack. Set when a thing comes out of the land-to-land vortex, on the puzzle
/// objects and by a save that had it. Read by the prey test (not prey) and by being eaten (LANDED instead of dead).
struct CannotBeEaten
{
};

} // namespace openblack::ecs::components
