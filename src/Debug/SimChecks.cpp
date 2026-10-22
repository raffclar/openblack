/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SimChecks.h"

#include <cctype>
#include <cmath>

#include <algorithm>
#include <array>
#include <numeric>
#include <utility>

#include <fmt/format.h>

namespace openblack::sim_checks
{

namespace
{

float Average(const auto& values)
{
	if (values.empty())
	{
		return 0.0f;
	}
	return std::accumulate(values.begin(), values.end(), 0.0f) / static_cast<float>(values.size());
}

bool ContainsAssert(std::string_view message)
{
	constexpr std::string_view k_Word = "assert";
	if (message.size() < k_Word.size())
	{
		return false;
	}
	for (size_t i = 0; i + k_Word.size() <= message.size(); ++i)
	{
		const auto part = message.substr(i, k_Word.size());
		if (std::ranges::equal(part, k_Word, [](char a, char b) {
			    return std::tolower(static_cast<unsigned char>(a)) == static_cast<unsigned char>(b);
		    }))
		{
			return true;
		}
	}
	return false;
}

} // namespace

std::string_view Name(FlagKind kind)
{
	switch (kind)
	{
	case FlagKind::NotANumber:
		return "not_a_number";
	case FlagKind::OutOfRange:
		return "out_of_range";
	case FlagKind::Stuck:
		return "stuck";
	case FlagKind::Growth:
		return "growth";
	case FlagKind::Log:
		return "log";
	case FlagKind::TurnTime:
		return "turn_time";
	}
	return "unknown";
}

std::optional<Flag> CheckValue(std::string_view what, double value, std::optional<Range> range, uint32_t turn)
{
	if (!std::isfinite(value))
	{
		return Flag {.kind = FlagKind::NotANumber, .what = std::string(what), .turn = turn, .detail = fmt::format("{}", value)};
	}
	if (range.has_value() && (value < range->lowest || value > range->highest))
	{
		return Flag {
		    .kind = FlagKind::OutOfRange,
		    .what = std::string(what),
		    .turn = turn,
		    .detail = fmt::format("{} is outside {} to {}", value, range->lowest, range->highest),
		};
	}
	return std::nullopt;
}

std::optional<uint32_t> LimitFor(std::span<const StuckLimit> limits, std::string_view slot, std::string_view name)
{
	const auto key = fmt::format("{}:{}", slot, name);
	const auto any = fmt::format("{}:*", slot);
	if (const auto own = std::ranges::find(limits, key, &StuckLimit::key); own != limits.end())
	{
		return own->turns;
	}
	if (const auto all = std::ranges::find(limits, any, &StuckLimit::key); all != limits.end())
	{
		return all->turns;
	}
	return std::nullopt;
}

StuckTracker::StuckTracker(std::vector<StuckLimit> limits)
    : _limits(std::move(limits))
{
}

std::optional<Flag> StuckTracker::Observe(std::string_view slot, std::string_view name, uint64_t serial, uint32_t turn)
{
	auto found = _current.find(slot);
	if (found == _current.end())
	{
		found = _current.emplace(std::string(slot), Current {}).first;
		found->second = {.name = std::string(name), .serial = serial, .since = turn, .flagged = false};
	}
	auto& current = found->second;
	// The turns can start again from 0, as on a move to the next land
	if (current.name != name || current.serial != serial || turn < current.since)
	{
		current = {.name = std::string(name), .serial = serial, .since = turn, .flagged = false};
	}
	const auto turns = turn - current.since;
	const auto key = fmt::format("{}:{}", slot, name);
	auto& longest = _longest[key];
	longest = std::max(longest, turns);
	const auto limit = LimitFor(_limits, slot, name);
	if (!limit.has_value() || turns <= *limit || current.flagged)
	{
		return std::nullopt;
	}
	current.flagged = true;
	return Flag {
	    .kind = FlagKind::Stuck,
	    .what = key,
	    .turn = turn,
	    .detail = fmt::format("the same for {} turns, more than its limit of {}", turns, *limit),
	};
}

std::vector<StuckTracker::Longest> StuckTracker::LongestRuns() const
{
	std::vector<Longest> runs;
	runs.reserve(_longest.size());
	for (const auto& [key, turns] : _longest)
	{
		const auto colon = key.find(':');
		const auto slot = std::string_view(key).substr(0, colon);
		const auto name = colon == std::string::npos ? std::string_view() : std::string_view(key).substr(colon + 1);
		runs.push_back({.key = key, .turns = turns, .limit = LimitFor(_limits, slot, name)});
	}
	std::ranges::stable_sort(runs, std::ranges::greater {}, &Longest::turns);
	return runs;
}

GrowthTracker::GrowthTracker(GrowthRule rule)
    : _rule(rule)
{
}

std::optional<Flag> GrowthTracker::Observe(std::string_view what, size_t size, uint32_t turn)
{
	auto found = _series.find(what);
	if (found == _series.end())
	{
		found = _series.emplace(std::string(what), Series {}).first;
	}
	auto& series = found->second;
	if (!series.sizes.empty() && size < series.sizes.back())
	{
		series.flagged = false;
	}
	series.sizes.push_back(size);
	while (series.sizes.size() > _rule.window)
	{
		series.sizes.pop_front();
	}
	if (series.flagged || series.sizes.size() < _rule.window || size < _rule.leastSize)
	{
		return std::nullopt;
	}
	const bool neverSmaller = std::ranges::is_sorted(series.sizes);
	const auto first = series.sizes.front();
	if (!neverSmaller || static_cast<float>(size) < static_cast<float>(first) * _rule.factor)
	{
		return std::nullopt;
	}
	series.flagged = true;
	return Flag {
	    .kind = FlagKind::Growth,
	    .what = std::string(what),
	    .turn = turn,
	    .detail = fmt::format("grew from {} to {} over {} records and never got smaller", first, size, _rule.window),
	};
}

TurnTimeTrend::TurnTimeTrend(TrendRule rule)
    : _rule(rule)
{
}

std::optional<Flag> TurnTimeTrend::Observe(float turnMs, uint32_t turn)
{
	++_seen;
	if (_seen <= _rule.warmUp)
	{
		return std::nullopt;
	}
	if (!_base.has_value())
	{
		_baseSamples.push_back(turnMs);
		if (_baseSamples.size() >= _rule.window)
		{
			_base = Average(_baseSamples);
		}
		return std::nullopt;
	}
	_recent.push_back(turnMs);
	while (_recent.size() > _rule.window)
	{
		_recent.pop_front();
	}
	if (_recent.size() < _rule.window)
	{
		return std::nullopt;
	}
	const auto recent = Average(_recent);
	const bool risen = recent > *_base * _rule.factor && recent - *_base > _rule.leastRiseMs;
	if (!risen)
	{
		_flagged = false;
		return std::nullopt;
	}
	if (_flagged)
	{
		return std::nullopt;
	}
	_flagged = true;
	return Flag {
	    .kind = FlagKind::TurnTime,
	    .what = "turn_ms",
	    .turn = turn,
	    .detail = fmt::format("the average turn took {:.3f} ms, against {:.3f} ms at the start", recent, *_base),
	};
}

std::optional<Flag> CheckLogMessage(LogLevel level, std::string_view message, uint32_t turn)
{
	const bool serious = level == LogLevel::Error || level == LogLevel::Critical;
	if (!serious && !ContainsAssert(message))
	{
		return std::nullopt;
	}
	const std::string_view levelName = level == LogLevel::Critical  ? "critical"
	                                   : level == LogLevel::Error   ? "error"
	                                   : level == LogLevel::Warning ? "warning"
	                                                                : "other";
	return Flag {.kind = FlagKind::Log, .what = std::string(levelName), .turn = turn, .detail = std::string(message)};
}

} // namespace openblack::sim_checks
