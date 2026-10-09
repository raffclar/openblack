/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <InspectorJson.h>
#include <entt/core/type_info.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/policy.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// The components' fields, registered once with EnTT's reflection so that the inspector can write any registered
/// component out as JSON, set its fields by name with their types checked, and add it to an entity, all without knowing
/// its type. Components that aren't registered are still listed by name.
namespace openblack::inspector::reflection
{

/// Nested values are written this deep, and lists only this many of their first elements
inline constexpr int k_DeepestNesting = 3;
inline constexpr size_t k_MostElements = 16;

namespace detail
{

template <typename>
inline constexpr bool k_IsOptional = false;
template <typename Type>
inline constexpr bool k_IsOptional<std::optional<Type>> = true;

template <typename>
inline constexpr bool k_IsList = false;
template <typename Type, typename Allocator>
inline constexpr bool k_IsList<std::vector<Type, Allocator>> = true;
template <typename Type, size_t Size>
inline constexpr bool k_IsList<std::array<Type, Size>> = true;

template <typename>
inline constexpr bool k_IsGlmVector = false;
template <glm::length_t Length, typename Type, glm::qualifier Qualifier>
inline constexpr bool k_IsGlmVector<glm::vec<Length, Type, Qualifier>> = true;

template <typename>
inline constexpr bool k_IsGlmMatrix = false;
template <glm::length_t Columns, glm::length_t Rows, typename Type, glm::qualifier Qualifier>
inline constexpr bool k_IsGlmMatrix<glm::mat<Columns, Rows, Type, Qualifier>> = true;

/// The integer type a range is checked as: character types (which the comparisons refuse) as the integers they are
template <typename Type>
using Comparable = std::conditional_t<
    std::is_same_v<Type, char>, std::conditional_t<std::is_signed_v<char>, signed char, unsigned char>,
    std::conditional_t<std::is_same_v<Type, wchar_t> || std::is_same_v<Type, char16_t> || std::is_same_v<Type, char32_t> ||
                           std::is_same_v<Type, char8_t>,
                       std::make_unsigned_t<std::conditional_t<sizeof(Type) == 1, int8_t,
                                                               std::conditional_t<sizeof(Type) == 2, int16_t, int32_t>>>,
                       std::conditional_t<std::is_same_v<Type, bool>, uint8_t, Type>>>;

/// What a JSON value is, for saying why it doesn't fit: "a string", "an array of 2"
[[nodiscard]] std::string Describe(const Json& value);

} // namespace detail

/// A value as JSON when its type is one JSON holds directly, or a list or option of such; none for anything else, which
/// is written through its registered fields instead
template <typename Type>
[[nodiscard]] std::optional<Json> Encode(const Type& value)
{
	if constexpr (std::is_same_v<Type, bool> || std::is_arithmetic_v<Type> || std::is_same_v<Type, std::string>)
	{
		return Json(value);
	}
	else if constexpr (std::is_enum_v<Type>)
	{
		// An enumeration is written as its number
		return Json(static_cast<int64_t>(value));
	}
	else if constexpr (std::is_same_v<Type, entt::entity>)
	{
		return value == entt::null ? Json(nullptr) : Json(entt::to_integral(value));
	}
	else if constexpr (detail::k_IsGlmVector<Type> || detail::k_IsGlmMatrix<Type>)
	{
		Json array = Json::array();
		for (glm::length_t i = 0; i < Type::length(); ++i)
		{
			array.push_back(*Encode(value[i]));
		}
		return array;
	}
	else if constexpr (detail::k_IsOptional<Type>)
	{
		if (!value.has_value())
		{
			return Json(nullptr);
		}
		return Encode(*value);
	}
	else if constexpr (detail::k_IsList<Type>)
	{
		Json array = Json::array();
		for (const auto& element : value)
		{
			if (array.size() == k_MostElements)
			{
				break;
			}
			auto encoded = Encode(element);
			if (!encoded.has_value())
			{
				return std::nullopt;
			}
			array.push_back(*std::move(encoded));
		}
		if (value.size() <= k_MostElements)
		{
			return array;
		}
		return Json {{"size", value.size()}, {"first", std::move(array)}};
	}
	else
	{
		return std::nullopt;
	}
}

/// A value of a type read from JSON, with the JSON's type and range checked; none, with why not, when it doesn't fit.
/// Only the types Encode writes directly are read: anything else is set through its fields one by one.
template <typename Type>
[[nodiscard]] std::optional<Type> Decode(const Json& json, std::string& error)
{
	if constexpr (std::is_same_v<Type, bool>)
	{
		if (json.is_boolean())
		{
			return json.get<bool>();
		}
		error = "needs a boolean, not " + detail::Describe(json);
	}
	else if constexpr (std::is_integral_v<Type> || std::is_enum_v<Type>)
	{
		using Underlying = detail::Comparable<
		    typename std::conditional_t<std::is_enum_v<Type>, std::underlying_type<Type>, std::type_identity<Type>>::type>;
		if (json.is_number_unsigned())
		{
			const auto number = json.get<uint64_t>();
			if (std::in_range<Underlying>(number))
			{
				return static_cast<Type>(static_cast<Underlying>(number));
			}
			error = "the number is out of the field's range";
		}
		else if (json.is_number_integer())
		{
			const auto number = json.get<int64_t>();
			if (std::in_range<Underlying>(number))
			{
				return static_cast<Type>(static_cast<Underlying>(number));
			}
			error = "the number is out of the field's range";
		}
		else
		{
			error = "needs a whole number, not " + detail::Describe(json);
		}
	}
	else if constexpr (std::is_floating_point_v<Type>)
	{
		if (json.is_number())
		{
			return static_cast<Type>(json.get<double>());
		}
		error = "needs a number, not " + detail::Describe(json);
	}
	else if constexpr (std::is_same_v<Type, std::string>)
	{
		if (json.is_string())
		{
			return json.get<std::string>();
		}
		error = "needs a string, not " + detail::Describe(json);
	}
	else if constexpr (std::is_same_v<Type, entt::entity>)
	{
		if (json.is_null())
		{
			return entt::entity {entt::null};
		}
		if (json.is_number_unsigned() && std::in_range<uint32_t>(json.get<uint64_t>()))
		{
			return static_cast<entt::entity>(json.get<uint32_t>());
		}
		error = "needs an entity's id or null, not " + detail::Describe(json);
	}
	else if constexpr (detail::k_IsGlmVector<Type> || detail::k_IsGlmMatrix<Type>)
	{
		if (!json.is_array() || json.size() != static_cast<size_t>(Type::length()))
		{
			error = "needs an array of " + std::to_string(Type::length()) + ", not " + detail::Describe(json);
			return std::nullopt;
		}
		Type value {};
		for (glm::length_t i = 0; i < Type::length(); ++i)
		{
			auto element = Decode<std::remove_cvref_t<decltype(value[i])>>(json[static_cast<size_t>(i)], error);
			if (!element.has_value())
			{
				return std::nullopt;
			}
			value[i] = *element;
		}
		return value;
	}
	else if constexpr (detail::k_IsOptional<Type>)
	{
		if (json.is_null())
		{
			return Type {};
		}
		auto inner = Decode<typename Type::value_type>(json, error);
		if (!inner.has_value())
		{
			return std::nullopt;
		}
		return Type {*std::move(inner)};
	}
	else
	{
		error = "can't be set whole: set its fields one by one";
	}
	return std::nullopt;
}

/// What is kept with each registered field: its name, and how its value is written out and read in
struct FieldInfo
{
	std::string name;
	/// The value as JSON, none when the generic writing through fields must do
	std::optional<Json> (*encode)(const entt::meta_any& value) {nullptr};
	/// A JSON value as a value of the field's type, none (with why not) when it doesn't fit
	std::optional<entt::meta_any> (*decode)(const entt::meta_ctx& context, const Json& value, std::string& error) {nullptr};
};

/// What is kept with each registered component: its storage in a registry, made if there is none yet
struct ComponentInfo
{
	entt::sparse_set& (*storage)(entt::registry& registry) {nullptr};
};

/// Registers a component and its fields by name: Reflect<Transform>(context).Field<&Transform::position>("position")
template <typename Type>
class Reflect
{
public:
	explicit Reflect(entt::meta_ctx& context)
	    : _factory(context)
	{
		_factory.template custom<ComponentInfo>(ComponentInfo {
		    .storage = [](entt::registry& registry) -> entt::sparse_set& { return registry.storage<Type>(); },
		});
	}

	template <auto Member>
	Reflect& Field(std::string_view name)
	{
		using Value = std::remove_cvref_t<decltype(std::declval<Type&>().*Member)>;
		// Fields are reached by reference, so that values that can't be copied are read too, and nested values are set
		// where they are
		_factory.template data<Member, entt::as_ref_t>(entt::hashed_string::value(name.data(), name.size()));
		_factory.template custom<FieldInfo>(FieldInfo {
		    .name = std::string(name),
		    .encode = [](const entt::meta_any& any) -> std::optional<Json> {
			    if (const auto* value = any.try_cast<const Value>(); value != nullptr)
			    {
				    return Encode(*value);
			    }
			    return std::nullopt;
		    },
		    .decode = [](const entt::meta_ctx& context, const Json& json, std::string& error) -> std::optional<entt::meta_any> {
			    if constexpr (std::is_copy_constructible_v<Value>)
			    {
				    auto value = Decode<Value>(json, error);
				    if (value.has_value())
				    {
					    return entt::meta_any {context, std::in_place_type<Value>, *std::move(value)};
				    }
			    }
			    else
			    {
				    error = "can't be set: its values can't be copied";
			    }
			    return std::nullopt;
		    },
		});
		return *this;
	}

private:
	entt::meta_factory<Type> _factory;
};

/// Registers the game's components and their fields
void RegisterComponents(entt::meta_ctx& context);
/// The generated registration of every component (see tools/inspector/generate_component_fields.py)
void RegisterComponentFields(entt::meta_ctx& context);

/// A type's name without its namespaces or the compiler's "struct " or "class ": "Transform"
[[nodiscard]] std::string ShortTypeName(const entt::type_info& info);

/// A component of an entity as JSON: its registered fields, or null when it has none registered
[[nodiscard]] Json ComponentToJson(const entt::meta_ctx& context, const entt::type_info& info, const void* component);

/// Whether a type has fields registered
[[nodiscard]] bool IsReflected(const entt::meta_ctx& context, const entt::type_info& info);

/// One field of a component as JSON, by a dotted path through nested registered values ("position", "crop.age"); none
/// when there is no such field
[[nodiscard]] std::optional<Json> ReadField(const entt::meta_ctx& context, const entt::type_info& info, const void* component,
                                            std::string_view path);

/// Sets one field of a component, by a dotted path, from JSON of the field's type: a value of a type JSON holds, or an
/// object of a nested value's fields. Empty when it was set, else why not.
[[nodiscard]] std::string SetField(const entt::meta_ctx& context, const entt::type_info& info, void* component,
                                   std::string_view path, const Json& value);

/// The storage of a component by its short name: the one the registry has, else one made for a registered component;
/// none when there is no component of that name
[[nodiscard]] entt::sparse_set* FindStorage(entt::registry& registry, const entt::meta_ctx& context, std::string_view name);
[[nodiscard]] const entt::sparse_set* FindStorage(const entt::registry& registry, std::string_view name);

} // namespace openblack::inspector::reflection
