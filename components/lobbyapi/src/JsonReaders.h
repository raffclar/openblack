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

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "LobbyApi.h"
#include "LobbyApiJson.h"

/// The readers of JSON members that the parsers of the lobby's API share. Each reader gives false when a member that the
/// shape needs is missing, or when a member has the wrong type. Nothing here throws.
namespace openblack::lobby_api::json_read
{

using Json = nlohmann::json;

inline std::string Dump(const Json& value)
{
	// Text that is not valid UTF-8 is replaced, because the library must not throw
	return value.dump(-1, ' ', false, Json::error_handler_t::replace);
}

inline std::optional<Json> ParseObject(std::string_view text)
{
	auto value = Json::parse(text.begin(), text.end(), nullptr, false);
	if (value.is_discarded() || !value.is_object())
	{
		return std::nullopt;
	}
	return value;
}

inline const Json* Member(const Json& object, std::string_view key)
{
	const auto found = object.find(key);
	return found != object.end() ? &*found : nullptr;
}

/// Reads a string member. A missing member or null is an error only when the member is required.
inline bool ReadString(const Json& object, std::string_view key, std::string& out, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!member->is_string())
	{
		return false;
	}
	out = member->get_ref<const std::string&>();
	return true;
}

template <typename Integer>
inline bool ReadInteger(const Json& object, std::string_view key, Integer& out, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (member->is_number_unsigned())
	{
		const auto value = member->get<uint64_t>();
		if (value > static_cast<uint64_t>(std::numeric_limits<Integer>::max()))
		{
			return false;
		}
		out = static_cast<Integer>(value);
		return true;
	}
	if (!member->is_number_integer())
	{
		return false;
	}
	const auto value = member->get<int64_t>();
	if (value < static_cast<int64_t>(std::numeric_limits<Integer>::min()) ||
	    value > static_cast<int64_t>(std::numeric_limits<Integer>::max()))
	{
		return false;
	}
	out = static_cast<Integer>(value);
	return true;
}

inline bool ReadNumber(const Json& object, std::string_view key, double& out, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!member->is_number())
	{
		return false;
	}
	out = member->get<double>();
	return true;
}

inline bool ReadBool(const Json& object, std::string_view key, bool& out, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!member->is_boolean())
	{
		return false;
	}
	out = member->get<bool>();
	return true;
}

/// Reads an array of objects, each with a reader. A missing array is empty when it is not required.
template <typename Item, typename Reader>
inline bool ReadArray(const Json& object, std::string_view key, std::vector<Item>& out, Reader&& read, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!member->is_array())
	{
		return false;
	}
	out.clear();
	out.reserve(member->size());
	for (const auto& element : *member)
	{
		if (!element.is_object())
		{
			return false;
		}
		Item item;
		if (!read(element, item))
		{
			return false;
		}
		out.push_back(std::move(item));
	}
	return true;
}

/// Reads an ISO 8601 time member as milliseconds since 1970. Null is 0 when the member is not required.
inline bool ReadTime(const Json& object, std::string_view key, int64_t& out, bool required = true)
{
	std::string text;
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!ReadString(object, key, text))
	{
		return false;
	}
	const auto time = ParseTime(text);
	if (!time.has_value())
	{
		return false;
	}
	out = *time;
	return true;
}

inline bool ReadAccountRef(const Json& object, AccountRef& out)
{
	return ReadInteger(object, "id", out.id) && ReadString(object, "name", out.name);
}

/// Reads a JSON object with a reader of its members
template <typename Shape, typename Reader>
std::optional<Shape> ParseWith(std::string_view text, Reader&& read)
{
	const auto object = ParseObject(text);
	if (!object.has_value())
	{
		return std::nullopt;
	}
	Shape shape;
	if (!read(*object, shape))
	{
		return std::nullopt;
	}
	return shape;
}
} // namespace openblack::lobby_api::json_read
