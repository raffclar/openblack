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
#include <glm/vec2.hpp>

// The miracle's look on the player's hand: the glowing S_Hand_Flow second pass, the Power_Up_Band rings and the
// in-hand particle effect of the seed. Wiki: docs/bw1-notes/magic.md, "The hand".

namespace openblack::magic::hand_fx
{
/// Every permanent band goes
void RemoveAllPermanentBands();
/// Every scribble cancel: G_ShakeHand_01 and one band flying off
void RemoveHandSpellVisuals();
/// Five temporary bands 0.1 s apart (2.4 s later if delayed) and
/// G_SpellPowerUpBand
void AddSpellToHandVisuals(bool delayed);
/// Permanent bands added or removed until there are `level` (at most 5)
void SetPowerUpLevel(int level, bool delayed);
/// The number of permanent bands
[[nodiscard]] int PowerUpLevel();
/// The tribal power column of a tribe whose tribal power is above 1. No tribe gains any in the original game:
/// not drawn.
void StartTribalPowerRing(int tribe);
void StopTribalPowerRing();
void ReleaseOrCreateTribalPowerRing();

/// Every frame (seconds = the game time step x 0.001, 0 while paused)
void Update(float seconds);

/// The hand's second pass with the flowing texture (for the renderer): alpha 0 = not drawn
struct Glow
{
	float alpha {0.0f};        ///< 0.8 while a miracle is in the hand
	glm::vec2 uvOffset {0.0f}; ///< the 8 x 4 atlas cell of the frame
};
[[nodiscard]] Glow GetGlow();

/// The seed's in-hand effect (GMagicInfo.particleTypeInHand of its level), replacing the old one
void CreateInHandEffect(entt::entity seed);
/// The in-hand effect goes
void ReleaseInHandEffect();
/// Every frame: to the hand (or its bone), strength = the seed's PSys power, magnitude = the hand's scale, a step of
/// max(1, the game time step) ms; drawn only once the seed is ready
void UpdateInHandEffect(float milliseconds);

/// A land is loaded: the bands and the effect go
void Reset();
} // namespace openblack::magic::hand_fx
