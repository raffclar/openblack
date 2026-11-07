/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <optional>

#include "Enums.h"

union SDL_Event;

namespace openblack::ecs::systems
{

/// The keys of the game on this computer: the miracle keys, the full screen, the pause, the speed keys, the developer
/// keys, the camera bookmarks, the temple's room keys, the leash shortcuts and the one-press actions of the options
/// screen. The turns of the world do not read this system, and the dedicated server does not make it.
class GameKeysSystemInterface
{
public:
	virtual ~GameKeysSystemInterface() = default;

	/// Does the work of a key that goes down. The result is false when the key stops the game.
	virtual bool KeyDown(const SDL_Event& event, bool inTemple) = 0;
	/// The keys of the temple's rooms, once a frame after the events
	virtual void UpdateTempleRoomKeys() = 0;
	/// The one-press actions of the options screen, once a frame after the events
	virtual void UpdateShortcutKeys() = 0;
	/// The one-press actions start at the temple of the player
	virtual void StartShortcutKeysAtTemple(PlayerNames player) = 0;
	/// Sends the commands of the leash shortcuts that went down in this frame
	virtual void SubmitKeyCommands() = 0;

	/// The menu closed now, on the clock of the computer
	virtual void MenuClosed() = 0;
	/// When the menu last closed, on the clock of the computer. Escape does not open the menu again for a short time.
	[[nodiscard]] virtual const std::optional<std::chrono::steady_clock::time_point>& GetMenuClosedAt() const = 0;
};

} // namespace openblack::ecs::systems
