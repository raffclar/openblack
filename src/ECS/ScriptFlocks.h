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
#include <cstdint>

#include <functional>

#include <entt/entity/fwd.hpp>

#include "3D/MapCoords.h"

namespace openblack::ecs
{
class Registry;
}

/// The flocks the scripts gather villagers into: made at a place, joined and left by livings, moved by the scripts
namespace openblack::ecs::script_flocks
{

/// A new flock at a place, with nobody in it
entt::entity Create(Registry& registry, const map_coords::MapCoords& place);
/// A flock goes: its members are in no flock any more
void Destroy(Registry& registry, entt::entity flock);

[[nodiscard]] bool IsFlock(const Registry& registry, entt::entity thing);
[[nodiscard]] bool IsMember(const Registry& registry, entt::entity flock, entt::entity living);
/// The flock a living is in, or none
[[nodiscard]] entt::entity FlockOf(const Registry& registry, entt::entity living);
[[nodiscard]] std::size_t Size(const Registry& registry, entt::entity flock);
/// Its leader, the last in its line, or none for an empty flock
[[nodiscard]] entt::entity Leader(const Registry& registry, entt::entity flock);

/// A living joins a flock in its line by its place. It leaves any flock it was in first, even this one, and a flock it
/// leaves empty goes. False when it was in this flock already, or the flock went as it left it.
bool AddLiving(Registry& registry, entt::entity flock, entt::entity living);
/// A living becomes a flock's leader, taking the place one past its last member's. A member already is moved to the
/// end of the line; a living in another flock is taken out of it.
void AddLeader(Registry& registry, entt::entity flock, entt::entity living);
/// A living leaves its flock. With `deleteWhenEmpty` a flock it leaves empty goes.
void Remove(Registry& registry, entt::entity living, bool deleteWhenEmpty);

/// Where a flock is: its leader's position, or its own place when it has nobody
[[nodiscard]] map_coords::MapCoords Position(const Registry& registry, entt::entity flock);
/// A script moves a flock: its own place, and the goal its leader walks to (without setting it off)
void MoveTo(Registry& registry, entt::entity flock, const map_coords::MapCoords& place);
/// A member chosen at random: one of its first `count` (one fewer leaving out `exclude` when there are others), the
/// draw counting from the start of the line and passing over `exclude`; none in an empty flock
[[nodiscard]] entt::entity RandomMember(const Registry& registry, entt::entity flock, entt::entity exclude,
                                        const std::function<uint32_t(uint32_t)>& random);

/// The position of a living as the flocks measure it
[[nodiscard]] map_coords::MapCoords PositionOf(const Registry& registry, entt::entity living);

} // namespace openblack::ecs::script_flocks
