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

#include <string_view>

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

	/// While a script holds the cinema bars, the temple hears no press: its camera and its controls see the button up.
	/// A press held through them isn't a press until it is let go either, as the temple only hears the button go down.
	[[nodiscard]] constexpr Button FilterUnderCinemaBars(Button button) noexcept
	{
		_held = button != Button::None;
		return Button::None;
	}

private:
	bool _held {true};
};

/// The second click of a double click is not a press in the temple. The camera gets the double click for one step only.
/// While the player holds the button after the double click, the temple and its controls see the button up.
class DoubleClickHold
{
public:
	[[nodiscard]] constexpr Button Filter(Button button) noexcept
	{
		if (button == Button::DoubleClick)
		{
			_held = true;
			return button;
		}
		if (button == Button::None)
		{
			_held = false;
		}
		return _held ? Button::None : button;
	}

private:
	bool _held {false};
};

/// What a press does in a room of the temple. A press that goes down on one of the room's controls goes to the control. A
/// press that goes down off the controls goes to the room's camera. While a press of either kind is held, the room keeps
/// the submesh that was under the cursor when the press went down, so a control that the cursor moves onto does not glow.
/// While a question box is up, the room takes no press.
class RoomPress
{
public:
	enum class State : uint8_t
	{
		Free,
		OnControl,
		OffControls,
	};

	/// @param pressed Whether the room hears the button down, after the rules of the presses held from outside, of the
	/// cinema bars and of the double click
	/// @param overControl Whether the cursor is on one of the room's controls
	/// @param boxUp Whether a question box takes the mouse
	constexpr void Step(bool pressed, bool overControl, bool boxUp) noexcept
	{
		const bool down = pressed && !_wasPressed;
		_wasPressed = pressed;
		if (boxUp || !pressed)
		{
			_state = State::Free;
			return;
		}
		if (down)
		{
			_state = overControl ? State::OnControl : State::OffControls;
		}
	}

	[[nodiscard]] constexpr State GetState() const noexcept { return _state; }
	/// Whether the room follows the submesh under the cursor: only while no press is held
	[[nodiscard]] constexpr bool FollowsHover() const noexcept { return _state == State::Free; }

private:
	State _state {State::Free};
	bool _wasPressed {false};
};

/// The held press the camera takes under the temple's fade; a double click always comes through
[[nodiscard]] constexpr Button UnderFade(Button button, float fade) noexcept
{
	return button == Button::Held && fade > k_FadeThatHoldsPresses ? Button::None : button;
}

/// The rule that kept a press from the temple's camera
enum class Taker : uint8_t
{
	None,            ///< The camera got the press
	HeldFromOutside, ///< The button was down before the player came in
	CinemaBars,      ///< A script had the cinema bars in
	Control,         ///< The press was on one of the room's controls
	Fade,            ///< The temple's fade covered the screen
};

/// The name of a rule that kept a press from the temple's camera, as tools show it
[[nodiscard]] constexpr std::string_view Name(Taker taker) noexcept
{
	switch (taker)
	{
	case Taker::None:
		return "none";
	case Taker::HeldFromOutside:
		return "held_from_outside";
	case Taker::CinemaBars:
		return "cinema_bars";
	case Taker::Control:
		return "control";
	case Taker::Fade:
		return "fade";
	}
	return "none";
}

/// The last press that went down, and what the temple did with it. Tools read it to know why a press did not move the
/// camera.
struct PressTrace
{
	Taker takenBy {Taker::None};
	/// The temple's fade when the press went down
	float fade {0.0f};
	/// Whether the camera looked at a scroll when the press went down
	bool lookingAtScroll {false};
	/// How many presses went down in this visit
	uint32_t presses {0};
};

/// Keeps the last press that went down, from the button before the temple's rules and after each rule
class PressWatch
{
public:
	/// @param raw The button as the player holds it
	/// @param afterHeld The button after the rule of a press held from outside, or of the cinema bars
	/// @param cinemaBars Whether a script had the cinema bars in
	/// @param control Whether one of the room's controls took the press
	/// @param afterFade The button after the rule of the fade
	constexpr void Note(Button raw, Button afterHeld, bool cinemaBars, bool control, Button afterFade, float fade,
	                    bool lookingAtScroll) noexcept
	{
		const bool down = raw != Button::None;
		if (down && !_wasDown)
		{
			_last = PressTrace {
			    .takenBy = FirstTaker(afterHeld, cinemaBars, control, afterFade),
			    .fade = fade,
			    .lookingAtScroll = lookingAtScroll,
			    .presses = _last.presses + 1,
			};
		}
		_wasDown = down;
	}

	/// The last press, or a trace with no presses when none went down
	[[nodiscard]] constexpr const PressTrace& Last() const noexcept { return _last; }

	/// The first rule that took the press away, in the order that the temple uses them
	[[nodiscard]] static constexpr Taker FirstTaker(Button afterHeld, bool cinemaBars, bool control, Button afterFade) noexcept
	{
		if (afterHeld == Button::None)
		{
			return cinemaBars ? Taker::CinemaBars : Taker::HeldFromOutside;
		}
		if (control)
		{
			return Taker::Control;
		}
		return afterFade == Button::None ? Taker::Fade : Taker::None;
	}

private:
	bool _wasDown {false};
	PressTrace _last {};
};

} // namespace openblack::temple_press
