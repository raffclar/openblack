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

#include "ECS/Components/SpellSeed.h"
#include "Particles/SpellLink.h"

namespace openblack
{
struct GSpellSeedInfo;
} // namespace openblack

// The miracle in the hand: its charge, power-up, readiness and the cast. Casting
// from the hand (the interface's states and gestures) is not ported yet; these are the seed's own functions.

namespace openblack::magic::seed
{
using ecs::components::SpellSeed;

[[nodiscard]] const GSpellSeedInfo& InfoOf(const SpellSeed& seed);

/// A new seed (position, seed type, player, power-up, multiplier): no icon, the creator is the interface's player,
/// scale = info.scale. The entity is an Object (a creation index).
entt::entity Create(const glm::vec3& worldPosition, SpellSeedType seedType, PlayerNames player, int powerUp, float multiplier);

/// The seed's magic at its power-up level (the base type for an empty level)
[[nodiscard]] MagicType MagicTypeOf(const SpellSeed& seed);

/// costToCreate(pu) - the store
[[nodiscard]] float GetChantNeeded(const SpellSeed& seed, int powerUp);
/// min(store / costToCreate, 1)
[[nodiscard]] float GetPower(const SpellSeed& seed);
/// The store and its copy both become chants
void SetChantStore(SpellSeed& seed, float chants);
void AddToChantStore(SpellSeed& seed, float chants);

/// The level; a charge above its cost goes back to the worship site. For the local interface: the tooltip, the
/// hand FX, the PSys preload and the level's sound: pending.
void SetPowerUp(entt::entity seed, int powerUp);

/// 0 -> ready; else inactive (not ready, no turns in the hand). Worship icons pass 1 (the seed waits
/// delayBeforeSeedActive), one-shots and caught fireballs 0 (ready at once).
void SetInactive(SpellSeed& seed, bool inactive);

/// The seed comes into the hand. Returns 1, or 3 when the seed was deleted
/// (its last spell cannot be recast, or flag bit 1).
int InterfaceSetInMagicHand(entt::entity seed);

/// Every game turn while held: ready after delayBeforeSeedActive (1.5 s); a seed
/// whose spell closed goes
void ProcessInHand(entt::entity seed);

/// The last spell's chants, age and object count go into the seed and the link is cleared; returns whether the spell
/// has enough chants and life for a recast (1 without a spell)
bool StoreChantsAndAgeFromSpell(entt::entity seed);

/// Stops the immersion (pending), seed.spell = none; a spell still bound to this seed closes
/// down, else only its PSys does
void ClearSpellLink(entt::entity seed);

/// deleteSeedOnceCast -> the seed goes; else it keeps the spell's chants (and
/// goes if they are not enough for a recast)
void ApplyUnlockProcess(entt::entity seed);

/// Every turn, from its spell: a seed that follows its spell (FollowsSpell) closes it when it is out of its player's
/// influence. Returns 1.
int ProcessFromSpell(entt::entity seed);

/// The seed follows its spell (not in the map, not cast in hand, not kept in hand, seedFollowsSpell, its spell (if any)
/// still open, and linked to a worship icon)
[[nodiscard]] bool FollowsSpell(const SpellSeed& seed);

/// A neutral seed while influence is everywhere (the gathering flag), or an available seed of the hand's player (its
/// interface's player)
[[nodiscard]] bool ValidForPlaceInHand(entt::entity seed, PlayerNames handPlayer);

/// Every frame, each spell draws its seed: a seed that follows its spell is put over it (on the objects under it, as
/// the spell adjusts it) and drawn, so the hand can see it and pick it up; no other seed out of the hand is drawn
void DrawSpells();

/// The magic's cast rule for the seed's player, then its class check
[[nodiscard]] bool CanCast(entt::entity seed, const glm::vec3& position);

/// An in-hand seed keeps one live spell; else the pre-cast steps, CastAtPos and the post-cast steps. handInfo is the
/// interface's spell info (hand position, velocity), magnitude the gesture's size. Returns 1 with the spell in `out`.
int Cast(entt::entity seed, const glm::vec3& position, entt::entity* out, float magnitude, const psys::ProcessInfo& handInfo);

/// Unlinks the worship icon, ClearSpellLink, and the object goes
void ToBeDeleted(entt::entity seed);
} // namespace openblack::magic::seed
