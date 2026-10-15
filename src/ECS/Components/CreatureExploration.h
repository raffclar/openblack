/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Creature/CreatureExplorationMap.h"

namespace openblack::ecs::components
{

/// The regions of the land that the creature went into. Each turn that the hand does not hold the creature, its mind
/// adds the region where it stands. Nothing removes a region.
struct CreatureExploration
{
	creature_exploration::Visited visited;
};

} // namespace openblack::ecs::components
