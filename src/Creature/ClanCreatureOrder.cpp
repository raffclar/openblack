/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ClanCreatureOrder.h"

#include <algorithm>
#include <array>

#include "Common/WindowsText.h"

namespace openblack::clan_creature_order
{
namespace
{

constexpr char k_Zero = '0';
constexpr char k_Nine = '9';
constexpr int32_t k_Ten = 10;

[[nodiscard]] char Upper(uint8_t byte)
{
	const auto c = static_cast<char>(byte);
	return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}

[[nodiscard]] bool IsSpace(char c)
{
	return c == ' ' || (c >= '\t' && c <= '\r');
}

/// The number of a text, as the C function atoi reads it. The text stops at its first zero byte.
[[nodiscard]] int32_t AsciiNumber(std::span<const char> text)
{
	auto c = text.begin();
	const auto end = std::find(text.begin(), text.end(), '\0');
	while (c != end && IsSpace(*c))
	{
		++c;
	}
	bool negative = false;
	if (c != end && (*c == '-' || *c == '+'))
	{
		negative = *c == '-';
		++c;
	}
	int32_t number = 0;
	for (; c != end && *c >= k_Zero && *c <= k_Nine; ++c)
	{
		number = number * k_Ten + (*c - k_Zero);
	}
	return negative ? -number : number;
}

} // namespace

std::optional<Order> Read(std::span<const uint8_t> data)
{
	if (data.size() < k_Text.size() ||
	    !std::ranges::equal(data.first(k_Text.size()), k_Text, [](uint8_t byte, char c) { return Upper(byte) == c; }))
	{
		return std::nullopt;
	}
	// The two bytes after the text give the row. A byte past the end of the data counts as a zero byte.
	std::array<char, k_RowDigits> digits {};
	for (size_t i = 0; i < k_RowDigits && k_Text.size() + i < data.size(); ++i)
	{
		digits.at(i) = static_cast<char>(data[k_Text.size() + i]);
	}
	Order order {.row = AsciiNumber(digits), .name = {}};

	// The name starts after the two digits and stops at the first zero byte
	const auto nameStart = std::min(data.size(), k_Text.size() + k_RowDigits);
	const auto rest = data.subspan(nameStart);
	const auto nameEnd = std::ranges::find(rest, uint8_t {0});
	std::string bytes(rest.begin(), nameEnd);
	order.name = windows_text::FromWindowsText(bytes);
	if (order.name.size() > k_MostNameCharacters)
	{
		order.name.resize(k_MostNameCharacters);
	}
	return order;
}

} // namespace openblack::clan_creature_order
