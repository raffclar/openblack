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

#include "Enums.h"

/// The player of a creature, as the things that it does count it. A creature that a script makes, such as the guide or
/// the Ogre, belongs to no player. The creature component keeps the neutral player for such a creature.
namespace openblack::creature_owner
{
/// The player that gets the credit for a thing that the creature throws, drops, lets go or knocks into the air. A creature
/// that belongs to no player gives the credit to no player, not to the neutral player.
[[nodiscard]] constexpr std::optional<PlayerNames> CreditedPlayer(PlayerNames owner)
{
	if (owner == PlayerNames::NEUTRAL)
	{
		return std::nullopt;
	}
	return owner;
}
} // namespace openblack::creature_owner
