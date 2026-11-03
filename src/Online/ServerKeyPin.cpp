/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ServerKeyPin.h"

namespace openblack::online::server_key
{
namespace
{

[[nodiscard]] std::optional<uint8_t> HexDigit(char digit) noexcept
{
	if (digit >= '0' && digit <= '9')
	{
		return static_cast<uint8_t>(digit - '0');
	}
	if (digit >= 'a' && digit <= 'f')
	{
		return static_cast<uint8_t>(digit - 'a' + 10);
	}
	if (digit >= 'A' && digit <= 'F')
	{
		return static_cast<uint8_t>(digit - 'A' + 10);
	}
	return std::nullopt;
}

} // namespace

std::optional<Pin> ParseHex(std::string_view text)
{
	if (text.size() != k_PinHexDigits)
	{
		return std::nullopt;
	}
	Pin pin {};
	for (size_t index = 0; index < pin.size(); ++index)
	{
		const auto high = HexDigit(text[index * 2]);
		const auto low = HexDigit(text[index * 2 + 1]);
		if (!high.has_value() || !low.has_value())
		{
			return std::nullopt;
		}
		pin.at(index) = static_cast<uint8_t>((*high << 4) | *low);
	}
	return pin;
}

std::string HexOf(const Pin& pin)
{
	constexpr std::string_view k_Digits = "0123456789abcdef";
	std::string text;
	text.reserve(k_PinHexDigits);
	for (const auto byte : pin)
	{
		text.push_back(k_Digits[byte >> 4]);
		text.push_back(k_Digits[byte & 0x0F]);
	}
	return text;
}

std::string CurlPinOf(const Pin& pin)
{
	constexpr std::string_view k_Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string text = "sha256//";
	for (size_t index = 0; index < pin.size(); index += 3)
	{
		const auto left = pin.size() - index;
		const uint32_t first = pin.at(index);
		const uint32_t second = left > 1 ? pin.at(index + 1) : 0u;
		const uint32_t third = left > 2 ? pin.at(index + 2) : 0u;
		const uint32_t group = (first << 16) | (second << 8) | third;
		text.push_back(k_Alphabet[(group >> 18) & 0x3F]);
		text.push_back(k_Alphabet[(group >> 12) & 0x3F]);
		text.push_back(left > 1 ? k_Alphabet[(group >> 6) & 0x3F] : '=');
		text.push_back(left > 2 ? k_Alphabet[group & 0x3F] : '=');
	}
	return text;
}

} // namespace openblack::online::server_key
