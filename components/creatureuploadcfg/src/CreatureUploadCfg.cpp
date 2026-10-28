/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureUploadCfg.h"

#include <cstddef>

#include <optional>
#include <string_view>
#include <vector>

namespace openblack::creatureuploadcfg
{
namespace
{

constexpr char32_t k_ByteOrderMark = 0xFEFF;
constexpr char32_t k_LineBreak = U'\n';

/// Adds a character to UTF-8 text
void AppendUtf8(std::string& text, char32_t c)
{
	if (c < 0x80)
	{
		text.push_back(static_cast<char>(c));
	}
	else if (c < 0x800)
	{
		text.push_back(static_cast<char>(0xC0 | (c >> 6)));
		text.push_back(static_cast<char>(0x80 | (c & 0x3F)));
	}
	else if (c < 0x10000)
	{
		text.push_back(static_cast<char>(0xE0 | (c >> 12)));
		text.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
		text.push_back(static_cast<char>(0x80 | (c & 0x3F)));
	}
	else
	{
		text.push_back(static_cast<char>(0xF0 | (c >> 18)));
		text.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
		text.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
		text.push_back(static_cast<char>(0x80 | (c & 0x3F)));
	}
}

/// The UTF-16 units of the file after the byte order mark
std::vector<char16_t> Utf16Units(std::span<const uint8_t> bytes)
{
	std::vector<char16_t> units;
	units.reserve(bytes.size() / 2);
	for (size_t i = 2; i + 1 < bytes.size(); i += 2)
	{
		units.push_back(static_cast<char16_t>(bytes[i] | (bytes[i + 1] << 8)));
	}
	return units;
}

/// UTF-8 text of UTF-16 units. A unit of a surrogate pair with no other half becomes U+FFFD.
std::string ToUtf8(std::u16string_view units)
{
	std::string text;
	for (size_t i = 0; i < units.size(); ++i)
	{
		const char32_t unit = units[i];
		if (unit >= 0xD800 && unit < 0xDC00 && i + 1 < units.size() && units[i + 1] >= 0xDC00 && units[i + 1] < 0xE000)
		{
			AppendUtf8(text, 0x10000 + ((unit - 0xD800) << 10) + (units[i + 1] - 0xDC00));
			++i;
		}
		else if (unit >= 0xD800 && unit < 0xE000)
		{
			AppendUtf8(text, 0xFFFD);
		}
		else
		{
			AppendUtf8(text, unit);
		}
	}
	return text;
}

/// The key and the text of a line
template <typename Char>
struct Line
{
	std::string key;
	std::basic_string<Char> text;
	/// True when a zero character stopped the text before the end of the line
	bool stoppedAtZero {false};
};

/// The key and the text of one line, without its line break. In UTF-16 lines (wide), a "#" in the text is part of it.
template <typename Char>
std::optional<Line<Char>> ParseLine(std::basic_string_view<Char> line, bool wide)
{
	size_t at = 0;
	while (at < line.size() && line[at] == ' ')
	{
		++at;
	}
	if (at == line.size() || line[at] == '#')
	{
		return std::nullopt;
	}
	std::string key;
	while (at < line.size() && line[at] != '=' && line[at] != ' ' && line[at] != '#')
	{
		key.push_back(static_cast<char>(line[at]));
		++at;
	}
	while (at < line.size() && (line[at] == ' ' || line[at] == '=' || line[at] == '\t'))
	{
		++at;
	}
	if (at < line.size() && line[at] == '#')
	{
		return std::nullopt;
	}
	std::basic_string<Char> text;
	while (at < line.size() && line[at] != 0 && (wide || line[at] != '#'))
	{
		text.push_back(line[at]);
		++at;
	}
	if (!wide && at < line.size() && line[at] == '#')
	{
		return std::nullopt;
	}
	return Line<Char> {.key = std::move(key), .text = std::move(text), .stoppedAtZero = at < line.size()};
}

Texts ReadWide(std::span<const uint8_t> bytes)
{
	Texts texts;
	const auto units = Utf16Units(bytes);
	const std::u16string_view all(units.data(), units.size());
	size_t start = 0;
	while (start < all.size())
	{
		const auto end = all.find(static_cast<char16_t>(k_LineBreak), start);
		if (end == std::u16string_view::npos)
		{
			// The game stops at the end of the file before a last line with no line break
			break;
		}
		const auto line = all.substr(start, end - start);
		start = end + 1;
		auto parsed = ParseLine<char16_t>(line, true);
		if (!parsed.has_value())
		{
			continue;
		}
		// The game removes the last character before the line break, which is the carriage return
		if (!parsed->stoppedAtZero && !parsed->text.empty())
		{
			parsed->text.pop_back();
		}
		texts.insert_or_assign(std::move(parsed->key), ToUtf8(parsed->text));
	}
	return texts;
}

Texts ReadNarrow(std::span<const uint8_t> bytes)
{
	Texts texts;
	const std::string_view all(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	size_t start = 0;
	while (start < all.size())
	{
		const auto end = all.find(static_cast<char>(k_LineBreak), start);
		if (end == std::string_view::npos)
		{
			// The game stops at the end of the file before a last line with no line break
			break;
		}
		const auto line = all.substr(start, end - start);
		start = end + 1;
		auto parsed = ParseLine<char>(line, false);
		if (parsed.has_value())
		{
			texts.insert_or_assign(std::move(parsed->key), std::move(parsed->text));
		}
	}
	return texts;
}

} // namespace

Texts Read(std::span<const uint8_t> bytes)
{
	if (bytes.size() >= 2 && static_cast<char32_t>(bytes[0] | (bytes[1] << 8)) == k_ByteOrderMark)
	{
		return ReadWide(bytes);
	}
	return ReadNarrow(bytes);
}

} // namespace openblack::creatureuploadcfg
