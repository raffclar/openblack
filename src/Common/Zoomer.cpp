/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Zoomer.h"

#include <cmath>

using namespace openblack;

namespace
{
constexpr float k_MinimumDuration = 0.001f;
constexpr float k_Third = 0.33333334f;
constexpr float k_Sixth = 0.16666667f;
constexpr float k_SmallestDeterminant = 1e-10f;
} // namespace

void Zoomer::Reset(float value)
{
	_value = value;
	_destination = value;
	_startValue = value;
	_speed = 0.0f;
	_destinationSpeed = 0.0f;
	_startSpeed = 0.0f;
	_elapsed = 0.0f;
	_duration = 0.0f;
	_coefficients = glm::vec3(0.0f);
}

void Zoomer::SetDestination(float destination, float seconds, float speed)
{
	// Too short a time, or none at all, puts it there
	if (!(seconds >= k_MinimumDuration))
	{
		Reset(destination);
		return;
	}
	_startValue = _value;
	_startSpeed = _speed;
	_destination = destination;
	_destinationSpeed = speed;
	_duration = seconds;
	_elapsed = 0.0f;

	// The end conditions, the value and speed it is to have at the end and its last term ending at nothing, solved as the
	// game solves them: by the inverse of their matrix, its determinant kept at least 1e-10 across
	const float s = seconds;
	const float a = (s * s) * 0.5f;
	const float b = (a * s) * k_Third;
	const float c = (a * a) * k_Sixth;
	const float cofactor11 = a - s * s;
	const float cofactor21 = a * s - b;
	const float cofactor22 = c - a * a;
	const float cofactor13 = s * b - a * a;
	const float cofactor23 = a * b - c * s;
	float determinant = (cofactor21 * b + cofactor13 * a) + cofactor11 * c;
	if (std::abs(determinant) < k_SmallestDeterminant)
	{
		determinant = determinant < 0.0f ? -k_SmallestDeterminant : k_SmallestDeterminant;
	}
	const float inverse = 1.0f / determinant;
	const float i11 = cofactor11 * inverse;
	const float i21 = cofactor21 * inverse;
	const float i22 = cofactor22 * inverse;
	const float i13 = cofactor13 * inverse;
	const float i23 = cofactor23 * inverse;
	const float r1 = (destination - _startValue) - _duration * _startSpeed;
	const float r2 = speed - _startSpeed;
	_coefficients.z = i21 * r2 + i11 * r1;
	_coefficients.y = i21 * r1 + i22 * r2;
	_coefficients.x = i23 * r2 + i13 * r1;
}

void Zoomer::Update(float deltaSeconds)
{
	const float t = deltaSeconds + _elapsed;
	_elapsed = t;
	// Along the path while there is time left, or the time is not a number
	if (t >= _duration)
	{
		_value = _destination;
		_speed = _destinationSpeed;
		_elapsed = _duration;
		return;
	}
	const float a = (t * t) * 0.5f;
	const float b = (t * a) * k_Third;
	_speed = ((t * _coefficients.x + a * _coefficients.y) + b * _coefficients.z) + _startSpeed;
	const float c = (a * a) * k_Sixth;
	_value = ((((c * _coefficients.z) + b * _coefficients.y) + a * _coefficients.x) + t * _startSpeed) + _startValue;
}

void Zoomer::UpdateInline(float deltaSeconds)
{
	const float t = deltaSeconds + _elapsed;
	_elapsed = t;
	if (t >= _duration)
	{
		_value = _destination;
		_speed = _destinationSpeed;
		_elapsed = _duration;
		return;
	}
	const float a = (t * t) * 0.5f;
	const float b = (t * a) * k_Third;
	_speed = ((a * _coefficients.y + b * _coefficients.z) + t * _coefficients.x) + _startSpeed;
	_value =
	    (((((a * a) * k_Sixth) * _coefficients.z + b * _coefficients.y) + t * _startSpeed) + a * _coefficients.x) + _startValue;
}

Zoomer3::Zoomer3(glm::vec3 point)
    : x(point.x)
    , y(point.y)
    , z(point.z)
{
}

void Zoomer3::Reset(glm::vec3 point)
{
	x.Reset(point.x);
	y.Reset(point.y);
	z.Reset(point.z);
}

void Zoomer3::SetDestination(glm::vec3 destination, float seconds)
{
	x.SetDestination(destination.x, seconds);
	y.SetDestination(destination.y, seconds);
	z.SetDestination(destination.z, seconds);
}

void Zoomer3::Update(float deltaSeconds)
{
	x.Update(deltaSeconds);
	y.Update(deltaSeconds);
	z.Update(deltaSeconds);
}

glm::vec3 Zoomer3::GetValue() const
{
	return {x.GetValue(), y.GetValue(), z.GetValue()};
}
