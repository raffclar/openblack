/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FriendList.h"

#include <charconv>

#include <algorithm>
#include <ranges>
#include <string>

#include "Gui/TextDatabase.h"

namespace openblack::online::friend_list
{

std::string Encode(std::span<const chat::people::FriendRecord> friends)
{
	std::string text(k_Header);
	text += '\n';
	for (const auto& each : friends)
	{
		auto name = gui::ToUtf8(each.name);
		std::ranges::replace_if(name, [](char c) { return c == '\t' || c == '\r' || c == '\n'; }, ' ');
		text += std::to_string(each.user);
		text += '\t';
		text += name;
		text += '\n';
	}
	return text;
}

std::vector<chat::people::FriendRecord> Decode(std::span<const uint8_t> bytes)
{
	const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	std::vector<chat::people::FriendRecord> friends;
	bool first = true;
	for (const auto part : text | std::views::split('\n'))
	{
		auto line = std::string_view(part.begin(), part.end());
		if (line.ends_with('\r'))
		{
			line.remove_suffix(1);
		}
		if (first)
		{
			if (line != k_Header)
			{
				return {};
			}
			first = false;
			continue;
		}
		const auto tab = line.find('\t');
		if (tab == std::string_view::npos)
		{
			continue;
		}
		chat::people::UserId user {0};
		const auto number = line.substr(0, tab);
		const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), user);
		if (error != std::errc {} || end != number.data() + number.size() || user == 0)
		{
			continue;
		}
		friends.push_back({.user = user, .name = gui::ToUtf16(line.substr(tab + 1))});
		if (friends.size() == chat::people::k_MostKeptFriends)
		{
			break;
		}
	}
	return friends;
}

} // namespace openblack::online::friend_list
