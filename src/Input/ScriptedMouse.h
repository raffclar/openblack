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

#include <optional>
#include <span>

#include <glm/vec2.hpp>

#include "GameActionMapInterface.h"

union SDL_Event;

/// A scripted pointer that reaches the game with a real mouse's timing.
///
/// SDL takes a real mouse's state (where the pointer is, the buttons held) from the window's messages as it pumps them
/// into its event queue, which the game does once a frame after its action map has read the pointer. So the action map
/// reads where the mouse was at the last pump, and a move made now is seen by the action map only on the next frame,
/// while the move's event reaches the game this frame. A scripted mouse's moves and buttons are sent as events and its
/// state is taken from them in the same way, as they are pumped: in their order, and a move moves the pointer on by so
/// much from wherever it is then (the game may have put the cursor somewhere meanwhile), within the window.
namespace openblack::input
{

using ScriptedPointer = GameActionInterface::ScriptedPointer;

/// A move of the scripted mouse by so much, in pixels, for the window's queue; where it ends is reported as it is pumped
[[nodiscard]] SDL_Event PumpedMotion(glm::ivec2 by, uint32_t windowId);
/// A press or a letting go of the scripted mouse's button, with its click count, for the window's queue
[[nodiscard]] SDL_Event PumpedButton(uint8_t button, bool down, uint8_t clicks, uint32_t windowId);

/// The scripted pointer once one of its events is pumped. The event is written as the window reports it then: where the
/// pointer is and the buttons held. Other events leave it as it was
[[nodiscard]] ScriptedPointer Pump(ScriptedPointer pointer, SDL_Event& event, glm::ivec2 windowSize);
/// Pumps each of the scripted mouse's events in turn, in their order; with no scripted pointer, they are left as sent
void PumpAll(std::optional<ScriptedPointer>& pointer, std::span<SDL_Event> events, glm::ivec2 windowSize);

/// Pumps the scripted mouse's events waiting in the window's queue, before the frame takes its events from it
void PumpQueued(std::optional<ScriptedPointer>& pointer, glm::ivec2 windowSize);
/// Where the scripted pointer will be, and its buttons, once its events waiting in the window's queue are pumped
[[nodiscard]] ScriptedPointer QueuedAhead(ScriptedPointer pointer, glm::ivec2 windowSize);

} // namespace openblack::input
