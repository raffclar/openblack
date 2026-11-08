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

namespace openblack::ecs::components
{

/// On the hand of the player at this computer: the hand sent a command that the wait covers, and the turn did not do it
/// yet (see hand_send_wait). Only this computer keeps it. The world does not read it.
struct HandSendWait
{
	/// The world save and the world parts that the server sends do not keep it
	static constexpr bool k_LocalOnly = true;

	/// The game turn of the last send, zero when no send waits
	uint32_t sentTurn {0};
	/// The game turn that last did a command of the player that the wait covers, zero after the clear
	uint32_t doneTurn {0};
};

} // namespace openblack::ecs::components
