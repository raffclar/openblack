/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DrawReadWatch.h"

#include <cstdlib>

#include <algorithm>
#include <utility>

#include <spdlog/spdlog.h>

namespace openblack::ecs
{

DrawReadWatch::DrawReadWatch(DrawReadCheck check, std::function<void(const Read&)> onFail)
    : _check(check)
    , _onFail(std::move(onFail))
{
}

void DrawReadWatch::Start() noexcept
{
	_thread = std::this_thread::get_id();
	_watching = _check != DrawReadCheck::Off;
}

void DrawReadWatch::Stop() noexcept
{
	_watching = false;
}

void DrawReadWatch::Note(std::string_view registry, std::string_view call,
                         std::initializer_list<std::string_view> components) noexcept
{
	if (!_watching || std::this_thread::get_id() != _thread)
	{
		return;
	}
	std::string what(call);
	what += '<';
	bool first = true;
	for (const auto component : components)
	{
		if (!first)
		{
			what += ", ";
		}
		what += component;
		first = false;
	}
	what += '>';
	Read found;
	{
		const std::scoped_lock lock(_readsMutex);
		const auto known =
		    std::ranges::find_if(_reads, [&](const Read& read) { return read.registry == registry && read.what == what; });
		if (known != _reads.end())
		{
			++known->count;
			return;
		}
		found = _reads.emplace_back(Read {.registry = std::string(registry), .what = what, .count = 1});
	}
	if (auto logger = spdlog::get("graphics"))
	{
		SPDLOG_LOGGER_WARN(logger, "The draw reads the {} directly: {}", found.registry, found.what);
	}
	if (_check != DrawReadCheck::Fail)
	{
		return;
	}
	if (_onFail)
	{
		_onFail(found);
		return;
	}
	if (auto logger = spdlog::get("graphics"))
	{
		SPDLOG_LOGGER_CRITICAL(logger, "The check of the draw's reads stops the game");
		logger->flush();
	}
	std::abort();
}

std::vector<DrawReadWatch::Read> DrawReadWatch::GetReads() const
{
	const std::scoped_lock lock(_readsMutex);
	return _reads;
}

} // namespace openblack::ecs
