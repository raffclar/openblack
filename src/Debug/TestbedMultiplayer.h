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
	_Count
};

/// The front end with the demonstration services, on its way to the stage. It gets there over the next frames.
[[nodiscard]] std::unique_ptr<gui::MultiplayerFrontEnd> MakeDemoFrontEnd(Stage stage, std::u16string profileName,
                                                                         std::string localMaps, std::string conditionTemplate);

} // namespace openblack::testbed_multiplayer
