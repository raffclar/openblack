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

#include "ECS/Systems/GameKeysSystemInterface.h"
#include "Input/ShortcutKeys.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class GameKeysSystem final: public GameKeysSystemInterface
{
public:
	bool KeyDown(const SDL_Event& event, bool inTemple) final;
	void UpdateTempleRoomKeys() final;
	void UpdateShortcutKeys() final { _shortcutKeys.Update(); }
	void StartShortcutKeysAtTemple(PlayerNames player) final { _shortcutKeys.StartAtTemple(player); }
	void SubmitKeyCommands() final;
	void MenuClosed() final { _menuClosedAt = std::chrono::steady_clock::now(); }
	[[nodiscard]] const std::optional<std::chrono::steady_clock::time_point>& GetMenuClosedAt() const final
	{
		return _menuClosedAt;
	}

private:
	/// The key switch of the game: Escape, the full screen, the pause and the other keys. False stops the game.
	static bool ProcessKeyPress(const SDL_Event& event);
	/// The camera's bookmark keys work while no script holds the interface and the help lets the camera move
	[[nodiscard]] static bool BookmarkKeysAllowed();

	/// The options screen's one-press actions: the temple and realm keys, the villagers' names and details
	input::ShortcutKeys _shortcutKeys;
	/// When the menu last closed, on the computer's clock
	std::optional<std::chrono::steady_clock::time_point> _menuClosedAt;
};

} // namespace openblack::ecs::systems
