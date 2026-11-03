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

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Enums.h"

/// The chat of a network game. A player types a line or picks a taunt from the quick chat, and sends it to the other
/// players that the list of players selects (all of them when none is selected). The line goes in the stream of the
/// turns, so each computer does it at the same turn:
/// - The taunt's advisors speak on every computer, also on the computer of the sender.
/// - The line comes into the incoming messages ("name: text") only on the computers of the players that get it, not on
///   the computer of the sender.
/// - The creature of the sender and the creatures of the players that get it say the line in their speech bubbles, on
///   every computer.
/// - Each computer writes the line to the chat log of its profile.
/// None of this is a part of the synced world.
namespace openblack::chat
{

/// The first text of the taunts
constexpr uint32_t k_FirstTauntText = 2137;
/// The taunts of the quick chat list, from the first text on
constexpr uint32_t k_TauntCount = 31;
/// A taunt that comes in with a larger number than this is lost
constexpr uint32_t k_LastTaunt = 50;
/// The most characters that the player types into the line of the chat
constexpr size_t k_MostTypedCharacters = 254;
/// The text of the advisors' one-off remark on the chat: "Quit the yacking..."
constexpr uint32_t k_YackingText = 3321;
/// The heading of the players in the list of players: "Players"
constexpr uint32_t k_PlayersText = 3543;

/// The text of a line as it goes to the other players. The game drops a staff code from the start of each line, but its
/// test is loose, so it also drops letters of a plain line:
/// 1. Each of '!', 'S', 'G', 'R', 'B' and 'O' goes when it is the next character, in that order.
/// 2. After an 'N' that is then the next character, the 'N' and up to 19 more characters go, until a '#' or the end.
/// 3. A '#' that is then the next character goes.
/// Thus "Sorry" goes as "orry", and "Nice to see you" goes as nothing (vanilla bug).
[[nodiscard]] std::u16string_view WithoutStaffCode(std::u16string_view text) noexcept;

/// The text of a taunt that comes in, none for a number past the last taunt
[[nodiscard]] std::optional<uint32_t> TauntText(uint32_t taunt) noexcept;

/// Which players get a line: the selected players of the list, or all of them when none is selected. A line with no
/// player does not go.
struct ListedPlayer
{
	PlayerNames slot {PlayerNames::PLAYER_ONE};
	bool selected {false};
};
[[nodiscard]] std::vector<PlayerNames> Recipients(std::span<const ListedPlayer> players);

/// Whether the line shows in the incoming messages of this computer: never for the sender's own line; for the other
/// players, when the line has no list of players or the list has the player of this computer. A computer with no god
/// (an observer) shows only a line with no list.
[[nodiscard]] bool ShowsHere(PlayerNames sender, std::optional<PlayerNames> here, std::span<const PlayerNames> recipients);

/// The players whose creatures say the line, other than the sender: the players of the list, or all players when the
/// line has no list
[[nodiscard]] bool CreatureSays(PlayerNames player, PlayerNames sender, std::span<const PlayerNames> recipients);

/// A line with the name of its sender: "name: text"
[[nodiscard]] std::u16string NamedLine(std::u16string_view name, std::u16string_view text);

/// The bubble of a creature that says the lines of the chat
namespace speech_bubble
{

/// The time to show that a line adds, and the least time to show after a line. A unit of the time to show lasts 5000
/// milliseconds of game time (k_TimePerHundredth). The bubble fades out in its last unit.
constexpr float k_LineTime = 3.0f;
constexpr float k_LeastTime = 3.0f;
/// The time to show of the bubble of the player's creature as the chat comes on the screen
constexpr float k_ShowTime = 3.0f;
/// What a hundredth of a game turn takes off the time to show
constexpr float k_TimePerHundredth = 0.0002f;
/// The sizes at the near depth: width, height and line height, in pixels of the default screen
constexpr float k_NearWidth = 250.0f;
constexpr float k_NearHeight = 100.0f;
constexpr float k_NearLineHeight = 19.0f;
/// The margin at the near depth: a fifth of half of the width
constexpr float k_NearMargin = k_NearWidth * 0.5f * 0.2f;
/// Past these alignments the bubble writes in the font of good, or of evil
constexpr float k_GoodFontAlignment = 0.333f;
constexpr float k_EvilFontAlignment = -0.333f;

struct Line
{
	/// The words: the text alone for the creature of the sender, else the name of the sender and the text
	std::u16string text;
	/// The god of the sender, which gives the colour of the line. A line of the ping server has no god and is white.
	std::optional<PlayerNames> sender {PlayerNames::PLAYER_ONE};
};

struct Bubble
{
	/// The newest line first. The bubble shows the newest line at the bottom.
	std::vector<Line> lines;
	float displayTime {0.0f};
	/// The scroll, in lines from the bottom
	float scroll {0.0f};
	/// How tall the lines were when the bubble was last drawn, for the scroll
	float contentHeight {0.0f};
};

/// A creature says a line. The creature of the sender gets the time of a line before the least time is applied; each
/// creature gets the time of a line after it. A new line scrolls the bubble to the bottom.
void Say(Bubble& bubble, Line line, bool ofSender);
/// Each frame: the time to show runs down by the frame's game time, to no less than 0
void Update(Bubble& bubble, float gameMilliseconds) noexcept;

/// The colour of a line, as 0xAARRGGBB: half of the colour of the sender's god, made light. A god with no colour gives
/// white.
[[nodiscard]] uint32_t LineColour(std::optional<uint32_t> godColour) noexcept;
/// The colour of the box, as 0xAARRGGBB: half of the colour of the creature's god, opaque. With no god colour, the box
/// has half of the default sky blue.
[[nodiscard]] uint32_t BoxColour(std::optional<uint32_t> godColour) noexcept;

/// The font of the bubble
enum class Font : uint8_t
{
	Neutral,
	Good,
	Evil,
};
[[nodiscard]] Font FontOf(float alignment) noexcept;

} // namespace speech_bubble

/// The line that the chat log of the profile gets: "name: text" and a new line, in the code page of Western Windows. A
/// character that the code page does not have becomes '?'.
[[nodiscard]] std::string LogLine(std::u16string_view name, std::u16string_view text);

/// The list of people of the chat. It has three groups, each with a heading, in this order:
/// - Players: the other players of the game.
/// - Friends: the friends of the profile, at most 25 kept.
/// - Others: other people that sent a line.
/// A group with no people does not show. Each heading shows its people under it when it is expanded.
namespace people
{

/// The lasting number of a person, which the friends of the profile keep
using UserId = uint32_t;

/// The texts of the headings and of the friend button
constexpr uint32_t k_FriendsText = 6618;
constexpr uint32_t k_OthersText = 6620;
/// "Make Friends" and "Make Not Friends"
constexpr uint32_t k_MakeFriendsText = 6779;
constexpr uint32_t k_MakeNotFriendsText = 6780;

/// The profile keeps at most this many friends
constexpr size_t k_MostKeptFriends = 25;
/// A friend that was seen less than this long ago is online, in milliseconds of the online clock
constexpr uint32_t k_OnlineTime = 450000;
/// The online clock starts at this time, so that a friend that was never seen is not online
constexpr uint32_t k_OnlineClockStart = k_OnlineTime;

/// The colour of a person with no god, as 0xAARRGGBB
constexpr uint32_t k_NoGodColour = 0x00AFEFFFu;
/// The colour of a person of the game's staff. Its full alpha gives the row a bar, also when it is not selected.
constexpr uint32_t k_StaffColour = 0xFFFFFF00u;
/// The numbers of the staff are after the first and before the last
constexpr UserId k_BeforeFirstStaff = 9999998;
constexpr UserId k_AfterLastStaff = 10000015;

struct Person
{
	/// The god slot of a player of the game. The other groups do not use it.
	PlayerNames slot {PlayerNames::PLAYER_ONE};
	/// None when the game does not know the lasting number of the person
	std::optional<UserId> user;
	std::u16string name;
	bool selected {false};
	/// The person is a friend of the profile
	bool isFriend {false};
	/// A friend that is online shows an envelope
	bool online {false};
	/// When the friend was last seen, on the online clock
	uint32_t lastSeen {0};
	/// The colour of the row, as 0xAARRGGBB. Full alpha gives the row a bar.
	uint32_t colour {k_NoGodColour};
};

struct Category
{
	/// The heading shows the people under it, else "..." after its name
	bool expanded {true};
	/// The speech button of the heading
	bool messagesOn {true};
	std::vector<Person> people;
};

enum class Group : uint8_t
{
	Players,
	Friends,
	Others,
};

struct List
{
	Category players;
	Category friends;
	Category others;
	/// The row of the last person that a click selected, for a click with Shift
	std::optional<size_t> lastSelected;

	[[nodiscard]] Category& Of(Group group) noexcept;
	[[nodiscard]] const Category& Of(Group group) const noexcept;
};

/// The other players of the game, in the order of their god slots, each selected
[[nodiscard]] Category MakePlayers(std::vector<Person> players);

/// A row of the list: the heading of a group, or a person of the group
struct Row
{
	Group group {Group::Players};
	/// None for the heading
	std::optional<size_t> person;
	bool operator==(const Row&) const = default;
};
/// The rows of the list: for each group with people, in the order Players, Friends, Others, the heading and then each
/// person when the group is expanded
[[nodiscard]] std::vector<Row> Rows(const List& list);
[[nodiscard]] size_t RowCount(const List& list);

/// A click on a row of the list:
/// - On a heading, the click removes the selection of all people. On the speech button of the heading it turns the
///   messages on or off, and the group expands with them. Elsewhere it expands or collapses the group.
/// - On a selected person with no modifier key, the click removes the selection of all people.
/// - Else, with no Ctrl, the click first removes the selection of all people.
/// - With Shift, the people of the rows from the last row that a click selected to this one change as this person
///   changes. The headings between them do not change.
/// - Else this person changes, and its row is the last row that a click selected.
struct Click
{
	size_t row {0};
	bool onSpeechButton {false};
	bool shift {false};
	bool ctrl {false};
	/// Another modifier key, such as Alt
	bool otherModifier {false};
};
void Apply(List& list, const Click& click);

/// The players that the list selects, for Recipients
[[nodiscard]] std::vector<ListedPlayer> Listed(const List& list);

/// Each friend marks the people of the other groups with its number as friends. Nothing removes these marks.
void MarkFriends(List& list);

/// The friend button under the list. It is usable when at least one person is selected and all the selected people
/// are friends, or none of them is. It removes friends when the first selected person is a friend.
struct FriendButton
{
	bool usable {false};
	bool removes {false};
	bool operator==(const FriendButton&) const = default;
};
[[nodiscard]] FriendButton FriendButtonOf(const List& list);

/// The friend button changes the friends:
/// 1. Each selected player that is a friend selects its friend. Each other selected player goes to the top of the
///    friends as a friend that is not selected, and stays in the players.
/// 2. Each selected other that is a friend selects its friend. Each other selected other moves to the top of the
///    friends, not selected.
/// 3. Each selected friend goes, and the player with its number is no longer selected and no longer a friend.
/// 4. The people get their friend marks again.
void MakeFriends(List& list);

/// A friend as the profile keeps it
struct FriendRecord
{
	UserId user {0};
	std::u16string name;
	bool operator==(const FriendRecord&) const = default;
};
/// The friends that the profile keeps: the first 25, from the top
[[nodiscard]] std::vector<FriendRecord> KeptFriends(const List& list);
/// The friends of the profile replace the friends of the list, in the same order, at most 25. The heading keeps
/// whether it is expanded and whether its messages are on.
void LoadFriends(List& list, std::span<const FriendRecord> records);

/// A person that is not a player and not a friend sent a line: the person goes to the top of the others, in the
/// colour of the staff when its number is a number of the staff. The own number and a friend do not go.
void AddOther(List& list, std::u16string_view name, UserId user, std::optional<UserId> own);

/// The friends that were seen now are seen at this time of the online clock. Then each friend is online when it is a
/// player of the game, or when it was seen less than 450 seconds ago. A time of a friend that is later than now goes
/// back to 0.
void UpdateOnline(List& list, std::span<const UserId> seen, uint32_t now);

/// The numbers of all friends, from the top. The list can have more than the 25 that the profile keeps.
[[nodiscard]] std::vector<UserId> FriendNumbers(const List& list);

} // namespace people

/// The width of the line of the chat: its text and 20 more, at least 90 and at most half of the layout
[[nodiscard]] int EditWidth(float textWidth, int layoutWidth) noexcept;

} // namespace openblack::chat
