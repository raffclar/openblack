/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/StatisticsRules.h"

namespace openblack::ecs::components
{

/// What a creature counts of its life: the people, animals and creatures it ate or killed, the fights it fought and
/// won, its poos and the mushrooms it ate. Its player's statistics and the creature's scroll in the temple show them.
using CreatureTallies = statistics::CreatureTallies;

} // namespace openblack::ecs::components
