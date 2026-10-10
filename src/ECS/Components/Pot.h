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

struct Pot
{
	uint32_t amount;
	uint32_t maxAmount;
	/// What kind of pot or pile it is, which says what it holds
	PotInfo type {PotInfo::FoodPot};
	/// Its food is poisoned: once poisoned, what is added keeps it so, until it is emptied and goes
	bool poisoned {false};
	/// The town it belongs to, none for none
	entt::entity town {entt::null};
};

} // namespace openblack::ecs::components
