/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFollow.h"

#include <cmath>

#include <algorithm>

#include "Common/X87Vector.h"
#include "DoubleClickFlight.h"
#include "ScriptCameraModel.h"

namespace openblack::creature_follow
{

namespace
{
/// The wheel's zoom: each count it moves draws the camera in by this share, about the half-thousandth parts below
constexpr float k_WheelZoomPerCount = -0.15f;
/// Shift and the cursor keys tilt the camera this many radians a pixel; Ctrl and the cursor keys, and the wheel, scale
/// the distance by a five-hundredth part a pixel
constexpr float k_TiltPerPixel = 0.002f;
constexpr float k_ZoomPixels = 500.0f;
constexpr float k_ZoomPerPixel = 0.002f;
/// The distance it looks at is taken this much of the way towards 50 when nearer, and towards 100 when further
constexpr float k_ClearNear = 50.0f;
constexpr float k_ClearNearShare = 0.8f;
constexpr float k_ClearFar = 100.0f;
constexpr float k_ClearFarShare = 0.1f;
/// The game's float of pi / 2048, which turns a doubled game angle to radians, and its float of a quarter turn
constexpr float k_HalfGameAngleToRadians = 0.0015339808305725455f;
constexpr float k_RelativeQuarterTurn = 1.5707963705062866f;
} // namespace

Target FlockTarget(const std::optional<FlockLeader>& leader, const map_coords::MapCoords& place, const GroundHeight& ground)
{
	if (leader.has_value())
	{
		// The leader's place on the map: x and z through a map position, at the height of the place
		return Target {
		    .position = {map_coords::Quantise(leader->position.x), leader->position.y,
		                 map_coords::Quantise(leader->position.z)},
		    .height = leader->height,
		    .gameAngle = std::nullopt,
		    .hasModel = false,
		};
	}
	// The flock's own place on the land, raised by its altitude. A flock with no leader adds no height.
	const auto xz = map_coords::ToMetres(place);
	return Target {
	    .position = {xz.x, ground(xz) + place.altitude, xz.y},
	    .height = 0.0f,
	    .gameAngle = std::nullopt,
	    .hasModel = false,
	};
}

glm::vec3 Focus(glm::vec3 position, float height)
{
	return position + glm::vec3(0.0f, height * 0.5f, 0.0f);
}

float ViewingDistance(float height)
{
	return height * k_ViewingDistancePerHeight;
}

HeadingPitch HeadingPitchOf(glm::vec3 point, glm::vec3 centre)
{
	// The follow camera measures the angles with the same maths as the script camera
	const auto look = script_camera::HeadingAndPitchFromPoints(point, centre);
	return {.heading = look.heading, .pitch = look.pitch};
}

View Start(glm::vec3 cameraOrigin, glm::vec3 cameraFocus, glm::vec3 creature, float height, StartDistance start)
{
	return Start({.origin = cameraOrigin, .targetOrigin = cameraOrigin, .targetFocus = cameraFocus}, creature, height, start);
}

View Start(const StartPoints& camera, glm::vec3 creature, float height, StartDistance start)
{
	const auto turned = HeadingPitchOf(camera.targetOrigin, camera.targetFocus);
	auto distance = ViewingDistance(height);
	const float acrossX = creature.x - camera.origin.x;
	const float acrossZ = creature.z - camera.origin.z;
	if (start == StartDistance::KeepWhenClose && std::sqrt((acrossZ * acrossZ) + (acrossX * acrossX)) < k_CloseAcross)
	{
		// Already close, it stays about as far as it is, but no nearer than twice the creature's height and no further
		// than it would start
		const auto now = x87::Distance<x87::SumOrder::Yxz>(creature, camera.origin);
		const auto nearest = height * 2.0f;
		distance = now > nearest ? std::min(now, distance) : nearest;
	}
	return Clamped({.yaw = turned.heading, .pitch = turned.pitch, .distance = distance});
}

View Clamped(View view)
{
	view.distance = std::clamp(view.distance, k_MinDistance, k_MaxDistance);
	view.pitch = std::max(view.pitch, k_MinPitch);
	return view;
}

float EaseSeconds(float secondsInMode)
{
	if (secondsInMode >= k_EaseSettleSeconds)
	{
		return k_EaseSeconds;
	}
	const auto settled = std::max(secondsInMode, 0.0f) / k_EaseSettleSeconds;
	return k_StartEaseSeconds + ((k_EaseSeconds - k_StartEaseSeconds) * settled);
}

glm::vec3 Origin(glm::vec3 focus, const View& view)
{
	return script_camera::PointFromDistanceHeadingAndPitch(focus, view.distance, view.yaw, view.pitch);
}

Keys KeysFor(int across, int along, float seconds)
{
	// Each way keeps only the whole pixels of the frame
	return {
	    .across = std::trunc(static_cast<float>(across) * k_KeyPixelsPerSecond * seconds),
	    .along = std::trunc(static_cast<float>(along) * k_KeyPixelsPerSecond * seconds),
	};
}

float RelativeHeading(float heading, uint16_t gameAngle)
{
	const float turned = static_cast<float>(static_cast<int32_t>(gameAngle) << 1) * k_HalfGameAngleToRadians;
	const float facing = turned - k_RelativeQuarterTurn;
	return heading - facing;
}

float OffsetDistance(const Offsets& offsets)
{
	const float dx = offsets.focus.x - offsets.eye.x;
	const float dy = offsets.focus.y - offsets.eye.y;
	const float dz = offsets.focus.z - offsets.eye.z;
	const float squares = (dz * dz) + (dy * dy);
	return std::sqrt(squares + (dx * dx));
}

Offsets OffsetsAtDistance(const Offsets& offsets, float distance)
{
	glm::vec3 line {offsets.eye.x - offsets.focus.x, offsets.eye.y - offsets.focus.y, offsets.eye.z - offsets.focus.z};
	if (line.x != 0.0f || line.y != 0.0f || line.z != 0.0f)
	{
		const float squares = (line.z * line.z) + (line.y * line.y);
		const float inverse = 1.0f / std::sqrt(squares + (line.x * line.x));
		line = {inverse * line.x, inverse * line.y, inverse * line.z};
	}
	line = {distance * line.x, distance * line.y, distance * line.z};
	return {.focus = offsets.focus, .eye = {offsets.focus.x + line.x, offsets.focus.y + line.y, offsets.focus.z + line.z}};
}

KeyOutcome Apply(View& view, const Keys& keys, float screenWidth)
{
	std::optional<Offsets> none;
	return Apply(view, none, keys, screenWidth);
}

KeyOutcome Apply(View& view, std::optional<Offsets>& offsets, const Keys& keys, float screenWidth)
{
	const auto zoom = ((keys.wheel * k_WheelZoomPerCount) + k_ZoomPixels) * k_ZoomPerPixel;
	view.distance = std::max(view.distance * zoom, k_MinDistance);
	if (keys.Moving())
	{
		// The cursor keys stop the offsets. The view starts from the heading, the pitch and the distance of the offsets.
		if (offsets.has_value())
		{
			const auto turned = HeadingPitchOf(offsets->eye, offsets->focus);
			view.yaw = turned.heading;
			view.pitch = turned.pitch;
			view.distance = OffsetDistance({.focus = offsets->eye, .eye = offsets->focus});
			offsets.reset();
		}
		const auto turn = keys.across * std::numbers::pi_v<float> / std::max(screenWidth, 1.0f);
		if (keys.ctrl)
		{
			view.yaw -= turn;
			view.distance = std::max(view.distance * ((keys.along + k_ZoomPixels) * k_ZoomPerPixel), k_MinDistance);
		}
		else if (keys.shift)
		{
			view.yaw -= turn;
			view.pitch = std::clamp(view.pitch - (keys.along * k_TiltPerPixel), k_MinKeyPitch, k_MaxKeyPitch);
		}
		else
		{
			return KeyOutcome::Leave;
		}
	}
	view.distance = std::clamp(view.distance * zoom, k_MinDistance, k_MaxKeyDistance);
	if (offsets.has_value())
	{
		*offsets = OffsetsAtDistance(*offsets, view.distance);
	}
	return KeyOutcome::Stay;
}

float ClearingDistance(float distance)
{
	if (distance < k_ClearNear)
	{
		distance += (k_ClearNear - distance) * k_ClearNearShare;
	}
	if (distance > k_ClearFar)
	{
		distance += (k_ClearFar - distance) * k_ClearFarShare;
	}
	return distance;
}

View ClearView(View view, glm::vec3 focus, const camera_limits::Ground& ground)
{
	return ClearView(view, Origin(focus, view), focus, ground);
}

View ClearView(View view, glm::vec3 eye, glm::vec3 focus, const camera_limits::Ground& ground)
{
	// The search starts from the angles and the distance of the eye that the camera has in this frame
	const auto look = HeadingPitchOf(eye, focus);
	const float distance = ClearingDistance(x87::Distance<x87::SumOrder::Zyx>(focus, eye));
	// The follow camera and the double click flight use the same search for a clear view
	const auto best = camera_fly::BestBearing(ground, {.heading = look.heading, .pitch = look.pitch}, distance, focus);
	// The camera measures the new angles again from the point that the search gives
	const auto point = script_camera::PointFromDistanceHeadingAndPitch(focus, distance, best.heading, best.pitch);
	const auto turned = HeadingPitchOf(point, focus);
	view.yaw = turned.heading;
	view.pitch = turned.pitch;
	return view;
}

} // namespace openblack::creature_follow
