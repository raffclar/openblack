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
#include <vector>

#include <glm/vec2.hpp>

namespace openblack::help
{

/// What decides whether a still mouse sends its messages
struct StillMessageInputs
{
	/// The game's window has the keyboard
	bool windowHasKeys {false};
	/// The input lock keeps the player's mouse and keyboard out, as while an agent drives the game
	bool playerInputBlocked {false};
	/// The camera holds the cursor while it turns
	bool cursorFrozen {false};
	/// A hand demonstration moves the hand
	bool playerMouseBlocked {false};
};

/// Whether a still mouse sends its messages. The messages come from the player's own mouse. Thus they stop while the
/// input lock keeps the player's mouse out, also when the window gets the keyboard from the desktop.
[[nodiscard]] constexpr bool SendsStillMessages(const StillMessageInputs& inputs) noexcept
{
	return inputs.windowHasKeys && !inputs.playerInputBlocked && !inputs.cursorFrozen && !inputs.playerMouseBlocked;
}

/// The help counts the movements of the mouse as the interface reads them. The window sends a message each time the
/// mouse moves. While the mouse is still, it also sends a message after each period without one, with the pointer
/// where it is. The messages add up the time from each message to the next. When this time is more than a limit, the
/// time starts again and the interface gets the place of the pointer. At each frame, the interface compares each
/// place that it got with the place before it, and the help counts each place that is different. Before the first place,
/// the place before is the top left corner.
class MouseMoves
{
public:
	/// The time of messages that the interface must get before it gets a place of the pointer, in milliseconds. The
	/// time must be more than this.
	static constexpr uint32_t k_SendAfterMilliseconds = 28;
	/// The longest time without a message while the mouse is still, in milliseconds
	static constexpr uint32_t k_StillMessageMilliseconds = 40;

	/// The mouse moved: the message's time in milliseconds and the place of the pointer
	void Message(uint32_t milliseconds, glm::ivec2 position);
	/// The frame's time in milliseconds and the place of the pointer. While the window takes the messages of a still
	/// mouse, it adds a message after each period without one.
	void Wait(uint32_t milliseconds, glm::ivec2 position, bool stillMessages);
	/// The interface's frame: the number of places that it got that are not the same as the place before
	[[nodiscard]] uint32_t TakeMoves();

private:
	std::optional<uint32_t> _lastMessage;
	/// When the last message, or the last message of a still mouse, came
	std::optional<uint32_t> _lastWake;
	uint32_t _sum {0};
	std::vector<glm::ivec2> _sent;
	/// The interface starts with its last place at the top left corner, so its first place counts unless the pointer is
	/// there
	glm::ivec2 _lastPlace {0, 0};
};

} // namespace openblack::help
