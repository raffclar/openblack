/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FrameQueue.h"

#include <algorithm>
#include <utility>

using namespace openblack::video;

namespace
{
void CopyPlane(const openblack::bink::PlaneView& plane, std::vector<uint8_t>& out)
{
	std::ranges::copy(plane.pixels.first(std::min(plane.pixels.size(), out.size())), out.begin());
}
} // namespace

FrameQueue::FrameQueue(bink::FrameReader reader, DecodeMode mode)
    : _reader(std::move(reader))
    , _frameCount(_reader.GetFile().FrameCount())
{
	// The decoder's planes have their full size from the start: every buffer is made once, here
	const auto picture = _reader.GetPicture();
	const std::array<const bink::PlaneView*, 3> planes = {&picture.y, &picture.u, &picture.v};
	for (auto& slot : _slots)
	{
		for (size_t i = 0; i < planes.size(); ++i)
		{
			slot.planes.at(i).assign(planes.at(i)->pixels.size(), 0);
			slot.strides.at(i) = planes.at(i)->stride;
		}
	}
	if (mode == DecodeMode::Worker)
	{
		_worker = std::thread([this]() { Run(); });
	}
}

FrameQueue::~FrameQueue()
{
	{
		const std::lock_guard lock(_mutex);
		_stop = true;
	}
	_changed.notify_all();
	if (_worker.joinable())
	{
		_worker.join();
	}
}

void FrameQueue::DecodeInto(DecodedFrame& slot)
{
	if (_reader.DecodeFrame(_next))
	{
		++_goodFrames;
	}
	const auto picture = _reader.GetPicture();
	CopyPlane(picture.y, slot.planes[0]);
	CopyPlane(picture.u, slot.planes[1]);
	CopyPlane(picture.v, slot.planes[2]);
	slot.frame = _next;
	slot.goodFrames = _goodFrames;
	++_next;
}

void FrameQueue::Run()
{
	while (true)
	{
		size_t index = 0;
		{
			std::unique_lock lock(_mutex);
			_changed.wait(lock, [this]() {
				return _stop || _next >= _frameCount || std::ranges::find(_states, SlotState::Free) != _states.end();
			});
			if (_stop || _next >= _frameCount)
			{
				return;
			}
			index = static_cast<size_t>(std::ranges::find(_states, SlotState::Free) - _states.begin());
			_states.at(index) = SlotState::Writing;
		}
		// The decoding itself runs unlocked: the slot is this thread's until it is marked ready
		DecodeInto(_slots.at(index));
		{
			const std::lock_guard lock(_mutex);
			_states.at(index) = SlotState::Ready;
			_published = _slots.at(index).frame + 1;
		}
		_changed.notify_all();
	}
}

const DecodedFrame* FrameQueue::Latest(uint32_t frame)
{
	if (!_worker.joinable() && _next <= frame && _next < _frameCount)
	{
		// Inline: every frame up to it in order, only the last one copied out
		const uint32_t last = std::min(frame, _frameCount - 1);
		while (_next < last)
		{
			if (_reader.DecodeFrame(_next))
			{
				++_goodFrames;
			}
			++_next;
		}
		const auto free = std::ranges::find(_states, SlotState::Free);
		if (free != _states.end())
		{
			const auto index = static_cast<size_t>(free - _states.begin());
			DecodeInto(_slots.at(index));
			_states.at(index) = SlotState::Ready;
			_published = _slots.at(index).frame + 1;
		}
	}

	{
		const std::lock_guard lock(_mutex);
		std::optional<size_t> newest;
		for (size_t i = 0; i < k_Slots; ++i)
		{
			if (_states.at(i) == SlotState::Ready && _slots.at(i).frame <= frame &&
			    (!newest || _slots.at(i).frame > _slots.at(*newest).frame))
			{
				newest = i;
			}
		}
		if (newest)
		{
			// Every frame before the newest is done with, the one shown before among them
			const uint32_t shown = _slots.at(*newest).frame;
			for (size_t i = 0; i < k_Slots; ++i)
			{
				if (_states.at(i) == SlotState::Ready && _slots.at(i).frame < shown)
				{
					_states.at(i) = SlotState::Free;
				}
			}
			_shown = newest;
		}
	}
	_changed.notify_all();
	return _shown ? &_slots.at(*_shown) : nullptr;
}

void FrameQueue::WaitFor(uint32_t frame)
{
	if (!_worker.joinable() || _frameCount == 0)
	{
		return;
	}
	const uint32_t last = std::min(frame, _frameCount - 1);
	std::unique_lock lock(_mutex);
	// Also once every buffer is full: the worker then waits for frames to be let go
	_changed.wait(lock,
	              [this, last]() { return _published > last || std::ranges::find(_states, SlotState::Free) == _states.end(); });
}
