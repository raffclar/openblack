/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WindowsText.h"

#include <cstdint>

#include <algorithm>
#include <array>
#include <iterator>

namespace openblack::windows_text
{
namespace
{

/// The characters of code page 1252 from 0x80 to 0x9F, 0 for a byte that it does not use
constexpr std::array<char16_t, 32> k_HighCharacters {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
    0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178};
constexpr char16_t k_FirstHighCharacter = 0x80;
constexpr char16_t k_FirstLatinCharacter = 0xA0;
constexpr char16_t k_LastLatinCharacter = 0xFF;

} // namespace

std::string ToWindowsText(std::u16string_view text)
{
	std::string written;
	written.reserve(text.size());
	for (const char16_t c : text)
	{
		if (c < k_FirstHighCharacter || (c >= k_FirstLatinCharacter && c <= k_LastLatinCharacter))
		{
			written.push_back(static_cast<char>(c));
			continue;
		}
		const auto high = std::ranges::find(k_HighCharacters, c);
		written.push_back(high == k_HighCharacters.end()
		                      ? '?'
		                      : static_cast<char>(k_FirstHighCharacter + std::distance(k_HighCharacters.begin(), high)));
	}
	return written;
}

std::u16string FromWindowsText(std::string_view text)
{
	std::u16string read;
	read.reserve(text.size());
	for (const char c : text)
	{
		const auto byte = static_cast<char16_t>(static_cast<uint8_t>(c));
		if (byte < k_FirstHighCharacter || byte >= k_FirstLatinCharacter)
		{
			read.push_back(byte);
			continue;
		}
		const auto high = k_HighCharacters.at(byte - k_FirstHighCharacter);
		read.push_back(high != 0 ? high : byte);
	}
	return read;
}

} // namespace openblack::windows_text
