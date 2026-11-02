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

/// The rules of the things that the command of a player can act on. The hand of a real player acts only on these
/// things, so the turn does no command of a changed client that acts on another thing.
namespace openblack::player_commands::owner_rules
{

/// Whether the hand of a player may take hold of the totem of a town, or set the share of worship of the town. Only the
/// totem of a town of the player takes the hand. A totem with no town has no player (none).
[[nodiscard]] constexpr bool MayUseTotem(std::optional<PlayerNames> townOwner, PlayerNames player) noexcept
{
	return player != PlayerNames::NEUTRAL && townOwner.has_value() && *townOwner == player;
}

/// Whether the hand of a player may call off its gift to a creature. The hand gives things only to the creature of its
/// own player, so it calls off a gift only to that creature.
[[nodiscard]] constexpr bool MayCallOffGift(PlayerNames creatureOwner, PlayerNames player) noexcept
{
	return player != PlayerNames::NEUTRAL && creatureOwner == player;
}

} // namespace openblack::player_commands::owner_rules
