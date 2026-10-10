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

#include <array>

/// The help system's profile of the player: how often and how lately the player has done each of the things the help
/// keeps count of (tapped a scroll, turned the camera, ...). The scripts read it to judge whether the player has
/// learnt a control yet.
namespace openblack::help::profile
{

/// The things counted, numbered as the scripts number them; 0 is never asked for by a script
constexpr uint32_t k_EventCount = 49;
/// The hand tapped a scroll or sign with a challenge or tip, or one without
constexpr uint32_t k_TapWithText = 34;
constexpr uint32_t k_TapWithoutText = 35;
/// Events that also count as another: 14 to 23 count as 24, 9 to 11 as 11, 1 to 42 as 43, and every one as 48
constexpr uint32_t k_AnyEvent = 48;

/// The profile's clock runs 100 ms a game turn while it is kept
constexpr uint32_t k_MillisecondsPerTurn = 100;
/// The latest times of an event kept, for its rate
constexpr uint32_t k_TimesKept = 64;
/// How quickly the smoothed rate follows the turns the event happens on
constexpr float k_RateFollow = 0.005f;

/// One event's count: its total, the times it last happened, and a smoothed share of the turns it happens on. It
/// counts at most once a turn.
class EventCount
{
public:
	/// It happens at the clock's time, unless it already has this turn
	void Trigger(uint32_t clock);
	/// The turn ends: the smoothed share follows whether it happened, and it may happen again
	void EndTurn();

	[[nodiscard]] uint32_t Total() const { return _total; }
	[[nodiscard]] float SmoothedRate() const { return _smoothedRate; }
	/// The seconds since it last happened, 0 if it never has. A clock behind its times (a game loaded from an earlier
	/// point) pulls them back to the clock and reads 0.
	[[nodiscard]] float SecondsSince(uint32_t clock);
	/// How many times a second it happened, over the times kept: the gaps between them over the time since the oldest.
	/// 0 with fewer than two kept or none of the clock passed since the oldest; a clock behind its times as above.
	[[nodiscard]] float PerSecond(uint32_t clock);

private:
	void PullTimesBackTo(uint32_t clock);

	uint32_t _total {0};
	float _smoothedRate {0.0f};
	uint32_t _head {0};
	uint32_t _kept {0};
	bool _triggeredThisTurn {false};
	std::array<uint32_t, k_TimesKept> _times {};
};

/// Every event's count and the profile's clock
class HelpProfile
{
public:
	/// An event happens; with it the events it also counts as. Nothing for a number past the last.
	void Trigger(uint32_t event);
	/// A game turn ends: each event may happen again, and the clock moves on
	void EndTurn();

	[[nodiscard]] uint32_t Clock() const { return _clock; }
	[[nodiscard]] const EventCount& Count(uint32_t event) const { return _counts.at(event); }
	[[nodiscard]] EventCount& Count(uint32_t event) { return _counts.at(event); }

	/// Whether the scripts may ask about an event: 1 to 48
	[[nodiscard]] static constexpr bool ScriptMayAsk(int32_t event) { return event > 0 && event < k_EventCount; }

private:
	std::array<EventCount, k_EventCount> _counts {};
	uint32_t _clock {0};
};

} // namespace openblack::help::profile
