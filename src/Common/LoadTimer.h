/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace openblack
{

/// Times the steps of a load one after another, logging how long each took and, when it goes, the whole load
class LoadTimer
{
public:
	using Clock = std::chrono::steady_clock;

	explicit LoadTimer(std::string name, Clock::time_point start = Clock::now());
	LoadTimer(const LoadTimer&) = delete;
	LoadTimer& operator=(const LoadTimer&) = delete;
	~LoadTimer();

	/// Ends the step that ran since the last one (or since the start) and logs it under this name
	void Step(std::string_view step);

	/// How long since the start, in milliseconds
	[[nodiscard]] double ElapsedMilliseconds() const;

private:
	std::string _name;
	Clock::time_point _start;
	Clock::time_point _last;
};

} // namespace openblack
