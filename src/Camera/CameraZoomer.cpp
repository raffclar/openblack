/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraZoomer.h"

#include "Common/Zoomer.h"

using namespace openblack::camera;

namespace
{
constexpr float k_Instant = 0.001f;
constexpr float k_Third = 0.33333334f;
constexpr float k_Sixth = 0.16666667f;
} // namespace

void Zoomer::SetPosition(float value)
{
	*this = Zoomer {};
	_value = value;
	_destination = value;
	_startValue = value;
}

void Zoomer::SetDestination(float destination, float speed, float seconds)
{
	// Too short a time, or a time that is not a number, puts it there
	if (!(seconds >= k_Instant))
	{
		SetPosition(destination);
		return;
	}
	_startSpeed = _speed;
	_startValue = _value;
	_destination = destination;
	_destinationSpeed = speed;
	_duration = seconds;
	_time = 0.0f;
	// Where it must be, how fast and no longer accelerating when it arrives, solved as the game solves it
	const auto coefficients = zoomer_path::Coefficients(_startValue, _startSpeed, destination, speed, seconds);
	_c1 = coefficients.x;
	_c2 = coefficients.y;
	_c3 = coefficients.z;
}

bool Zoomer::Step(float seconds)
{
	_time += seconds;
	if (!(_time < _duration))
	{
		_value = _destination;
		_speed = _destinationSpeed;
		_time = _duration;
		return false;
	}
	return true;
}

void Zoomer::Update(float seconds)
{
	if (!Step(seconds))
	{
		return;
	}
	// The game's order of float operations for the camera's position and focus
	const float t = _time;
	const float half = (t * t) * 0.5f;
	const float sixth = (t * half) * k_Third;
	_speed = (((t * _c1) + (half * _c2)) + (sixth * _c3)) + _startSpeed;
	const float twentyFourth = (half * half) * k_Sixth;
	_value = ((((twentyFourth * _c3) + (sixth * _c2)) + (half * _c1)) + (t * _startSpeed)) + _startValue;
}

void Zoomer::UpdateLens(float seconds)
{
	if (!Step(seconds))
	{
		return;
	}
	// The game's order of float operations for the lens: the speed adds its terms in a different order
	const float t = _time;
	const float half = (t * t) * 0.5f;
	const float sixth = (t * half) * k_Third;
	_speed = (((half * _c2) + (sixth * _c3)) + (t * _c1)) + _startSpeed;
	const float twentyFourth = (half * half) * k_Sixth;
	_value = ((((twentyFourth * _c3) + (sixth * _c2)) + (half * _c1)) + (_startSpeed * t)) + _startValue;
}
