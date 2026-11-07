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
#include <SDL_mouse.h>

namespace openblack::input
{

/// The left, middle and right buttons of the mouse, as the events of the window tell them.
///
/// A press or a let go sets the button. The mouse motion also sets all three from its button mask. Thus a press or a
/// let go that the menu or the debug windows took does not keep a button down, as on a move out of the temple.
struct PointerButtons
{
	bool left {false};
	bool middle {false};
	bool right {false};

	/// Applies the event to the buttons. The result is true when the event lets go of the right button: a let go of
	/// the button, or a motion with no right button in its mask, while the button was down.
	[[nodiscard]] bool Apply(const SDL_Event& event)
	{
		const bool pressOrLetGo = event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP;
		const bool down = event.type == SDL_MOUSEBUTTONDOWN;
		if (pressOrLetGo && event.button.button == SDL_BUTTON_LEFT)
		{
			left = down;
		}
		if (pressOrLetGo && event.button.button == SDL_BUTTON_MIDDLE)
		{
			middle = down;
		}
		const bool rightLetGo = right && ((event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) ||
		                                  (event.type == SDL_MOUSEMOTION && (event.motion.state & SDL_BUTTON_RMASK) == 0));
		if (pressOrLetGo && event.button.button == SDL_BUTTON_RIGHT)
		{
			right = down;
		}
		if (event.type == SDL_MOUSEMOTION)
		{
			left = (event.motion.state & SDL_BUTTON_LMASK) != 0;
			middle = (event.motion.state & SDL_BUTTON_MMASK) != 0;
			right = (event.motion.state & SDL_BUTTON_RMASK) != 0;
		}
		return rightLetGo;
	}
};

/// What the buttons of the mouse do with the hand after an event. The hand and the camera read these flags.
struct HandButtons
{
	/// The hand grips the land: the middle button, or the left button not on a creature and not for a fight
	bool gripping {false};
	/// The camera turns: the middle button, or the left and the right buttons together
	bool rotating {false};
	/// The select button holds the land, not on a creature and not for a fight
	bool selectGrip {false};
	/// The other button (the right) is down
	bool otherButton {false};
};

/// The flags of the hand from the buttons. In the temple the hand grips nothing, because the temple has no land and its
/// camera takes the clicks. A miracle in the hand does not stop a grip of the land.
[[nodiscard]] constexpr HandButtons HandButtonsOf(const PointerButtons& buttons, bool inTemple, bool onCreature,
                                                  bool fightPressed)
{
	return {
	    .gripping = !inTemple && (buttons.middle || (buttons.left && !onCreature && !fightPressed)),
	    .rotating = !inTemple && (buttons.middle || (buttons.left && buttons.right)),
	    .selectGrip = !inTemple && buttons.left && !onCreature && !fightPressed,
	    .otherButton = buttons.right,
	};
}

} // namespace openblack::input
