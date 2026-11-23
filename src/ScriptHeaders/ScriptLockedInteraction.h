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

#include <entt/entity/entity.hpp>

#include "Enums.h"

/// Whether a thing is locked in an interaction with a hand, as the scripts ask it
namespace openblack::script::locked_interaction
{

/// The locks on the thing that do not depend on the player of the hand
struct Locks
{
	/// A hand holds the thing, a creature, with the button
	bool heldCreature {false};
	/// The hand grips the thing, a totem of a town
	bool grippedTotem {false};
	/// A hand holds the thing, a totem of a puzzle
	bool heldPuzzleTotem {false};
};

/// Whether the thing is locked: one of the locks holds, or the hand of a player scoops from it. The game marks the
/// thing when the lock of a hand starts on every computer, so the hand of each player counts. `lockedOnOf` gives what
/// the hand of a player scoops from, none when it scoops from nothing.
template <typename LockedOnOf>
[[nodiscard]] bool IsLocked(const Locks& locks, entt::entity thing, const LockedOnOf& lockedOnOf)
{
	if (locks.heldCreature || locks.grippedTotem || locks.heldPuzzleTotem)
	{
		return true;
	}
	for (auto player = static_cast<uint8_t>(PlayerNames::PLAYER_ONE); player < static_cast<uint8_t>(PlayerNames::NEUTRAL);
	     ++player)
	{
		if (lockedOnOf(static_cast<PlayerNames>(player)) == std::optional(thing))
		{
			return true;
		}
	}
	return false;
}

} // namespace openblack::script::locked_interaction
