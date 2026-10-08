/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "Enums.h"

// The interface status (a player's hand) as the worship code sees it. openblack has one hand, the local human
// player's (PlayerMagic.playerType 1); other players have no interface status, so their icons never charge for a hand.

namespace openblack::worship::interface
{
/// The player has an interface status here
[[nodiscard]] bool HasInterface(PlayerNames player);

/// There is space in the hand (nothing held; another flag of the original's check is not known)
[[nodiscard]] bool IsHandReadyForObject(PlayerNames player);

/// The spell seed in the player's hand, or entt::null
[[nodiscard]] entt::entity HeldSpellSeed(PlayerNames player);

/// A spell seed placed in the magic hand: the hand holds it and the seed's InterfaceSetInMagicHand runs. 1, or 3 when
/// the seed went (and nothing is held).
int PlaceSeedInMagicHand(PlayerNames player, entt::entity seed);
} // namespace openblack::worship::interface
