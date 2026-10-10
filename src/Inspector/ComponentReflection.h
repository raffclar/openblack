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

#include <algorithm>
#include <array>
#include <bitset>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <InspectorJson.h>
#include <entt/core/type_info.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/entity/registry.hpp>
#include <entt/meta/container.hpp>
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
inline constexpr int k_DeepestNesting = 4;
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

template <typename>
inline constexpr bool k_IsBitset = false;
template <size_t Size>
inline constexpr bool k_IsBitset<std::bitset<Size>> = true;

template <typename>
inline constexpr bool k_IsStdArray = false;
template <typename Type, size_t Size>
inline constexpr bool k_IsStdArray<std::array<Type, Size>> = true;

/// A fixed list's size, 0 for any other type
template <typename Type>
inline constexpr size_t k_FixedSize = 0;
template <typename Type, size_t Size>
inline constexpr size_t k_FixedSize<std::array<Type, Size>> = Size;

/// Whether a value of a type is read whole from JSON: the types JSON holds directly, and options and lists of them
template <typename Type>
[[nodiscard]] constexpr bool Decodable()
{
	if constexpr (std::is_same_v<Type, bool> || std::is_arithmetic_v<Type> || std::is_enum_v<Type> ||
	              std::is_same_v<Type, std::string> || std::is_same_v<Type, entt::entity> || k_IsGlmVector<Type> ||
	              k_IsGlmMatrix<Type> || k_IsBitset<Type>)
	{
		return true;
	}
	else if constexpr (k_IsOptional<Type> || k_IsList<Type>)
	{
		return Decodable<typename Type::value_type>();
	}
	else
	{
		return false;
	}
}

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

/// A number or a truth written as text ("6.0", "3", "true"), as some tools send a value they don't know the type of;
/// none for any other text, which stays text
[[nodiscard]] std::optional<Json> ScalarFromText(const Json& value);

/// Text of 16-bit units (a name from a creature's file) as UTF-8
[[nodiscard]] std::string Utf8(std::u16string_view text);

/// A float as the shortest number that reads back as it ("0.1", not its double's "0.10000000149011612"), so that answers
/// stay small
[[nodiscard]] double Shortest(float value);

} // namespace detail

/// A value as JSON when its type is one JSON holds directly, or a list or option of such; none for anything else, which
/// is written through its registered fields instead
template <typename Type>
[[nodiscard]] std::optional<Json> Encode(const Type& value)
{
	if constexpr (std::is_same_v<Type, float>)
	{
		return Json(detail::Shortest(value));
	}
	else if constexpr (std::is_same_v<Type, bool> || std::is_arithmetic_v<Type> || std::is_same_v<Type, std::string>)
	{
		return Json(value);
	}
	else if constexpr (std::is_enum_v<Type>)
	{
		// An enumeration is written as its number
		return Json(static_cast<int64_t>(value));
	}
	else if constexpr (std::is_same_v<Type, std::u16string>)
	{
		return Json(detail::Utf8(value));
	}
	else if constexpr (std::is_same_v<Type, entt::entity>)
	{
		return value == entt::null ? Json(nullptr) : Json(entt::to_integral(value));
	}
	else if constexpr (detail::k_IsBitset<Type>)
	{
		// Its bits from the first, as truths
		Json array = Json::array();
		for (size_t i = 0; i < value.size(); ++i)
		{
			array.push_back(value.test(i));
		}
		return array;
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
	// A field taking a number or a truth takes one written as text too, read as JSON; never another field's type
	if constexpr (std::is_same_v<Type, bool> || std::is_arithmetic_v<Type> || std::is_enum_v<Type>)
	{
		if (json.is_string())
		{
			if (const auto scalar = detail::ScalarFromText(json); scalar.has_value())
			{
				return Decode<Type>(*scalar, error);
			}
		}
	}
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
		// Every failure falls through to the last return, which would otherwise be unreachable code for these types
		if (json.is_array() && json.size() == static_cast<size_t>(Type::length()))
		{
			Type value {};
			bool decoded = true;
			for (glm::length_t i = 0; decoded && i < Type::length(); ++i)
			{
				auto element = Decode<std::remove_cvref_t<decltype(value[i])>>(json[static_cast<size_t>(i)], error);
				decoded = element.has_value();
				if (decoded)
				{
					value[i] = *element;
				}
			}
			if (decoded)
			{
				return value;
			}
		}
		else
		{
			error = "needs an array of " + std::to_string(Type::length()) + ", not " + detail::Describe(json);
		}
	}
	else if constexpr (detail::k_IsOptional<Type>)
	{
		if (json.is_null())
		{
			return Type {};
		}
		if (auto inner = Decode<typename Type::value_type>(json, error); inner.has_value())
		{
			return Type {*std::move(inner)};
		}
	}
	else if constexpr (detail::k_IsBitset<Type>)
	{
		Type bits;
		const bool fits = json.is_array() && json.size() == bits.size() &&
		                  std::ranges::all_of(json, [](const Json& bit) { return bit.is_boolean(); });
		if (fits)
		{
			for (size_t i = 0; i < bits.size(); ++i)
			{
				bits.set(i, json[i].get<bool>());
			}
			return bits;
		}
		error = "needs an array of " + std::to_string(bits.size()) + " truths, not " + detail::Describe(json);
	}
	else if constexpr (detail::k_IsList<Type> && detail::Decodable<Type>())
	{
		// A list from an array of its elements: a fixed one of its own size, any other of any size
		const auto size = detail::k_IsStdArray<Type> ? std::optional(detail::k_FixedSize<Type>) : std::nullopt;
		if (!json.is_array())
		{
			error = "needs an array, not " + detail::Describe(json);
		}
		else if (size.has_value() && json.size() != *size)
		{
			error = "needs an array of " + std::to_string(*size) + ", not " + detail::Describe(json);
		}
		else
		{
			Type list {};
			bool decoded = true;
			for (size_t i = 0; decoded && i < json.size(); ++i)
			{
				auto element = Decode<typename Type::value_type>(json[i], error);
				decoded = element.has_value();
				if (!decoded)
				{
					error = "element " + std::to_string(i) + " " + error;
				}
				else if constexpr (detail::k_IsStdArray<Type>)
				{
					list[i] = *std::move(element);
				}
				else
				{
					list.push_back(*std::move(element));
				}
			}
			if (decoded)
			{
				return list;
			}
		}
	}
	else if constexpr (detail::k_IsList<Type>)
	{
		error = "can't be set whole: set its elements one by one (\"field.2\")";
	}
	else
	{
		error = "can't be set whole: set its fields one by one";
	}
	return std::nullopt;
}

/// Whether a value of a type that holds entities (an entity, an optional one, or a list of them) holds this one
template <typename Type>
[[nodiscard]] constexpr bool HoldsEntities()
{
	if constexpr (std::is_same_v<Type, entt::entity>)
	{
		return true;
	}
	else if constexpr (detail::k_IsOptional<Type> || detail::k_IsList<Type>)
	{
		return std::is_same_v<typename Type::value_type, entt::entity>;
	}
	else
	{
		return false;
	}
}

template <typename Type>
[[nodiscard]] bool RefersTo(const Type& value, entt::entity target)
{
	if constexpr (std::is_same_v<Type, entt::entity>)
	{
		return value == target;
	}
	else if constexpr (detail::k_IsOptional<Type>)
	{
		return value == target;
	}
	else
	{
		return std::find(value.begin(), value.end(), target) != value.end();
	}
}

/// The test of whether a field's value holds an entity, for the types that hold entities; none for the rest
template <typename Type>
[[nodiscard]] constexpr auto RefersToFunction() -> bool (*)(const entt::meta_any&, entt::entity)
{
	if constexpr (HoldsEntities<Type>())
	{
		return [](const entt::meta_any& any, entt::entity target) {
			const auto* value = any.try_cast<const Type>();
			return value != nullptr && RefersTo(*value, target);
		};
	}
	else
	{
		return nullptr;
	}
}

/// What is kept with each registered field: its name, and how its value is written out and read in
struct FieldInfo
{
	std::string name;
	/// For a field holding entities, whether it holds this one; none for any other field
	bool (*refersTo)(const entt::meta_any& value, entt::entity target) {nullptr};
	/// The value as JSON, none when the generic writing through fields must do
	std::optional<Json> (*encode)(const entt::meta_any& value) {nullptr};
	/// A JSON value as a value of the field's type, none (with why not) when it doesn't fit
	std::optional<entt::meta_any> (*decode)(const entt::meta_ctx& context, const Json& value, std::string& error) {nullptr};
	/// For a list indexed by an enumeration, its elements' names, one list for each level of lists within lists (empty
	/// for a level indexed by number): written as an object by those names, and set by name or by number
	std::vector<std::span<const std::string_view>> indexNames;
};

/// What is kept with a type of value that fields and lists hold (not a component, nor a value with fields of its own):
/// how a value of it is read from JSON to set an element of a list, and for an option, how what it holds is reached
struct ValueInfo
{
	/// A JSON value as a value of the type, none (with why not) when it doesn't fit; none for a type set only through
	/// its fields or elements
	std::optional<entt::meta_any> (*decode)(const entt::meta_ctx& context, const Json& value, std::string& error) {nullptr};
	/// For an option, what it holds by reference, or an empty value when it holds nothing
	entt::meta_any (*unwrap)(entt::meta_any& option) {nullptr};
	/// For an option, what it holds by reference, made with its default value first when it holds nothing; empty when
	/// it can't be made
	entt::meta_any (*engage)(entt::meta_any& option) {nullptr};
};

namespace detail
{

template <typename Type>
[[nodiscard]] std::optional<entt::meta_any> DecodeAny(const entt::meta_ctx& context, const Json& json, std::string& error)
{
	if constexpr (std::is_copy_constructible_v<Type>)
	{
		auto value = Decode<Type>(json, error);
		if (value.has_value())
		{
			return entt::meta_any {context, std::in_place_type<Type>, *std::move(value)};
		}
	}
	else
	{
		error = "can't be set: its values can't be copied";
	}
	return std::nullopt;
}

/// Registers how values of a type held in fields and lists are read and reached, and those of the types it holds
template <typename Type>
void RegisterValue(entt::meta_ctx& context)
{
	if constexpr (k_IsOptional<Type>)
	{
		using Held = typename Type::value_type;
		entt::meta_factory<Type> {context}.template custom<ValueInfo>(ValueInfo {
		    .decode = Decodable<Type>() ? &DecodeAny<Type> : nullptr,
		    .unwrap = [](entt::meta_any& option) -> entt::meta_any {
			    if (auto* value = option.try_cast<Type>(); value != nullptr && value->has_value())
			    {
				    return entt::meta_any {option.context(), std::in_place_type<Held&>, **value};
			    }
			    if (const auto* value = option.try_cast<const Type>(); value != nullptr && value->has_value())
			    {
				    return entt::meta_any {option.context(), std::in_place_type<const Held&>, **value};
			    }
			    return {};
		    },
		    .engage = [](entt::meta_any& option) -> entt::meta_any {
			    auto* value = option.try_cast<Type>();
			    if (value == nullptr)
			    {
				    return {};
			    }
			    if constexpr (std::is_default_constructible_v<Held>)
			    {
				    if (!value->has_value())
				    {
					    value->emplace();
				    }
			    }
			    return value->has_value() ? entt::meta_any {option.context(), std::in_place_type<Held&>, **value}
			                              : entt::meta_any {};
		    },
		});
		RegisterValue<Held>(context);
	}
	else if constexpr (k_IsList<Type>)
	{
		if constexpr (Decodable<Type>())
		{
			entt::meta_factory<Type> {context}.template custom<ValueInfo>(ValueInfo {.decode = &DecodeAny<Type>});
		}
		RegisterValue<typename Type::value_type>(context);
	}
	else if constexpr (Decodable<Type>())
	{
		entt::meta_factory<Type> {context}.template custom<ValueInfo>(ValueInfo {.decode = &DecodeAny<Type>});
	}
}

} // namespace detail

/// Registers a value type only for its fields, not as a component that can be added to an entity:
/// Reflect<Needs>(context, ValueOnly {})
struct ValueOnly
{
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
	    : _context(&context)
	    , _factory(context)
	{
		_factory.template custom<ComponentInfo>(ComponentInfo {
		    .storage = [](entt::registry& registry) -> entt::sparse_set& { return registry.storage<Type>(); },
		});
	}

	Reflect(entt::meta_ctx& context, ValueOnly /*value*/)
	    : _context(&context)
	    , _factory(context)
	{
	}

	template <auto Member>
	Reflect& Field(std::string_view name)
	{
		return Field<Member>(name, {});
	}

	/// A field that is a list indexed by enumerations, with their names for each level of lists within lists (an empty
	/// list of names for a level indexed by number)
	template <auto Member>
	Reflect& Field(std::string_view name, std::vector<std::span<const std::string_view>> indexNames)
	{
		using Value = std::remove_cvref_t<decltype(std::declval<Type&>().*Member)>;
		// Fields are reached by reference, so that values that can't be copied are read too, and nested values are set
		// where they are
		_factory.template data<Member, entt::as_ref_t>(entt::hashed_string::value(name.data(), name.size()));
		_factory.template custom<FieldInfo>(FieldInfo {
		    .name = std::string(name),
		    .refersTo = RefersToFunction<Value>(),
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
		    .indexNames = std::move(indexNames),
		});
		detail::RegisterValue<Value>(*_context);
		return *this;
	}

	/// A value reached through functions, for a type whose data is private: a getter giving it from a const reference,
	/// and a setter taking it, or nullptr for one that is only read. Set, it is read, changed and set back whole.
	template <auto Getter, auto Setter>
	Reflect& Property(std::string_view name)
	{
		using Value = std::remove_cvref_t<std::invoke_result_t<decltype(Getter), const Type&>>;
		_factory.template data<Setter, Getter>(entt::hashed_string::value(name.data(), name.size()));
		_factory.template custom<FieldInfo>(FieldInfo {
		    .name = std::string(name),
		    .refersTo = RefersToFunction<Value>(),
		    .encode = [](const entt::meta_any& any) -> std::optional<Json> {
			    if (const auto* value = any.try_cast<const Value>(); value != nullptr)
			    {
				    return Encode(*value);
			    }
			    return std::nullopt;
		    },
		    .decode = &detail::DecodeAny<Value>,
		});
		detail::RegisterValue<Value>(*_context);
		return *this;
	}

private:
	entt::meta_ctx* _context;
	entt::meta_factory<Type> _factory;
};

/// Registers the game's components and their fields
void RegisterComponents(entt::meta_ctx& context);
/// The generated registration of every component and of the values their fields hold (see
/// tools/inspector/generate_component_fields.py)
void RegisterComponentFields(entt::meta_ctx& context);
/// The registration of values whose data is private, which the generator leaves to be written by hand
void RegisterHandWrittenFields(entt::meta_ctx& context);

/// A type's name without its namespaces or the compiler's "struct " or "class ": "Transform"
[[nodiscard]] std::string ShortTypeName(const entt::type_info& info);

/// The type of the components a storage holds. EnTT 3.16 names it info() and deprecates type(); 3.15 has only type()
template <typename Storage>
[[nodiscard]] const entt::type_info& StorageType(const Storage& storage)
{
	if constexpr (requires { storage.info(); })
	{
		return storage.info();
	}
	else
	{
		return storage.type();
	}
}

/// A component of an entity as JSON: its registered fields, or null when it has none registered
[[nodiscard]] Json ComponentToJson(const entt::meta_ctx& context, const entt::type_info& info, const void* component);

/// The dotted paths of a component's registered fields that hold an entity, through nested registered values and lists
/// of them: ["opponent"], ["moves.2.target"]
[[nodiscard]] std::vector<std::string> References(const entt::meta_ctx& context, const entt::type_info& info,
                                                  const void* component, entt::entity target);

/// Whether a type has fields registered
[[nodiscard]] bool IsReflected(const entt::meta_ctx& context, const entt::type_info& info);

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

/// The storage of a component by its short name: the one the registry has, else one made for a registered component;
/// none when there is no component of that name
[[nodiscard]] entt::sparse_set* FindStorage(entt::registry& registry, const entt::meta_ctx& context, std::string_view name);
[[nodiscard]] const entt::sparse_set* FindStorage(const entt::registry& registry, std::string_view name);

} // namespace openblack::inspector::reflection
