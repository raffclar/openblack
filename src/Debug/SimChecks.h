/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// The stability checks of a long simulation run. The run record gives them the state of the game every some turns, and
/// they flag the problems that they find. These checks are tools: their thresholds do not come from the game.
namespace openblack::sim_checks
{

enum class FlagKind : uint8_t
{
	/// A value that is not a number, or that is infinite
	NotANumber,
	/// A value outside the range that it must stay in
	OutOfRange,
	/// A plan or an activity that stays the same for longer than its limit
	Stuck,
	/// A list or a count that grows and does not stop
	Growth,
	/// An error, a critical message or an assert in the log
	Log,
	/// The time of a turn goes up as the run continues
	TurnTime,
};

[[nodiscard]] std::string_view Name(FlagKind kind);

/// A problem that a check found
struct Flag
{
	FlagKind kind {FlagKind::NotANumber};
	/// The value, the list or the message that the flag is about
	std::string what;
	uint32_t turn {0};
	std::string detail;
};

/// The range that a value must stay in, the two ends included
struct Range
{
	float lowest {0.0f};
	float highest {1.0f};
};

/// A flag when the value is not a number or is infinite, or when it is outside its range (if it has one)
[[nodiscard]] std::optional<Flag> CheckValue(std::string_view what, double value, std::optional<Range> range, uint32_t turn);

/// The longest time that a plan or an activity can stay the same, by its key: "plan:" or "activity:" and the name. A
/// key of "plan:*" or "activity:*" applies to all the names that have no row. A row with no turns gives no limit: the
/// check then only keeps the longest time that it saw.
struct StuckLimit
{
	std::string key;
	std::optional<uint32_t> turns;
};

/// The limit for the name in the slot ("plan" or "activity"), from its own row or from the row of the slot's "*"
[[nodiscard]] std::optional<uint32_t> LimitFor(std::span<const StuckLimit> limits, std::string_view slot,
                                               std::string_view name);

/// Finds a plan or an activity that stays the same for too long. The serial tells a new plan from the same plan again.
class StuckTracker
{
public:
	explicit StuckTracker(std::vector<StuckLimit> limits = {});

	/// A flag the first time that the thing in the slot stays the same for longer than its limit
	std::optional<Flag> Observe(std::string_view slot, std::string_view name, uint64_t serial, uint32_t turn);

	struct Longest
	{
		std::string key;
		uint32_t turns {0};
		std::optional<uint32_t> limit;
	};
	/// The longest time that each key stayed the same, the longest first
	[[nodiscard]] std::vector<Longest> LongestRuns() const;

	void SetLimits(std::vector<StuckLimit> limits) { _limits = std::move(limits); }
	[[nodiscard]] const std::vector<StuckLimit>& Limits() const { return _limits; }

private:
	struct Current
	{
		std::string name;
		uint64_t serial {0};
		uint32_t since {0};
		bool flagged {false};
	};
	std::vector<StuckLimit> _limits;
	std::map<std::string, Current, std::less<>> _current;
	std::map<std::string, uint32_t, std::less<>> _longest;
};

/// When a list grows without a bound: over a window of records it never gets smaller, it gets to at least a number of
/// items, and it gets to a number of times its size at the start of the window
struct GrowthRule
{
	size_t window {50};
	size_t leastSize {32};
	float factor {2.0f};
};

/// Finds lists and counts that grow without a bound
class GrowthTracker
{
public:
	explicit GrowthTracker(GrowthRule rule = {});

	/// A flag once when the list starts to grow without a bound; a list that gets smaller can flag again later
	std::optional<Flag> Observe(std::string_view what, size_t size, uint32_t turn);

	[[nodiscard]] size_t SeriesCount() const { return _series.size(); }

private:
	struct Series
	{
		std::deque<size_t> sizes;
		bool flagged {false};
	};
	GrowthRule _rule;
	std::map<std::string, Series, std::less<>> _series;
};

/// When the turn time goes up: after some records to warm up, the average of a window of records is the base. A
/// later window whose average is a number of times the base, and more than the base by an amount, gives a flag.
struct TrendRule
{
	size_t warmUp {5};
	size_t window {10};
	float factor {1.5f};
	float leastRiseMs {0.5f};
};

/// Finds a turn time that goes up as the run continues
class TurnTimeTrend
{
public:
	explicit TurnTimeTrend(TrendRule rule = {});

	/// A flag once when the time goes up; it can flag again after the time comes back down
	std::optional<Flag> Observe(float turnMs, uint32_t turn);

	[[nodiscard]] std::optional<float> BaseMs() const { return _base; }

private:
	TrendRule _rule;
	size_t _seen {0};
	std::vector<float> _baseSamples;
	std::optional<float> _base;
	std::deque<float> _recent;
	bool _flagged {false};
};

/// The level of a log message, as the run record sees it
enum class LogLevel : uint8_t
{
	Other,
	Warning,
	Error,
	Critical,
};

/// A flag for an error or a critical message, or for a message about an assert at any level
[[nodiscard]] std::optional<Flag> CheckLogMessage(LogLevel level, std::string_view message, uint32_t turn);

} // namespace openblack::sim_checks
