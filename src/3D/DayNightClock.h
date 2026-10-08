/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

namespace openblack
{

/// The original's day/night clock (the land's time, the game info's cycle and the sky's thresholds).
///
/// Two clocks in hours 0..24:
/// - visual time runs linearly, one day in `duration` seconds of game time (1700 s by default), advanced once per
///   game turn;
/// - script time is the visual time remapped piecewise-linearly so that the day/night thresholds of the cycle land on
///   the standard hours 3.5 / 7.5 / 8 / 8.5. GET_GAME_TIME / SET_GAME_TIME, the sun, the moon and the sky textures use
///   it.
class DayNightClock
{
public:
	/// Standard (script time) thresholds: full night, dusk start, dusk end, full day
	static constexpr std::array<float, 4> k_ScriptTimes = {3.5f, 7.5f, 8.0f, 8.5f};
	/// The script's default time properties: 1700 s per day, 8.3 % night, 7 % change
	static constexpr float k_DefaultDuration = 1700.0f;
	static constexpr float k_DefaultNight = 0.083f;
	static constexpr float k_DefaultChange = 0.07f;

	/// As a land opens: scale 1, default cycle, noon
	void Reset();
	/// Once per game turn: the time advanced by scale * 0.1 s of a 0.1 s turn
	void ProcessTurn();

	/// SET_GAME_TIME_PROPERTIES (fractions of the whole day)
	void SetCycle(float duration, float night, float change);
	/// The land script's SET_NIGHTTIME: night <= 1, change <= 1 - night
	void SetCycleFromLand(float duration, float night, float change);
	/// GAME_TIME_ON_OFF: 1 runs the clock, 0 stops it
	void SetRunning(bool running) { _scale = running ? 1.0f : 0.0f; }
	/// SET_GAME_TIME: the visual time forced to ScriptToVisual(hour), with the sky's jump (sky_type::Jump)
	void SetScriptTime(float hour);
	/// MOVE_GAME_TIME: the visual time slides to ScriptToVisual(hour) in `seconds`
	void MoveScriptTime(float hour, float seconds);

	[[nodiscard]] float GetVisualTime() const { return _visualTime; }
	/// GET_GAME_TIME
	[[nodiscard]] float GetScriptTime() const { return VisualToScript(_visualTime); }
	/// The sky type of the visual time, computed now: 2 night, 1 dusk, 0 day (sky_type::At). What the
	/// sky drew this frame is sky_type::Frame().
	[[nodiscard]] float SkyType() const { return SkyType(_visualTime); }
	/// Sky type > the double 1.2 (sky_type::IsVisualNight)
	[[nodiscard]] bool IsVisualNight() const;

	[[nodiscard]] float ScriptToVisual(float hour) const;
	[[nodiscard]] float VisualToScript(float hour) const;
	/// sky_type::At with this clock's thresholds
	[[nodiscard]] float SkyType(float hour) const;
	[[nodiscard]] const std::array<float, 4>& GetVisualTimes() const { return _times; }

private:
	/// New target, and with `seconds` != 0 the slide speed
	void SetTarget(float hour, float seconds);

	float _visualTime {12.0f};
	float _target {12.0f};
	float _step {2.5f};        ///< hours per second of game time towards the target
	float _moveSeconds {0.0f}; ///< MOVE_GAME_TIME in progress
	float _dayRate {0.0f};     ///< hours per second
	float _nightRate {0.0f};
	float _scale {1.0f};
	std::array<float, 4> _times {}; ///< the cycle's day/night thresholds
};

} // namespace openblack
