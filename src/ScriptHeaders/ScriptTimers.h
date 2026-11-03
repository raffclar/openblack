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

/// The scripts' timers: a timer counts game turns from when it was set and holds how many it was set for
namespace openblack::script::timers
{

/// The game's turns are this long, in milliseconds
constexpr uint32_t k_MillisecondsPerTurn = 100;

/// A timer as it was last set: the turn it was set on and how many turns it was set for
struct Timer
{
	uint32_t setTurn {0};
	int32_t turns {0};
};

/// The turns a timer of some seconds runs for: the seconds times the turns in a second, cut down to whole turns
[[nodiscard]] int32_t TurnsFor(float seconds);
/// The turns that a script gives a spell dispenser to make its next bubble. A time of no whole turns gives none, and the
/// dispenser keeps the turns that it has.
[[nodiscard]] std::optional<uint32_t> DispenserTurns(float seconds);
/// A timer set now for some seconds
[[nodiscard]] Timer Set(uint32_t turn, float seconds);
/// The seconds a timer has left, none once it has run out
[[nodiscard]] float SecondsRemaining(const Timer& timer, uint32_t turn);
/// The seconds since a timer was set
[[nodiscard]] float SecondsSinceSet(const Timer& timer, uint32_t turn);

/// The countdown timer of the scripts. The game has one. It counts game turns down and stops at zero. The game keeps a
/// flag that shows or hides the timer, but the game does not draw the timer.
class CountdownTimer
{
public:
	/// Starts the timer for a number of seconds and shows it. The turns are the seconds times the turns in a second, cut
	/// to whole turns. A time below zero counts as zero.
	void Start(float seconds);
	/// Stops the timer
	void Remove() { _running = false; }
	void Hide() { _visible = false; }
	void Reveal() { _visible = true; }
	/// Counts one game turn down while the timer runs. The timer stops when the count gets to zero. A timer that started
	/// with zero turns counts down below zero and wraps, so it runs for a very long time.
	void ProcessTurn();
	/// Stops the timer and hides it, as a new land starts. The turns stay.
	void Reset();

	[[nodiscard]] bool IsRunning() const { return _running; }
	[[nodiscard]] bool IsVisible() const { return _visible; }
	[[nodiscard]] uint32_t GetTurnsLeft() const { return _turnsLeft; }
	/// The whole seconds that are left. The game divides the turns by the turns in a second and removes the fraction.
	[[nodiscard]] float GetSecondsLeft() const;

	/// Gives the values that a saved game keeps to a visitor
	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self._running, self._turnsLeft, self._visible);
	}

private:
	bool _running {false};
	uint32_t _turnsLeft {0};
	bool _visible {false};
};

} // namespace openblack::script::timers
