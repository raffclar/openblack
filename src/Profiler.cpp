/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Profiler.h"

#include <cassert>

#include <algorithm>

openblack::Profiler::Profiler()
    : _mainThread(std::this_thread::get_id())
{
}

openblack::Profiler::~Profiler() = default;

openblack::Profiler::ThreadScopes& openblack::Profiler::ScopesOfThisThread()
{
	const auto thread = std::this_thread::get_id();
	const std::scoped_lock lock(_threadsMutex);
	auto found = std::ranges::find_if(_threads, [thread](const auto& scopes) { return scopes->thread == thread; });
	if (found != _threads.end())
	{
		return **found;
	}
	auto& scopes = *_threads.emplace_back(std::make_unique<ThreadScopes>());
	scopes.thread = thread;
	return scopes;
}

void openblack::Profiler::Begin(Stage stage)
{
	if (!IsMainThread())
	{
		auto& scopes = ScopesOfThisThread();
		const std::scoped_lock lock(scopes.mutex);
		assert(scopes.open.size() < 255);
		scopes.open.push_back({.stage = stage,
		                       .level = static_cast<uint8_t>(scopes.open.size()),
		                       .start = std::chrono::steady_clock::now(),
		                       .end = {}});
		return;
	}
	assert(_currentLevel < 255);
	auto& entry = _entries.at(_currentEntry).stages.at(static_cast<uint8_t>(stage));
	entry.level = _currentLevel;
	_currentLevel++;
	entry.start = std::chrono::steady_clock::now();
	entry.finalized = false;
}

void openblack::Profiler::End(Stage stage)
{
	if (!IsMainThread())
	{
		auto& scopes = ScopesOfThisThread();
		const std::scoped_lock lock(scopes.mutex);
		assert(!scopes.open.empty() && scopes.open.back().stage == stage);
		if (scopes.open.empty())
		{
			return;
		}
		auto scope = scopes.open.back();
		scopes.open.pop_back();
		scope.end = std::chrono::steady_clock::now();
		scopes.closed.push_back(scope);
		return;
	}
	assert(_currentLevel > 0);
	auto& entry = _entries.at(_currentEntry).stages.at(static_cast<uint8_t>(stage));
	assert(!entry.finalized);
	_currentLevel--;
	assert(entry.level == _currentLevel);
	entry.end = std::chrono::steady_clock::now();
	entry.finalized = true;
	entry.total += entry.end - entry.start;
	++entry.calls;
}

void openblack::Profiler::MergeInto(Entry& entry)
{
	assert(IsMainThread());
	std::vector<ThreadScope> closed;
	const std::scoped_lock lock(_threadsMutex);
	for (const auto& scopes : _threads)
	{
		{
			const std::scoped_lock scopesLock(scopes->mutex);
			closed.swap(scopes->closed);
		}
		// The scopes closed in the order of their ends; each stage keeps the last one, with the time of all of them
		for (const auto& scope : closed)
		{
			auto& stage = entry.stages.at(static_cast<uint8_t>(scope.stage));
			stage.level = static_cast<uint8_t>(std::min(255, _currentLevel + scope.level));
			stage.start = scope.start;
			stage.end = scope.end;
			stage.finalized = true;
			stage.total += scope.end - scope.start;
			++stage.calls;
		}
		closed.clear();
	}
}

void openblack::Profiler::MergeThreads()
{
	MergeInto(_entries.at(_currentEntry));
}

void openblack::Profiler::Frame()
{
	auto& prevEntry = _entries.at(_currentEntry);
	// The scopes that the other threads closed in the frame that ends belong to it
	MergeInto(prevEntry);
	_currentEntry = (_currentEntry + 1) % k_BufferSize;
	auto& entry = _entries.at(_currentEntry);
	prevEntry.frameEnd = entry.frameStart = std::chrono::steady_clock::now();
	// The new frame's stages have taken no time yet
	for (auto& stage : entry.stages)
	{
		stage.total = {};
		stage.calls = 0;
	}
}
