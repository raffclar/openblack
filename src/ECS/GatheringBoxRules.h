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

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ChatRules.h"

/// The gathering box and the ping server. While a world runs, the game keeps in touch with the ping server:
/// - Each 20 seconds, the game tells the server that its player plays, and asks which friends of the player are online.
/// - The server sends the lines of other people, of the staff of the game and of its announcements.
/// - The host of a network game also tells the server the state of the conditions of the land.
/// None of this is a part of the synced world. The game uses the time of the computer, not the time of the turns.
namespace openblack::gathering_box
{

using chat::people::UserId;

/// The time between two pings of the ping server, in milliseconds
constexpr uint32_t k_PingTime = 20001;
/// A line from this number opens the gathering box and expands its others
constexpr UserId k_AdminUser = 9999999;
/// A line from the ping server keeps at most this many characters
constexpr size_t k_MostLineCharacters = 254;
/// The name of a person in the lists keeps at most this many characters
constexpr size_t k_MostNameCharacters = 47;

/// The clock of the pings. The first check after the start is always due. Each due check starts the clock again,
/// also when the game does not ping because it has no connection.
class PingClock
{
public:
	/// True when the game pings now. The time is the time of the computer, in milliseconds.
	[[nodiscard]] bool Due(uint32_t now) noexcept;

private:
	std::optional<uint32_t> _start;
};

/// A question of the staff. A line that starts with the secret word is not a line of the chat: it opens a box that
/// asks the question, and the answer goes back to the sender.
namespace question
{

/// The secret word at the start of a question
constexpr std::u16string_view k_SecretWord = u"!SaMuElGR";
/// A '!' after the secret word asks for a line of text, else the question has a Yes and a No
constexpr char16_t k_LineMark = u'!';
/// The answers of the Yes and the No
constexpr std::u16string_view k_YesAnswer = u"YES";
constexpr std::u16string_view k_NoAnswer = u"NO";
/// The label of the No button. The game does not take it from the texts.
constexpr std::u16string_view k_NoLabel = u"NO";
/// The texts of the first button: "Yes." for a question with a Yes and a No, "OK" for a question that asks for a line
constexpr uint32_t k_YesText = 2593;
constexpr uint32_t k_OkText = 2596;
/// The line of the answer keeps at most this many characters
constexpr size_t k_MostAnswerCharacters = 100;

enum class Kind : uint8_t
{
	YesNo,
	Line,
};

struct Question
{
	Kind kind {Kind::YesNo};
	/// The name of the sender, a space and the words of the question
	std::u16string text;
	/// The sender gets the answer
	UserId sender {0};
	bool operator==(const Question&) const = default;
};

/// The question in a line, if the line has one. The line must have more characters than the secret word, and start
/// with it. The player can make the questions plain lines of the chat with a setting.
[[nodiscard]] std::optional<Question> Parse(std::u16string_view name, std::u16string_view text, UserId sender, bool plainChat);

/// The text of the first button of the box
[[nodiscard]] constexpr uint32_t FirstButtonText(Kind kind) noexcept
{
	return kind == Kind::YesNo ? k_YesText : k_OkText;
}

/// The keys of the box:
/// - Return in the line of the answer moves the focus to the first button. Elsewhere, Return pushes the first button.
/// - Escape pushes the No button, also when the box asks for a line and does not show the No button.
enum class Key : uint8_t
{
	Return,
	Escape,
};
enum class Push : uint8_t
{
	None,
	FocusFirstButton,
	First,
	No,
};
[[nodiscard]] Push PushOf(Key key, bool lineHasFocus) noexcept;

/// The answer that a button gives: for a question with a Yes and a No, the Yes or the No; for a question that asks
/// for a line, the line, from either button
[[nodiscard]] std::u16string AnswerOf(Kind kind, Push push, std::u16string_view line);

} // namespace question

/// A line from the ping server keeps its first 254 characters
[[nodiscard]] std::u16string_view ClippedLine(std::u16string_view text) noexcept;

/// The ping server saw a person, from a line of the person or from a notice that the person is here:
/// 1. A player with the number gets the name.
/// 2. A friend with the number gets the name, is online and was seen now, on the online clock.
/// 3. An other with the number gets the name. If no other has the number, the person goes to the top of the others.
///    The own number and a friend do not go (see AddOther). A player with the number goes.
/// 4. A person with the number of the admin expands the others.
/// Returns true when the gathering box opens: the person is the admin.
bool SeePerson(chat::people::List& list, std::u16string_view name, UserId user, std::optional<UserId> own, uint32_t now);

/// A line of a person shows when a group with its messages on has the person
[[nodiscard]] bool ShowsLine(const chat::people::List& list, UserId user);

/// When the player picked at least one person, a line goes to the picked people of the friends and of the others
/// through the ping server, in that order. Returns their numbers, none when nobody is picked.
[[nodiscard]] std::vector<UserId> PickedFriendsAndOthers(const chat::people::List& list);

/// The report of the host of an internet game, at each ping and when the game is decided
namespace host_report
{

/// The game has this many conditions
constexpr size_t k_ConditionCount = 15;
/// The report has the values of the first four players
constexpr size_t k_ReportedPlayers = 4;
/// The bytes of the game at the start of the report
constexpr size_t k_GameBytes = 64;

/// The state of a condition of the land for one player
struct PlayerShare
{
	int32_t current {0};
	/// The player met the condition
	bool met {false};
};

struct Condition
{
	/// The aim of the condition, which a player that met it reports
	int32_t aim {0};
	std::array<PlayerShare, k_ReportedPlayers> players {};
};

struct Facts
{
	std::array<uint8_t, k_GameBytes> game {};
	/// The game is decided
	bool decided {false};
	/// A value of the game is 1. Its meaning is not known.
	bool valueIsOne {false};
	/// The conditions that the land uses
	std::array<std::optional<Condition>, k_ConditionCount> conditions {};
};

/// The bytes of the report, little endian:
/// 1. The 64 bytes of the game.
/// 2. A number that is 1 when the value of the game is 1, else 0. The game first writes 2 there for a decided game, and
///    then writes over it (vanilla bug), so the report never shows a decided game.
/// 3. A number with a bit for each condition that the land uses, from the first condition up.
/// 4. For each of these conditions: its aim, and for each of the first four players the aim when the player met the
///    condition, else the current value.
[[nodiscard]] std::vector<uint8_t> Encode(const Facts& facts);

} // namespace host_report

} // namespace openblack::gathering_box
