/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <tuple>
#include <utility>

#include <entt/core/fwd.hpp>
#include <glm/vec2.hpp>

namespace openblack
{
enum class MeshId : uint32_t;
}

namespace openblack::ecs::components
{
struct Transform;
}

namespace openblack::ecs::archetypes
{
std::pair<glm::vec2, float> GetFixedObstacleBoundingCircle(MeshId meshId, const components::Transform& transform);
/// The same circle for a model by its id; none when the resources do not have the model
[[nodiscard]] std::optional<std::pair<glm::vec2, float>> GetFixedObstacleBoundingCircle(entt::id_type meshId,
                                                                                        const components::Transform& transform);
/// A new thing of the world takes its place in the game's one order of things made, and the count goes up. Without the
/// count, as in tests that do not make it, the place is 0 and nothing is counted.
uint32_t TakeThingMadePlace();
} // namespace openblack::ecs::archetypes
