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
#include <glm/vec2.hpp>

namespace openblack::ecs::components
{

/// The villager another is on its way to make love with, or making love with
struct Lover
{
	entt::entity partner {entt::null};
};

/// Where a breeder disciple walks about drawing villagers of the other sex to it
struct BreederSpot
{
	glm::vec2 spot {0.0f};
};

} // namespace openblack::ecs::components
