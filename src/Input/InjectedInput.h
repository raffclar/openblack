/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

union SDL_Event;

/// Telling the input the game makes for itself (the testbed's scenarios, the debug inspector driving it for an agent)
/// from the player's mouse and keyboard, so that the player's can be kept out while an agent drives the game
namespace openblack::input
{

/// Marks a mouse or keyboard event made by the game itself rather than by the player's devices
void MarkInjected(SDL_Event& event);
/// Whether a mouse or keyboard event was made by the game itself
[[nodiscard]] bool IsInjected(const SDL_Event& event);
/// Whether an event is the player's mouse or keyboard at work: moving, buttons, the wheel, keys and typing
[[nodiscard]] bool IsPlayerDeviceEvent(const SDL_Event& event);

} // namespace openblack::input
