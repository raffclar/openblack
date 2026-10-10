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
/// Plasma beams leap between points of the heart from its first turn until the fade
inline constexpr float k_BeamsFrom = 0.0f;
inline constexpr float k_BeamsOver = 14.0f;

/// How long the spot visuals last, in seconds, and the smoke's spread of them
inline constexpr float k_GlowSeconds = 6.0f;
inline constexpr float k_ExplosionSeconds = 15.0f;
inline constexpr float k_SmokeSecondsScale = 7.5f;
inline constexpr float k_SmokeShareFrom = 0.8f;
inline constexpr float k_SmokeShareSpread = 0.4f;
/// The smoke is ten times the size of other spot visuals
inline constexpr float k_SmokeMagnitude = 10.0f;

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

/// Whether the heart beams at the clock
[[nodiscard]] constexpr bool Beaming(float clock)
{
	return Passes(k_BeamsFrom, clock, k_BeamsFrom + k_BeamsOver);
}
/// How far through its beaming the heart is at the clock, from 0 to 1
[[nodiscard]] float BeamShare(float clock);
/// The seconds from one beam to the next, shortening from four tenths to two over the beaming, so that they come about
/// one every four turns at first and one every two at the end
inline constexpr float k_FirstBeamGap = 0.4f;
inline constexpr float k_LastBeamGap = 0.2f;
/// The next beam's moment on the beam clock
[[nodiscard]] float NextBeam(float beamClock, float share);
/// A beam's life in seconds, its texture's speed and its alpha: short-lived, faster and brighter as the beaming goes on
inline constexpr float k_FirstBeamLife = 3.0f;
inline constexpr float k_LastBeamLife = 0.7f;
inline constexpr float k_FirstBeamSpeed = 1.0f;
inline constexpr float k_LastBeamSpeed = 1.5f;
inline constexpr float k_FirstBeamAlpha = 50.0f;
inline constexpr float k_LastBeamAlpha = 200.0f;
struct BeamLook
{
	float life;
	float speed;
	uint8_t alpha;
};
[[nodiscard]] BeamLook BeamLookAt(float share);

/// The heart's alpha as it fades: whole until the fade, gone at its end
[[nodiscard]] uint8_t HeartAlpha(float clock);

/// Turns a spot visual of so many seconds lasts at so many milliseconds a turn: whole turns a second times the seconds,
/// cut down to whole turns
[[nodiscard]] int32_t TurnsFor(uint32_t millisecondsPerTurn, float seconds);
/// The smoke's turns, from a random share drawn up to its spread
[[nodiscard]] int32_t SmokeTurns(uint32_t millisecondsPerTurn, float share);

/// What decides whether a temple being destroyed ends the game this turn
struct GameOverInputs
{
	/// The game has already ended
	bool over {false};
	/// A skirmish, played on a playground land, or a game played online: losing a temple doesn't end either this way
	bool skirmish {false};
	bool multiplayer {false};
	/// The local player's temple is being destroyed
	bool localTempleDestroying {false};
};
/// Whether the game ends: once, for the local player losing their temple, outside skirmishes and online games
[[nodiscard]] constexpr bool GameOverStarts(const GameOverInputs& inputs)
{
	return !inputs.over && !inputs.skirmish && !inputs.multiplayer && inputs.localTempleDestroying;
}

} // namespace openblack::temple_destruction
