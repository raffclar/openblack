/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Enums.h"

namespace openblack::ecs::components
{

/// The last player to interact with a villager: the player of the hand that dropped or threw it. Present only while
/// one is known; the drowning death reads it (the neutral player without it).
struct VillagerLastInteraction
{
	PlayerNames player {PlayerNames::NEUTRAL};
};

} // namespace openblack::ecs::components
