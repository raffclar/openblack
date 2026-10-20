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
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

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

/// The village centre's door on the ground (x, z), for a centre that stands at `position`, turned by `rotation` and
/// sized by `scale`. The sums are made in this order, each one rounded to a float.
[[nodiscard]] inline glm::vec2 VillageCentreDoor(glm::vec3 position, const glm::mat3& rotation, glm::vec3 scale)
{
	const glm::vec3 side = rotation[0] * scale.x;
	const glm::vec3 back = rotation[2] * scale.z;
	const float x = (position.x - back.x * k_VillageCentreDoorBack) + side.x * k_VillageCentreDoorSide;
	const float z = (side.z * k_VillageCentreDoorSide - back.z * k_VillageCentreDoorBack) + position.z;
	return {x, z};
}

} // namespace openblack::ecs::abode_door
