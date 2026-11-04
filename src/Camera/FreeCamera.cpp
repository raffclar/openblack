/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FreeCamera.h"

#include <cmath>

#include <numbers>

#include "Common/X87.h"
#include "ScriptCameraModel.h"

namespace openblack::free_camera
{

namespace
{

/// The milliseconds of the frame as seconds, as the game rounds them
float SecondsOf(int32_t milliseconds)
{
	constexpr float k_SecondsPerMillisecond = 0.001f;
	return static_cast<float>(milliseconds) * k_SecondsPerMillisecond;
}

/// The squared length, with the parts added in the game's order
float LengthSquared(const glm::vec3& v)
{
	return ((v.y * v.y) + (v.z * v.z)) + (v.x * v.x);
}

/// The direction of a vector, as the game makes it. A vector of no length stays as it is.
glm::vec3 Normalised(glm::vec3 v)
{
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
	{
		return v;
	}
	const float inverse = 1.0f / std::sqrt(LengthSquared(v));
	return {v.x * inverse, v.y * inverse, v.z * inverse};
}

glm::vec3 Scaled(const glm::vec3& v, float scale)
{
	return {v.x * scale, v.y * scale, v.z * scale};
}

} // namespace

void ReadKeys(State& state, const Keys& keys, int32_t milliseconds)
{
	const float seconds = SecondsOf(milliseconds);
	// The game cuts the count of each key to a whole number towards zero
	const auto count = [seconds](bool less, bool more) {
		if (less)
		{
			return static_cast<int32_t>(seconds * -k_KeyCountPerSecond);
		}
		return more ? static_cast<int32_t>(seconds * k_KeyCountPerSecond) : 0;
	};
	state.across = count(keys.left, keys.right);
	state.along = count(keys.forwards, keys.backwards);
	state.shift = keys.shift;
	state.ctrl = keys.ctrl;
}

View Step(const View& now, State& state, const Input& input)
{
	glm::vec3 origin = now.origin;
	glm::vec3 focus = now.focus;
	const glm::vec3 gap {origin.x - focus.x, origin.y - focus.y, origin.z - focus.z};
	const float distance = std::sqrt(((gap.z * gap.z) + (gap.y * gap.y)) + (gap.x * gap.x));
	// The heading and the pitch of the focus as the eye sees it
	const auto turned = script_camera::HeadingAndPitchFromPoints(focus, origin);
	float heading = turned.heading;
	float pitch = turned.pitch;

	// The way the camera looks, the way to its right, and the way up from its view
	const glm::vec3 forward = Normalised({focus.x - origin.x, focus.y - origin.y, focus.z - origin.z});
	const glm::vec3 right = Normalised({forward.z - 0.0f, 0.0f, 0.0f - forward.x});
	const glm::vec3 up = Normalised({
	    (right.z * forward.y) - (right.y * forward.z),
	    (forward.z * right.x) - (right.z * forward.x),
	    (right.y * forward.x) - (forward.y * right.x),
	});

	// The speed falls with time
	const float seconds = SecondsOf(input.milliseconds);
	state.speed = x87::Exp(seconds * k_SpeedFall) * state.speed;
	// While the hand grips the land, the mouse turns the camera and the speed goes up
	if (input.gripsLand)
	{
		constexpr float k_Middle = 0.5f;
		heading = (((x87::Div(input.mouse.x, input.screen.x) - k_Middle) * seconds) * k_MouseTurn) + heading;
		pitch = pitch - (((x87::Div(input.mouse.y, input.screen.y) - k_Middle) * seconds) * k_MouseTurn);
		state.speed = seconds + state.speed;
	}
	const glm::vec3 flight = Scaled(Scaled(forward, state.speed), k_SpeedDistance);
	origin = flight + origin;
	focus = flight + focus;

	const auto across = static_cast<float>(state.across);
	const auto along = static_cast<float>(state.along);
	if (state.ctrl)
	{
		// To the side and up and down
		const glm::vec3 move = Scaled(Scaled(right, across) - Scaled(up, along), k_KeyMove);
		origin = move + origin;
		focus = move + focus;
	}
	else if (state.shift)
	{
		// Turn and tilt
		constexpr float k_HalfTurn = -std::numbers::pi_v<float>;
		heading = heading - x87::Div(across * k_HalfTurn, input.screen.x);
		const float tilted = pitch - (along * k_KeyTilt);
		if (tilted <= k_LowestPitch)
		{
			pitch = k_LowestPitch;
		}
		else if (tilted < k_HighestPitch)
		{
			pitch = tilted;
		}
		else
		{
			pitch = k_HighestPitch;
		}
	}
	else
	{
		// To the side and along the view
		const glm::vec3 move = Scaled(Scaled(right, across) - Scaled(forward, along), k_KeyMove);
		origin = move + origin;
		focus = move + focus;
	}
	state.across = 0;
	state.along = 0;

	// The focus is in front of the eye at the same distance as before, as the heading and pitch give it
	focus = script_camera::PointFromDistanceHeadingAndPitch(origin, distance, heading, pitch);
	return {.origin = origin, .focus = focus};
}

} // namespace openblack::free_camera
