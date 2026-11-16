/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MainLobbyRoom.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include <LobbyApiJson.h>

namespace openblack::online::main_room
{

namespace
{

[[nodiscard]] char LowerAscii(char c) noexcept
{
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

/// The order of the names of the people: no difference between upper case and lower case letters, then the account
[[nodiscard]] bool NameBefore(const lobby_api::AccountRef& a, const lobby_api::AccountRef& b)
{
	const auto lower = [](const std::string& name) { return name | std::views::transform(LowerAscii); };
	const auto& aName = lower(a.name);
	const auto& bName = lower(b.name);
	if (std::ranges::lexicographical_compare(aName, bName))
	{
		return true;
	}
	if (std::ranges::lexicographical_compare(bName, aName))
	{
		return false;
	}
	return a.id < b.id;
}

} // namespace

bool MainLobbyRoom::Apply(const lobby_api::PushEvent& event)
{
	if (event.type == lobby_api::events::k_Subscribed)
	{
		const auto subscribed = lobby_api::ParseChannelSubscribed(event.data);
		if (!subscribed.has_value() || subscribed->channel != lobby_api::k_LobbyChannel)
		{
			return false;
		}
		_people = subscribed->members;
		SortPeople();
		// The history takes the place of the lines: after a lost connection it has the lines that the player missed
		_lines.clear();
		_historySeq = 0;
		for (const auto& line : subscribed->history)
		{
			Add({.account = line.from.id, .name = line.from.name, .text = line.text, .staff = line.staff, .system = false});
			_historySeq = std::max(_historySeq, line.seq);
		}
		++_version;
		return true;
	}
	if (event.type == lobby_api::events::k_ChatJoined || event.type == lobby_api::events::k_ChatLeft)
	{
		const auto presence = lobby_api::ParseChatPresence(event.data);
		if (!presence.has_value() || presence->channel != lobby_api::k_LobbyChannel)
		{
			return false;
		}
		const auto found = std::ranges::find(_people, presence->account.id, &lobby_api::AccountRef::id);
		if (event.type == lobby_api::events::k_ChatJoined)
		{
			if (found != _people.end())
			{
				return false;
			}
			_people.push_back(presence->account);
			SortPeople();
		}
		else
		{
			if (found == _people.end())
			{
				return false;
			}
			_people.erase(found);
		}
		++_version;
		return true;
	}
	if (event.type == lobby_api::events::k_ChatLine)
	{
		const auto line = lobby_api::ParseChatLine(event.data);
		if (!line.has_value() || line->channel != lobby_api::k_LobbyChannel)
		{
			return false;
		}
		// The history of a new connection can have the line already
		if (event.seq.has_value() && *event.seq <= _historySeq)
		{
			return false;
		}
		Add({.account = line->from.id, .name = line->from.name, .text = line->text, .staff = line->staff, .system = false});
		++_version;
		return true;
	}
	return false;
}

void MainLobbyRoom::AddSystemLine(std::string text)
{
	Add({.account = 0, .name = {}, .text = std::move(text), .staff = false, .system = true});
	++_version;
}

void MainLobbyRoom::Clear()
{
	_lines.clear();
	_people.clear();
	_historySeq = 0;
	++_version;
}

void MainLobbyRoom::Add(Line line)
{
	_lines.push_back(std::move(line));
	while (_lines.size() > k_MostLines)
	{
		_lines.pop_front();
	}
}

void MainLobbyRoom::SortPeople()
{
	std::ranges::sort(_people, NameBefore);
}

} // namespace openblack::online::main_room
