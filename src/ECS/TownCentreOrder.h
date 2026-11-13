/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>
#include <vector>

#include <entt/entity/entity.hpp>

/// The order in which the game draws the village centres: the newest centre first. The game adds a new centre to the
/// start of its list, and a centre that goes keeps the order of the others.
namespace openblack::ecs::town_centre_order
{

/// Makes the order agree with the centres that are present, given newest first. A centre that is no longer present
/// goes from the order, and the others keep their order. The new centres go to the start of the order, newest first.
void Update(std::vector<entt::entity>& order, std::span<const entt::entity> present);

} // namespace openblack::ecs::town_centre_order
