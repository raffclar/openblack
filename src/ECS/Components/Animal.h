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

#include "Enums.h"

namespace openblack::ecs::components
{

/// An animal of the scripts (CREATE_ANIMAL / CREATE_NEW_ANIMAL). Only the ground animals exist so far; they stand
/// still (no animal AI yet). `humanShadowed`: drawn with the ground blob shadows.
struct Animal
{
	AnimalInfo type;
	uint32_t age;
	entt::entity flock {entt::null}; ///< Its components::Flock
	bool humanShadowed {true};
	/// The town (also on the town's list): only the animals that can be shepherded (sheep, goat, tortoise, zebra, cow,
	/// horse, pig) keep the script's town
	entt::entity town {entt::null};
	/// The player it belongs to (the spells' animals), -1 none
	int32_t player {-1};
	/// Its place in its flock's member list (0 at creation; set when the leader is added)
	uint8_t flockOrder {0};
};

} // namespace openblack::ecs::components
