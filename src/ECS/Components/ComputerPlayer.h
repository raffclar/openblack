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

#include "Enums.h"

namespace openblack::ecs::components
{

/// The computer mind of a player. A land script switches it on, and its first turn after that starts it on the land.
struct ComputerPlayer
{
	/// The mind plays the player
	bool switchedOn {false};
	/// The player is a computer player. Switching the mind on marks the player; switching it off keeps the mark.
	bool marked {false};
	/// The mind made its start on the land
	bool started {false};
	/// The player that a land script named for the size of this player's creature. Any name starts the size change.
	std::optional<PlayerNames> creatureLike;
};

} // namespace openblack::ecs::components
