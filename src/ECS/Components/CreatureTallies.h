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

#include "Creature/CreatureMeals.h"

namespace openblack::ecs::components
{

/// What a creature counts of its life: what it ate and killed, its poos and the mushrooms it ate. Its fights and wins
/// are in its fight record. Its player's statistics and the creature's scroll in the temple show them.
struct CreatureTallies
{
	creature_meals::Tallies meals;
	uint32_t poos {0};
	/// The magic mushrooms and toadstools that it ate
	uint32_t mushroomsEaten {0};
};

} // namespace openblack::ecs::components
