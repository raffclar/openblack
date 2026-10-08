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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// Where miracles come from (docs/bw1-notes/magic.md): towns hold
// magic types, the player's citadel has a worship site per tribe with an icon per spell seed, villagers dance there and
// fill its battery with prayer power, the icons charge from it and put the seed into the hand. Plus the one-shot
// orbs' dispensers and the fireflies. This header is what Magic/MagicLoop.cpp and the hand call.
//
//   Worship/Citadel            the six sites around the heart, PostLoadCleanup
//   Worship/WorshipSite        the site, its battery and chant accounting, its icons' places
//   Worship/WorshipSpellIcon   charging, tapping, the seed into the hand, refunds
//   Worship/TownCentreSpellIcon the town centre's icons
//   Worship/TownMagic          the magic a town holds
//   Worship/WorshipPercentage  the totem drag, who goes to worship
//   Worship/SpellSeedGraphic   the floating seed over an icon or in an orb
//   Worship/PlayerSpellIcons   the player's icons (the gesture selection's queries)
//   Worship/SpellDispenser     the miracle dispensers
//   Worship/FireFlyReward      the fireflies' one-shots
//   ECS/Systems/Implementations/VillagerWorship  the villagers' worship states

namespace openblack::worship
{
/// A land is loaded (before its script)
void OnLoadMap();

/// The worship part of the spells' turn: the SpellSeedGraphic list and the players' spell icons, before the maintain
/// requests
void ProcessSpellIcons();

/// Once per game turn in the players/dances slot: on the first turn the players' post-load cleanup (the original runs
/// it right after the land's script), the test hooks, then the spell dispensers ((inferred) the abodes' own process
/// is the towns')
void ProcessTurn(uint32_t turn);

/// Per frame (game time): the PSys global phase, the icons' charge rings, the sites' strain pulse, the totems' rise
void Update(float seconds);

/// The hand's tap on an object: worship icons, town centre icons and one-shot orbs. 0 when the object is none of them
/// or refused the tap.
[[nodiscard]] bool InterfaceValidToTap(entt::entity object, PlayerNames player);
int InterfaceTap(entt::entity object, PlayerNames player);

/// For any object the hand takes (the firefly on it)
void OnPlacedInMagicHand(entt::entity object);

/// The object is a spell-seed return point of the player (a spell dispenser, a WorshipTotem or a spell icon), so a
/// seed may always be given to it
[[nodiscard]] bool IsSeedReturnPoint(entt::entity object, PlayerNames player);

/// A spell seed put down on a site's ground, or thrown or given to a totem, an icon or a dispenser: 3 when the seed
/// went (its chants back in the battery), 0 when the object is none of those (ApplySeedToPosition: 0x17 off a site)
int ApplySeedToObject(entt::entity seed, entt::entity object);
int ApplySeedToPosition(entt::entity seed, const glm::vec3& position);
/// A forced throw: the charge back to its icon's site; the seed goes either way (3)
int ReturnSeedToItsSite(entt::entity seed);
/// The seed leaves the hand: its icon stops charging for that hand and the interface remembers the seed type
void OnSeedOutOfHand(entt::entity seed, PlayerNames player);

/// OPENBLACK_TEST_MANA / _WORSHIP / _TOWN_SPELL / _TAP_ICON / _DISPENSER / _FIREFLY_REWARD (WorshipDebugHooks.cpp)
void RunDebugHooks(uint32_t turn);
void ResetDebugHooks();
} // namespace openblack::worship
