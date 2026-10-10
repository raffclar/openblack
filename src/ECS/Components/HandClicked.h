/*******************************************************************************
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

#include "3D/MapCoords.h"

namespace openblack::ecs::components
{

/// What the player last clicked with the hand's Action button, for the scripts to ask about: a thing, and a place on the
/// land. Each is forgotten 15 seconds after it was clicked, or when a script clears it. See hand_click.
struct HandClicked
{
	/// The thing last clicked, none once forgotten
	entt::entity thing {entt::null};
	/// The game turn the thing was clicked on
	uint32_t thingTurn {0};
	/// The place last clicked on the land; forgotten, it is the map's corner
	map_coords::MapCoords place {};
	/// The game turn the place was clicked on
	uint32_t placeTurn {0};
};

} // namespace openblack::ecs::components
