/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/VillagerDeathRules.h"

namespace openblack::ecs::components
{

/// What a town keeps of its people's deaths: when the latest was, how many, by whom and of what. The scripts ask how many
/// died of chanting at worship.
struct TownDeaths
{
	villager_death::DeathCounts counts;
};

} // namespace openblack::ecs::components
