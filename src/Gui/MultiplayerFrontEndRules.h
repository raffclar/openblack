/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <LobbyApi.h>
#include <glm/vec4.hpp>

#include "ECS/GameEndConditions.h"
#include "ECS/Systems/LobbyClientSystemInterface.h"

/// The rules of the multiplayer front end: the boxes over the temple's future room that find a game and show its room.
/// Pure functions with no global state.
namespace openblack::gui::multiplayer
{

/// The boxes, in the order that the player goes through them
enum class Box : uint8_t
{
	ChooseConnection,
	FindGames,
	Login,
	GameList,
	/// The game room with its tabs
	Room,
	/// The page of the game room behind Next: the winning conditions, the time limit and Ready
	Conditions,
};

/// How the player finds the games
enum class LobbyKind : uint8_t
{
	/// The lobby service on the internet
	Internet,
	/// The game servers on the local network, with no lobby
	Lan,
};

/// The tabs of the game room, in the order of the game: Clans and Player Details show only on the internet
enum class RoomTab : uint8_t
{
	Chat,
	Landscape,
	Clans,
	PlayerDetails,
};
inline constexpr size_t k_RoomTabCount = 4;

/// The most characters of the name and the password in the login box
inline constexpr size_t k_MostLoginCharacters = 32;
/// The most characters of a line in the chat box
inline constexpr size_t k_MostChatCharacters = lobby_api::k_MostChatCharacters;

/// What a box tells the player in a message box over it
enum class Message : uint8_t
{
	None,
	/// "No connections available. Please install an internet connection or LAN.", with OK
	NoConnections,
	/// "Incorrect password", with OK
	IncorrectPassword,
	/// "You are not registered. Please visit the Black & White registration pages.", with Yes and No. Yes opens the
	/// registration page.
	NotRegistered,
	/// "Invalid details. Please try again.", with OK
	InvalidDetails,
	/// "Connection Failed.", with OK: the lobby did not give the list of the games, or a join of a game on the internet
	/// failed
	ConnectionFailed,
	/// "Could not connect to remote machine.", with OK: the join of a game on the local network failed
	CouldNotConnect,
	/// "A person with this registration name was already in the game.", with OK: the server has the account in its room
	AlreadyInRoom,
	/// The account has no email address. The box has the text of NotRegistered, with Yes and No. Yes opens the account
	/// page of the lobby's site.
	EmailRequired,
	/// "The channel host left the channel that you were in.", with OK: the game socket closed while the player was in
	/// the room. OK goes back to the game list.
	HostLeft,
	/// "Waiting for host to start transferring files...", with no button: the player has the land of the start, and the
	/// server waits for the other players' replies
	WaitingForTransfer,
	/// "Sending/Receiving start data...", with no button: the creature files of the gods go to all players
	SendingStartData,
	/// "The host tried to start the game but you are still downloading the map.", with OK: the player does not have the
	/// land of the start
	StillDownloading,
	/// "The game could not be started because other people are still downloading the map.", with OK: another player
	/// does not have the land of the start
	OthersDownloading,
	/// "%s has kicked you from the game.", with OK, with the name of the room's server: the server removed the player
	/// from the room. OK goes back to the game list.
	KickedFromGame,
};

/// The buttons of a message box, as the game's message boxes have them
enum class MessageButtons : uint8_t
{
	/// One OK in the middle
	Ok,
	/// Yes at the left, No at the right
	YesNo,
	/// No button: the box goes when the work is done
	None,
};

[[nodiscard]] MessageButtons ButtonsOf(Message message) noexcept;

/// What the login box finds wrong in its fields before it calls the lobby
enum class LoginProblem : uint8_t
{
	None,
	/// The name is not 3 to 32 letters and digits
	InvalidDetails,
	/// The password is empty
	IncorrectPassword,
};

[[nodiscard]] LoginProblem CheckLogin(std::u16string_view name, std::u16string_view password);

/// The message for a failed login. The lobby gives the same answer for an unknown name and a wrong password, so the
/// box asks the lobby if the name is free: a free name has no account, and the player is not registered.
/// @param nameAvailable The lobby's answer about the name, none when the box did not ask or got no answer
[[nodiscard]] Message LoginFailureMessage(ecs::systems::LobbyErrorKind error, std::optional<bool> nameAvailable) noexcept;

/// Whether the box asks the lobby about the name after a failed login
[[nodiscard]] bool AsksAboutName(ecs::systems::LobbyErrorKind error) noexcept;

/// The message when the lobby gives no ticket for a game. A full room is not here: the front end then asks for an
/// observer's ticket. The game shows "Connection Failed." for each failure of a join on the internet.
/// @param code The code of the lobby's problem, empty when there is none
[[nodiscard]] Message TicketFailureMessage(ecs::systems::LobbyErrorKind error, std::string_view code) noexcept;

/// The message when the join of the game server fails
/// @param refusal The reason that the server gave in its refusal, empty when the socket closed with no refusal
[[nodiscard]] Message JoinFailureMessage(std::string_view refusal, LobbyKind kind) noexcept;

/// A value for a text with printf codes: %s takes text, %d and %u take a number
using TextArgument = std::variant<std::u16string_view, int64_t>;

/// The text with its %s, %d, %u and %02u codes replaced by the arguments in order, and %% by %. A code with no
/// argument stays as it is.
[[nodiscard]] std::u16string FormatText(std::u16string_view format, std::span<const TextArgument> arguments);

/// The texts of the game list's lines
struct GameLineTexts
{
	/// " - PLAYING"
	std::u16string playing;
	/// " (Wrong Version)"
	std::u16string wrongVersion;
	/// "(Needs Password)"
	std::u16string needsPassword;
	/// "Locked"
	std::u16string locked;
};

/// A line of the game list: "name (players/max)" and, as they apply, " - PLAYING", " (Wrong Version)",
/// "(Needs Password)" and "-Locked". A game with no maximum shows "name (players)". Only the internet list shows
/// "-Locked".
[[nodiscard]] std::u16string GameLine(const lobby_api::ServerEntry& server, const GameLineTexts& texts,
                                      std::string_view gameVersion, LobbyKind kind);

/// Whether a game name has the filter in it, without regard to case. An empty filter takes every name.
[[nodiscard]] bool MatchesFilter(std::u16string_view name, std::u16string_view filter);

/// The games of the list that the filter takes, by their places in the list. The filter applies only while no game is
/// picked, because the filter and the game's password share one box.
[[nodiscard]] std::vector<size_t> FilteredGames(std::span<const lobby_api::ServerEntry> servers, std::u16string_view filter,
                                                bool filterOn);

/// The lines of the connections' list: "1.Local Area Network", then the internet connection as "2. name (default)"
[[nodiscard]] std::vector<std::u16string> ConnectionLines(bool lan, std::optional<std::u16string_view> internet,
                                                          std::u16string_view lanText, std::u16string_view defaultText);

/// A place in the room of a game server: a god, or an observer
struct Seat
{
	lobby_api::AccountRef account;
	/// The god's number from 1, 0 for an observer
	int32_t god {0};
	std::optional<int64_t> clan;
};

/// The places of the room's players: the gods are numbered in the order of the list, the observers are 0
[[nodiscard]] std::vector<Seat> Seats(std::span<const lobby_api::RoomPlayer> players);

/// The texts of the lines of the room's player list
struct RosterTexts
{
	/// "God %d"
	std::u16string_view god;
	/// "Observers"
	std::u16string_view observers;
	/// " (CURRENT)"
	std::u16string_view current;
};

/// A line of the room's player list: "God 3: name" for a god, or "Observers: name". The line of the player's own place
/// has " (CURRENT)" after the name. When the ping display is on, the ping comes last on the own line only.
/// @param ping The text of the player's own ping, none when the display is off or has no number yet
[[nodiscard]] std::u16string RosterLine(const Seat& seat, std::u16string_view name, bool own, const RosterTexts& texts,
                                        const std::optional<std::u16string>& ping);

/// The most gods in a game, each with a chat colour of its own
inline constexpr int32_t k_MostGods = 32;
/// The colour of a god's lines in the chat, as 32-bit 0xAARRGGBB: the observers (and any number that is not a god's)
/// grey, gods 1 to 4 red, green, blue and yellow, and gods 5 to 32 a colour of their own each
[[nodiscard]] uint32_t GodColourBits(int32_t god) noexcept;
/// The colour of a god's lines in the chat
[[nodiscard]] glm::vec4 GodColour(int32_t god) noexcept;
/// The colour of the player's own lines in the chat: light grey, as 0xAARRGGBB
[[nodiscard]] uint32_t OwnLineColourBits() noexcept;
/// The colour of the player's own lines in the chat
[[nodiscard]] glm::vec4 OwnLineColour() noexcept;

/// A line of the chat box
struct ChatItem
{
	std::u16string text;
	glm::vec4 colour {1.0f};
};

/// The player's own lines read "name> text" in light grey; other players' lines read "name: text" in their god's
/// colour
[[nodiscard]] ChatItem ChatLine(std::u16string_view from, std::u16string_view text, bool own, int32_t god);

/// The text of a chat line, cut to the first 254 characters
[[nodiscard]] std::u16string ClampChat(std::u16string_view text);

/// The time limit as hours and minutes: "1:30"
[[nodiscard]] std::u16string TimeLimitText(uint32_t minutes);

/// The time limit of a network game in minutes, when the host does not change it
inline constexpr uint32_t k_DefaultTimeLimitMinutes = 30;

/// The parts of the conditions page that show to a player who is not the host. The host's controls never show: the
/// box of each condition, the value of a condition with its arrows, and the arrows of the time limit. Ready always
/// shows.
struct ConditionsPage
{
	/// "Please select a map first."
	bool selectMap;
	/// The list of the conditions of the game, one line for each condition that the host switched on
	bool conditions;
	/// "Time Limit (Hours:Minutes):" and the time limit
	bool timeLimit;
	bool viewChat;
};

/// With no land, the page shows only "Please select a map first." and Ready
[[nodiscard]] constexpr ConditionsPage ConditionsPageOf(bool landKnown) noexcept
{
	return {.selectMap = !landKnown, .conditions = landKnown, .timeLimit = landKnown, .viewChat = landKnown};
}

/// A land of the local maps' file, "Online Maps\localmaps.txt": a line "number:name:file:players:"
struct LocalMap
{
	std::u16string name;
	std::string file;
	int32_t players {0};
};

[[nodiscard]] std::vector<LocalMap> ParseLocalMaps(std::string_view text);

/// The land of the maps that the server plays, by its file, its name or the path of its map file. None when the player
/// does not have it.
[[nodiscard]] std::optional<size_t> FindLand(std::span<const LocalMap> maps, std::string_view land);

/// The name of the text that describes one of the first three local lands (Bombardment, King of the hill, The four
/// corners of Eden); none for the other lands
[[nodiscard]] std::optional<std::string_view> LandInfoText(size_t index) noexcept;

/// The texts of the lines of a clan's information
struct ClanInfoTexts
{
	/// "Clan Description:"
	std::u16string description;
	/// "Clan Leader:"
	std::u16string leader;
	/// "Clan URL:"
	std::u16string url;
	/// "Credits"
	std::u16string credits;
};

/// The lines of the clan's information: "Clan Description: ...", "Clan Leader: ...", "Clan URL: ..." and "Credits
/// 12.50"
[[nodiscard]] std::vector<std::u16string> ClanInfoLines(const lobby_api::ClanDetails& clan, const ClanInfoTexts& texts);

/// The grey picture of a clan's logo: 64 by 64 opacities from 0 to 15, row after row from the top
inline constexpr int32_t k_LogoSize = 64;
using LogoOpacities = std::array<uint8_t, static_cast<size_t>(k_LogoSize* k_LogoSize)>;

/// The logo from the bytes of a BMP file of 8, 24 or 32 bits a pixel. Each pixel becomes one of 256 greys, as Windows
/// changes a colour to a grey, and the top 4 bits of the grey are its opacity. The picture is cut to its top-left 64 by 64
/// pixels, and a smaller picture has black (no opacity) around it. None when the bytes are not such a file.
[[nodiscard]] std::optional<LogoOpacities> DecodeClanLogo(std::span<const uint8_t> bmp);

/// A line of the lobby's player database: a label and a value, as the server gives them
struct PlayerDetail
{
	std::u16string label;
	std::u16string value;
};

/// The lines of the Player Details tab under its title: "label:value" for each line of the player database, in its
/// order. The game shows no other lines there.
[[nodiscard]] std::vector<std::u16string> PlayerDetailLines(std::span<const PlayerDetail> details);

/// The limits of a winning condition for a land: its first value, the least and the most, and the steps of a click
/// with the left and the right mouse button
struct ConditionLimits
{
	int32_t value {0};
	int32_t least {0};
	int32_t most {0};
	int32_t leftStep {0};
	int32_t rightStep {0};
};

/// The winning conditions of a network game, in the order of the game
inline constexpr size_t k_ConditionCount = 15;
using ConditionTemplate = std::array<ConditionLimits, k_ConditionCount>;

/// The name of the text of a winning condition, such as "Number of villagers you kill"
[[nodiscard]] std::string_view ConditionText(size_t index) noexcept;

/// The conditions of a land from "Online Maps\ConditionTemplate.txt": the lines after "FORMAP <file>.txt", by the
/// names of the conditions. None when the file has no block for the land.
[[nodiscard]] std::optional<ConditionTemplate> FindConditions(std::string_view text, std::string_view landFile);

/// The number of a winning condition from its name in the template file, such as "WC_HOUSES_BUILT". None for another
/// name.
[[nodiscard]] std::optional<size_t> ConditionIndex(std::string_view key) noexcept;

/// Why the picks are not a pick that the host can make on the limits of a land, or none when they are. Each value must
/// be from the least to the most value of its condition. No limits: the template file has no block for the land.
[[nodiscard]] std::optional<std::string> ConditionsRefusal(const std::optional<ConditionTemplate>& limits,
                                                           std::span<const game_end::ConditionPick> picks);

/// The lines of the list on the conditions page, one for each condition that the host switched on. The page adds the
/// conditions in the order of the game, and each new line goes to the top. Thus the last condition of the game shows
/// first. When a condition is in the picks two times, the last value is the value of the line.
[[nodiscard]] std::vector<game_end::ConditionPick> ConditionLines(std::span<const game_end::ConditionPick> picks);

/// The text of a line on the conditions page: the name of the condition and its value, "Number of houses built (20)"
[[nodiscard]] std::u16string ConditionLineText(std::u16string_view name, int32_t value);

/// The front end's flow from box to box. It knows nothing of the lobby: the controller tells it what happened.
struct Flow
{
	Box box {Box::ChooseConnection};
	LobbyKind kind {LobbyKind::Internet};
	/// The front end ends: the player left it with Back on the first box
	bool left {false};
};

/// Next on the box: the connection goes to the find box, the find box to the login box (internet) or to the game list
/// (LAN), and the game room to its conditions page. The login box and the game list move on only when the lobby or
/// the server answers, so Next does not move them.
void Next(Flow& flow);
/// Back on the box: the first box leaves the front end. The game list goes back to the login box on the internet and
/// to the find box on the LAN. The game room and its conditions page leave the room to the game list.
void Back(Flow& flow);

} // namespace openblack::gui::multiplayer
