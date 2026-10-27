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

} // namespace openblack
