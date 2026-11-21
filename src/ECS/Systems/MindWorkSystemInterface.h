/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <functional>
#include <optional>

namespace openblack::ecs::systems
{

/// The worker threads that share the work of one computer god while it works out its needs.
///
/// The world does not change while a god works out its needs. Thus the scores of many things can come from many
/// threads at the same time. Each job reads the world and writes only its own result at its index. The calling thread
/// then makes the choices from the results, in the same order as with one thread. A job must not use the game's random
/// numbers, change the world, or ask for data that the game makes when it first reads it (the cells of the map, a model
/// that is not loaded yet, a component store that does not exist yet).
class MindWorkSystemInterface
{
public:
	/// A job gets its index
	using Job = std::function<void(size_t index)>;

	virtual ~MindWorkSystemInterface() = default;

	/// Does the job one time for each index from 0 to count, then returns. When the count is at most onCaller, or the
	/// system has no threads, the calling thread does all the jobs in the order of the indices.
	virtual void ForEach(size_t count, size_t onCaller, const Job& job) = 0;
	/// Use this many threads beside the calling thread. 0 makes the calling thread do all the jobs in order. None gives
	/// the threads that the platform shares with the other systems.
	virtual void SetThreads(std::optional<size_t> threads) = 0;
	/// How many threads do the jobs beside the calling thread
	[[nodiscard]] virtual size_t GetThreads() const = 0;
};

} // namespace openblack::ecs::systems
