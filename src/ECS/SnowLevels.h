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
#include <optional>
#include <span>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs
{
class Registry;
}

/// The snow that each thing on the land shows (components::SnowLevel). When the land draws a thing, the level of the
/// thing moves a small step towards the level of the snow where it stands. A thing that the land does not draw keeps
/// its level. A new thing starts with no snow.
namespace openblack::ecs::snow_levels
{

/// How much snow a thing carries: the rate of its level for the depth of the snow, of 256, and its maximum level
struct Hold
{
	int32_t rate;
	int32_t cap;
};

/// The snow that a thing carries. A creature and a bird carry no snow.
[[nodiscard]] std::optional<Hold> HoldOf(const Registry& registry, entt::entity entity);

/// One step for each of `things` that the land draws: its level moves towards the snow where it stands, by at most
/// `step`. `depthAt` gives the depth of the snow at a point.
void Step(Registry& registry, std::span<const entt::entity> things, const std::function<float(glm::vec2)>& depthAt,
          int32_t step);

/// The snow that a thing shows, 0 to 255
[[nodiscard]] int32_t LevelOf(const Registry& registry, entt::entity entity);

} // namespace openblack::ecs::snow_levels
