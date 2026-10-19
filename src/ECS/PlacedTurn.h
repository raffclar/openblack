/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat3x3.hpp>

#include "Common/X87Angle.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Registry.h"

/// How a placed thing turns and leans. The game keeps the angles of a placed thing and makes its turn from them.
namespace openblack::ecs::placed_turn
{

/// Whether a thing can lean about x and z as well as turn about the upright axis. Mobile statics, mobile objects, pots,
/// rocks and dead trees can lean. All other things stand upright.
[[nodiscard]] inline bool CanLean(const Registry& registry, entt::entity entity)
{
	return registry.AnyOf<components::MobileStatic, components::MobileObject, components::Pot, components::DeadTree>(entity);
}

/// The turn of a thing after a turn about the upright axis by `radians`, in the direction of `glm::eulerAngleY`. The game
/// adds the turn to the angle about y and keeps the leans, so the result is the game's turn from the new angles.
[[nodiscard]] inline glm::mat3 TurnedAboutUpright(const glm::mat3& turn, float radians)
{
	const auto angles = x87::GameTurnMatrixAngles(turn);
	return x87::GameTurnMatrix(angles.x, angles.y - radians, angles.z);
}

} // namespace openblack::ecs::placed_turn
