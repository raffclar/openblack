/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::gui::box_buttons
{

/// The mouse buttons that the flag reads. The other buttons of a mouse do not count.
struct Buttons
{
	bool left {false};
	bool right {false};
	bool middle {false};
};

/// The game's flag that a mouse button is held, which the tattoo editor's creature reads for its look at the pointer.
/// - When a box becomes the active box, the flag takes the left button only.
/// - Each pass of the interface while a box is active sets the flag from the left, right and middle buttons.
/// - The pass of each frame comes after the draw. Thus the draw reads the buttons of the frame before.
/// - A pass also comes on each game turn, before the draw. Then the draw reads the buttons of this frame.
class HeldFlag
{
public:
	/// One frame while the box is active, before its draw. True when the box becomes the active box in this frame.
	constexpr void Frame(Buttons buttons, bool becameActive) noexcept
	{
		const bool any = buttons.left || buttons.right || buttons.middle;
		_drawn = becameActive ? buttons.left : _passed;
		_drawnAfterTurn = becameActive ? buttons.left : any;
		_passed = any;
	}

	/// What the draw of this frame reads. True when a game turn comes before the draw in this frame.
	[[nodiscard]] constexpr bool Drawn(bool turnBeforeDraw) const noexcept { return turnBeforeDraw ? _drawnAfterTurn : _drawn; }

private:
	bool _passed {false};
	bool _drawn {false};
	bool _drawnAfterTurn {false};
};

} // namespace openblack::gui::box_buttons
