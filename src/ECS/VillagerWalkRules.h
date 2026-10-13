/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Enums.h"

/// In which of its states a villager goes along its walk. The game moves a villager along its walk only while the
/// state it is in walks it: going somewhere, or dancing at its place in a dance. In any other state the walk waits:
/// the villager stands where it was, and goes on with the walk if it comes back to a walking state. So a villager
/// called away mid-walk, to kiss, to look at something or to pause for a second, stops at once. Pure.
namespace openblack::ecs::villager_walk
{

/// Whether a villager in this state goes along its walk this turn
[[nodiscard]] bool Walks(VillagerStates state);

} // namespace openblack::ecs::villager_walk
