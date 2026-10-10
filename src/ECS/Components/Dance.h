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

#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/DanceRules.h"

namespace openblack::dance
{
struct DanceFile;
}

namespace openblack::ecs::components
{

/// A dance: villagers gathered in groups, each group dancing its part of the dance's file about a place
struct Dance
{
	/// Its row in the dances' table
	uint32_t type {0};
	/// The place it is danced about
	map_coords::MapCoords place;
	/// The thing danced about; the dance ends when it does
	entt::entity centre {entt::null};
	/// Dancing, or waiting to start again
	bool dancing {false};
	/// The turns it dances for before stopping to start again, none for no end
	uint32_t durationTurns {0};
	/// The turn it last started
	uint32_t startTurn {0};
	/// Whether it has waited for dancers yet, and since when
	bool waiting {false};
	uint32_t waitStartTurn {0};
	/// Whether it starts by itself once it has its dancers
	bool autostart {false};
	/// Made by a script, rather than by a town
	bool madeByScript {false};
	/// Its groups, their order of choosing and its dancers
	dance_rules::Groups groups;
	/// Its beat, one a turn, and how many lots of 600 beats it runs before starting again
	float beat {0.0f};
	uint32_t loops {1};
	std::shared_ptr<const dance::DanceFile> file;
};

/// A living dancing in a dance: the dance and its group there; its place in the group is its place in the group's list
struct Dancer
{
	entt::entity dance {entt::null};
	std::size_t group {0};
};

} // namespace openblack::ecs::components
