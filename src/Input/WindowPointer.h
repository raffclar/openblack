/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <SDL_events.h>
#include <glm/vec2.hpp>

/// The place of the pointer for the screens that read the pointer themselves, as the menus and the start-up boxes do
namespace openblack::input
{

/// The place of the pointer in the window.
/// - On the desktop: the place of the system cursor that SDL knows.
/// - On the web: the place of the pointer of the game. While the page locks the pointer, the game moves its own
///   pointer, and SDL does not know its place.
[[nodiscard]] glm::ivec2 WindowPointer();

/// Give the place of the pointer of the game to a mouse event of a screen that takes the events itself. This is
/// necessary on the web only, where the game moves its own pointer. On the desktop the call does nothing.
void PointWindowEvent(SDL_Event& event);

} // namespace openblack::input
