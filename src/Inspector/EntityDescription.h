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
#include <string>
#include <vector>

#include <InspectorJson.h>
#include <entt/entity/entity.hpp>
#include <entt/meta/context.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;
} // namespace openblack

namespace openblack::ecs
{
class Registry;
}

/// What the inspector says of an entity in one line: its kind (what it is in the game, such as Tree or Villager), a
/// label with the kind and its type, and where it is
namespace openblack::inspector
{

struct EntityDescription
{
	std::string kind;
	std::string label;
	std::optional<glm::vec3> position;
};

/// An entity's id as the inspector gives it, and the entity from such an id (none for anything else)
[[nodiscard]] uint32_t ToId(entt::entity entity);
[[nodiscard]] std::optional<entt::entity> FromId(const Json& id);

/// Describes an entity; the info tables, when given, name its type ("Tree: BEECH" rather than "Tree 3")
[[nodiscard]] EntityDescription Describe(const ecs::Registry& registry, entt::entity entity, const InfoConstants* info);

/// An entity in a list: its id, kind, label and position
[[nodiscard]] Json ToListItem(entt::entity entity, const EntityDescription& description);

/// The names of the components an entity has, in the registry's order
[[nodiscard]] std::vector<std::string> ComponentNames(const ecs::Registry& registry, entt::entity entity);

/// An entity's components as JSON by their names: the ones asked for, or all of them; components with no fields
/// registered are null
[[nodiscard]] Json ComponentsToJson(const ecs::Registry& registry, const entt::meta_ctx& context, entt::entity entity,
                                    const std::vector<std::string>& names);

} // namespace openblack::inspector
