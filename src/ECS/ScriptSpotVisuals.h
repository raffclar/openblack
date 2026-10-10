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

#include <functional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{
class Registry;
}

/// The things that hold the spot visuals scripts start: each visual a script starts is a thing of its own, so that the
/// script holds an object and not the visual's number
namespace openblack::ecs::script_spot_visuals
{

/// The thing for a visual a script started, where the visual was made
entt::entity MakeThing(Registry& registry, uint32_t effect, glm::vec3 position);

/// The things whose visual has ended go from the world
void RemoveEnded(Registry& registry, const std::function<bool(uint32_t effect)>& running);

} // namespace openblack::ecs::script_spot_visuals
