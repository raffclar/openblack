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
#include <entt/fwd.hpp>

#include "3D/MapCoords.h"

// Where a town may put a fixed building: the town rectangle, the fixed check IsSuitableForFixedAbodeInTown and its
// abode wrapper IsOkToCreateAtPos. Not map_collide::IsOkToCreateAtPos (the CREATE_* commands' rule).

namespace openblack
{
struct GAbodeInfo;
}

namespace openblack::ecs::town_placement
{
/// IsSuitableForFixedAbodeInTown's "inside the nearest other town's rectangle" result
constexpr uint32_t k_InsideOtherTown = 0xE;

/// Resets the rectangle (min = INT32_MAX, max = 0), then widens it by every abode and every field (openblack's fields
/// are abodes). Called only where the original calls it: when a structure joins the town (AbodeArchetype::Create:
/// `newcomer`, whose radius is 0 there, see the .cpp) and when one leaves (the abode already out of the town)
void SetTownArea(entt::entity town, entt::entity newcomer = entt::null);
/// `object`'s radius around its position widens the town's rectangle (used alone when a field is created)
void ExtendTownArea(entt::entity town, entt::entity object);
/// The rectangle's centre as it is: the mean of min and max in metres, back to fixed point, altitude 0 (used by the
/// scenic forest). No town: {0, 0, 0}
[[nodiscard]] map_coords::MapCoords GetTownAreaCentre(entt::entity town);
/// The mesh's collide descriptor cells (map_cells::DescriptorCells, reach scale x half diagonal + 1); for each: the
/// position's own cell on the map and not water, and no MultiMapFixed of the cell's fixed list with
/// d < R_obj + scale x max(hx, hz) (strict). No mesh: 0
[[nodiscard]] bool IsSuitableForFixed(const map_coords::MapCoords& pos, entt::id_type meshId, float yAngle, float scale);
/// The same for an object, and the position's cell must not be on the coast line, and `under` (a MultiMapFixed, or
/// entt::null) is not an obstacle. Its callers: the scaffolds (placing, in the hand, building a planned building) and
/// the town choosing a new planned building
[[nodiscard]] bool IsSuitableForFixedObject(const map_coords::MapCoords& pos, entt::id_type meshId, float yAngle, float scale,
                                            entt::entity under);
/// The position's own cell test of both checks: on the map, a block, not water; with refuseCoast (the object check)
/// not on the coast line either
[[nodiscard]] bool PositionCellIsBuildable(const map_coords::MapCoords& pos, bool refuseCoast);
/// With a town, the nearest OTHER town (any tribe) whose rectangle is within 4 cells -> k_InsideOtherTown (accepted
/// WITHOUT the fixed tests: literal); else IsSuitableForFixed (0 / 1)
[[nodiscard]] uint32_t IsSuitableForFixedAbodeInTown(const map_coords::MapCoords& pos, entt::id_type meshId, entt::entity town,
                                                     float yAngle, float scale);
/// IsSuitableForFixedAbodeInTown(pos, info's mesh, town, angle, scale) != 0
[[nodiscard]] bool IsOkToCreateAtPos(const GAbodeInfo& info, const map_coords::MapCoords& pos, float yAngle, float scale,
                                     entt::entity town);
} // namespace openblack::ecs::town_placement
