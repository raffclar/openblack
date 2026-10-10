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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{
class Registry;
}

/// Where a creature's home is
namespace openblack::ecs::creature_home
{

/// A creature's home: the home its leash keeps it at, else its player's temple's place for it; none without either
[[nodiscard]] std::optional<glm::vec3> HomeOf(const Registry& registry, entt::entity creature);

} // namespace openblack::ecs::creature_home
