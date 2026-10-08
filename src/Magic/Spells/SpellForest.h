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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// SpellForest: the forest miracle (MAGIC_TYPE 13, NATURE). Its PSys (SF_Forest) drops a seed from 9.4 m; when it lands
// (SpellEvent 3) the whole forest appears at once, up to finalNoTrees (18) saplings in a spiral from 2 to 11 m around
// the cast point, and every turn the spell grows them (growSpeed) towards their target scale, or shrinks them all
// (decaySpeed) while it wants fewer trees than it has (no strength left: none). The trees are MagicTrees in a Forest
// container (ECS/Forests, ECS/TreeGrowth, Magic/Objects/MagicTree). Wiki: docs/bw1-notes/magic.md.

namespace openblack::magic
{
/// A forest spell's own state
struct SpellForestData
{
	uint32_t forestId {0};      ///< The Forest it made: an ECS/Trees forest id, 0 none
	bool forestCreated {false}; ///< Cleared at allocation
	int maxTrees {-1};          ///< SetMaxObjectsToCreate (-1 -> finalNoTrees)
	/// the forest went with its last magic tree (ToBeDeleted), seen by the next Process
	bool forestDeleted {false};
};

namespace spell_forest
{
/// The forest's outer and inner radius (m)
constexpr float k_ForestRadius = 11.0f;
constexpr float k_InnerRadius = 2.0f;
/// The spiral's turns per tree (17 / 13)
constexpr float k_TurnsPerTree = 17.0f / 13.0f;

/// round(maxTrees (finalNoTrees when -1) x (strength > 0 ? 1 : 0))
[[nodiscard]] int TreesWanted(float strength, int maxTrees, uint32_t finalNoTrees);
/// Tree i of n in the spiral, as the offset from the cast point: f = i x (n > 1 ?
/// 1 / (n - 1) : 1), r = 2 + (11 - 2) x sqrt(1 - (1 - f)^2), angle = f x n x 1.30769 x 2 pi, (r cos, r sin)
[[nodiscard]] glm::vec2 SpiralOffset(int i, int n);
/// The MapCoords the tree goes to: (trunc(x x 6553.6), trunc(z x 6553.6), 0) back in metres
[[nodiscard]] glm::vec2 ToMapCoords(glm::vec2 point);
/// The tree's target scale, 1 - 0.5 x distance to the cast point / 11
[[nodiscard]] float TargetScale(float distance);
/// min(maxTrees, forest ? its trees : (created ? 0 : finalNoTrees))
[[nodiscard]] int MaxObjectsToCreate(int maxTrees, uint32_t finalNoTrees, bool hasForest, uint32_t trees, bool created);
/// costPerGameTurn + trees x costPerEvent
[[nodiscard]] float CostToMaintain(float costPerGameTurn, float costPerEvent, uint32_t trees);
/// The seed's altitude (MapCoords y) is at least the tallest tree's height, or -5 without a forest (until SpellEvent 3
/// made it). Called by the seed's draw every frame: the seed sits under the land while the PSys seed falls, then on the
/// ground in the middle of the new forest, then on top of its tallest tree as the trees grow.
[[nodiscard]] float AdjustSpellSeedAltitude(bool hasForest, float tallestTree, float altitude);
/// AdjustSpellSeedPos of a forest spell on the seed's altitude
[[nodiscard]] float AdjustSpellSeedPos(entt::entity spell, float altitude);

/// The cast check at a position: in bounds, land, no Abode of the cell there whose Get2DRadius reaches the point, and
/// ValidPlaceForTree
[[nodiscard]] bool CanCastAt(const glm::vec3& position);
/// In bounds, land and not a fixed cell (the cell's first fixed object, the last one put in it, is a multi-cell fixed
/// object: a building, a field, a feature, a static... not a tree)
[[nodiscard]] bool ValidPlaceForTree(const glm::vec3& position);
/// The magicTreeTypes[GameRand(4)] of the terrain material at the cell of the position (the snow material 27 when the
/// snow there is >= 27)
[[nodiscard]] TreeInfo RandomTreeType(const glm::vec3& position);

/// The spell's state (for the trace and the tests); nullptr if it is not a forest spell
[[nodiscard]] const SpellForestData* DataOf(entt::entity spell);
/// GetMaxObjectsToCreate of a spell
[[nodiscard]] int GetMaxObjectsToCreate(entt::entity spell);
} // namespace spell_forest
} // namespace openblack::magic
