/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SystemPointer.h"

#include <SDL_events.h>
#include <SDL_mouse.h>
#include <SDL_video.h>

namespace openblack::windowing::system_pointer
{

void Warp(SDL_Window* window, glm::ivec2 position, bool background)
{
	if (!MayTouch(background) || window == nullptr)
	{
		return;
	}
	SDL_WarpMouseInWindow(window, position.x, position.y);
}

bool SetRelative(bool relative, bool background)
{
	if (!MayTouch(background))
	{
		return false;
	}
	return SDL_SetRelativeMouseMode(relative ? SDL_TRUE : SDL_FALSE) == 0;
}

void Show(bool show, bool background)
{
	if (!MayTouch(background))
	{
		return;
	}
	SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE);
}

void Grab(SDL_Window* window, bool grab, bool background)
{
	if (!MayTouch(background) || window == nullptr)
	{
		return;
	}
	SDL_SetWindowGrab(window, grab ? SDL_TRUE : SDL_FALSE);
}

} // namespace openblack::windowing::system_pointer
