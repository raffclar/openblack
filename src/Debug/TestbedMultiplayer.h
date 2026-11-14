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

#include <memory>
#include <string>

namespace openblack::gui
{
class MultiplayerFrontEnd;
}

/// The testbed's multiplayer front end: a demonstration lobby and game socket in the game, with no network, that put
/// each box of the front end on the screen in one step
namespace openblack::testbed_multiplayer
{

/// The boxes and pages that a scenario can show
enum class Stage : uint8_t
{
	ChooseConnection,
	FindGames,
	Login,
	GameList,
	RoomChat,
	RoomLandscape,
	RoomClans,
	RoomPlayerDetails,
	Conditions,
	WaitingForStart,
	LanGameList,
	NotRegistered,
	/// The chat of the room of 32 gods, a line from each god
	RoomChat32,
	/// The join of a full room, which the player comes into as an observer
	FullRoomObserver,
	/// The start of the game: the creature files go to all players
	StartTransfer,
	/// The start of the game stops: another player does not have the land
	StartCancelled,
	/// The start of the game stops: the player's land file is not the server's
	KickedFromGame,
	/// The game room's chat, with the ping after the player's own name (deviation 143, as --show-ping does)
	RoomPing,
	/// The game room's box "Could not download map from Internet.": the lobby did not give the room's land
	MapDownloadFailed,
	/// The conditions page of a game whose server picked winning conditions and a time limit
	ConditionsPicked,
	/// The chat of a room on the local network, which goes through the room's game server
	LanRoomChat,
	/// The private lines of the chat of a room on the local network: the player picked a player in the roster
	LanRoomPrivateChat,
	/// The lobby list of the lobby sessions, with a session picked
	LobbyList,
	/// The box "Create session"
	LobbyCreate,
	/// The room of a session that the player made: the host's controls, the ready ticks, a player without the map
	LobbyRoomHost,
	/// The room of a session that the player joined, ready
	LobbyRoomPlayer,
	/// The room waits in the line for a free game server
	LobbyQueue,
	/// The host kicked the player: the box tells it over the lobby list
	LobbyKicked,
	/// The join of a session with a password asks for the password
	LobbyPassword,
	/// The host tries a map with too few places for the players of the room
	LobbyMapTooSmall,
	/// The host opens the box of the settings of the session before the start
	LobbySettings,
	/// The main lobby room at the right of the lobby list: the people, the last lines, a line of the staff and a line
	/// of the player
	LobbyChatRoom,
	/// The events of a joined room give their notices one after the other: a join, a ready, the countdown and a kick
	LobbyNotices,
	_Count
};

/// The front end with the demonstration services, on its way to the stage. It gets there over the next frames.
[[nodiscard]] std::unique_ptr<gui::MultiplayerFrontEnd> MakeDemoFrontEnd(Stage stage, std::u16string profileName,
                                                                         std::string localMaps, std::string conditionTemplate);

} // namespace openblack::testbed_multiplayer
