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

// One-shot miracles: the orbs on the land, and a fully charged
// seed straight into the hand.

namespace openblack::magic::one_off
{
/// The orb (seed 0..29) at a world point; entt::null for a bad seed
entt::entity Create(const glm::vec3& worldPosition, SpellSeedType seedType, int powerUp, float scale);

/// If the hand is free, a seed at the hand (linked to the player's best worship icon for it) charged for free with its
/// full cost, the magic marked ever enabled, placed in the magic hand and ready at once. The seed, or entt::null.
entt::entity CreateSpellIntoHand(PlayerNames player, SpellSeedType seedType, int powerUp, float multiplier);

/// CreateSpellIntoHand with the orb's seed, then immersion 14 (pending: casting from the hand), the bubble pop sound
/// and the orb goes (3). 0 when the hand could not take it.
int InterfaceTap(entt::entity orb, PlayerNames player);

/// Picking the orb itself: only "ever enabled"
void InterfaceSetInMagicHand(entt::entity orb, PlayerNames player);

/// For every orb: phase = fmod(phase + ms x 18 x 0.001, 16), frame = int(phase), the texture offset
/// ((frame % 4) / 4, (frame / 4) / 4). milliseconds: the game time step of this frame.
void UpdateFrames(float milliseconds);
} // namespace openblack::magic::one_off
