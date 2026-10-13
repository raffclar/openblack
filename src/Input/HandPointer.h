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

namespace openblack::input
{

/// The point on the screen the hand is put under.
///
/// The game places the hand on the line of sight through the pointer every frame, whether the mouse moved or not, so
/// from the first frame of a land the hand is under the pointer. The window only reports the pointer when the mouse
/// moves, though: until it first does, the hand follows the pointer the game itself keeps (where the cursor is, or
/// resting in the middle of the window while the cursor hasn't been over it), never a corner of the screen.
class HandPointer
{
public:
	/// The mouse moved, or the game put the pointer somewhere: from now on the hand follows what the window reports
	void Moved() { _moved = true; }

	/// Where the hand is put under this frame: what the window reported last, or before the mouse has moved, where the
	/// game's pointer is
	[[nodiscard]] glm::ivec2 Frame(glm::ivec2 reported, glm::ivec2 gamePointer) const
	{
		return _moved ? reported : gamePointer;
	}

	[[nodiscard]] bool HasMoved() const { return _moved; }

private:
	bool _moved {false};
};

} // namespace openblack::input
