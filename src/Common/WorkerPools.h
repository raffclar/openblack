/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <memory>
#include <optional>

#include "WorkerPool.h"

namespace openblack
{

/// The source of the worker pools of the systems and the renderer.
///
/// On the desktop, each pool has threads of its own. In the browser, each thread of the game takes one of the workers
/// that the page made at the start. A thread past these workers gets a new worker, and the new worker loads the
/// program from the site in the middle of play. Thus in the browser all pools share one set of threads, and the game
/// stays inside the workers of the page.
class WorkerPools
{
public:
	/// With a count, all pools share this many threads. With no count, each pool has threads of its own.
	explicit WorkerPools(std::optional<size_t> sharedThreads);

	/// The count of the shared threads on this platform, or none when each pool has threads of its own
	[[nodiscard]] static std::optional<size_t> SharedThreadCount();

	/// A new pool for the jobs of one system
	[[nodiscard]] std::unique_ptr<WorkerPool> Make() const;

	/// The threads that all pools share, or none
	[[nodiscard]] const std::shared_ptr<WorkerThreads>& GetShared() const { return _shared; }

private:
	std::shared_ptr<WorkerThreads> _shared;
};

/// A new pool from the worker pools of the game. With no such service, as in a test, the pool has threads of its own.
[[nodiscard]] std::unique_ptr<WorkerPool> MakeWorkerPool();

} // namespace openblack
