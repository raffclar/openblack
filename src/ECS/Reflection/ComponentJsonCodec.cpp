/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#if defined(OPENBLACK_INSPECTOR)

#include "ComponentJsonCodec.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace openblack::ecs::reflection::detail
{

double Shortest(float value)
{
	if (!std::isfinite(value))
	{
		return static_cast<double>(value);
	}
	// Nine significant digits always read back as the same float; fewer often do
	std::array<char, 32> text {};
	for (int precision = 6; precision <= 9; ++precision)
	{
		std::snprintf(text.data(), text.size(), "%.*g", precision, static_cast<double>(value));
		if (std::strtof(text.data(), nullptr) == value)
		{
			return std::strtod(text.data(), nullptr);
		}
	}
	return static_cast<double>(value);
}

std::string Utf8(std::u16string_view text)
{
	std::string utf8;
	for (size_t i = 0; i < text.size(); ++i)
	{
		uint32_t code = text[i];
		// A pair of surrogates is one character beyond the first 65536
		if (code >= 0xD800 && code < 0xDC00 && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] < 0xE000)
		{
			code = 0x10000 + ((code - 0xD800) << 10) + (text[i + 1] - 0xDC00);
			++i;
		}
		if (code < 0x80)
		{
			utf8 += static_cast<char>(code);
		}
		else if (code < 0x800)
		{
			utf8 += static_cast<char>(0xC0 | (code >> 6));
			utf8 += static_cast<char>(0x80 | (code & 0x3F));
		}
		else if (code < 0x10000)
		{
			utf8 += static_cast<char>(0xE0 | (code >> 12));
			utf8 += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
			utf8 += static_cast<char>(0x80 | (code & 0x3F));
		}
		else
		{
			utf8 += static_cast<char>(0xF0 | (code >> 18));
			utf8 += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
			utf8 += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
			utf8 += static_cast<char>(0x80 | (code & 0x3F));
		}
	}
	return utf8;
}

std::string Describe(const Json& value)
{
	switch (value.type())
	{
	case Json::value_t::null:
		return "null";
	case Json::value_t::boolean:
		return "a boolean";
	case Json::value_t::string:
		return "a string";
	case Json::value_t::array:
		return "an array of " + std::to_string(value.size());
	case Json::value_t::object:
		return "an object";
	case Json::value_t::number_float:
		return "a number with a fraction";
	case Json::value_t::number_integer:
	case Json::value_t::number_unsigned:
		return "a whole number";
	default:
		return "something else";
	}
}

std::optional<Json> ScalarFromText(const Json& value)
{
	if (!value.is_string())
	{
		return std::nullopt;
	}
	auto parsed = inspector::Parse(value.get<std::string>());
	if (!parsed.has_value() || !(parsed->is_number() || parsed->is_boolean()))
	{
		return std::nullopt;
	}
	return parsed;
}

} // namespace openblack::ecs::reflection::detail

#endif
