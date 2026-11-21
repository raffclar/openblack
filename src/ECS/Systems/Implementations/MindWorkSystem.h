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

#include <atomic>
#include <memory>
#include <optional>

#include "ECS/Systems/MindWorkSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack
{
class WorkerPool;
}

namespace openblack::ecs::systems
{

class MindWorkSystem final: public MindWorkSystemInterface
{
public:
	MindWorkSystem();
	MindWorkSystem(const MindWorkSystem&) = delete;
	MindWorkSystem& operator=(const MindWorkSystem&) = delete;
	MindWorkSystem(MindWorkSystem&&) = delete;
	MindWorkSystem& operator=(MindWorkSystem&&) = delete;
	~MindWorkSystem() override;

	void ForEach(size_t count, size_t onCaller, const Job& job) override;
	void SetThreads(std::optional<size_t> threads) override;
	[[nodiscard]] size_t GetThreads() const override;

	/// The runs of indices that each lane gets for one call. A thread takes a run, not one index, so that the threads
	/// share fewer counters.
	static constexpr size_t k_RunsPerLane = 4;

private:
	/// The pool, made at the first call that needs the threads
	WorkerPool* Pool();

	/// The threads that the user asked for, or none for the threads of the platform
	std::optional<size_t> _threads;
	std::unique_ptr<WorkerPool> _pool;
	/// True while a call gives jobs to the threads. A call from another thread at the same time does its jobs alone.
	std::atomic<bool> _busy {false};
};

} // namespace openblack::ecs::systems
