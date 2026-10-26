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

/// The time to show that a line adds, and the least time to show after a line, in seconds
constexpr float k_LineTime = 3.0f;
constexpr float k_LeastTime = 3.0f;
/// The time to show of the bubble of the player's creature as the chat comes on the screen, in seconds
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
	/// The god of the sender, which gives the colour of the line
	PlayerNames sender {PlayerNames::PLAYER_ONE};
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

/// The list of players of the chat: a heading, and under it the other players of the game when it is open
namespace people
{

struct Person
{
	PlayerNames slot {PlayerNames::PLAYER_ONE};
	std::u16string name;
	bool selected {false};
};

struct List
{
	/// The heading shows the players under it, else "..." after its name
	bool expanded {true};
	/// The speech button of the heading
	bool messagesOn {true};
	std::vector<Person> players;
	/// The row of the last player that a click selected, for a click with Shift
	std::optional<size_t> lastSelected;
};

/// The other players of the game, in the order of their god slots, each selected
[[nodiscard]] List MakeList(std::vector<Person> players);
/// The rows of the list: the heading first, then each player when the list is expanded
[[nodiscard]] size_t RowCount(const List& list) noexcept;

/// A click on a row of the list:
/// - On the heading, the click removes the selection. On the speech button of the heading it turns the messages on or
///   off, and the list expands with them. Elsewhere it expands or collapses the list.
/// - On a selected player with no modifier key, the click removes the selection of all players.
/// - Else, with no Ctrl, the click first removes the selection of all players.
/// - With Shift, the players from the last player that a click selected to this one change as this player changes.
/// - Else this player changes, and is the last player that a click selected.
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

} // namespace people

/// The width of the line of the chat: its text and 20 more, at least 90 and at most half of the layout
[[nodiscard]] int EditWidth(float textWidth, int layoutWidth) noexcept;

} // namespace openblack::chat
