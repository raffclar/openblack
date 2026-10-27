/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MainThreadQueue.h"

#include <cassert>
#include <cstdlib>

#include <algorithm>
#include <format>

#include <spdlog/spdlog.h>

using namespace openblack;

namespace
{

/// What the calling thread does now: the queue of the turn that it does, the step of that turn, and how deep it is in work
/// from a queue. It is per thread, because each thread does its own part.
struct ThreadPart
{
	MainThreadQueue* turn {nullptr};
	std::optional<uint32_t> turnStep;
	uint32_t work {0};
};

thread_local ThreadPart t_part;

} // namespace

MainThreadQueue::MainThreadQueue()
    : _mainThread(std::this_thread::get_id())
{
}

MainThreadQueue::~MainThreadQueue()
{
	// The queue does not do the work that still waits, because the services that it needs go with the game
	assert(_deferrals == 0);
}

MainThreadQueue::Deferral::Deferral(MainThreadQueue& queue)
    : _queue(queue)
{
	assert(queue.IsMainThread());
	++_queue._deferrals;
}

MainThreadQueue::Deferral::~Deferral()
{
	--_queue._deferrals;
}

MainThreadQueue::TurnScope::TurnScope(MainThreadQueue& queue)
    : _previous(std::exchange(t_part.turn, &queue))
    , _previousStep(std::exchange(t_part.turnStep, std::nullopt))
{
}

MainThreadQueue::TurnScope::~TurnScope()
{
	t_part.turn = _previous;
	t_part.turnStep = _previousStep;
}

MainThreadQueue::TurnElsewhere::TurnElsewhere(MainThreadQueue& queue)
    : _queue(queue)
{
	assert(queue.IsMainThread());
	_queue._turnsElsewhere.fetch_add(1, std::memory_order_acq_rel);
}

MainThreadQueue::TurnElsewhere::~TurnElsewhere()
{
	_queue._turnsElsewhere.fetch_sub(1, std::memory_order_acq_rel);
}

MainThreadQueue::WorkScope::WorkScope()
{
	++t_part.work;
}

MainThreadQueue::WorkScope::~WorkScope()
{
	--t_part.work;
}

bool MainThreadQueue::IsMainThread() const noexcept
{
	return std::this_thread::get_id() == _mainThread;
}

bool MainThreadQueue::RunsAtOnce() const noexcept
{
	if (IsMainThread())
	{
		return _deferrals == 0;
	}
	// Another thread that does no turn of this queue keeps its direct calls, as before the queue
	return t_part.turn != this;
}

bool MainThreadQueue::InTurnCode() const noexcept
{
	return t_part.turn == this && t_part.work == 0;
}

bool MainThreadQueue::InTurn() const noexcept
{
	return t_part.turn == this;
}

bool MainThreadQueue::TurnRunsElsewhere() const noexcept
{
	return _turnsElsewhere.load(std::memory_order_acquire) > 0;
}

void MainThreadQueue::SetTurnStep(std::optional<uint32_t> step) noexcept
{
	if (t_part.turn == this)
	{
		t_part.turnStep = step;
	}
}

void MainThreadQueue::Keep(MainThreadApi api, std::function<void()> work)
{
	const std::scoped_lock lock(_pendingMutex);
	_pending.push_back({.api = api, .work = std::move(work)});
}

void MainThreadQueue::Wait(MainThreadApi api, std::function<void()> work)
{
	WaitingCall call {.api = api, .work = std::move(work)};
	std::unique_lock lock(_waitingMutex);
	_waiting.push_back(&call);
	_waitingChanged.notify_all();
	_waitingChanged.wait(lock, [&call] { return call.done; });
}

void MainThreadQueue::ServeUntil(const std::function<bool()>& done)
{
	assert(IsMainThread());
	std::unique_lock lock(_waitingMutex);
	while (true)
	{
		_waitingChanged.wait(lock, [this, &done] { return !_waiting.empty() || done(); });
		if (_waiting.empty())
		{
			return;
		}
		auto* call = _waiting.front();
		_waiting.pop_front();
		lock.unlock();
		// The turn waits for the call, so meanwhile the main thread may use the services that the turn holds. The work
		// that the turn posted before the call comes first.
		const auto elsewhere = _turnsElsewhere.exchange(0, std::memory_order_acq_rel);
		RunPending();
		{
			const WorkScope scope;
			call->work();
		}
		_turnsElsewhere.fetch_add(elsewhere, std::memory_order_acq_rel);
		lock.lock();
		call->done = true;
		_waitingChanged.notify_all();
	}
}

void MainThreadQueue::Wake()
{
	// The lock makes sure that the main thread is in its wait, or did not read `done` yet
	const std::scoped_lock lock(_waitingMutex);
	_waitingChanged.notify_all();
}

size_t MainThreadQueue::RunPending()
{
	assert(IsMainThread());
	size_t done = 0;
	const WorkScope scope;
	// Work that posts more work adds it at the end, so the loop takes the list again until it is empty
	while (true)
	{
		std::vector<Pending> pending;
		{
			const std::scoped_lock lock(_pendingMutex);
			pending.swap(_pending);
		}
		if (pending.empty())
		{
			return done;
		}
		for (auto& piece : pending)
		{
			piece.work();
			++done;
		}
	}
}

size_t MainThreadQueue::GetPendingCount() const
{
	const std::scoped_lock lock(_pendingMutex);
	return _pending.size();
}

std::array<size_t, static_cast<size_t>(MainThreadApi::_Count)> MainThreadQueue::GetPendingByApi() const
{
	std::array<size_t, static_cast<size_t>(MainThreadApi::_Count)> counts {};
	const std::scoped_lock lock(_pendingMutex);
	for (const auto& piece : _pending)
	{
		++counts.at(static_cast<size_t>(piece.api));
	}
	return counts;
}

void MainThreadQueue::SetCheck(TurnApiCheck check) noexcept
{
	_check.store(check, std::memory_order_relaxed);
}

TurnApiCheck MainThreadQueue::GetCheck() const noexcept
{
	return _check.load(std::memory_order_relaxed);
}

void MainThreadQueue::SetFailHandler(std::function<void(const DirectCall&)> handler)
{
	const std::scoped_lock lock(_callsMutex);
	_onFail = std::move(handler);
}

std::vector<DirectCall> MainThreadQueue::GetDirectCalls() const
{
	const std::scoped_lock lock(_callsMutex);
	return _calls;
}

void MainThreadQueue::ClearDirectCalls()
{
	const std::scoped_lock lock(_callsMutex);
	_calls.clear();
}

void MainThreadQueue::CheckDirect([[maybe_unused]] MainThreadApi api, [[maybe_unused]] std::string_view what,
                                  [[maybe_unused]] std::source_location where) noexcept
{
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	auto* queue = t_part.turn;
	// The turn uses a service that it holds directly
	if (queue == nullptr || t_part.work != 0 || HeldByTurn(api) || queue->GetCheck() == TurnApiCheck::Off)
	{
		return;
	}
	queue->Record(api, what, where, t_part.turnStep);
#endif
}

void MainThreadQueue::CheckHeld([[maybe_unused]] MainThreadApi api, [[maybe_unused]] std::string_view what,
                                [[maybe_unused]] std::source_location where) noexcept
{
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (GetCheck() == TurnApiCheck::Off || !IsMainThread() || !TurnRunsElsewhere())
	{
		return;
	}
	// No step: the main thread made the call, not the turn
	Record(api, std::format("main thread: {}", what), where, std::nullopt);
#endif
}

void MainThreadQueue::Record(MainThreadApi api, std::string_view what, const std::source_location& where,
                             std::optional<uint32_t> step) noexcept
{
	const auto place = std::format("{}:{}", where.file_name(), where.line());
	std::function<void(const DirectCall&)> onFail;
	DirectCall first;
	{
		const std::scoped_lock lock(_callsMutex);
		auto found = std::ranges::find_if(_calls, [api, what, &place, step](const DirectCall& call) {
			return call.api == api && call.what == what && call.where == place && call.turnStep == step;
		});
		if (found != _calls.end())
		{
			++found->count;
			return;
		}
		first = _calls.emplace_back(
		    DirectCall {.api = api, .what = std::string(what), .where = place, .turnStep = step, .count = 1});
		onFail = _onFail;
	}
	if (auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_WARN(logger, "Turn code at step {} uses the {} directly: {} ({})",
		                   step.has_value() ? std::to_string(*step) : std::string("?"), Name(api), what, place);
	}
	if (GetCheck() != TurnApiCheck::Fail)
	{
		return;
	}
	if (onFail)
	{
		onFail(first);
		return;
	}
	if (auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_CRITICAL(logger, "The check of the turn's calls stops the game");
		logger->flush();
	}
	std::abort();
}
