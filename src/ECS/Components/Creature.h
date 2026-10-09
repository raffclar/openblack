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
#include <string>

#include <entt/fwd.hpp>

#include "Creature.h"
#include "Enums.h"

namespace openblack::ecs::components
{

struct Creature
{
	/// The player the creature belongs to
	PlayerNames owner;
	/// Whether it is the one creature its owner can put a leash on. Each player has at most one; the leash system keeps
	/// it so, making a player's first creature theirs to lead.
	bool leashable {false};
	CreatureType species;
	entt::id_type mind;
	/// What the creature has become, which its body shows: alignment from -1 (evil) to 1 (good), fatness and strength
	/// from 0 to 1
	float alignment {0.0f};
	/// A change to its alignment still to come, as what its miracles did to the world moves it: it scales the change of a
	/// turn and is then gone
	float pendingAlignment {0.0f};
	float fatness {0.5f};
	float strength {0.5f};
	/// How big the creature is: 1 is about 15 units tall, whatever its species' mesh
	float size {1.0f};
	/// The smaller size its body is shown at while it is in its temple's pen, its own size kept; none elsewhere
	std::optional<float> penSize;
	/// How many objects its miracles have destroyed
	float objectsDestroyed {0.0f};
	/// Whether a miracle that takes the last of its life knocks it out; a script can make it unable to die, when such a
	/// miracle gives it back all its life instead
	bool canDie {true};
};

/// The size its body is shown at: its own size, or the smaller one while it is in its temple's pen. Whatever goes by
/// how big the creature looks and moves (its height, weight, reach, pace, animation speed, how much magic it can hold)
/// takes this one; only its growth, eating, effort and what is saved of it keep to its own size.
[[nodiscard]] inline float ShownSize(const Creature& creature)
{
	return creature.penSize.value_or(creature.size);
}
} // namespace openblack::ecs::components
