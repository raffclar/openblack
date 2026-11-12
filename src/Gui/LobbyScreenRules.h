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

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Lobbies.h>
#include <LobbyApi.h>

#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "Online/LobbyRoom.h"

/// The rules of the screens of the lobby sessions: the session list, the box that makes a session and the room of a
/// session. The rules have no state and make no calls, so that the tests can check them.
namespace openblack::gui::lobby_screens
{

/// The columns of the session list. A lock in front of the name shows a session with a password.
enum class Column : uint8_t
{
	Name,
	Host,
	Map,
	Gods,
	Status,
};
inline constexpr size_t k_ColumnCount = 5;

/// The order of the session list: a column and its direction, or none for the default order
struct Sort
{
	std::optional<Column> column;
	bool descending {false};
};

/// A click on the heading of a column: the first click sorts up, the second sorts down, the third goes back to the
/// default order
[[nodiscard]] Sort ClickHeading(Sort sort, Column column) noexcept;
/// The text of a heading, with "^" or "v" after the heading of the sort column
[[nodiscard]] std::u16string HeadingText(std::u16string_view name, Column column, Sort sort);

/// What the status column of a session shows
enum class Status : uint8_t
{
	Open,
	/// Every god slot that takes a player has a player
	Full,
	/// The host started the countdown
	Starting,
	/// The session waits in the line for a free game server
	Queued,
	/// The game of the session started
	InGame,
	/// The session needs a different build of openblack
	WrongVersion,
};

[[nodiscard]] Status StatusOf(const lobby_api::LobbySummary& lobby, std::string_view ownBuild);
/// The text of the status column. The time is milliseconds since 1970 (UTC), for the time that a game plays.
[[nodiscard]] std::u16string StatusText(const lobby_api::LobbySummary& lobby, Status status, int64_t nowMs,
                                        std::u16string_view wrongVersionText);
/// "11/32": the players in god slots and the god slots
[[nodiscard]] std::u16string GodsText(const lobby_api::LobbySummary& lobby);
/// The name column: "# " in front of the name of a session with a password
[[nodiscard]] std::u16string NameText(const lobby_api::LobbySummary& lobby);
/// The sessions that the filter shows, in the order of the sort
[[nodiscard]] std::vector<lobby_api::LobbySummary> ShownLobbies(std::span<const lobby_api::LobbySummary> lobbies,
                                                                const online::lobby::ListFilter& filter,
                                                                std::string_view ownBuild, Sort sort);

/// The width of a text at a size, from the font
using MeasureText = std::function<float(std::u16string_view text)>;
/// The text, made shorter with "..." at its end until it fits the width
[[nodiscard]] std::u16string FitText(std::u16string_view text, float width, const MeasureText& measure);

/// The line under the list: the free game servers
[[nodiscard]] std::u16string ServersText(const std::optional<lobby_api::PoolState>& pool);
/// The line about the map of a session: its name, its host, and the bytes to download for a map of the service
[[nodiscard]] std::u16string DetailText(const lobby_api::LobbySummary& lobby, std::u16string_view mapName);

/// What a join of the picked session does
enum class JoinStep : uint8_t
{
	Join,
	/// The session needs a password: the box asks for it
	AskPassword,
	/// The session takes no player now: the box tells why
	Refuse,
};
[[nodiscard]] JoinStep JoinStepOf(const lobby_api::LobbySummary& lobby, std::string_view ownBuild);
/// The code of the problem that a join of a session that takes no player gives, as the service gives it
[[nodiscard]] std::string_view RefusalOf(const lobby_api::LobbySummary& lobby, std::string_view ownBuild);

/// How the box tells a problem of a call to the service
enum class ProblemBox : uint8_t
{
	/// A box with the text and OK
	Notice,
	/// The box of the password asks again
	AskPassword,
	/// A question: "No open game. Create one?"
	AskCreate,
	/// The vanilla box "Incorrect password"
	IncorrectPassword,
	/// The vanilla box "Connection Failed."
	ConnectionFailed,
};
struct Problem
{
	ProblemBox box {ProblemBox::ConnectionFailed};
	/// The text of a Notice or of AskCreate
	std::string text;
};
/// The box for a problem of a call. Each code of the service has its own clear text, and only a problem of the network
/// shows the vanilla "Connection Failed.".
[[nodiscard]] Problem ProblemOf(const ecs::systems::LobbyError& error, lobby_api::ClientKind client);
/// The box for a refusal of the game server at the join of the game of a session ("version", "map", "ticket" and
/// others)
[[nodiscard]] std::string GameRefusalText(std::string_view refusal, lobby_api::ClientKind client);

/// The values of the box that makes a session
struct CreateForm
{
	std::u16string name;
	/// The place of the map in the list of the lands
	std::optional<size_t> map;
	int32_t godSlots {lobby_api::k_LeastGodSlots};
	bool isPrivate {false};
	std::u16string password;
	int32_t timeLimitMinutes {lobby_api::k_DefaultTimeLimitMinutes};
};
/// The steps of the time limit in the box
inline constexpr int32_t k_TimeLimitStep = 10;
/// The name that the box gives first: "<profile>'s game", at most 32 characters
[[nodiscard]] std::u16string DefaultSessionName(std::u16string_view profileName);
/// The god slots that a map takes: 2 up to its players, and at most 32
[[nodiscard]] int32_t MostGodSlots(int32_t mapPlayers) noexcept;
[[nodiscard]] int32_t ClampGodSlots(int32_t slots, int32_t mapPlayers) noexcept;
[[nodiscard]] int32_t ClampTimeLimit(int32_t minutes) noexcept;
/// Why the box cannot make the session: none when it can
[[nodiscard]] std::optional<std::string> CreateProblem(const CreateForm& form);
/// The request of the box. The map is the land of the game, or a map of the service with its SHA-256.
[[nodiscard]] lobby_api::LobbyCreateRequest CreateRequestOf(const CreateForm& form, const lobby_api::LobbyMap& map);

/// The texts of a line of the slots of the room
struct SlotTexts
{
	/// "God %d", the vanilla text of the room
	std::u16string god;
	/// " (HOST)", the vanilla text of the room
	std::u16string host;
	/// " (CURRENT)", the vanilla text of the room
	std::u16string current;
};
/// "God 3: name (HOST)", "God 4: - Open -", "God 5: - Computer -" or "God 6: - Closed -"
[[nodiscard]] std::u16string SlotLine(const online::lobby::SlotView& slot, const SlotTexts& texts);
/// The note at the right of a slot: "downloading map" for a player with no map, "away" for a player whose connection
/// went, else nothing
[[nodiscard]] std::u16string SlotNote(const online::lobby::SlotView& slot);

/// What the host can do to a slot
enum class HostAction : uint8_t
{
	Kick,
	Ban,
	MakeHost,
	Open,
	Computer,
	Closed,
};
/// The actions of the host for a slot: a player of another person can be kicked, banned or made the host; an empty slot
/// can be Open, Computer or Closed. Before the start only, but for kick and ban, which the host can also do after the
/// start.
[[nodiscard]] std::vector<HostAction> HostActionsOf(const online::lobby::LobbyRoom& room, int32_t slot);
[[nodiscard]] std::u16string_view HostActionText(HostAction action) noexcept;
/// The players that a map must take: the players in god slots of the room
[[nodiscard]] int32_t PlayersOfRoom(const online::lobby::LobbyRoom& room);
/// Whether the host can change the room to a map of some places. The host cannot pick a map with fewer places than
/// the players of the room, and must kick players first (user, 2026-11-12).
[[nodiscard]] bool MapFitsRoom(int32_t mapPlaces, int32_t players) noexcept;
/// The text of the box that refuses a map with too few places: "Remove players first: this land has 4 places."
[[nodiscard]] std::string TooManyPlayersText(int32_t mapPlaces);
/// The names of the members that do not have the map of the room yet, in the order of the members
[[nodiscard]] std::vector<std::string> PlayersWithoutMap(const online::lobby::LobbyRoom& room);
/// The text that shows when the pointer is on the warning symbol of a player without the custom map. The words are
/// the user's words (2026-11-12).
inline constexpr std::u16string_view k_MapWarningText =
    u"This players needs to download the custom map first and may delay the start of play.";
/// Whether the slot shows the warning symbol: the room plays a custom map, and the player of the slot does not have it
/// yet. The host and all the players see the symbol.
[[nodiscard]] bool ShowsMapWarning(const online::lobby::LobbyRoom& room, const online::lobby::SlotView& slot);
/// Whether the Start button of the host shows the same warning symbol: a member does not have the custom map yet
[[nodiscard]] bool StartShowsMapWarning(const online::lobby::LobbyRoom& room);
/// The line that each loaded player sees while the game waits for the players that still load. The words are the
/// user's words (2026-11-12).
inline constexpr std::u16string_view k_LoadPauseText = u"The game is paused: waiting for remaining players to load";
/// The lines of the load pause: the line above with the time to the end of the pause, "(1:59)", and the names of the
/// players that still load on a second line. The game server gives the end of the pause, so that each player sees the
/// same time (user, 2026-11-12). The times are milliseconds since 1970 (UTC). No lines when no player loads.
[[nodiscard]] std::vector<std::u16string> LoadPauseLines(std::span<const std::string> loadingNames, int64_t deadlineMs,
                                                         int64_t nowMs);
/// The whole seconds to the end of the pause, rounded up, and 0 after the end
[[nodiscard]] int64_t LoadPauseSecondsLeft(int64_t deadlineMs, int64_t nowMs) noexcept;
/// The kinds of the slots after the host sets the kind of an empty slot
[[nodiscard]] std::vector<lobby_api::SlotKind> SlotKindsAfter(const lobby_api::Lobby& lobby, int32_t slot,
                                                              lobby_api::SlotKind kind);

/// The line under the caption of the room: the code, the password, the phase
[[nodiscard]] std::u16string RoomStatusText(const online::lobby::LobbyRoom& room);
/// What the button at the bottom right of the room does
enum class StartButton : uint8_t
{
	/// The player is not the host, or the game started: the button does not show
	None,
	Start,
	/// The countdown or the wait in the line: the host can stop it
	Cancel,
};
[[nodiscard]] StartButton StartButtonOf(const online::lobby::LobbyRoom& room) noexcept;
/// Leave asks first when the player is ready or is the host
[[nodiscard]] bool LeaveAsks(const online::lobby::LobbyRoom& room) noexcept;

/// The map that a session needs from the player
enum class MapNeed : uint8_t
{
	/// The player has the land of the game
	Have,
	/// The map is a map of the service: the game downloads it, and checks it by its SHA-256
	Download,
	/// The land of the game is not on this computer
	Missing,
};
[[nodiscard]] MapNeed MapNeedOf(const lobby_api::LobbyMap& map, bool hasLandOfGame) noexcept;

/// The keys of the screens of the sessions
enum class KeyAction : uint8_t
{
	None,
	/// F5: get the list again
	Refresh,
	/// Ctrl+F: the search box takes the keyboard
	Search,
	/// Ctrl+J: the box of the code of a private session
	JoinByCode,
	/// R: ready on or off, when no edit box has the keyboard
	ToggleReady,
	/// Ctrl+Return: the host starts the game
	Start,
	/// Escape: back, or leave the room
	Back,
	/// Return in the list: join the picked session
	Join,
};
/// The screen that has the keys
enum class KeyScreen : uint8_t
{
	List,
	Create,
	Room,
};
struct KeyPress
{
	int key {0};
	bool control {false};
	/// An edit box has the keyboard: the letters go to it
	bool editing {false};
};
[[nodiscard]] KeyAction KeyActionOf(KeyScreen screen, const KeyPress& press) noexcept;

} // namespace openblack::gui::lobby_screens
