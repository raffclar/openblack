/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "Creature/CreatureSwap.h"

namespace openblack::ecs::components
{

/// A creature whose mind has just been swapped with another's: the size and alignment the other body had, which its
/// body takes on little by little while it carries out the swap. It stays until the next swap.
struct CreatureSwap
{
	/// The creature it swapped minds with
	entt::entity other {entt::null};
	/// The shape its mind brought from the other body
	creature_swap::Shape target {};
};

} // namespace openblack::ecs::components
