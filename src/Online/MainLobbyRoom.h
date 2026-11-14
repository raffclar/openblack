/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <deque>
#include <string>
#include <vector>

#include <LobbyApi.h>

/// The main lobby room (deviation 267): the chat at the right of the list of the sessions. The lobby sends its lines and
/// the people in it on the push channel "lobby". This object keeps what the panel shows, and has no other state, so
/// that the tests can give it events.
namespace openblack::online::main_room
{

/// The most lines that the panel keeps. The oldest line goes first.
inline constexpr size_t k_MostLines = 100;

/// A line of the room, as the lobby sent it, or a line of the game itself
struct Line
{
	/// The account of the sender, 0 for a line of the game itself
	int64_t account {0};
	std::string name;
	std::string text;
	/// The line of a confirmed staff account
	bool staff {false};
	/// A line of the game itself, for example a line that the lobby refused
	bool system {false};
};

class MainLobbyRoom
{
public:
	/// Uses an event of the push channel. True when the lines or the people changed.
	/// - "subscribed" of the channel "lobby": the people, and the last lines of the room in place of the lines.
	/// - "chat.joined" and "chat.left" of the channel: a person comes or goes.
	/// - "chat.line" of the channel: a new line. A line that the history gave already does not come again.
	bool Apply(const lobby_api::PushEvent& event);
	/// Adds a line of the game itself
	void AddSystemLine(std::string text);
	/// Forgets the lines and the people, when the player leaves the list
	void Clear();

	[[nodiscard]] const std::deque<Line>& Lines() const noexcept { return _lines; }
	/// The people in the room, in the order of their names
	[[nodiscard]] const std::vector<lobby_api::AccountRef>& People() const noexcept { return _people; }
	/// A number that changes each time the lines or the people change
	[[nodiscard]] uint64_t Version() const noexcept { return _version; }

private:
	void Add(Line line);
	void SortPeople();

	std::deque<Line> _lines;
	std::vector<lobby_api::AccountRef> _people;
	/// The number of the newest line that the history gave
	int64_t _historySeq {0};
	uint64_t _version {0};
};

} // namespace openblack::online::main_room
