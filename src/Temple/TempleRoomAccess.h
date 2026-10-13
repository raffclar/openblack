/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "3D/TempleInteriorInterface.h"

/// Which rooms of the temple are open to the player in a single player game
namespace openblack::temple
{

/// What a room being open depends on
struct RoomAccessFacts
{
	/// The main room has been drawn as the room the player is in, or the room the camera goes into, since the game
	/// started. Before that, only the main room is open.
	bool roomsOpened {false};
	/// The player has a creature
	bool hasCreature {false};
	/// The room the player is kept in once the game is over, if any. That room is open.
	std::optional<TempleRoom> keptIn;
};

/// Whether the player can go into a room: through its door, with its key, or into the temple straight to it
[[nodiscard]] constexpr bool IsRoomAvailable(TempleRoom room, const RoomAccessFacts& facts)
{
	switch (room)
	{
	case TempleRoom::Main:
		return true;
	case TempleRoom::CreatureCave:
		return facts.hasCreature && facts.roomsOpened;
	case TempleRoom::Multi:
	case TempleRoom::SaveGame:
		return facts.keptIn == room || facts.roomsOpened;
	case TempleRoom::Challenge:
	case TempleRoom::Options:
	case TempleRoom::Credits:
		return facts.roomsOpened;
	default:
		return false;
	}
}

} // namespace openblack::temple
