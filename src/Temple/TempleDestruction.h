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

/// The timeline of a temple whose heart has lost all its life: a clock in seconds that runs from the turn after, and
/// what happens as it passes each moment, until the temple goes 22 seconds on. Free of state, so it is tested on its own.
namespace openblack::temple_destruction
{

/// The heart's loop sound starts on the first turn
inline constexpr float k_LoopFrom = 0.0f;
/// The glow over the heart, at ten seconds
inline constexpr float k_GlowAt = 10.0f;
/// The heart fades out from fourteen seconds over four, while a shell is shown over it
inline constexpr float k_FadeFrom = 14.0f;
inline constexpr float k_FadeOver = 4.0f;
/// The explosion, with the loop sound giving way to a last sound
inline constexpr float k_ExplodeAt = 21.5f;
/// The smoke, and the end, when the temple goes
inline constexpr float k_EndAt = 0.5f + 21.5f;
/// Plasma arcs over the heart between its first turn and the fade
inline constexpr float k_ArcsUntil = 14.0f;

/// How long the spot visuals last, in seconds, and the smoke's spread of them
inline constexpr float k_GlowSeconds = 6.0f;
inline constexpr float k_ExplosionSeconds = 15.0f;
inline constexpr float k_SmokeSecondsScale = 7.5f;
inline constexpr float k_SmokeShareFrom = 0.8f;
inline constexpr float k_SmokeShareSpread = 0.4f;

/// Whether the clock passes a moment this turn: at or after it before, and before it after
[[nodiscard]] constexpr bool Passes(float before, float moment, float after)
{
	return before <= moment && moment < after;
}

/// What happens on a turn that moves the clock from before to after, in the order it happens
struct Events
{
	bool loopStarts {false};
	bool glow {false};
	bool explosion {false};
	bool smoke {false};
	bool end {false};
};
[[nodiscard]] Events Between(float before, float after);

/// The heart's alpha as it fades: whole until the fade, gone at its end
[[nodiscard]] uint8_t HeartAlpha(float clock);

/// Turns a spot visual of so many seconds lasts at so many milliseconds a turn: whole turns a second times the seconds,
/// cut down to whole turns
[[nodiscard]] int32_t TurnsFor(uint32_t millisecondsPerTurn, float seconds);
/// The smoke's turns, from a random share drawn up to its spread
[[nodiscard]] int32_t SmokeTurns(uint32_t millisecondsPerTurn, float share);

} // namespace openblack::temple_destruction
