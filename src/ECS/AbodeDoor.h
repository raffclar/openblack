/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

/// The two places of a town's building that villagers use. The door is where a child goes into the crèche and where a
/// building site puts its wood pile. The arrive place is where a villager stands when it reaches the building: where it
/// hides, and where its footpath walk ends. For most buildings the two are the same: the model's door, or the
/// building's middle when the model has no door. Some kinds differ. Pure, for the villager home code and its tests.
namespace openblack::ecs::abode_door
{

/// How a kind of building gives its door
enum class Door
{
	/// The model's door, or the middle when the model has none
	Model,
	/// Always the building's middle
	Middle,
	/// A fixed point of the village centre: two metres along its x axis and twelve metres back along its z axis, at its
	/// size
	VillageCentre,
};

/// The door of a kind of building
[[nodiscard]] constexpr Door DoorOf(AbodeType type)
{
	switch (type)
	{
	case AbodeType::TownCentre:
		return Door::VillageCentre;
	case AbodeType::Field:
		return Door::Middle;
	default:
		return Door::Model;
	}
}

/// The door of a thing at a site: a field and a fish farm have their middle; a building of the abodes has the door of its
/// kind; any other thing has its model's door, or its middle when the model has none
[[nodiscard]] constexpr Door DoorOfThing(std::optional<AbodeType> abode, bool fieldOrFishFarm)
{
	if (fieldOrFishFarm)
	{
		return Door::Middle;
	}
	return abode.has_value() ? DoorOf(*abode) : Door::Model;
}

/// Whether a villager reaching this kind of building stands at its middle and not at its door
[[nodiscard]] constexpr bool ArrivesAtMiddle(AbodeType type)
{
	switch (type)
	{
	case AbodeType::Creche:
	case AbodeType::Wonder:
	case AbodeType::Graveyard:
	case AbodeType::TownCentre:
	case AbodeType::Field:
		return true;
	default:
		return false;
	}
}

/// The village centre's door in its own space: two metres along its x axis and twelve metres back along its z axis
constexpr float k_VillageCentreDoorSide = 2.0f;
constexpr float k_VillageCentreDoorBack = 12.0f;

/// The village centre's door as a point, for a centre that stands at `position`, turned by `rotation` and sized by
/// `scale`. The sums are made in this order, each one rounded to a float.
[[nodiscard]] inline glm::vec3 VillageCentreDoor(glm::vec3 position, const glm::mat3& rotation, glm::vec3 scale)
{
	const glm::vec3 side = rotation[0] * scale.x;
	const glm::vec3 back = rotation[2] * scale.z;
	const float x = (position.x - back.x * k_VillageCentreDoorBack) + side.x * k_VillageCentreDoorSide;
	const float y = (side.y * k_VillageCentreDoorSide - back.y * k_VillageCentreDoorBack) + position.y;
	const float z = (side.z * k_VillageCentreDoorSide - back.z * k_VillageCentreDoorBack) + position.z;
	return {x, y, z};
}

/// The village centre's door as a map position: x and z change from metres to whole map units, and the altitude is the
/// height of the door point above the land there. `groundAt` gives the height of the land at a map position.
template <typename GroundAt>
[[nodiscard]] constexpr map_coords::MapCoords VillageCentreDoorPlace(glm::vec3 door, const GroundAt& groundAt)
{
	return map_coords::FromWorld(door, groundAt);
}

/// The door of a building as a map position, as a walk keeps it. The door in metres changes to whole map units, and the
/// altitude is 0. A model door that falls on the line x = 0 or z = 0 of the map counts as no door. When the model has no
/// door, the door is the map position of the building, as the map keeps it. The village centre always uses its fixed
/// door, and a building of the kind Middle always uses its map position.
[[nodiscard]] constexpr map_coords::MapCoords DoorPlace(Door door, std::optional<glm::vec2> doorMetres,
                                                        const map_coords::MapCoords& middle)
{
	if (door == Door::Middle || !doorMetres.has_value())
	{
		return middle;
	}
	const map_coords::MapCoords place {.x = map_coords::ToFixed(doorMetres->x), .z = map_coords::ToFixed(doorMetres->y)};
	if (door == Door::Model && (place.x == 0 || place.z == 0))
	{
		return middle;
	}
	return place;
}

} // namespace openblack::ecs::abode_door
