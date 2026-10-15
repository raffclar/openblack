/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <span>
#include <vector>

#include "Creature/CreatureIdleMind.h"

/// How long ago, in game turns, a creature last did each action. The count is not a clock: it grows only when a plan
/// ends, by the turns that the plan's agenda ran, and only for the other actions of the plan's desire. So an action that
/// the creature does not consider while it is busy does not get older. The planner reads these counts for the novelty
/// of an action, and for the rules that stop the creature doing some actions again too soon.
namespace openblack::creature_action_history
{
/// A new creature has done each action this many game turns ago, from 0 up to one less, picked at random
constexpr uint32_t k_FreshTurnsRange = 120;
[[nodiscard]] std::vector<uint32_t> Fresh(size_t actionCount, const creature_mind::Random& random);

/// The plan's action ends, carried out or not: it was done just now. Every other action of the plan's desire, from the
/// first 30 of its list, gets older by the turns that the agenda ran. The list ends at its first entry that is no action.
constexpr size_t k_ListedActions = 30;
void PlanEnded(std::span<uint32_t> turnsSinceDone, uint32_t action, std::span<const uint32_t> desireActions,
               uint32_t agendaTurns);
} // namespace openblack::creature_action_history
