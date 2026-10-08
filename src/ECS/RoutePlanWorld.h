/*******************************************************************************
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
#include "RoutePlanner/Point2D.h"

// The route planner's world side: what
// the ObstacleGrid sees of the land: the central land_avoid mask (3D/LandAvoid.h, built at every land load) and the
// holder's square callback is CheckSquareFunction (installed at game creation). The
// objects add their circles through AddToRoutePlan when CreatureMustAvoid says so. Only the
// footpaths' case is ported: no creature (the creature's branches are (pending, the creature port)).

namespace openblack::route_planner
{
class ObstacleGrid;
} // namespace openblack::route_planner

namespace openblack::ecs::route_plan_world
{
/// The push-out callback: instead of AddObject, each circle the object would add
using CircleFn = void (*)(void* context, entt::entity object, const route_planner::Point2D& centre, float radius,
                          int32_t notify);

/// ObstacleGrid::InstallCallbacks(CheckSquareFunction, nullptr), at every land load (after land_avoid::Validate)
void OnLandLoaded();

/// the cell's 3 x 3 LandAvoid circles, then its objects' (fixed list, then
/// mobile list)
void CheckSquareFunction(int32_t cellX, int32_t cellZ, route_planner::ObstacleGrid& holder);
/// whether the creature must avoid the object, with no creature
[[nodiscard]] bool CreatureMustAvoid(entt::entity object);
/// whether this creature must avoid the object: as with no creature, except that another player's map shield is
/// avoided and a dead tree is avoided while it burns, unless a script controls it
[[nodiscard]] bool CreatureMustAvoid(entt::entity object, entt::entity creature);
/// the object's circles (no creature)
void AddToRoutePlan(entt::entity object, route_planner::ObstacleGrid& holder, int32_t notify, CircleFn callback = nullptr,
                    void* context = nullptr);

/// x, z x 10 / 65536 (the altitude read and dropped)
[[nodiscard]] route_planner::Point2D ToPoint2D(const map_coords::MapCoords& coords);
/// truncated x x 6553.6, z x 6553.6, altitude 0
[[nodiscard]] map_coords::MapCoords ToMapCoords(const route_planner::Point2D& point);
/// The circles' id: the object
[[nodiscard]] int32_t IdOf(entt::entity object);
} // namespace openblack::ecs::route_plan_world
