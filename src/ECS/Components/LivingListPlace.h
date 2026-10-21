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

namespace openblack::ecs::components
{

/// The place of a creature, a villager or an animal in the game's list of the living. Each new living thing goes to the
/// front of the list, and the turn of the living takes the newest first.
struct LivingListPlace
{
	/// How many living things the game made before this one
	uint64_t made {0};
};

} // namespace openblack::ecs::components
