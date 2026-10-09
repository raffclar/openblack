/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InspectorJson.h"

#include <charconv>

using namespace openblack::inspector;

std::string openblack::inspector::Dump(const Json& value)
{
	return value.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::optional<Json> openblack::inspector::Parse(std::string_view text)
{
	auto value = Json::parse(text.begin(), text.end(), nullptr, false);
	if (value.is_discarded())
	{
		return std::nullopt;
	}
	return value;
}

const Json* openblack::inspector::Find(const Json& value, std::string_view path)
{
	const Json* at = &value;
	while (!path.empty())
	{
		const auto dot = path.find('.');
		const auto part = path.substr(0, dot);
		path = dot == std::string_view::npos ? std::string_view {} : path.substr(dot + 1);
		if (at->is_object())
		{
			const auto it = at->find(part);
			if (it == at->end())
			{
				return nullptr;
			}
			at = &*it;
		}
		else if (at->is_array())
		{
			size_t index = 0;
			const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), index);
			if (error != std::errc {} || end != part.data() + part.size() || index >= at->size())
			{
				return nullptr;
			}
			at = &(*at)[index];
		}
		else
		{
			return nullptr;
		}
	}
	return at;
}

std::optional<double> openblack::inspector::Number(const Json& value)
{
	if (value.is_number())
	{
		return value.get<double>();
	}
	if (value.is_boolean())
	{
		return value.get<bool>() ? 1.0 : 0.0;
	}
	return std::nullopt;
}

std::optional<double> openblack::inspector::NumberMember(const Json& object, std::string_view key)
{
	if (!object.is_object())
	{
		return std::nullopt;
	}
	const auto it = object.find(key);
	if (it == object.end() || !it->is_number())
	{
		return std::nullopt;
	}
	return it->get<double>();
}

std::optional<std::string> openblack::inspector::StringMember(const Json& object, std::string_view key)
{
	if (!object.is_object())
	{
		return std::nullopt;
	}
	const auto it = object.find(key);
	if (it == object.end() || !it->is_string())
	{
		return std::nullopt;
	}
	return it->get<std::string>();
}

std::optional<bool> openblack::inspector::BoolMember(const Json& object, std::string_view key)
{
	if (!object.is_object())
	{
		return std::nullopt;
	}
	const auto it = object.find(key);
	if (it == object.end() || !it->is_boolean())
	{
		return std::nullopt;
	}
	return it->get<bool>();
}
