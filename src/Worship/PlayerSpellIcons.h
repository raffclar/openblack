/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

// The player's worship icons as a whole: what the gesture selection (the power-up system, the selection stages, the
// power-up gestures) and the request packets (request, repeat, power-up charge, cancel) ask. `player` stands for the
// interface status: openblack's hand is the local player's.

namespace openblack::worship::player
{
/// From the spell processing, before the maintain requests: every player's citadel (citadel::ProcessSpellIcons)
void ProcessSpellIcons();

/// The icons of the player's six worship sites, in slot order
[[nodiscard]] std::vector<entt::entity> Icons(PlayerNames player);

/// An icon is charging for the player's hand (the scribble cancel, casting.md §4)
[[nodiscard]] bool AnyIconChargingFor(PlayerNames player);
/// The largest charge fraction of the icons charging for the player
[[nodiscard]] float MaxChargeFraction(PlayerNames player);
/// Some icon is ValidForRequestSpell(status, -1, true)
[[nodiscard]] bool AnyRequestableIcon(PlayerNames player);
/// Some icon whose seed's selectionGesture is `category` (1 SPIRAL, 2 INVERSE_SPIRAL) is requestable
[[nodiscard]] bool AnyRequestableIconOfCategory(PlayerNames player, GestureType category);
/// Some site has a functional icon of that seed that is requestable (-1: no)
[[nodiscard]] bool IconValidForRequest(PlayerNames player, SpellSeedType seed);
/// Of the six sites, the requestable icon of that seed at the site with the most chants available; entt::null for none
[[nodiscard]] entt::entity FindBestSpellIconForSpellSeed(PlayerNames player, SpellSeedType seed);
/// The request packet: the best icon's RequestSpell(status, -1, true): the seed into the hand when full, else it starts
/// charging
bool RequestSpell(PlayerNames player, SpellSeedType seed);
/// The R gesture's condition / the repeat packet: the same with the interface's last seed type (set when a seed goes
/// into or out of the magic hand)
[[nodiscard]] bool CanRepeat(PlayerNames player);
bool RepeatLastSpell(PlayerNames player);
void SetLastSeedType(PlayerNames player, SpellSeedType seed);
[[nodiscard]] SpellSeedType LastSeedType(PlayerNames player);
/// The power-up packet: the held seed's icon charges to that power-up level
bool SetChargingPowerUp(entt::entity icon, PlayerNames player, int powerUp);
/// The cancel packet (the scribble): the charge of the player's that started last is cancelled
bool CancelMostRecentCharge(PlayerNames player);
/// CLEAR_PLAYER_SPELL_CHARGING: every charge of the player is cancelled; false without a citadel
bool CancelAllSpellsCharging(PlayerNames player);
/// IS_SPELL_CHARGING / IS_THAT_SPELL_CHARGING: an icon (of that magic) whose chant store is above 0
[[nodiscard]] bool AnySpellCharging(PlayerNames player);
[[nodiscard]] bool IsThatSpellCharging(PlayerNames player, MagicType type);

/// After a magic type is enabled or disabled: every icon of the player's six sites redraws its levels
void OnMagicTypesChanged(PlayerNames player);

/// A land is loaded
void Reset();
} // namespace openblack::worship::player
