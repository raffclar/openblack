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

#include <functional>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

namespace openblack::ecs::villager
{
/// One row of the villager state table (the rows openblack has are in LivingActionSystem.cpp k_VillagerStateTable).
/// An empty slot is "no function", as in the original: the callers then act as if it had returned 1
/// (CallExitStateFunction, CallEntryStateFunction).
struct VillagerStateTableEntry
{
	/// The state function, called each turn while it is the TOP state
	std::function<uint32_t(components::LivingAction&)> state = nullptr;
	/// The entry, with the final state from before the change and the state entered. 1 = accepted (the caller
	/// sets the state), 0x23 = accepted and the function set the states itself, anything else = refused
	std::function<uint32_t(components::LivingAction&, VillagerStates final, VillagerStates next)> entryState = nullptr;
	/// The exit, with the state that follows. 1 = it may leave
	std::function<uint32_t(components::LivingAction&, VillagerStates next)> exitState = nullptr;
	std::function<bool(components::LivingAction&)> saveState = nullptr;
	std::function<bool(components::LivingAction&)> loadState = nullptr;
	/// The town emergency's answer: not used (no caller). The original's column for all 255 rows is
	/// k_TownEmergencyReaction (VillagerOriginalFns.h), read by villager::ReactsToTownEmergency
	std::function<bool(components::LivingAction&)> field0x50 = nullptr;
	std::function<bool(components::LivingAction&)> field0x60 = nullptr; ///< The state's clip function
	/// The into / out-of clip function: not used, the clips go by k_StateAnimFns (ECS/VillagerAnimationTable.h)
	std::function<int(components::LivingAction&)> transitionAnimation = nullptr;
	std::function<bool(components::LivingAction&)> validate = nullptr; ///< Called by ProcessState, result unused
};
} // namespace openblack::ecs::villager
