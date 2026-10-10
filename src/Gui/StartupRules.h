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

#include <optional>

/// The rules of what the game shows as it starts: the two logo pictures and the first run's pre-intro film. Free of
/// any game state so that the start-up screens and the tests share them.
namespace openblack::startup
{

/// The logo film holds two still pictures, each shown in turn
inline constexpr int32_t k_LogoStills = 2;

/// A value eased from where it is to a destination in a set time, arriving with a set speed and no acceleration: its
/// position over time is a quartic. The game's single precision sums, in the game's order
class Zoomer
{
public:
	/// Moves on from the position and speed it has now. Times under a millisecond jump straight there
	void SetDestination(float destination, float destinationSpeed, float seconds) noexcept;
	/// Puts it at a position, at rest with nowhere to go
	void Snap(float position) noexcept;
	/// Moves the clock on by whole milliseconds
	void Advance(int32_t milliseconds) noexcept;

	[[nodiscard]] float Position() const noexcept { return _position; }
	[[nodiscard]] float Speed() const noexcept { return _speed; }
	/// It has got where it was going
	[[nodiscard]] bool Arrived() const noexcept { return _time == _duration; }

private:
	float _position {0.0f};
	float _destination {0.0f};
	float _destinationSpeed {0.0f};
	float _speed {0.0f};
	float _time {0.0f};
	float _duration {0.0f};
	float _startPosition {0.0f};
	float _startSpeed {0.0f};
	float _acceleration {0.0f};
	float _jerk {0.0f};
	float _snap {0.0f};
};

/// One still of the logo film: it fades in over 0.7 s, holds for 1.7 s and fades out over 1.1 s, the last part of it
/// still opaque. A key or a mouse button while it fades in or holds starts the fade out at once; during the fade out it
/// ends the still
class LogoStill
{
public:
	LogoStill() noexcept;

	/// A frame `milliseconds` after the last, with the player pressing something or not. Returns the alpha to draw the
	/// still with this frame, 0 to 255
	uint8_t Frame(int32_t milliseconds, bool pressed) noexcept;
	/// The still has been shown for the last time
	[[nodiscard]] bool Ended() const noexcept { return _phase >= 3; }

private:
	/// Fading in, holding, fading out
	void NextPhase() noexcept;

	Zoomer _zoomer;
	int32_t _phase {0};
};

/// The alpha a logo still is drawn with for the eased position: 255 per unit, rounded down, kept within 0 and 255
[[nodiscard]] uint8_t LogoAlpha(float position) noexcept;

/// Whether the pre-intro film plays: asked for on the command line, or on a first run, before any player profile
[[nodiscard]] constexpr bool PlaysPreIntro(bool requested, bool hasProfiles) noexcept
{
	return requested || !hasProfiles;
}

/// The pre-intro shows each of its frames once, and stops after its last or on a key
[[nodiscard]] constexpr bool PreIntroGoesOn(int32_t framesShown, int32_t frameCount, bool keyPressed) noexcept
{
	return framesShown < frameCount && !keyPressed;
}

/// How long the tips screen takes to fade in when it first shows, from a frame's time in milliseconds
[[nodiscard]] constexpr float NextTipFade(float fade, int32_t milliseconds) noexcept
{
	return fade + static_cast<float>(milliseconds) * 0.001f;
}

} // namespace openblack::startup
