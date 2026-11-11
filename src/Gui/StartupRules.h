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

/// The keys and the mouse buttons that the logo stills and the pre-intro read. The game keeps the last key that went
/// down until any key goes up, and it keeps a click of the left or the right button until the input is reset. A held
/// key that repeats goes down again.
class StartupKeys
{
public:
	/// A key goes down, also when it repeats
	void KeyDown(bool escape) noexcept { _escapeDown = escape; }
	/// Any key goes up: no key is down after it
	void KeyUp() noexcept { _escapeDown = false; }
	/// The left or the right mouse button goes down
	void Click() noexcept { _clicked = true; }
	/// The game forgets the keys and the clicks
	void Reset() noexcept
	{
		_escapeDown = false;
		_clicked = false;
	}

	/// Escape is the last key that went down, and no key went up after it
	[[nodiscard]] bool EscapeDown() const noexcept { return _escapeDown; }
	/// Escape or a click skips a logo still
	[[nodiscard]] bool SkipsLogo() const noexcept { return _escapeDown || _clicked; }

private:
	bool _escapeDown {false};
	bool _clicked {false};
};

/// One still of the logo film: it fades in over 0.7 s, holds for 1.7 s and fades out over 1.1 s, the last part of it
/// still opaque. Escape or a click while it fades in or holds starts the fade out at once; during the fade out it ends
/// the still
class LogoStill
{
public:
	LogoStill() noexcept;

	/// A frame `milliseconds` after the last, with the player pressing something or not. Returns the alpha to draw the
	/// still with this frame, 0 to 255
	uint8_t Frame(int32_t milliseconds, bool pressed) noexcept;
	/// A frame that reads the keys. A press before the fade out resets the keys, so the next still does not see it. A
	/// press during the fade out stays, so the next still starts its fade out at once
	uint8_t Frame(int32_t milliseconds, StartupKeys& keys) noexcept;
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

/// What decides whether the pre-intro film plays
struct PreIntroAsk
{
	/// The command line asked for the film
	bool requested {false};
	/// A player profile exists
	bool hasProfiles {false};
	/// The command line removed the logo pictures, or started a testbed scenario. These are runs for tools and developers.
	bool toolRun {false};
};

/// Whether the pre-intro film plays: asked for on the command line, or on a first run, before any player profile. A run
/// for tools does not play it on a first run, because the film holds the load for about 100 seconds.
[[nodiscard]] constexpr bool PlaysPreIntro(const PreIntroAsk& ask) noexcept
{
	return ask.requested || (!ask.hasProfiles && !ask.toolRun);
}

/// Whether the first game of the program plays the logo effect: a plain start does. A developer's or a tool's run plays it
/// only when asked, and a multiplayer game never does.
[[nodiscard]] constexpr bool PlaysLogoEffect(bool developerRun, bool asked, bool multiplayer) noexcept
{
	return !multiplayer && (!developerRun || asked);
}

/// The pre-intro shows each of its frames once, and stops after its last or when Escape is down. Other keys and the
/// mouse do not stop it
[[nodiscard]] constexpr bool PreIntroGoesOn(int32_t framesShown, int32_t frameCount, bool escapeDown) noexcept
{
	return framesShown < frameCount && !escapeDown;
}

/// How long the tips screen takes to fade in when it first shows, from a frame's time in milliseconds
[[nodiscard]] constexpr float NextTipFade(float fade, int32_t milliseconds) noexcept
{
	return fade + static_cast<float>(milliseconds) * 0.001f;
}

/// The first fade in of the tips screen, which a clock in milliseconds times. It starts at zero and stops at one or more.
class TipFadeIn
{
public:
	/// Starts the fade at the time of the clock now
	explicit constexpr TipFadeIn(uint32_t startMilliseconds) noexcept
	    : _last(startMilliseconds)
	{
	}

	/// The amount of the fade that the next frame draws
	[[nodiscard]] constexpr float Fade() const noexcept { return _fade; }
	/// The fade is complete
	[[nodiscard]] constexpr bool Over() const noexcept { return _fade >= 1.0f; }
	/// Moves the fade on by the time since the last step, at the time of the clock now
	constexpr void Step(uint32_t nowMilliseconds) noexcept
	{
		_fade = NextTipFade(_fade, static_cast<int32_t>(nowMilliseconds - _last));
		_last = nowMilliseconds;
	}

private:
	float _fade {0.0f};
	uint32_t _last;
};

} // namespace openblack::startup
