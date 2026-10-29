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

#include <span>

#include <entt/entity/entity.hpp>

/// The rules by which a script walks through the things in a container (a town or a flock), one thing at a time
namespace openblack::ecs::script_containers
{

/// The first thing of a town for a script: the first person of the first building in the town's list. When that
/// building has nobody in it, the town gives its first homeless person, and it does not look at its other buildings.
[[nodiscard]] entt::entity FirstInTown(std::span<const entt::entity> firstBuildingPeople,
                                       std::span<const entt::entity> homeless);

/// The thing that comes after a thing in the order of a container. None when it is the last, or when it is not in the
/// container.
[[nodiscard]] entt::entity NextAfter(std::span<const entt::entity> members, entt::entity after);

/// Whether a thing is not near a place: its distance is more than the radius. A distance that is not a number is not
/// more than the radius.
[[nodiscard]] bool IsNotNear(float distance, float radius);

/// The number of adults of a town as a script reads it
[[nodiscard]] float TownAdults(uint32_t adults);
/// The room for adults in the built houses of a town as a script reads it
[[nodiscard]] float TownAdultRoom(int32_t room);
/// The number of adults who live in a building as a script reads it. The building counts them in one byte.
[[nodiscard]] float AbodeAdults(uint32_t adults);
/// The room for adults of the kind of a building as a script reads it
[[nodiscard]] float AbodeAdultRoom(uint32_t room);

/// The object that a villager holds after a script sets it: the lowest byte of the number that the script gives
[[nodiscard]] uint8_t CarriedObjectFromScript(uint32_t value);

} // namespace openblack::ecs::script_containers
