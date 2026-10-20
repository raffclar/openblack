/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandKnock.h"

using namespace openblack::hand_knock;

void Tap::Knock(glm::vec3 point)
{
	if (!_point.has_value())
	{
		_time = std::chrono::microseconds::zero();
	}
	_point = point;
}

bool Tap::Hold(bool holds)
{
	if (holds && _point.has_value())
	{
		_time = std::chrono::microseconds::zero();
	}
	return _point.has_value() && !holds;
}

uint32_t Tap::Play(std::chrono::microseconds frame, uint32_t clipMs)
{
	const auto timeMs = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(_time).count());
	_time += frame;
	if (_time >= std::chrono::milliseconds(clipMs))
	{
		Stop();
	}
	return timeMs;
}
