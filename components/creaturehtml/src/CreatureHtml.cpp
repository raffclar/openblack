/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHtml.h"

#include <algorithm>
#include <ranges>

using namespace openblack;

namespace
{

constexpr char k_EndOfFile = 0x1A;

/// The template as a Windows text file reads: up to the end-of-file character, without the carriage returns before
/// line feeds
std::string ReadAsText(std::string_view page)
{
	std::string text;
	text.reserve(page.size());
	for (size_t i = 0; i < page.size(); ++i)
	{
		const char c = page[i];
		if (c == k_EndOfFile)
		{
			break;
		}
		if (c == '\r' && i + 1 < page.size() && page[i + 1] == '\n')
		{
			continue;
		}
		text.push_back(c);
	}
	return text;
}

/// The value of a name, the last of those with the name
const creaturehtml::Value* Find(std::span<const creaturehtml::Value> values, std::string_view name)
{
	const auto found = std::ranges::find(values | std::views::reverse, name, &creaturehtml::Value::name);
	return found == (values | std::views::reverse).end() ? nullptr : &*found;
}

/// Fills one part of a line and adds it to the page
void FillPart(std::string_view part, std::span<const creaturehtml::Value> values, std::string& page)
{
	part = part.substr(0, part.find('\0'));
	size_t at = 0;
	while (at < part.size())
	{
		if (part[at] != '[' || at + 1 >= part.size() || part[at + 1] != '$')
		{
			page.push_back(part[at]);
			++at;
			continue;
		}
		at += 2;
		const auto end = std::min(part.find('$', at), part.size());
		const auto* value = Find(values, part.substr(at, end - at));
		if (value == nullptr)
		{
			// A name with no value stops the part here
			return;
		}
		page += value->value;
		at = end;
		// The "$" goes, then the character after it, when there is one
		if (at + 1 < part.size())
		{
			++at;
		}
		++at;
	}
}

} // namespace

std::string creaturehtml::Fill(std::string_view page, std::span<const Value> values)
{
	const auto text = ReadAsText(page);
	std::string filled;
	filled.reserve(text.size());
	size_t start = 0;
	while (start < text.size())
	{
		// A part ends after its line feed, or once it is as long as the longest part
		size_t end = start;
		while (end < text.size() && end - start < k_LongestPart)
		{
			++end;
			if (text[end - 1] == '\n')
			{
				break;
			}
		}
		FillPart(std::string_view(text).substr(start, end - start), values, filled);
		start = end;
	}

	// The game writes the page as a Windows text file
	std::string written;
	written.reserve(filled.size() + filled.size() / 16);
	for (const char c : filled)
	{
		if (c == '\n')
		{
			written.push_back('\r');
		}
		written.push_back(c);
	}
	return written;
}
