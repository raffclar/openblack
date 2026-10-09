/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>
#include <string_view>

#include <InspectorJson.h>
#include <entt/core/type_info.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/meta/context.hpp>
#include <entt/meta/factory.hpp>

/// The components' fields, registered once with EnTT's reflection so that the inspector can write any registered
/// component out as JSON without knowing its type. Components that aren't registered are still listed by name.
namespace openblack::inspector::reflection
{

/// The name a field was registered under, kept with it as reflection's own data
struct FieldName
{
	std::string name;
};

/// Registers a type's fields by name: Reflect<Transform>(context).Field<&Transform::position>("position")
template <typename Type>
class Reflect
{
public:
	explicit Reflect(entt::meta_ctx& context)
	    : _factory(context)
	{
	}

	template <auto Member>
	Reflect& Field(std::string_view name)
	{
		_factory.template data<Member>(entt::hashed_string::value(name.data(), name.size()));
		_factory.template custom<FieldName>(FieldName {std::string(name)});
		return *this;
	}

private:
	entt::meta_factory<Type> _factory;
};

/// Registers the game's components and the value types their fields hold
void RegisterComponents(entt::meta_ctx& context);

/// A type's name without its namespaces or the compiler's "struct " or "class ": "Transform"
[[nodiscard]] std::string ShortTypeName(const entt::type_info& info);

/// A component of an entity as JSON: its registered fields, or null when it has none registered
[[nodiscard]] Json ComponentToJson(const entt::meta_ctx& context, const entt::type_info& info, const void* component);

/// Whether a type has fields registered
[[nodiscard]] bool IsReflected(const entt::meta_ctx& context, const entt::type_info& info);

} // namespace openblack::inspector::reflection
