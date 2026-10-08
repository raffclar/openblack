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

#include "3D/MapCoords.h"
#include "Enums.h"

// The town's side of the scaffolds: which building a scaffold of n offers near a town, and the wonder's power and
// scale. The plan written is the scaffold's own (components::ScaffoldPlan), not one of the town's plans.

namespace openblack::ecs::components
{
struct ScaffoldPlan;
}

namespace openblack::ecs::scaffold_plans
{
/// best = 0; the abode numbers 0..15 (or only `limit` when it is not -1); for each: the town's tribe, for the
/// WONDER (10) the scaffold's `tribe` when not -1 and the scale WonderScale(pos); the abode info for (tribe, number);
/// skipped when its ScaffoldsRequired is 0 or above n (unsigned), or, unless `force`, when the position does not suit
/// its mesh; the strictly larger GetDesireToBeBuilt(info, n) wins: out's info, angle (*angle) and scale. The scale is
/// reloaded every iteration. Returns the best desire (0: none)
float GetNewPlannedBuilding(entt::entity town, components::ScaffoldPlan& out, const map_coords::MapCoords& pos,
                            const float& angle, float scale, uint32_t n, entt::entity under, Tribe tribe, int32_t limit,
                            bool force);
/// a = GameFloatRand(2 pi); 8 tries of GetNewPlannedBuilding at a, a + pi / 4, ...: the first > 0 -> true
bool ChoosePlanForScaffold(entt::entity town, components::ScaffoldPlan& out, const map_coords::MapCoords& pos, uint32_t n,
                           Tribe tribe, entt::entity under, int32_t limit, bool force);
/// The raw desire 14 (For_Wonder) plus what the objects around pos are worth (the town artifact value + the
/// impressive value x 0.1, within r = max(15 x raw, 15) m); at least 0.25
[[nodiscard]] float GetWonderPower(entt::entity town, const map_coords::MapCoords& pos);
/// p x (1 - 0.1 p), p = min(GetWonderPower(pos), 5)
[[nodiscard]] float WonderScale(entt::entity town, const map_coords::MapCoords& pos);
} // namespace openblack::ecs::scaffold_plans
