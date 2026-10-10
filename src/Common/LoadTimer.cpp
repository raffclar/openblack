/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LoadTimer.h"

#include <utility>

#include <spdlog/spdlog.h>

using namespace openblack;

namespace
{
[[nodiscard]] double Milliseconds(LoadTimer::Clock::duration duration)
{
	return std::chrono::duration<double, std::milli>(duration).count();
}
} // namespace

LoadTimer::LoadTimer(std::string name, Clock::time_point start)
    : _name(std::move(name))
    , _start(start)
    , _last(start)
{
}

LoadTimer::~LoadTimer()
{
	if (const auto logger = spdlog::get("game"))
	{
		logger->info("Load {}: {:.1f} ms in all", _name, ElapsedMilliseconds());
	}
}

void LoadTimer::Step(std::string_view step)
{
	const auto now = Clock::now();
	if (const auto logger = spdlog::get("game"))
	{
		logger->info("Load {}: {} {:.1f} ms", _name, step, Milliseconds(now - _last));
	}
	_last = now;
}

double LoadTimer::ElapsedMilliseconds() const
{
	return Milliseconds(Clock::now() - _start);
}
