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
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <InspectorJson.h>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Common/GameHeading.h"
#include "ECS/Reflection/ReflectionTraits.h"

/// The JSON of the reflection, for the inspector: how a value of a field is written as JSON and read from JSON
namespace openblack::ecs::reflection
{

using Json = inspector::Json;

/// Lists in JSON have only this many of their first elements
inline constexpr size_t k_MostElements = 16;

namespace detail
{

/// Whether a value of a type is read whole from JSON: the types JSON holds directly, and options and lists of them
template <typename Type>
[[nodiscard]] constexpr bool Decodable()
{
	if constexpr (std::is_same_v<Type, bool> || std::is_arithmetic_v<Type> || std::is_enum_v<Type> ||
	              std::is_same_v<Type, GameHeading> || std::is_same_v<Type, std::string> ||
	              std::is_same_v<Type, entt::entity> || k_IsGlmVector<Type> || k_IsGlmMatrix<Type> || k_IsBitset<Type>)
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
	else if constexpr (std::is_same_v<Type, GameHeading>)
	{
		// A heading is written as its radians
		return Json(detail::Shortest(value.Radians()));
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
	else if constexpr (requires {
		                   value.floatVal;
		                   value.intVal;
	                   })
	{
		// A script value holds a float or an integer in the same bits. The value does not tell which, so both are written.
		return Json {{"float", detail::Shortest(value.floatVal)}, {"int", value.intVal}};
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
	else if constexpr (std::is_same_v<Type, GameHeading>)
	{
		// A heading is read as its radians
		if (const auto radians = Decode<float>(json, error); radians.has_value())
		{
			return GameHeading(*radians);
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

} // namespace openblack::ecs::reflection
