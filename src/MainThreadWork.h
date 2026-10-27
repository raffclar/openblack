/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <type_traits>
#include <utility>

#include "Common/MainThreadQueue.h"
#include "Locator.h"

namespace openblack
{

/// Gives work that needs a service of the main thread to the main thread's queue (see MainThreadQueue). With no queue,
/// as in a test that makes its own services, the work runs at once.
template <typename Work>
void PostToMainThread(MainThreadApi api, Work&& work)
{
	if (Locator::mainThreadQueue::has_value())
	{
		Locator::mainThreadQueue::value().Post(api, std::forward<Work>(work));
		return;
	}
	std::invoke(work);
}

/// Work given to the main thread from this thread now runs at once (see MainThreadQueue::RunsAtOnce). Work that waits
/// must keep copies of what it uses.
[[nodiscard]] inline bool MainThreadWorkRunsAtOnce()
{
	return !Locator::mainThreadQueue::has_value() || Locator::mainThreadQueue::value().RunsAtOnce();
}

/// The calling thread does a turn, and it is not the main thread
[[nodiscard]] inline bool InTurnOnOtherThread()
{
	if (!Locator::mainThreadQueue::has_value())
	{
		return false;
	}
	const auto& queue = Locator::mainThreadQueue::value();
	return queue.InTurn() && !queue.IsMainThread();
}

/// Does the work on the main thread and gives back its result. A turn on another thread waits for it (see
/// MainThreadQueue::CallOnMain). With no queue, the work runs at once.
template <typename Work>
std::invoke_result_t<Work&> CallOnMainThread(MainThreadApi api, Work&& work)
{
	if (Locator::mainThreadQueue::has_value())
	{
		return Locator::mainThreadQueue::value().CallOnMain(api, std::forward<Work>(work));
	}
	return std::invoke(work);
}

} // namespace openblack
