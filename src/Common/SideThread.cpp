/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SideThread.h"

#include <cassert>

#include <utility>

using namespace openblack;

SideThread::SideThread()
    : _thread([this](const std::stop_token& stop) { Loop(stop); })
{
}

SideThread::~SideThread()
{
	if (!Done())
	{
		std::unique_lock lock(_mutex);
		_changed.wait(lock, [this] { return Done(); });
	}
	_thread.request_stop();
	_changed.notify_all();
}

void SideThread::Start(std::function<void()> work, std::function<void()> ended)
{
	const std::scoped_lock lock(_mutex);
	assert(Done());
	_work = std::move(work);
	_ended = std::move(ended);
	_error = nullptr;
	_done.store(false, std::memory_order_release);
	_changed.notify_all();
}

bool SideThread::Done() const noexcept
{
	return _done.load(std::memory_order_acquire);
}

void SideThread::Wait()
{
	std::exception_ptr error;
	{
		std::unique_lock lock(_mutex);
		_changed.wait(lock, [this] { return Done(); });
		error = std::exchange(_error, nullptr);
	}
	if (error)
	{
		std::rethrow_exception(error);
	}
}

std::thread::id SideThread::GetId() const noexcept
{
	return _thread.get_id();
}

void SideThread::Loop(const std::stop_token& stop)
{
	std::unique_lock lock(_mutex);
	while (true)
	{
		_changed.wait(lock, stop, [this] { return static_cast<bool>(_work); });
		if (!_work)
		{
			// The stop came, and no work waits
			return;
		}
		auto work = std::exchange(_work, nullptr);
		auto ended = std::exchange(_ended, nullptr);
		lock.unlock();
		std::exception_ptr error;
		try
		{
			work();
		}
		catch (...)
		{
			error = std::current_exception();
		}
		lock.lock();
		_error = error;
		_done.store(true, std::memory_order_release);
		_changed.notify_all();
		if (ended)
		{
			// Without the lock, because `ended` may wake a thread that then calls Wait
			lock.unlock();
			ended();
			lock.lock();
		}
	}
}
