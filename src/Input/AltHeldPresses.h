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
#include <optional>

namespace openblack::input::alt_presses
{

/// The mouse inputs whose presses Alt stops. The inputs come in the order that the game examines their presses.
enum class Source : uint8_t
{
	LeftButton,
	RightButton,
	MiddleButton,
	WheelUp,
	WheelDown,
};

inline constexpr size_t k_SourceCount = 5;

/// The one-press action of each mouse input that acts now, or 0 when a press of the input does not act
using SourceActions = std::array<uint64_t, k_SourceCount>;

/// What a press changes
struct PressResult
{
	/// The actions of this press that act now
	uint64_t acts {0};
	/// The actions of a press that waited and acts now, because of this press
	uint64_t waitedActs {0};
	/// The actions of an earlier press of this frame that do not act now. Their press waits again or goes.
	uint64_t takenBack {0};
};

/// The game's rule for the one-press actions of the mouse. The game keeps one press for each button until a frame
/// examines it, and it examines the presses once each frame:
/// - With Alt held, no press acts. The press of a button stays kept. A turn of the wheel goes to the held actions only,
///   and its one-press action is lost.
/// - Without Alt, only one press acts in a frame: the first press that acts, in the order left, right, middle, wheel.
///   The other presses of the buttons stay kept for the next frame. A turn of the wheel loses its one-press action.
/// - When no press acts in a frame, the presses of the left and right buttons go. The press of the middle button stays
///   kept until it acts.
/// The held actions of the mouse do not stop.
class AltHeldPresses
{
public:
	/// A press of a mouse input. It gives the actions that act now and the actions that stop now.
	[[nodiscard]] constexpr PressResult Press(Source source, bool altHeld, const SourceActions& actions) noexcept
	{
		const auto bit = Bit(source);
		_thisFrame |= bit;
		_altHeld = altHeld;
		if (IsButton(source))
		{
			_kept |= bit;
		}
		if (altHeld)
		{
			return {};
		}
		if (_acting == source)
		{
			// A second press of the input that acts in this frame is the same press
			_kept = static_cast<uint8_t>(_kept & ~bit);
			return {.acts = actions.at(Index(source))};
		}
		if (!IsButton(source))
		{
			// The wheel comes after the buttons, so a turn acts only when no other press acts in this frame
			if (_acting.has_value() || actions.at(Index(source)) == 0)
			{
				return {};
			}
			_acting = source;
			return {.acts = actions.at(Index(source))};
		}
		if (actions.at(Index(source)) == 0 || (_acting.has_value() && IsButton(*_acting) && *_acting < source))
		{
			return {};
		}
		PressResult result;
		if (_acting.has_value())
		{
			// This press comes first in the order, so the press that acted waits again. A turn of the wheel is lost.
			result.takenBack = actions.at(Index(*_acting));
			if (IsButton(*_acting))
			{
				_kept |= Bit(*_acting);
			}
			_acting.reset();
		}
		const auto taken = TakeKept(actions);
		if (_acting == source)
		{
			result.acts = taken;
		}
		else
		{
			result.waitedActs = taken;
		}
		return result;
	}

	/// Alt goes down. The press that acted in this frame did not act yet, because the game reads Alt at the end of the
	/// frame. It gives the actions to take back. The press of a button stays kept.
	[[nodiscard]] constexpr uint64_t AltDown(const SourceActions& actions) noexcept
	{
		_altHeld = true;
		if (!_acting.has_value())
		{
			return 0;
		}
		const auto takenBack = actions.at(Index(*_acting));
		if (IsButton(*_acting))
		{
			_kept |= Bit(*_acting);
		}
		_acting.reset();
		return takenBack;
	}

	/// Alt goes up. The first kept press acts, or else a turn of the wheel in this frame acts. It gives the actions
	/// that act now.
	[[nodiscard]] constexpr uint64_t AltUp(const SourceActions& actions) noexcept
	{
		_altHeld = false;
		if (const auto kept = TakeKept(actions); kept != 0 || _acting.has_value())
		{
			return kept;
		}
		for (const auto wheel : {Source::WheelUp, Source::WheelDown})
		{
			if ((_thisFrame & Bit(wheel)) != 0 && actions.at(Index(wheel)) != 0)
			{
				_acting = wheel;
				return actions.at(Index(wheel));
			}
		}
		return 0;
	}

	/// A new frame starts. It gives the actions of the kept press that acts now.
	[[nodiscard]] constexpr uint64_t NewFrame(bool altHeld, const SourceActions& actions) noexcept
	{
		// The game reads Alt at the end of each frame. When no press acted without Alt at the end of the last frame, the
		// left and right presses go.
		if (!_altHeld && !_acting.has_value())
		{
			_kept = static_cast<uint8_t>(_kept & Bit(Source::MiddleButton));
		}
		_altHeld = altHeld;
		_thisFrame = 0;
		_acting.reset();
		return altHeld ? 0 : TakeKept(actions);
	}

	/// The mouse input whose press acts in this frame, with one bit for each input in the order of Source
	[[nodiscard]] constexpr uint8_t ActedThisFrame() const noexcept { return _acting.has_value() ? Bit(*_acting) : 0; }

	/// True when a press of a button waits for a later frame
	[[nodiscard]] constexpr bool HasKeptPresses() const noexcept { return _kept != 0; }

private:
	[[nodiscard]] static constexpr size_t Index(Source source) noexcept { return static_cast<size_t>(source); }
	[[nodiscard]] static constexpr uint8_t Bit(Source source) noexcept
	{
		return static_cast<uint8_t>(1u << static_cast<uint8_t>(source));
	}
	[[nodiscard]] static constexpr bool IsButton(Source source) noexcept { return source <= Source::MiddleButton; }

	/// The first kept press that acts, in the order of the buttons, when no press acts in this frame. A kept press
	/// that does not act stays kept and does not stop the presses after it.
	[[nodiscard]] constexpr uint64_t TakeKept(const SourceActions& actions) noexcept
	{
		if (_acting.has_value())
		{
			return 0;
		}
		for (const auto button : {Source::LeftButton, Source::RightButton, Source::MiddleButton})
		{
			const auto bit = Bit(button);
			if ((_kept & bit) != 0 && actions.at(Index(button)) != 0)
			{
				_kept = static_cast<uint8_t>(_kept & ~bit);
				_acting = button;
				return actions.at(Index(button));
			}
		}
		return 0;
	}

	uint8_t _kept = 0;
	uint8_t _thisFrame = 0;
	/// The mouse input whose press acts in this frame
	std::optional<Source> _acting;
	/// Alt is held, as the last call told
	bool _altHeld = false;
};

} // namespace openblack::input::alt_presses
