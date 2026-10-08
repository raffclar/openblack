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

// MagicTree: the trees of the forest miracle. A normal Tree entity (TreeArchetype) plus components::MagicTree. Wiki:
// docs/bw1-notes/miracles.md ("Forest").

namespace openblack::magic::magic_tree
{
/// A tree (maxScale 1.0, so growing, in the forest: ECS/Trees' forest id, TreeArchetype::Create) with the magic flag,
/// the wood multiplier, the spell's player (without a spell the neutral one: ScriptPlayer.h), and a
/// REACT_TO_MAGIC_TREE reaction for that player.
/// position: x, z on the map (the tree stands on the land).
entt::entity Create(const glm::vec3& position, entt::entity spell, TreeInfo type, uint32_t forestId, float angle, float scale,
                    float woodValueMultiplier);

/// The tree's own deletion (ECS/Trees' DeleteTree); its reactions go in the
/// tree-deleted listener (MagicTree.cpp), and "the forest goes with its last tree" is SpellForest's
/// (ForestLostAMagicTree)
void ToBeDeleted(entt::entity tree);
/// True once (then forgotten) when a magic tree of that forest was deleted since the last call
[[nodiscard]] bool ForestLostAMagicTree(uint32_t forestId);
/// A land is loaded
void Clear();
/// ECS/Trees' deletion listener of the magic trees: added once the game's tree state exists, before any tree
void RegisterTreeListener();

/// Its REACT_TO_MAGIC_TREE goes
void StartOnFire(entt::entity tree);
/// REACT_TO_MAGIC_TREE again (its player), unless the map is being cleared: the original skips the reaction then, which
/// is why the objects being torn down make no reactions, tell no towns and so on. openblack only calls EndOnFire from
/// the fire system (ECS/Fire/FireEffect.cpp, when a fire goes out), never while a land is being cleared, so the
/// condition always holds here: no guard to port.
void EndOnFire(entt::entity tree);

/// The wood value multiplier: a magic tree's own, 1 for a plain tree (a tree's wood value = life x this x woodValue x
/// scale x a constant)
[[nodiscard]] float WoodValueMultiplier(entt::entity tree);
} // namespace openblack::magic::magic_tree
