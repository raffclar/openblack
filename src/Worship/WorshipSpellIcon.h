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
#include <glm/vec3.hpp>

#include "Enums.h"

// WorshipSpellIcon and the SpellIcon base it shares with the town centre's icons: charging from the site's prayer
// power, the fully charged seed into the hand, cancelling, the chant store. `player` stands for the interface status
// (Worship/InterfaceStatus.h).

namespace openblack::worship::icon
{
/// The icon object (mesh 203 at the site's scale and angle), its SpellSeedGraphic, in the site's list and its power-up
/// levels drawn
entt::entity Create(const glm::vec3& worldPosition, SpellSeedType seed, entt::entity site, int16_t slot);

/// Out of the site, its seeds go, then its graphic goes
void ToBeDeleted(entt::entity icon);

/// The site's player
[[nodiscard]] PlayerNames PlayerOf(entt::entity icon);
[[nodiscard]] SpellSeedType SeedTypeOf(entt::entity icon);
/// The magic type of the seed's power-up level
[[nodiscard]] MagicType MagicTypeOf(entt::entity icon, int powerUp);

/// The graphic shows the highest power-up level the player has enabled (0..2, or -1), at alpha 0.5
void UpdatePowerUpGraphics(entt::entity icon);

/// Every turn, from the site's icon update: the removal countdown, the neutral self-charge (influence everywhere), and
/// while charging: the chants into the held seed, and when full the seed's power-up (held) or the seed into the hand,
/// the voice, the charge ends. 3 when it removed itself.
int Process(entt::entity icon);

/// A seed of this icon in a hand of the icon's player
[[nodiscard]] entt::entity GetHeldSpellSeed(entt::entity icon);
/// The held seed's need at the charged level, else costToCreate of that level
[[nodiscard]] float GetChantRequired(entt::entity icon);
/// The held seed's need, else required - store
[[nodiscard]] float GetChantNeeded(entt::entity icon);
/// The charge fraction (store / required; for a held seed need / seed need at that level)
[[nodiscard]] float ChargeFraction(entt::entity icon);
/// Charging (for that player, or any with `anyPlayer`)
[[nodiscard]] bool IsCharging(entt::entity icon, PlayerNames player, bool anyPlayer);

/// The original's quirk is kept: below the requirement it returns what was added; over it the store stops at the
/// requirement and it returns the excess `x - (required - store)`, and that is what the site is charged.
float AddToChantStore(entt::entity icon, float chants);
/// Takes min(x, store) out of the store and returns it
float RemoveFromChantStore(entt::entity icon, float chants);
/// The store back into the site's battery
void ReturnAllChantsToWorshipSite(entt::entity icon);
/// The store into the seed, at most its need
float UseCreateChants(entt::entity icon, entt::entity seed);

/// A seed of this icon at pos, linked to it (its creator is the icon); a one-off seed into the hand uses it when the
/// player has an icon of that seed
[[nodiscard]] entt::entity CreateSeed(entt::entity icon, const glm::vec3& position, PlayerNames player, int powerUp,
                                      float multiplier);
[[nodiscard]] bool ValidForPutFullyChargedSeedInHand(entt::entity icon, PlayerNames player);
/// A seed of this icon (CreateSeed) with the store in it, into the hand, then made inactive (it waits
/// delayBeforeSeedActive). 1 when it was put.
int PutFullyChargedSeedInHand(entt::entity icon, PlayerNames player);
/// For that player's charge (or a store): stop, and the store back to the site
bool CancelCharge(entt::entity icon, PlayerNames player);
/// Not already charging, and chants to charge from (the site's or the store) when asked; the seed goes in at once when
/// already full
bool StartCharge(entt::entity icon, PlayerNames player, int powerUp, bool requireChants);
[[nodiscard]] bool ValidForStartCharge(entt::entity icon, PlayerNames player, int powerUp, bool requireChants);
/// Built, and ValidForPut... or ValidForStartCharge
[[nodiscard]] bool ValidForRequestSpell(entt::entity icon, PlayerNames player, int powerUp, bool requireChants);
/// The player's spell request: the seed if full, else start charging
bool RequestSpell(entt::entity icon, PlayerNames player, int powerUp, bool requireChants);
/// A seed of this icon is held and the power-up level's magic is enabled for the player
[[nodiscard]] bool PowerUpValid(entt::entity icon, PlayerNames player, int powerUp);
/// The player's power-up request: charge the held seed to that power-up level
bool SetChargingPowerUp(entt::entity icon, PlayerNames player, int powerUp);

/// The full seed into the hand, else cancel, else start charging
int HandleValidatedTap(entt::entity icon, PlayerNames player);
/// The player's own icon, or any neutral one with influence everywhere (the original also requires it built)
[[nodiscard]] bool InterfaceValidToTap(entt::entity icon, PlayerNames player);
/// A worship icon is its own; a town centre's icon asks its town's site for the icon of its seed; entt::null
[[nodiscard]] entt::entity WorshipIconOf(entt::entity icon);
/// For a worship icon or a town centre icon (it forwards to the site's icon of its seed): HandleValidatedTap, and for
/// the local player the tap sound (IN_GAME 42 G_ClickOnSpell_01 at pitch {100, 115, 130, 145, 155, 175}[placement
/// index] %). 1 when an icon took the tap.
int InterfaceTap(entt::entity icon, PlayerNames player);

/// The site pays; an icon without a site pays from its store
float MaintainSpell(entt::entity icon, float amount);

/// A seed made from the icon joins / leaves its list (and the seed's flag bit 0)
void AddSeed(entt::entity icon, entt::entity seed);
void RemoveSeed(entt::entity icon, entt::entity seed);

/// The per-frame part of the icon's magic drawing: the pulse ring while charging. phase: the particle system's global
/// phase, 0..1 over 3.33 s (Worship/SpellSeedGraphic.h).
void UpdateChargingVisual(entt::entity icon, float phase);
} // namespace openblack::worship::icon
