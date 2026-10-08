/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

// The abode's queries the idle villagers use. Positions are MapCoords x / z (ecs::town_queries).

namespace openblack::ecs::abode_queries
{
/// ecs::IsAvailable (valid and not marked Unavailable)
[[nodiscard]] bool IsAvailable(entt::entity abode);
/// Not flagged as under construction && GetPercentBuilt >= 1 (abodes::IsBuilt)
[[nodiscard]] bool IsBuilt(entt::entity abode);
/// IsAvailable, IsBuilt, and the abode info's thresholdForStopBeingFunctional < its life (the percent repaired); then
/// IsBuilt again
[[nodiscard]] bool IsFunctional(entt::entity abode);
/// The door point: the world door point in map coordinates (x 6553.6, truncated; y 0); when there is none, or its x or
/// its z is 0, the abode's position. (approximate) the door point of the L3D (extra point 0, HasDoorPosition)
/// through the abode's Transform: the engine's own transform is not read
[[nodiscard]] glm::ivec2 GetArrivePos(entt::entity abode);
/// The door point under its own name, for any multi-map fixed object (GetArrivePos above reads only its Transform and
/// Mesh): the creche door children walk to, the multi-map fixed branch of ArrivePosOf
[[nodiscard]] inline glm::ivec2 GetDoorPos(entt::entity multiMapFixed)
{
	return GetArrivePos(multiMapFixed);
}
/// door + GetPosFromAngle(Get3DAngleFromXZ(pos, door) + GameFloatRand(2 pi / p1) - pi / p1, GameFloatRand(p3) + p2)
/// (the angle is drawn first)
[[nodiscard]] glm::ivec2 GetPosOutside(entt::entity abode, float p1, float p2, float p3);
} // namespace openblack::ecs::abode_queries
