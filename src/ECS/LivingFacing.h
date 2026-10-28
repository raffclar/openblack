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

namespace openblack::ecs
{
class Registry;
}

/// The way a walker faces, as the game keeps it: one angle across the land, in radians, and the walk's angle in game
/// angles. A walker that turns comes out of the map's cells and goes in again, at the front of the list of its cell.
namespace openblack::ecs::living_facing
{

/// The walker faces along a game angle (2048 to the circle): its walk takes the angle, and it faces the angle across the
/// land that the game angle gives
void FaceGameAngle(Registry& registry, entt::entity walker, uint16_t gameAngle);

/// The walker faces an angle across the land, in radians. Its walk takes the game angle nearest to it.
void FaceAngle(Registry& registry, entt::entity walker, float yAngle);

/// The walker faces an angle across the land, in radians, and its walk keeps its angle. A thing with no walk only turns.
void FaceOnly(Registry& registry, entt::entity walker, float yAngle);

} // namespace openblack::ecs::living_facing
