/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GpuCommands.h"

#include <cstdlib>

#include <mutex>
#include <set>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

namespace openblack::engine::gpu
{

void Submit(std::function<void()> command)
{
	// one thread: at once (the queue for the drawing thread comes with the threads)
	if (command)
	{
		const ScopedPhase draw(Phase::Draw);
		command();
	}
}

void Flush() {}

namespace
{
// (pending) thread_local once the logic and the draw run on their own threads: each has its own phase
Phase g_Phase = Phase::Logic;

bool Enabled()
{
	static const bool s_Enabled = [] {
		const char* value = std::getenv("OPENBLACK_GPU_CALLS");
		return value != nullptr && value[0] == '1';
	}();
	return s_Enabled;
}
} // namespace

ScopedPhase::ScopedPhase(Phase phase)
    : _previous(g_Phase)
{
	g_Phase = phase;
}

ScopedPhase::~ScopedPhase()
{
	g_Phase = _previous;
}

void NoteResourceCall(std::string_view what, std::string_view name)
{
	if (g_Phase != Phase::Logic || !Enabled())
	{
		return;
	}
	static std::mutex s_Mutex;
	static std::set<std::pair<std::string, std::string>> s_Seen;
	const std::lock_guard lock(s_Mutex);
	if (s_Seen.emplace(std::string(what), std::string(name)).second)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "GPU call in the logic phase: {} '{}'", what, name);
	}
}

} // namespace openblack::engine::gpu
