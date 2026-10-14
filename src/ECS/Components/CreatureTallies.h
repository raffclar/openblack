/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Creature/CreatureMeals.h"

namespace openblack::ecs::components
{

/// On a creature after its first meal: the counts of what it ate
struct CreatureTallies
{
	creature_meals::Tallies meals;
};

} // namespace openblack::ecs::components
