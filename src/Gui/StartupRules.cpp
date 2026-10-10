/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StartupRules.h"

#include <cmath>

#include <algorithm>
#include <array>

namespace openblack::startup
{

namespace
{
constexpr float k_Third = 1.0f / 3.0f;
constexpr float k_Sixth = 1.0f / 6.0f;
/// A determinant smaller than this is taken as this, keeping its sign
constexpr float k_SmallestDeterminant = 1e-10f;
/// Shorter moves than this jump straight to their destination
constexpr float k_ShortestMove = 0.001f;

/// The logo still's three moves: where it goes and in how long
struct Move
{
	float destination;
	float seconds;
};
constexpr Move k_FadeIn {.destination = 1.0f, .seconds = 0.7f};
constexpr Move k_Hold {.destination = 2.0f, .seconds = 1.7f};
constexpr Move k_FadeOut {.destination = 0.0f, .seconds = 1.1f};
} // namespace

void Zoomer::Snap(float position) noexcept
{
	*this = {};
	_position = position;
	_destination = position;
	_startPosition = position;
}

void Zoomer::SetDestination(float destination, float destinationSpeed, float seconds) noexcept
{
	if (seconds < k_ShortestMove)
	{
		Snap(destination);
		return;
	}
	_startSpeed = _speed;
	_startPosition = _position;
	_destination = destination;
	_destinationSpeed = destinationSpeed;
	_duration = seconds;
	_time = 0.0f;

	// The quartic's acceleration, jerk and snap make it arrive at the destination at the speed asked for with no
	// acceleration: three equations, solved through the inverse of their matrix in the order the game sums them
	const float half = seconds * seconds * 0.5f;
	const float sixth = half * seconds * k_Third;
	const float twentyFourth = half * half * k_Sixth;
	const std::array<float, 9> m = {twentyFourth, sixth, half, sixth, half, seconds, half, seconds, 1.0f};
	const float cofactor = m[8] * m[4] - m[7] * m[5];
	float determinant = ((m[2] * m[7] - m[8] * m[1]) * m[3] + (m[5] * m[1] - m[2] * m[4]) * m[6]) + cofactor * m[0];
	if (!(k_SmallestDeterminant <= std::abs(determinant)))
	{
		determinant = determinant < 0.0f ? -k_SmallestDeterminant : k_SmallestDeterminant;
	}
	const float inverse = 1.0f / determinant;
	const float i11 = cofactor * inverse;
	const float i21 = (m[6] * m[5] - m[8] * m[3]) * inverse;
	const float i12 = (m[2] * m[7] - m[8] * m[1]) * inverse;
	const float i22 = (m[8] * m[0] - m[6] * m[2]) * inverse;
	const float i13 = (m[5] * m[1] - m[2] * m[4]) * inverse;
	const float i23 = (m[2] * m[3] - m[0] * m[5]) * inverse;

	const float distance = (_destination - _startPosition) - _duration * _startSpeed;
	const float speedChange = _destinationSpeed - _startSpeed;
	_snap = i21 * speedChange + i11 * distance;
	_jerk = i12 * distance + i22 * speedChange;
	_acceleration = i23 * speedChange + i13 * distance;
}

void Zoomer::Advance(int32_t milliseconds) noexcept
{
	_time = static_cast<float>(milliseconds) * 0.001f + _time;
	if (_time < _duration)
	{
		const float half = _time * _time * 0.5f;
		const float sixth = _time * half * k_Third;
		_speed = ((_acceleration * _time + _snap * sixth) + _jerk * half) + _startSpeed;
		_position =
		    ((((half * half * k_Sixth) * _snap + _time * _startSpeed) + _jerk * sixth) + _acceleration * half) + _startPosition;
	}
	else
	{
		_position = _destination;
		_speed = _destinationSpeed;
		_time = _duration;
	}
}

uint8_t LogoAlpha(float position) noexcept
{
	// Rounded towards zero, as the game converts
	const auto alpha = static_cast<int32_t>(position * 255.0f);
	return static_cast<uint8_t>(std::clamp(alpha, 0, 255));
}

LogoStill::LogoStill() noexcept
{
	_zoomer.SetDestination(k_FadeIn.destination, 0.0f, k_FadeIn.seconds);
}

void LogoStill::NextPhase() noexcept
{
	++_phase;
	const auto& move = _phase == 1 ? k_Hold : k_FadeOut;
	_zoomer.SetDestination(move.destination, 0.0f, move.seconds);
}

uint8_t LogoStill::Frame(int32_t milliseconds, bool pressed) noexcept
{
	_zoomer.Advance(milliseconds);
	// The frame shows the alpha from before anything pressed changes it
	const uint8_t alpha = LogoAlpha(_zoomer.Position());
	if (pressed && _phase < 2)
	{
		// Straight to fully opaque, and from there the fade out
		_phase = 1;
		_zoomer.Snap(1.0f);
		NextPhase();
	}
	else if (pressed && _phase == 2)
	{
		_zoomer.Snap(0.0f);
		NextPhase();
	}
	else if (_zoomer.Arrived())
	{
		NextPhase();
	}
	return alpha;
}

} // namespace openblack::startup
