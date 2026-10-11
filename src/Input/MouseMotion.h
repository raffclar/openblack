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

/// The mouse's own movement over a frame, as the game reads it from the mouse itself.
///
/// When the game puts the cursor somewhere (as dragging round the screen's edge holds it on a ring), the window
/// reports the jump as a movement of the pointer with the next frame's events. That jump isn't the player moving the
/// mouse, so it is taken off again: only the mouse's own movement is left.
class MouseMotion
{
public:
	/// A new frame starts: the jumps of the cursor put since the last one arrive with this frame's events
	void NewFrame()
	{
		_delta = -_putBy;
		_putBy = glm::ivec2(0);
	}
	/// The window reports the pointer moving by so much
	void Moved(glm::ivec2 by) { _delta += by; }
	/// The game put the cursor somewhere, so far from where it was
	void Put(glm::ivec2 jump) { _putBy += jump; }

	/// How far the mouse itself moved this frame
	[[nodiscard]] glm::ivec2 Delta() const { return _delta; }

private:
	glm::ivec2 _delta {0};
	glm::ivec2 _putBy {0};
};

} // namespace openblack::input
