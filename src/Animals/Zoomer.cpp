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

#include <array>

using namespace openblack::animals;

namespace
{
/// A glide shorter than this is over at once
constexpr float k_Instant = 0.001f;
/// A determinant nearer to nought than this counts as this, with its sign
constexpr float k_SmallestDeterminant = 1.0e-10f;

/// A 3 by 3 matrix, row after row
using Matrix3 = std::array<float, 9>;

/// The inverse of a 3 by 3 matrix from its cofactors. Each operation rounds to a float, as in the game
Matrix3 Inverse(const Matrix3& m)
{
	const float first = m[8] * m[4] - m[7] * m[5];
	const float second = (m[2] * m[7] - m[8] * m[1]) * m[3];
	const float third = (m[5] * m[1] - m[2] * m[4]) * m[6];
	float determinant = (second + third) + first * m[0];
	if (std::abs(determinant) < k_SmallestDeterminant)
	{
		determinant = determinant < 0.0f ? -k_SmallestDeterminant : k_SmallestDeterminant;
	}
	const float scale = 1.0f / determinant;
	return {
	    first * scale,
	    (m[2] * m[7] - m[8] * m[1]) * scale,
	    (m[5] * m[1] - m[2] * m[4]) * scale,
	    (m[6] * m[5] - m[8] * m[3]) * scale,
	    (m[8] * m[0] - m[6] * m[2]) * scale,
	    (m[2] * m[3] - m[0] * m[5]) * scale,
	    (m[7] * m[3] - m[6] * m[4]) * scale,
	    (m[6] * m[1] - m[0] * m[7]) * scale,
	    (m[0] * m[4] - m[3] * m[1]) * scale,
	};
}
} // namespace

void Zoomer::SetTarget(float target, float targetSpeed, float seconds)
{
	if (seconds < k_Instant)
	{
		*this = Zoomer(target);
		return;
	}
	_start = _value;
	_startSpeed = _speed;
	_target = target;
	_targetSpeed = targetSpeed;
	_duration = seconds;
	_elapsed = 0.0f;
	// The coefficients that bring the curve to the target at the target's speed and without acceleration at the end:
	// c4 q + c3 c + c2 h = position, c4 c + c3 h + c2 t = velocity, c4 h + c3 t + c2 = 0. The inverse gives the solution
	const float t = seconds;
	const float h = (t * t) * 0.5f;
	const float c = (h * t) * (1.0f / 3.0f);
	const float q = (h * h) * (1.0f / 6.0f);
	const Matrix3 inverse = Inverse({q, c, h, c, h, t, h, t, 1.0f});
	const float position = (target - _start) - seconds * _startSpeed;
	const float velocity = targetSpeed - _startSpeed;
	_c4 = inverse[3] * velocity + inverse[0] * position;
	_c3 = inverse[1] * position + inverse[4] * velocity;
	_c2 = inverse[5] * velocity + inverse[2] * position;
}

float Zoomer::Step(float seconds)
{
	_elapsed = seconds + _elapsed;
	if (_elapsed < _duration)
	{
		const float t = _elapsed;
		const float h = (t * t) * 0.5f;
		const float c = (t * h) * (1.0f / 3.0f);
		const float q = (h * h) * (1.0f / 6.0f);
		_speed = ((t * _c2 + h * _c3) + c * _c4) + _startSpeed;
		_value = (((q * _c4 + c * _c3) + h * _c2) + t * _startSpeed) + _start;
	}
	else
	{
		_value = _target;
		_speed = _targetSpeed;
		_elapsed = _duration;
	}
	return _value;
}
