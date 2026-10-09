/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// The creatures a player has had, kept on the player in the order they got them. The first still theirs is their
/// primary creature: the one the story, scripts, the leash, fights, the advisors and the Creature Cave are about.
struct PlayerCreatures
{
	std::vector<entt::entity> acquired;
};

} // namespace openblack::ecs::components
