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

#include <optional>
#include <vector>

#include <entt/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureMarks.h"

/// Marks left on a creature's skin where a line from a blow or a burn meets its body as it is posed at the end of its
/// newest game turn
namespace openblack::graphics
{
class L3DMesh;
}

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::creature_scars
{

/// The bones of the creature's model in the model's space, as its body is posed at the end of its newest game turn and
/// placed by the placement. Before its first turn pose, the bones that it is drawn with.
[[nodiscard]] std::vector<glm::mat4> TurnBones(const Registry& registry, entt::entity creature, const graphics::L3DMesh& model,
                                               const glm::mat4& placement);

/// Where the creature's groin bone is, as its body is posed at the end of its newest game turn; none for a creature
/// without a posed rig
[[nodiscard]] std::optional<glm::vec3> GroinOf(entt::entity creature);
/// Where the creature's belly bone is, as its body is posed at the end of its newest game turn; none for a creature
/// without a posed rig
[[nodiscard]] std::optional<glm::vec3> BellyOf(entt::entity creature);

/// A wound of a kind and column where the line from a point along a direction first meets the creature's skin;
/// nothing where it misses
void MarkAlong(entt::entity creature, glm::vec3 from, glm::vec3 direction, uint8_t kind, uint8_t column);
/// The wound that MarkAlong gives, with no change to the creature
[[nodiscard]] std::optional<creature_marks::Mark> WoundAlong(entt::entity creature, glm::vec3 from, glm::vec3 direction,
                                                             uint8_t kind, uint8_t column);

/// A creature that catches fire is burnt on its skin in three tries, each from a random point about its groin
void BurnOnCatching(entt::entity creature);

} // namespace openblack::ecs::creature_scars
