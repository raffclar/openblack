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

#include "PlayerSlots.h"

namespace openblack::computer_player
{

/// The alignment that a computer god and its creature start a skirmish or a network game at (deviation 280). The
/// player of this computer or the host of the network game chooses it for each computer god.
enum class StartAlignment : uint8_t
{
	/// The god and its creature start at the alignments that the game gives them
	Vanilla,
	/// The god and its creature each start at a random alignment
	Random,
};

/// The start alignment of each player, by player number
using StartAlignments = std::array<StartAlignment, player_slots::k_PlayerNumbers>;

} // namespace openblack::computer_player
