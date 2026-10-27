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

#include "Creature/LeashRope.h"

namespace openblack::ecs::components
{

/// The rope of the leash that a creature wears, as the frames of this computer draw it. The frames move the rope; the
/// world never reads it, and the world save keeps none of it. The lengths of the leash are part of the world (see
/// CreatureLeash::Worn).
struct CreatureLeashRope
{
	/// The rope from the hand, or what it is tied to, to the creature's collar
	leash_rope::Rope rope;
	/// Whether the rope has been placed from its ends yet
	bool started {false};
	/// While the hand holds the rope of the tied leash, the rope is drawn as two of its own: from the collar to the hand,
	/// and from what it is tied to to the hand
	struct HeldRopes
	{
		leash_rope::Rope toCollar;
		leash_rope::Rope toTied;
	};
	std::optional<HeldRopes> held;
};

} // namespace openblack::ecs::components
