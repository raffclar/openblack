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

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// Simulation-only things of the map script. None of them is drawn when the land loads, and openblack doesn't simulate
/// them yet: they are kept as data. Each is an entity of its own, without a Transform.

/// CREATE_ARENA: a creature fight arena. Its light sheet (grey 180 light on the land) is only drawn while a fight is
/// on.
struct Arena
{
	glm::vec3 position;
	float radius; ///< its radius
};

// CREATE_WEATHER_CLIMATE(_RAIN/_TEMP/_WIND) make the climates of ECS/Weather/Climate (the one copy: id, info, radii,
// rain, temperatures, wind; and the world's climate).

/// CREATE_DRINK_WAYPOINT: an invisible point where the creature goes to drink.
struct DrinkWaypoint
{
	glm::vec3 position;
};

} // namespace openblack::ecs::components
