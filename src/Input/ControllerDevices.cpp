/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ControllerDevices.h"

#include <algorithm>

#include <SDL.h>

using namespace openblack::input;

void ControllerDevices::OpenConnected()
{
	if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0)
	{
		return;
	}
	for (int index = 0; index < SDL_NumJoysticks(); ++index)
	{
		if (SDL_IsGameController(index) == SDL_TRUE)
		{
			if (auto* controller = SDL_GameControllerOpen(index); controller != nullptr)
			{
				_controllers.emplace_back(controller);
			}
		}
	}
}

void ControllerDevices::Handle(const SDL_Event& event)
{
	if (event.type == SDL_CONTROLLERDEVICEADDED)
	{
		auto* controller = SDL_GameControllerOpen(event.cdevice.which);
		if (controller == nullptr)
		{
			return;
		}
		// SDL gives the same controller again when it is open already: keep one handle
		const auto known =
		    std::ranges::any_of(_controllers, [controller](const auto& open) { return open.get() == controller; });
		if (known)
		{
			SDL_GameControllerClose(controller);
			return;
		}
		_controllers.emplace_back(controller);
	}
	else if (event.type == SDL_CONTROLLERDEVICEREMOVED)
	{
		std::erase_if(_controllers, [&event](const auto& open) {
			return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(open.get())) == event.cdevice.which;
		});
	}
}
