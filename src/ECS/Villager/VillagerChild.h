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

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"

// The children and the creche: IsMotherAlive, ChildGotoCreche, the state 113 CHILD_AT_CRECHE and the creche's
// promenade (GetNextDstPromemade). The child's decision (ChildDecideWhatToDo, CheckChild) and 114
// CHILD_FOLLOWS_MOTHER are in VillagerDecide.h, 115 CHILD_BECOMES_ADULT in VillagerAge.h. The creche is the town's
// (town_queries::GetCreche). Positions are MapCoords x / z (ecs::town_queries).

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// GetNextDstPromemade's walk index: the path p (low word) and the step k (high word, signed)
struct PromenadeStep
{
	int32_t path {0};  ///< p, 0..n-1 (n - 1 = -1 when the creche has no path)
	int32_t step {0};  ///< k, 0..9
	int32_t point {0}; ///< j, the path's point: k for k 0..4, 9 - k for k 5..9
	int32_t index {0}; ///< the new walk index: (k << 16) | p
};
/// GetNextDstPromemade's index step over n paths (`atDoor`: the child stands on the creche's door): at the door p =
/// GameRand(n), outside [0, n) -> 0, k = 0. Else p clamped to [0, n - 1]; ++k, negative -> 0; k > 9 -> k = 0, p =
/// GameRand(n), outside [0, n) -> 0
[[nodiscard]] PromenadeStep NextPromenadeStep(int32_t index, int32_t n, bool atDoor);

// ---- the child ---------------------------------------------------------------------------------------------------

/// The mother is set, available, of the child's tribe, a mother (female) and not dead -> 1; else 0
uint32_t IsMotherAlive(entt::entity villager);
/// A town whose creche IsFunctional -> SetupMoveToOnFootpath(creche, its door, 113); 1. Else 0
uint32_t ChildGotoCreche(entt::entity villager);
/// n = the creche mesh's extra metrics / 5 (signed); NextPromenadeStep(index, n, from == the door); n == 0 -> the
/// door; else the extra metric 5p + j in the world + (GameFloatRand(1) - 0.5 (the second draw), GameFloatRand(1) -
/// 0.5 (the first)) in x and z. index = (k << 16) | p
[[nodiscard]] glm::ivec2 GetNextDstPromemade(entt::entity creche, int32_t& index, glm::ivec2 from);
/// State 113 CHILD_AT_CRECHE: CheckChild == 1 -> 1; no town -> 0; CheckNeededForTownDesire == 1 -> 1; no creche ->
/// 0. Day (sky type <= 1.2) and a functional creche: the creche's anim sound effect {0, 0, 0x13, 0, 0x52}, the next
/// promenade point (SetupMoveToPos(it, 113)); 1. Else: an abode whose first inhabitant is at home -> GoHome; no abode
/// and touching the creche (0.001) -> the next point; 0
uint32_t ChildAtCreche(components::LivingAction& action);
} // namespace openblack::ecs::villager
