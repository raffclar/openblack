/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Zoomer.h"

#include <glm/mat4x3.hpp>

#include "3D/ObjectMatrix.h"

using namespace openblack;

namespace
{
constexpr float k_MinSeconds = 0.001f; ///< Shorter moves jump straight to the target
constexpr float k_Half = 0.5f;
constexpr float k_Third = 0.33333334f; ///< 1/3 as a float
constexpr float k_Sixth = 0.16666667f; ///< 1/6 as a float
} // namespace

void Zoomer::SetPosition(float position)
{
	value = destination = startValue = position;
	speed = destinationSpeed = startSpeed = 0.0f;
	time = duration = 0.0f;
	c2 = c3 = c4 = 0.0f;
}

void Zoomer::SetDestinationWithSpeedAndTime(float target, float targetSpeed, float seconds)
{
	// Too short a time (or NaN) snaps to the target
	if (!(seconds >= k_MinSeconds))
	{
		SetPosition(target);
		return;
	}
	startSpeed = speed;
	startValue = value;
	destination = target;
	destinationSpeed = targetSpeed;
	duration = seconds;
	time = 0.0f;
	const float a = (seconds * seconds) * k_Half;
	const float b = (a * seconds) * k_Third;
	const float c = (a * a) * k_Sixth;
	// The end conditions as a matrix, translation 0
	const glm::mat4x3 m(glm::vec3(c, b, a), glm::vec3(b, a, seconds), glm::vec3(a, seconds, 1.0f), glm::vec3(0.0f));
	const auto inv = affine::Inverse(m);
	const float r1 = (destination - startValue) - duration * startSpeed;
	const float r2 = destinationSpeed - startSpeed;
	c4 = (inv[1][0] * r2 + inv[0][0] * r1) + inv[3][0];
	c3 = (inv[0][1] * r1 + inv[1][1] * r2) + inv[3][1];
	c2 = (inv[1][2] * r2 + inv[0][2] * r1) + inv[3][2];
}

void Zoomer::Update(float seconds)
{
	const float t = seconds + time;
	time = t;
	// The curve while t < duration (or NaN), the destination after
	if (t >= duration)
	{
		value = destination;
		speed = destinationSpeed;
		time = duration;
		return;
	}
	const float a = (t * t) * k_Half;
	const float b = (t * a) * k_Third;
	speed = ((t * c2 + a * c3) + b * c4) + startSpeed;
	const float c = (a * a) * k_Sixth;
	value = ((((c * c4) + b * c3) + a * c2) + t * startSpeed) + startValue;
}

void Zoomer3::SetPosition(const glm::vec3& position)
{
	for (int i = 0; i < 3; ++i)
	{
		axis[i].SetPosition(position[i]);
	}
}

void Zoomer3::SetDestinationWithTime(const glm::vec3& target, float seconds)
{
	// The original inlines y and z with an extra zero term in the sums, which does not change the result
	for (int i = 0; i < 3; ++i)
	{
		axis[i].SetDestinationWithSpeedAndTime(target[i], 0.0f, seconds);
	}
}

void Zoomer3::Update(float seconds)
{
	for (auto& zoomer : axis)
	{
		zoomer.Update(seconds);
	}
}

glm::vec3 Zoomer3::GetCurrentValue() const
{
	return {axis[0].value, axis[1].value, axis[2].value};
}

glm::vec3 Zoomer3::GetDestination() const
{
	return {axis[0].destination, axis[1].destination, axis[2].destination};
}

glm::vec3 Zoomer3::GetSpeed() const
{
	return {axis[0].speed, axis[1].speed, axis[2].speed};
}

glm::vec3 Zoomer3::GetStartValue() const
{
	return {axis[0].startValue, axis[1].startValue, axis[2].startValue};
}

glm::vec3 Zoomer3::GetStartSpeed() const
{
	return {axis[0].startSpeed, axis[1].startSpeed, axis[2].startSpeed};
}

glm::vec3 Zoomer3::GetDestinationSpeed() const
{
	return {axis[0].destinationSpeed, axis[1].destinationSpeed, axis[2].destinationSpeed};
}
