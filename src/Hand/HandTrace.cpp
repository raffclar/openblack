/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandTrace.h"

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack::hand_trace;

void HandTrace::Enable(size_t capacity)
{
	_capacity = capacity;
	_ring.assign(capacity, Frame {});
	_next = 0;
	_count = 0;
	_current.reset();
}

void HandTrace::Disable()
{
	_capacity = 0;
	_ring.clear();
	_ring.shrink_to_fit();
	_next = 0;
	_count = 0;
	_current.reset();
}

void HandTrace::BeginFrame(uint32_t frame, float seconds)
{
	if (!Enabled())
	{
		return;
	}
	_current = Frame {.frame = frame, .seconds = seconds};
}

void HandTrace::Note(std::string_view writer, glm::vec3 position)
{
	if (!_current.has_value())
	{
		return;
	}
	if (_current->writeCount < k_MaxWrites)
	{
		_current->writes.at(_current->writeCount) = {.writer = writer, .position = position};
		++_current->writeCount;
	}
	else if (_current->writesLost < UINT8_MAX)
	{
		++_current->writesLost;
	}
}

Frame* HandTrace::Current()
{
	return _current.has_value() ? &*_current : nullptr;
}

void HandTrace::EndFrame()
{
	if (!_current.has_value() || !Enabled())
	{
		_current.reset();
		return;
	}
	_ring.at(_next) = *_current;
	_next = (_next + 1) % _capacity;
	_count = std::min(_count + 1, _capacity);
	_current.reset();
}

std::vector<Frame> HandTrace::Frames() const
{
	std::vector<Frame> frames;
	frames.reserve(_count);
	const size_t first = (_next + _capacity - _count) % std::max<size_t>(_capacity, 1);
	for (size_t i = 0; i < _count; ++i)
	{
		frames.push_back(_ring.at((first + i) % _capacity));
	}
	return frames;
}

std::vector<Jump> openblack::hand_trace::FindJumps(std::span<const Frame> frames, float threshold, size_t returnWithin)
{
	std::vector<Jump> jumps;
	for (size_t i = 1; i < frames.size(); ++i)
	{
		const auto& before = frames[i - 1];
		const auto& now = frames[i];
		if (!before.screen.has_value() || !now.screen.has_value())
		{
			continue;
		}
		float moved = glm::distance(*before.screen, *now.screen);
		const bool wristShown = before.wristScreen.has_value() && now.wristScreen.has_value();
		const bool byWrist = wristShown && glm::distance(*before.wristScreen, *now.wristScreen) > moved;
		if (byWrist)
		{
			moved = glm::distance(*before.wristScreen, *now.wristScreen);
		}
		const float cursorMoved = glm::distance(before.cursor, now.cursor);
		const float beyond = moved - cursorMoved;
		if (beyond <= threshold)
		{
			continue;
		}
		bool cameBack = false;
		for (size_t j = i + 1; j < frames.size() && j <= i + returnWithin; ++j)
		{
			const auto& later = frames[j];
			const bool back =
			    byWrist ? later.wristScreen.has_value() && glm::distance(*later.wristScreen, *before.wristScreen) <= threshold
			            : later.screen.has_value() && glm::distance(*later.screen, *before.screen) <= threshold;
			if (back)
			{
				cameBack = true;
				break;
			}
		}
		const auto writes = now.Writes();
		jumps.push_back({.index = i,
		                 .frame = now.frame,
		                 .pixels = moved,
		                 .beyondCursor = beyond,
		                 .cameBack = cameBack,
		                 .writer = writes.empty() ? std::string_view {} : writes.back().writer});
	}
	return jumps;
}
