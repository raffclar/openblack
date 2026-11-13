/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec2.hpp>

namespace openblack::input
{

/// Holds the game's cursor still while the camera is turned with the mouse.
///
/// Turning the camera around the hand (the middle button) or turning and zooming it with both buttons is driven by the
/// mouse's movement alone. While it lasts the game stops following the pointer: the cursor, and the hand drawn at it,
/// stay where they were when the turning started. When it ends the pointer is put back at that spot, so the hand
/// doesn't jump to wherever the pointer had wandered.
/// The conditions that let the mouse hold the cursor while it turns the camera
struct FreezeGate
{
	/// The player is in the temple, where the camera is not turned with the mouse
	bool inTemple {false};
	/// The world's own camera is the one in use
	bool worldCamera {false};
	/// The land's scripts let the mouse turn the camera round the hand
	bool rotateAroundMouse {false};
	/// The camera was put back inside the fence and ignores its controls until all of them are let go
	bool heldBackAtFence {false};
	/// A box of the menu is on the screen
	bool frontEndBox {false};
};

/// Whether the mouse may hold the cursor while it turns the camera. The camera that ignores its controls lets the
/// cursor go, and the cursor then follows the pointer again.
[[nodiscard]] constexpr bool FreezeAllowed(const FreezeGate& gate)
{
	return !gate.inTemple && gate.worldCamera && gate.rotateAroundMouse && !gate.heldBackAtFence && !gate.frontEndBox;
}

class CursorFreeze
{
public:
	struct Result
	{
		/// Where the game's cursor is this frame
		glm::ivec2 cursor;
		/// The turning started this frame: the pointer should be hidden and held
		bool started {false};
		/// The turning ended this frame: the pointer should be shown again and moved here
		std::optional<glm::ivec2> warpTo;
	};

	/// Once a frame, with whether the camera is being turned with the mouse and where the pointer is
	[[nodiscard]] Result Update(bool freeze, glm::ivec2 pointer);

	[[nodiscard]] bool IsFrozen() const { return _frozenAt.has_value(); }

private:
	std::optional<glm::ivec2> _frozenAt;
};

} // namespace openblack::input
