/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScreenFade.h"

#include "Common/X87.h"

using namespace openblack::gui;

namespace
{
/// The amount that the fade moves in a millisecond
constexpr float k_AmountPerMillisecond = 0.001f;
/// The steps of the fade's alpha
constexpr float k_AlphaSteps = 255.0f;
} // namespace

void ScreenFade::FadeFrom(float amount)
{
	_amount = amount;
	_target = 0.0f;
}

void ScreenFade::FadeFrom(float amount, glm::vec3 colour)
{
	FadeFrom(amount);
	_colour = colour;
	_turns = 0;
}

void ScreenFade::FadeThrough(glm::vec3 colour)
{
	_amount = 0.0f;
	_target = 1.0f;
	_colour = colour;
	_turns = 0;
}

void ScreenFade::Update(uint32_t realMilliseconds, bool inTemple)
{
	if (_amount == _target && !inTemple)
	{
		return;
	}
	const float step = x87::Mul(static_cast<double>(realMilliseconds), k_AmountPerMillisecond);
	if (_amount < _target)
	{
		// The fade turns only when it goes past its amount
		_amount = x87::Add(step, _amount);
		if (_amount > _target)
		{
			_amount = _target;
			_target = 0.0f;
			++_turns;
		}
	}
	else
	{
		// A fade at its amount steps down too, and turns again
		_amount = x87::Sub(_amount, step);
		if (_amount < _target)
		{
			_amount = _target;
			++_turns;
			if (_target <= 0.0f)
			{
				_colour = glm::vec3(0.0f);
			}
		}
	}
}

glm::vec4 ScreenFade::GetColour() const
{
	if (_amount > 1.0f)
	{
		return {_colour, 1.0f};
	}
	const auto alpha = static_cast<int32_t>(x87::Mul(_amount, k_AlphaSteps));
	return {_colour, static_cast<float>(alpha) / k_AlphaSteps};
}
