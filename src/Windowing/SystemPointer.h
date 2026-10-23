/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>

struct SDL_Window;

/// The pointer of the system (the user's mouse cursor): the only place where the game moves it, hides it, holds it in
/// the window or captures it. A background run (a tool or an agent, --background) never touches the pointer of the
/// system, because the pointer is the user's: each function does nothing in a background run. A check of the build
/// (tools/build/check_system_pointer.py, test test_system_pointer_calls) finds a call of the system pointer anywhere
/// else.
namespace openblack::windowing::system_pointer
{

/// Whether the game may move, hide, hold or capture the pointer of the system
[[nodiscard]] constexpr bool MayTouch(bool background) noexcept
{
	return !background;
}

/// Puts the pointer of the system at a place in the window
void Warp(SDL_Window* window, glm::ivec2 position, bool background);

/// Holds the pointer of the system in the window and gives the movements of the mouse only (relative mode), or lets it
/// go. Gives true when the mode changed, false in a background run or when the system refused it.
bool SetRelative(bool relative, bool background);

/// Shows or hides the pointer of the system over the window
void Show(bool show, bool background);

/// Keeps the pointer of the system in the window, or lets it go
void Grab(SDL_Window* window, bool grab, bool background);

} // namespace openblack::windowing::system_pointer
