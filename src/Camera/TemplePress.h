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

namespace openblack::temple_press
{

/// The mouse button as the temple's camera sees it
enum class Button : uint8_t
{
	None = 0,
	Held = 1,
	DoubleClick = 2,
};

/// While the temple's fade covers more than this, a held press does nothing to the camera: coming in, under the fade
/// from 1.2, the player can't cut the flight short for the first second
constexpr float k_FadeThatHoldsPresses = 0.2f;

/// Cutting a room's flight in short covers the screen again, fading from this over a second
constexpr float k_SkippedFlightFade = 1.0f;

/// A press the player was already holding as they came into the temple isn't a press inside it until it is let go,
/// as the temple only hears the button go down while the player is in it
class HeldFromOutside
{
public:
	[[nodiscard]] constexpr Button Filter(Button button) noexcept
	{
		if (button == Button::None)
		{
			_held = false;
		}
		return _held && button == Button::Held ? Button::None : button;
	}

private:
	bool _held {true};
};

/// The held press the camera takes under the temple's fade; a double click always comes through
[[nodiscard]] constexpr Button UnderFade(Button button, float fade) noexcept
{
	return button == Button::Held && fade > k_FadeThatHoldsPresses ? Button::None : button;
}

} // namespace openblack::temple_press
