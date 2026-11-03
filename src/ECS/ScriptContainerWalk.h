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

#include "Enums.h"

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

/// The room for adults and children in the built houses of a town as a script reads it: the sum of the two counts
[[nodiscard]] float TownCapacity(int32_t adultRoom, int32_t childRoom);
/// The room for adults and children of the kind of a building as a script reads it. The sum of the two counts wraps
/// round as an unsigned number of 32 bits.
[[nodiscard]] float AbodeCapacity(uint32_t adultRoom, uint32_t childRoom);

/// The lowest speed of the things in a container, in metres a turn. Each speed takes the place of the lowest speed
/// only when it is less. An empty container gives the largest float.
[[nodiscard]] float SlowestSpeed(std::span<const float> speeds);

/// A building of a town as a script looks for one
struct TownBuilding
{
	entt::entity entity {entt::null};
	AbodeNumber number {AbodeNumber::Invalid};
	/// How much of it is built, 0 to 1
	float built {1.0f};
	/// A script holds it
	bool inScript {false};
};

/// The building that a script finds in a town, in the order of the town's list (the newest first): the first of the kind
/// given that is built at least as much as given and that no script holds. A script that does not exclude the buildings
/// that scripts hold gets the first building of the list, of any kind. None when no building agrees.
[[nodiscard]] entt::entity BuildingInTown(std::span<const TownBuilding> buildings, AbodeNumber number, float minBuilt,
                                          bool excludingScripted);

/// The object that a villager holds after a script sets it: the lowest byte of the number that the script gives
[[nodiscard]] uint8_t CarriedObjectFromScript(uint32_t value);

} // namespace openblack::ecs::script_containers
