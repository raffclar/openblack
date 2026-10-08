/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MouseButtons.h"

using namespace openblack::input;

void openblack::input::ApplyMouseButton(MouseButtonsState& state, const MouseButtonEvent& event)
{
	switch (event.button)
	{
	case MouseButton::Left:
		state.left = !state.left;
		break;
	case MouseButton::Middle:
		state.middle = !state.middle;
		if (event.down)
		{
			state.middlePressPosition = event.position;
		}
		break;
	case MouseButton::Right:
		state.right = event.down;
		break;
	case MouseButton::Other:
		break;
	}
}

glm::ivec2 openblack::input::MouseMotion(MouseMotionState& state, const glm::ivec2& position)
{
	if (!state.previous.has_value())
	{
		state.previous = position;
	}
	const auto delta = position - *state.previous;
	state.previous = position;
	return delta;
}
