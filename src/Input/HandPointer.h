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

/// The point on the screen the hand is put under.
///
/// The game places the hand on the line of sight through the pointer every frame, whether the mouse moved or not, so
/// from the first frame of a land the hand is under the pointer. The pointer is only known once the cursor has been
/// over the window (or an agent or script holds it): until then it is the top left corner of the window, as it was in
/// the original game until its window first heard from the mouse. The window only reports the pointer when the mouse
/// moves, so until it first does, the hand follows the pointer the game itself keeps.
class HandPointer
{
public:
	/// The mouse moved, or the game put the pointer somewhere: from now on the hand follows what the window reports
	void Moved() { _moved = true; }

	/// Where the hand is put under this frame: what the window reported last, or before the mouse has moved, where the
	/// game's pointer is, the top left corner while that isn't known yet
	[[nodiscard]] glm::ivec2 Frame(glm::ivec2 reported, std::optional<glm::ivec2> gamePointer) const
	{
		return _moved ? reported : gamePointer.value_or(glm::ivec2 {0, 0});
	}

	[[nodiscard]] bool HasMoved() const { return _moved; }

private:
	bool _moved {false};
};

} // namespace openblack::input
