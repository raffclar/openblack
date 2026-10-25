/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// The miracle seed in the left hand of a player. Each player of a network game has a hand, and each hand holds its
/// own seed. The turns change it, the same on every computer.
struct HandHeldSeed
{
	/// The seed in the hand, or none
	entt::entity seed {entt::null};
};

} // namespace openblack::ecs::components
