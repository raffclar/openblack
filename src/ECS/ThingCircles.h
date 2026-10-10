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

#include <entt/entity/fwd.hpp>

#include "ECS/WallHugRules.h"

namespace openblack::ecs
{
class Registry;
}

/// The circles a thing standing fixed on the land covers on the ground. Walkers go round them, and an arena looking for
/// room keeps clear of them.
namespace openblack::ecs::thing_circles
{

/// How a thing on the map stands in the way, from what it is
[[nodiscard]] wall_hug::ThingShape ShapeOf(const Registry& registry, entt::entity thing);

/// The circles a thing on the map covers, in the order the walkers come across them (see WallHugRules.h)
[[nodiscard]] std::vector<wall_hug::BlockingCircle> CirclesOf(const Registry& registry, entt::entity thing);

} // namespace openblack::ecs::thing_circles
