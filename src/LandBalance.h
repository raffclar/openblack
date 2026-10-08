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

namespace openblack::land_balance
{

/// The land balance: 8 multipliers the land script sets with SET_GLOBAL_LAND_BALANCE, all 1 when a map loads (with
/// its features). Known uses: 4 = villager speed (a villager's state speed), 5 = the wood value of trees, 7 = the
/// belief speed constant.
constexpr size_t k_Count = 8;

void Reset();
void Set(int index, float value);
[[nodiscard]] float Get(size_t index);
/// The lost-town scale: SET_LOST_TOWN_SCALE (land script case 104); 1.0 when the land balance is reset (Reset). Read
/// by the town belief's fold (ecs::town_belief)
void SetLostTownScale(float scale);
[[nodiscard]] float LostTownScale();

} // namespace openblack::land_balance
