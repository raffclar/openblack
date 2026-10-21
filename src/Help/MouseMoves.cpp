/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MouseMoves.h"

#include <algorithm>

using namespace openblack::help;

void MouseMoves::Message(uint32_t milliseconds, glm::ivec2 position)
{
	// The first message has no time before it
	const auto previous = _lastMessage.value_or(milliseconds);
	_lastMessage = milliseconds;
	_lastWake = milliseconds;
	const auto elapsed = static_cast<int32_t>(milliseconds - previous);
	if (elapsed <= 0)
	{
		return;
	}
	_sum += static_cast<uint32_t>(elapsed);
	if (_sum > k_SendAfterMilliseconds)
	{
		_sum = 0;
		_sent.push_back(position);
	}
}

void MouseMoves::Wait(uint32_t milliseconds, glm::ivec2 position, bool stillMessages)
{
	if (!stillMessages)
	{
		// The wait starts again when the window takes these messages again
		_lastWake = milliseconds;
		return;
	}
	if (!_lastWake.has_value())
	{
		_lastWake = milliseconds;
		return;
	}
	while (milliseconds - *_lastWake >= k_StillMessageMilliseconds)
	{
		const auto at = *_lastWake + k_StillMessageMilliseconds;
		Message(at, position);
	}
}

uint32_t MouseMoves::TakeMoves()
{
	const auto moves = std::ranges::count_if(_sent, [this](glm::ivec2 place) {
		const bool moved = _lastPlace != place;
		_lastPlace = place;
		return moved;
	});
	_sent.clear();
	return static_cast<uint32_t>(moves);
}
