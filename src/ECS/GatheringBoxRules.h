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

/// The setting that makes the questions plain lines of the chat. Only this value of the setting does it.
constexpr uint32_t k_PlainChatSetting = 5;

/// True when the setting makes the questions plain lines of the chat. With no setting, the lines can ask questions.
[[nodiscard]] constexpr bool IsPlainChat(std::optional<uint32_t> setting) noexcept
{
	return setting == k_PlainChatSetting;
}

} // namespace question

/// The staff code. A typed line that starts with the code goes also to the ping server, as a line of the staff or as
/// announcements. The line also goes to the players and to the picked people as each typed line does.
namespace staff_code
{

/// The start of a line of the staff
constexpr std::u16string_view k_Code = u"!SGRB";
/// After the code, this letter makes the words announcements
constexpr char16_t k_AnnounceMark = u'O';
/// After the code and the letter of the announcements, this letter starts a name that ends at k_NameEnd
constexpr char16_t k_NameMark = u'N';
constexpr char16_t k_NameEnd = u'#';
/// The name keeps at most this many characters
constexpr size_t k_MostNameCharacters = 19;
/// The announcements have at most this many lines, each with at most this many characters
constexpr size_t k_MostAnnouncements = 15;
constexpr size_t k_MostAnnouncementCharacters = 14;
/// The ping server gets a line of the staff for this number
constexpr UserId k_ServerUser = 0;

struct Send
{
	/// The words are announcements, not a line
	bool announce {false};
	/// The name that the line shows in place of the name of the player
	std::optional<std::u16string> name;
	/// The words after the code
	std::u16string text;
	bool operator==(const Send&) const = default;
};

/// What a typed line sends with the staff code, none when the line does not start with the code. After the code:
/// 1. An optional k_AnnounceMark.
/// 2. An optional k_NameMark, then the name. The name stops before k_NameEnd, or after k_MostNameCharacters
///    characters. The character after the name is not a part of the words. The game skips it also when it is not
///    k_NameEnd.
/// 3. The words. A name that goes to the end of the line gives no words.
[[nodiscard]] std::optional<Send> Parse(std::u16string_view line);

/// The lines of the announcements in the words. Each space, the end of the words and the last character that a line
/// keeps close a line. A space stays at the end of its line. The end of the words closes a line also when the line has
/// no characters. The words after the last line that the announcements keep do not go.
[[nodiscard]] std::vector<std::u16string> AnnouncementLines(std::u16string_view words);

/// The staff number of an announcement line: the first line has the number of the admin, and each next line has the
/// next number. The game sends the last line first, so the first line goes to the top of the others.
[[nodiscard]] constexpr UserId AnnouncerOf(size_t line) noexcept
{
	return k_AdminUser + static_cast<UserId>(line);
}

} // namespace staff_code

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
/// The game has this many god teams, and a god team has at most this many humans
constexpr size_t k_TeamCount = 4;
constexpr size_t k_TeamMembers = 4;
/// The report has the members of the first two god teams
constexpr size_t k_ReportedTeams = 2;
/// The bytes of the members of the reported teams at the start of the report
constexpr size_t k_TeamBytes = k_ReportedTeams * k_TeamMembers * 8;
/// The report keeps only the low 29 bits of the number of a member
constexpr uint32_t k_MemberNumberMask = 0x1FFFFFFF;
/// The mark of the report is 1 on this turn only
constexpr uint32_t k_MarkTurn = 1;

/// A human in a god team. An empty place has the number 0 and the clan 0.
struct TeamMember
{
	/// The lasting number of the human
	uint32_t number {0};
	/// The clan of the team, 0 for no clan
	uint32_t clan {0};
};

/// The members of each god team, by team and by place in the team. The game fills the table one time at the start.
using TeamTable = std::array<std::array<TeamMember, k_TeamMembers>, k_TeamCount>;

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
	TeamTable teams {};
	/// The game turn when the host makes the report
	uint32_t turn {0};
	/// The game is decided
	bool decided {false};
	/// The conditions that the land uses
	std::array<std::optional<Condition>, k_ConditionCount> conditions {};
};

/// The bytes of the report, little endian:
/// 1. For the first two god teams, for each place in the team: the number of the member (low 29 bits) and the clan.
/// 2. A number that is 1 on the first game turn, else 0. The game first writes 2 there for a decided game, and then
///    writes over it (vanilla bug), so the report never shows a decided game.
/// 3. A number with a bit for each condition that the land uses, from the first condition up.
/// 4. For each of these conditions: its aim, and for each of the first four players the aim when the player met the
///    condition, else the current value.
[[nodiscard]] std::vector<uint8_t> Encode(const Facts& facts);

} // namespace host_report

} // namespace openblack::gathering_box
