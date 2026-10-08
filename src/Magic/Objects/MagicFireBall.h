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

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// MagicFireBall: the invisible Object each ball of a fireball spell carries. It is made by the PSys rule
// AttatchFireBallToAtom (Particles/Rules/Fireball.cpp) and follows its atom; its FireEffect (at initialTemperature x the
// effect's strength) does the burning. The enemy's can be caught; a held fire seed absorbs one.
// Wiki: docs/bw1-notes/miracles.md (fireball).

namespace openblack::magic::fireball
{
/// The object at pos with GMagicFireBallInfo[row], for an atom (not in the map cells), then its temperature
/// (strength x initialTemperature, the spell's creator) and affected by rain unless a script cast it
entt::entity Create(const glm::vec3& position, int infoRow, uint32_t effect, uint32_t atomKey,
                    std::optional<PlayerNames> player, bool scriptCast, entt::entity source);

/// AttatchFireBallToAtom's update of its object: the atom's position (x, z; the height above the land), SetScale(atom
/// scale x rule scale); true when the ball has cooled below deletionTemperature (the atom is then deflected)
bool FollowAtom(entt::entity fireball, const glm::vec3& position, float scale, uint32_t turn);

/// The effect's strength now: GetHeatCapacity reads it live (1 without the effect)
[[nodiscard]] float Strength(entt::entity fireball);

/// Off the fireball list, its fire goes
void ToBeDeleted(entt::entity fireball);

/// 1 unless it is the catcher's own (the status player == its player)
[[nodiscard]] bool ValidForPlaceInHand(entt::entity fireball, PlayerNames player);
/// Tapped or set in the magic hand: with the hand free and valid, a new FIRE seed at the ball (seed 2, pu -1,
/// multiplier 1) into the hand, ready and fully charged; the ball goes. The seed, or entt::null.
entt::entity Catch(entt::entity fireball, PlayerNames player);
/// A held FIRE seed absorbs the ball: the seed's power x (1 + catchIncreaseFactor), and the ball goes
void DeleteAndPutIntoSpellSeed(entt::entity fireball, entt::entity seed);

/// Every fireball, newest first
[[nodiscard]] const std::vector<entt::entity>& All();
/// Once per turn right after the spells stepped their PSys: a ball its atom did not refresh (the atom, or the whole
/// effect, is gone) goes with it (as when the atom's data is destroyed)
void ProcessTurn(uint32_t turn);
/// A land is loaded
void Clear();
} // namespace openblack::magic::fireball
