/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InputSteps.h"

#include <algorithm>

#include <glm/common.hpp>
#include <glm/vec2.hpp>

using namespace openblack::input_steps;

Steps Clock::Advance(std::chrono::microseconds frame)
{
	Steps steps;
	if (frame.count() <= 0)
	{
		return steps;
	}
	const auto counts = static_cast<int64_t>(frame.count()) * static_cast<int64_t>(k_StepsPerSecond);
	const auto before = _owed;
	_owed += counts;
	while (_owed >= k_CountsPerStep - k_EarlyCounts && steps.count < k_MostSteps)
	{
		// The step falls where the count gets to the next whole step, or at the end of the frame when it comes early
		const auto at = static_cast<int64_t>(steps.count + 1) * k_CountsPerStep - before;
		steps.points.at(steps.count) =
		    std::clamp(static_cast<float>(static_cast<double>(at) / static_cast<double>(counts)), 0.0f, 1.0f);
		steps.times.at(steps.count) = StepTime(_taken++);
		++steps.count;
		_owed -= k_CountsPerStep;
	}
	// The time of a frame longer than the most steps is not used
	if (_owed >= k_CountsPerStep)
	{
		_owed %= k_CountsPerStep;
	}
	return steps;
}

void Clock::Restart()
{
	_owed = 0;
	_taken = 0;
}

std::array<StepMouse, k_MostSteps> MouseSplit::Split(const FrameMouse& frame, const Steps& steps)
{
	std::array<StepMouse, k_MostSteps> split {};
	if (steps.count == 0)
	{
		_delta += frame.delta;
		_pushDelta += frame.pushDelta;
		_wheel += frame.wheel;
		_wheelActions |= frame.wheelActions;
		return split;
	}
	glm::ivec2 takenDelta {0, 0};
	glm::ivec2 takenPush {0, 0};
	for (uint32_t i = 0; i < steps.count; ++i)
	{
		const float part = steps.Point(i);
		const auto upToDelta = glm::ivec2(glm::round(glm::vec2(frame.delta) * part));
		const auto upToPush = glm::ivec2(glm::round(glm::vec2(frame.pushDelta) * part));
		auto& step = split.at(i);
		step.position =
		    frame.jumped ? frame.to : glm::ivec2(glm::round(Between(glm::vec2(frame.from), glm::vec2(frame.to), part)));
		step.delta = upToDelta - takenDelta;
		step.pushDelta = upToPush - takenPush;
		takenDelta = upToDelta;
		takenPush = upToPush;
	}
	// The first step takes what the frames with no step kept, and the wheel
	split.at(0).delta += _delta;
	split.at(0).pushDelta += _pushDelta;
	split.at(0).wheel = _wheel + frame.wheel;
	split.at(0).wheelActions = _wheelActions | frame.wheelActions;
	// The part of the frame after its last step waits for the next step
	_delta = frame.delta - takenDelta;
	_pushDelta = frame.pushDelta - takenPush;
	_wheel = 0.0f;
	_wheelActions = 0;
	return split;
}

void MouseSplit::Restart()
{
	*this = {};
}
