/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include <SDL_events.h>
#include <SDL_gamecontroller.h>

namespace openblack::input
{

/// The game controllers that the player plugs in. A controller sends no button events until the game opens it, so this
/// opens each controller that is there and each that comes later, and closes each that goes.
class ControllerDevices
{
public:
	/// Opens the controllers that are plugged in already. Call it once, after SDL started its controller part.
	void OpenConnected();
	/// Opens a controller that came, and closes a controller that went. Other events are not its.
	void Handle(const SDL_Event& event);

	[[nodiscard]] size_t Count() const noexcept { return _controllers.size(); }

private:
	struct Closer
	{
		void operator()(SDL_GameController* controller) const noexcept { SDL_GameControllerClose(controller); }
	};
	std::vector<std::unique_ptr<SDL_GameController, Closer>> _controllers;
};

} // namespace openblack::input
