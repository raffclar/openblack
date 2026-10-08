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

// SpellSeedGraphic: the seed's mesh floating over a spell icon or inside a one-shot orb, its holder effect
// (GSpellSeedInfo.holderParticle) and its power-up band.

namespace openblack::worship::seed_graphic
{
/// The mesh (GSpellSeedInfo.mesh) at pos.y + meshHeight x scale (-1.5), the holder effect at pos.y +
/// holderHeight x scale, and the band when pu != -1. worldPosition: the point it floats at.
entt::entity Create(const glm::vec3& worldPosition, SpellSeedType seed, PlayerNames player, float scale, int powerUp);

/// Removes the graphic with its 3D objects and effect
void Delete(entt::entity graphic);

/// The power-up level (the band object appears for a level and stays when the level goes back to -1;
/// DrawSpellGraphic draws it pu + 1 times, none at -1)
void SetPowerUpType(entt::entity graphic, int powerUp);
/// Auto-updated graphics step their holder effect every turn (ProcessTurn)
void SetAutoUpdate(entt::entity graphic, bool autoUpdate);
/// The power-up band's size factor (0.5 on a worship icon)
void SetBandScale(entt::entity graphic, float bandScale);

/// Called by a one-shot orb every drawn frame (point: the orb's drawn matrix applied to its mesh box centre, scale =
/// orb scale x 0.6): the mesh at point + meshHeight x scale, the effect at + holderHeight x scale, and the holder
/// effect moved there, magnitude = scale, stepped by milliseconds
void DrawUpdateAtPos(entt::entity graphic, const glm::vec3& point, float scale, float milliseconds);
/// The holder effect stepped by milliseconds
void UpdateOnly(entt::entity graphic, float milliseconds);
/// Player seeds: the mesh turning about y at 2 rad/s, at GSpellSeedInfo.scale x scale, drawn with the owner's alpha
/// (0xFF opaque; the orb gives 0x95); the bands
void DrawSpellGraphic(entt::entity graphic, uint8_t alpha, float milliseconds);
/// For every worship-site and town-centre icon: UpdateOnly + DrawSpellGraphic with alpha 0xFF, every frame
void UpdateIconGraphics(float milliseconds);

/// Every turn, with the spells: the auto-updated graphics step their holder effect; every 30 turns the FLYING_FLOCK
/// mesh is redone for the player's alignment
void ProcessTurn();

/// The particle systems' global phase (+ ms x 0.001 / 3.33, 0..1), per frame
void UpdatePhase(float milliseconds);
[[nodiscard]] float Phase();
} // namespace openblack::worship::seed_graphic
