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
#include <string_view>

#include <InspectorJson.h>
#include <entt/core/type_info.hpp>
#include <entt/meta/context.hpp>

#include "ECS/Reflection/ComponentReflection.h"
#include "Inspector/ComponentJsonCodec.h"

/// The JSON face of the reflection of the components, which only the inspector uses
namespace openblack::ecs::reflection
{

/// A component of an entity as JSON: its registered fields, or null when it has none registered
[[nodiscard]] Json ComponentToJson(const entt::meta_ctx& context, const entt::type_info& info, const void* component);

/// One field of a component as JSON, by a dotted path through nested registered values, what options hold and the
/// elements of lists, by number or by the name of the enumeration indexing them ("position", "crop.age",
/// "desires.desires.Hunger.value", "reaches.0"); none when there is no such field
[[nodiscard]] std::optional<Json> ReadField(const entt::meta_ctx& context, const entt::type_info& info, const void* component,
                                            std::string_view path);
/// The same, with why there is nothing there
[[nodiscard]] std::optional<Json> ReadField(const entt::meta_ctx& context, const entt::type_info& info, const void* component,
                                            std::string_view path, std::string& why);

/// Sets one field of a component, by a dotted path as ReadField takes it, from JSON of the field's type: a value of a
/// type JSON holds, an object of a nested value's fields, or an array of a list's elements. An empty option on the way
/// is made with its default value first. Empty when it was set, else why not.
[[nodiscard]] std::string SetField(const entt::meta_ctx& context, const entt::type_info& info, void* component,
                                   std::string_view path, const Json& value);

} // namespace openblack::ecs::reflection

namespace openblack::inspector
{
namespace reflection = ecs::reflection;
} // namespace openblack::inspector
