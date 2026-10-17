/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// Where the player's last leash order aimed the creature. The order to a place or to a thing sets the point two
/// metres over the place, and the thing. The point stays after the order ends, and the saved game keeps it.
struct CreatureTarget
{
	glm::vec3 point {0.0f};
	/// The thing that the order aimed at, none for an order to a place
	std::optional<entt::entity> object;
	/// The order is in force: the creature did not stop what it does since, and the thing is still there
	bool has {false};
};

} // namespace openblack::ecs::components
