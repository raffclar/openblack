/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>

namespace openblack::input::alt_presses
{

/// The mouse inputs whose presses Alt stops. The buttons come in the order that the game takes their kept presses.
enum class Source : uint8_t
{
	LeftButton,
	RightButton,
	MiddleButton,
	WheelUp,
	WheelDown,
};

inline constexpr size_t k_SourceCount = 5;

/// The one-press actions of each mouse input, from the bindings that apply now
using SourceActions = std::array<uint64_t, k_SourceCount>;

/// The game's rule for the one-press actions of the mouse while Alt is held. The game reads Alt once a frame:
/// - Without Alt, a press of a button or a turn of the wheel does its action.
/// - With Alt, the press of a button stays kept. The game keeps one press for each button.
/// - With Alt, a turn of the wheel goes to the held actions only. Its one-press action is lost.
/// - On each frame without Alt, the first kept press does its action: the left button, then the right, then the middle.
/// The held actions of the mouse do not stop.
class AltHeldPresses
{
public:
	/// A press of a mouse input. It gives the actions that act now.
	[[nodiscard]] constexpr uint64_t Press(Source source, bool altHeld, const SourceActions& actions) noexcept
	{
		const auto bit = Bit(source);
		_thisFrame |= bit;
		if (altHeld)
		{
			if (IsButton(source))
			{
				_kept |= bit;
			}
			return 0;
		}
		_actedThisFrame |= bit;
		return actions.at(Index(source));
	}

	/// Alt goes down. The presses of this frame did not act yet, because the game reads Alt at the end of the frame.
	/// It gives the actions to take back. The presses of the buttons stay kept.
	[[nodiscard]] constexpr uint64_t AltDown(const SourceActions& actions) noexcept
	{
		uint64_t takenBack = 0;
		for (size_t index = 0; index < k_SourceCount; ++index)
		{
			const auto bit = static_cast<uint8_t>(1u << index);
			if ((_actedThisFrame & bit) == 0)
			{
				continue;
			}
			takenBack |= actions.at(index);
			if (IsButton(static_cast<Source>(index)))
			{
				_kept |= bit;
			}
		}
		_actedThisFrame = 0;
		_tookThisFrame = false;
		return takenBack;
	}

	/// Alt goes up. The turns of the wheel in this frame act, and so does the first kept press when no kept press acted
	/// in this frame. It gives the actions that act now.
	[[nodiscard]] constexpr uint64_t AltUp(const SourceActions& actions) noexcept
	{
		uint64_t acts = 0;
		for (const auto wheel : {Source::WheelUp, Source::WheelDown})
		{
			const auto bit = Bit(wheel);
			if ((_thisFrame & bit) != 0 && (_actedThisFrame & bit) == 0)
			{
				_actedThisFrame |= bit;
				acts |= actions.at(Index(wheel));
			}
		}
		return acts | TakeKept(actions);
	}

	/// A new frame starts. It gives the actions of the kept press that acts now.
	[[nodiscard]] constexpr uint64_t NewFrame(bool altHeld, const SourceActions& actions) noexcept
	{
		_thisFrame = 0;
		_actedThisFrame = 0;
		_tookThisFrame = false;
		return altHeld ? 0 : TakeKept(actions);
	}

	/// True when a press of a button waits for Alt to go up
	[[nodiscard]] constexpr bool HasKeptPresses() const noexcept { return _kept != 0; }

private:
	[[nodiscard]] static constexpr size_t Index(Source source) noexcept { return static_cast<size_t>(source); }
	[[nodiscard]] static constexpr uint8_t Bit(Source source) noexcept
	{
		return static_cast<uint8_t>(1u << static_cast<uint8_t>(source));
	}
	[[nodiscard]] static constexpr bool IsButton(Source source) noexcept { return source <= Source::MiddleButton; }

	/// The first kept press, in the order of the buttons, when no kept press acted in this frame. A kept press of a button
	/// that has no one-press action does not act and goes.
	[[nodiscard]] constexpr uint64_t TakeKept(const SourceActions& actions) noexcept
	{
		if (_tookThisFrame)
		{
			return 0;
		}
		for (const auto button : {Source::LeftButton, Source::RightButton, Source::MiddleButton})
		{
			const auto bit = Bit(button);
			if ((_kept & bit) == 0)
			{
				continue;
			}
			_kept = static_cast<uint8_t>(_kept & ~bit);
			if (actions.at(Index(button)) != 0)
			{
				_actedThisFrame |= bit;
				_tookThisFrame = true;
				return actions.at(Index(button));
			}
		}
		return 0;
	}

	uint8_t _kept = 0;
	uint8_t _thisFrame = 0;
	uint8_t _actedThisFrame = 0;
	bool _tookThisFrame = false;
};

} // namespace openblack::input::alt_presses
