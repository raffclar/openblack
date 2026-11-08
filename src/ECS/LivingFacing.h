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

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>

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

/// The angle across the land that a villager or an animal keeps, in radians. The game keeps this angle and changes the
/// walk's angle with it. An animal keeps it as its heading, and a villager keeps it with its walk. Other things have no
/// such angle.
[[nodiscard]] std::optional<float> KeptAngleOf(const Registry& registry, entt::entity thing);

/// The world axes of a villager or an animal from the angle that it keeps and its size, as the game makes them for a
/// thing that stands upright. Then the axes turn a quarter about the up axis to the axes that the model has.
/// The quarter turn does not change the up axis, which is the only axis that the pull of the hand uses.
[[nodiscard]] glm::mat3 WorldAxesOf(float yAngle, float scale);

} // namespace openblack::ecs::living_facing
