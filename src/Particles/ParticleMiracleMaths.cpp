/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ParticleMiracleMaths.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>

#include "Common/X87.h"
#include "Common/X87Vector.h"

using namespace openblack::particles;

namespace
{
constexpr float k_HalfPi = std::numbers::pi_v<float> / 2.0f;
/// A launch slower than this (squared) doesn't count as moving. The game keeps it as a double.
constexpr double k_StillLaunchSquared = 0.01;
/// A slide slower than this (squared) has no direction of its own, and is made one this long along x
constexpr float k_StillSlideSquared = 1e-4f;
constexpr float k_StillSlide = 1e-4f;
/// A lob whose target is closer than this (squared) across the ground is nudged away
constexpr float k_TooCloseSquared = 0.1f;
constexpr float k_TooCloseNudge = 0.1f;
/// The squared flight time a lob re-solved for its steepest angle takes when the steepest angle cannot reach the aim
constexpr float k_ShortestFlightSquared = 0.1f;

glm::vec3 NormaliseOrZero(glm::vec3 v)
{
	const float length = glm::length(v);
	return length > 0.0f ? v / length : glm::vec3(0.0f);
}
} // namespace

glm::vec3 maths::LiftDirection(glm::vec3 direction, float lift)
{
	// The heading stays wide; the climb is rounded when the lift is added
	const double yaw = x87::Atan2(direction.z, direction.x);
	float pitch = x87::Add(x87::Atan2(direction.y, std::sqrt((direction.x * direction.x) + (direction.z * direction.z))), lift);
	if (!(pitch > -k_HalfPi))
	{
		pitch = -k_HalfPi;
	}
	else if (!(pitch < k_HalfPi))
	{
		pitch = k_HalfPi;
	}
	const float cosPitch = x87::RoundToFloat(x87::Cos(pitch));
	return {x87::Mul(x87::Cos(yaw), cosPitch), x87::RoundToFloat(x87::Sin(pitch)), x87::Mul(x87::Sin(yaw), cosPitch)};
}

float maths::ThrowSpeedFromHand(float handSpeed, const ThrowSpeeds& speeds)
{
	const auto& in = speeds.hand;
	const auto& out = speeds.thrown;
	if (handSpeed >= in[2])
	{
		return out[2];
	}
	const size_t i = handSpeed < in[1] ? 0 : 1;
	const float span = in[i + 1] - in[i];
	const float share = span > 0.0f ? (handSpeed - in[i]) / span : 1.0f;
	return out[i] + share * (out[i + 1] - out[i]);
}

maths::Launch maths::HandThrow(glm::vec3 handVelocity, float lift, const ThrowSpeeds& speeds)
{
	// The game adds the squares from z to x
	const float handSpeed =
	    std::sqrt(((handVelocity.z * handVelocity.z) + (handVelocity.y * handVelocity.y)) + (handVelocity.x * handVelocity.x));
	if (handSpeed <= 0.0f)
	{
		return {};
	}
	// The game takes the heading and climb of the hand's movement itself, not of its unit direction
	return {.direction = LiftDirection(handVelocity, lift), .speed = ThrowSpeedFromHand(handSpeed, speeds)};
}

maths::Launch maths::Lob(glm::vec3 from, glm::vec3 target, float gravity)
{
	const glm::vec2 across(target.x - from.x, target.z - from.z);
	if (glm::dot(across, across) < k_TooCloseSquared)
	{
		target = glm::vec3(from.x + k_TooCloseNudge, from.y, from.z + k_TooCloseNudge);
	}
	const glm::vec3 aim = from + (target - from) * k_LobAimShare;
	const glm::vec3 delta = aim - from;
	const float distance = std::sqrt(delta.x * delta.x + delta.z * delta.z);
	float seconds = distance * k_LobSecondsPerMetre;
	if (!(seconds >= k_LobMinimumSeconds))
	{
		seconds = k_LobMinimumSeconds;
	}
	// Starting at v, gravity brings it down by half g t squared: it reaches the aim after t. The game multiplies by one
	// over the time.
	const float fall = -gravity;
	const auto velocityFor = [&](float t) {
		const float share = 1.0f / t;
		return delta * share - glm::vec3(0.0f, fall * (t * 0.5f), 0.0f);
	};
	glm::vec3 v = velocityFor(seconds);
	const float horizontalSquared = v.z * v.z + v.x * v.x;
	if (x87::Greater(v.y * v.y + horizontalSquared, k_StillLaunchSquared))
	{
		const double steepest = x87::Tan(k_LobSteepest);
		if (x87::Greater(v.y / std::sqrt(horizontalSquared), steepest))
		{
			// The flight that leaves at the steepest angle instead
			float flightSquared = (delta.y - x87::Mul(distance, steepest)) * (-2.0f / gravity);
			if (!(flightSquared >= 0.0f))
			{
				flightSquared = k_ShortestFlightSquared;
			}
			seconds = std::sqrt(flightSquared);
			v = velocityFor(seconds);
		}
	}
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
	{
		return {.direction = v, .speed = 0.0f};
	}
	const float speed = std::sqrt(x87::Dot<x87::SumOrder::Zxy>(v, v));
	const float share = 1.0f / speed;
	return {.direction = v * share, .speed = speed};
}

glm::vec3 maths::BounceOffSlope(glm::vec3 velocity, glm::vec3 normal, float groundDrag, float dt, float horizontalBounce,
                                float verticalBounce)
{
	const float into = x87::Dot<x87::SumOrder::Zyx>(velocity, normal);
	if (!(into < 0.0f))
	{
		return velocity;
	}
	const glm::vec3 normalPart = normal * into;
	glm::vec3 slide = velocity - normalPart;
	// A slide too small to have a way is made a tiny one along x
	glm::vec3 slideDirection = slide;
	if (x87::Dot<x87::SumOrder::Zyx>(slide, slide) < k_StillSlideSquared)
	{
		slideDirection = glm::vec3(k_StillSlide, 0.0f, 0.0f);
	}
	float slideSpeed = 0.0f;
	if (slideDirection.x != 0.0f || slideDirection.y != 0.0f || slideDirection.z != 0.0f)
	{
		slideSpeed = std::sqrt(x87::Dot<x87::SumOrder::Zyx>(slideDirection, slideDirection));
		slideDirection *= 1.0f / slideSpeed;
	}
	float drag = dt * groundDrag;
	if (!(drag > 0.0f))
	{
		drag = 0.0f;
	}
	else if (!(drag < slideSpeed))
	{
		drag = slideSpeed;
	}
	slide -= slideDirection * drag;
	return slide * horizontalBounce - normalPart * verticalBounce;
}

bool maths::InStrikeCone(glm::vec3 origin, float cosHeading, float sinHeading, float cosHalfAngle, glm::vec3 point)
{
	float x = point.x - origin.x;
	float z = point.z - origin.z;
	// The way across the land becomes unit length by the inverse of its length. A way of no length stays as it is.
	if (x != 0.0f || z != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt((x * x) + (z * z));
		x *= inverse;
		z *= inverse;
	}
	return (x * cosHeading) + (sinHeading * z) > cosHalfAngle;
}

int maths::StrikesAtOnce(int limit, int targets)
{
	return std::max(0, std::min(limit, (targets + 1) / 2));
}

maths::ForkSplit maths::SplitTargets(std::span<const glm::vec3> tips, glm::vec3 origin, glm::vec3 centroid, glm::vec3 split)
{
	ForkSplit result;
	const glm::vec2 way(centroid.x - origin.x, centroid.z - origin.z);
	for (size_t i = 0; i < tips.size(); ++i)
	{
		const float along = (tips[i].x - split.x) * way.x + (tips[i].z - split.z) * way.y;
		(along > 0.0f ? result.ahead : result.behind).push_back(i);
	}
	if (tips.size() >= 2)
	{
		if (result.ahead.empty())
		{
			result.ahead.push_back(result.behind.back());
			result.behind.pop_back();
		}
		else if (result.behind.empty())
		{
			result.behind.push_back(result.ahead.back());
			result.ahead.pop_back();
		}
	}
	return result;
}

float maths::ForkJointScale(float scale, int depth, int joint, float step)
{
	const float from = scale / (static_cast<float>(depth) + 1.0f);
	const float to = scale / (static_cast<float>(depth + 1) + 1.0f);
	return (to - from) * static_cast<float>(joint) * step + from;
}

bool maths::InsideSphere(glm::vec3 point, glm::vec3 centre, float radius, float margin)
{
	const glm::vec3 d = point - centre;
	const float r = margin + radius;
	return r * r > x87::Dot<x87::SumOrder::Zyx>(d, d);
}

glm::vec3 maths::SphereEntry(glm::vec3 from, glm::vec3 to, glm::vec3 centre, float radius, float margin)
{
	// Make the way one long, unless it has no length
	glm::vec3 direction = to - from;
	float length = 0.0f;
	if (direction.x != 0.0f || direction.y != 0.0f || direction.z != 0.0f)
	{
		length = std::sqrt(x87::Dot<x87::SumOrder::Zxy>(direction, direction));
		const float share = 1.0f / length;
		direction *= share;
	}
	const glm::vec3 w = from - centre;
	// The quadratic of the line and the sphere. The game multiplies by the squared length of the direction where the
	// formula divides by it. The direction is one long, so the two are nearly the same.
	const float a = x87::Dot<x87::SumOrder::Zxy>(direction, direction);
	const float b = x87::Dot<x87::SumOrder::Zxy>(w, direction) * 2.0f;
	const float r = margin + radius;
	const float c = x87::Dot<x87::SumOrder::Zyx>(w, w) - r * r;
	const float disc = b * b - c * a * 4.0f;
	if (!(disc >= 0.0f))
	{
		return to;
	}
	const float root = std::sqrt(disc);
	const float near = (-b - root) * a * 0.5f;
	const float far = (root - b) * a * 0.5f;
	if (!(far >= 0.0f) || !(length >= near))
	{
		return to;
	}
	if (near < 0.0f)
	{
		// The start is inside the sphere
		return from;
	}
	if (near > 0.0f && far != length)
	{
		return direction * near + from;
	}
	return to;
}

glm::vec3 maths::DeflectOffSphere(glm::vec3 point, glm::vec3 centre, glm::vec3 velocity)
{
	glm::vec3 n = point - centre;
	if (n.x != 0.0f || n.y != 0.0f || n.z != 0.0f)
	{
		const float share = 1.0f / std::sqrt(x87::Dot<x87::SumOrder::Zyx>(n, n));
		n *= share;
	}
	const glm::vec3 twice = n + n;
	const float along = x87::Dot<x87::SumOrder::Xzy>(n, velocity);
	return velocity - twice * along;
}

bool openblack::particles::maths::RushedPastCamera(glm::vec3 before, glm::vec3 now, glm::vec3 velocity, glm::vec3 camera)
{
	constexpr float k_PassingDistance = 40.0f;
	constexpr float k_PassingSpeed = 20.0f;
	const auto nowAway = now - camera;
	const auto beforeAway = before - camera;
	constexpr float k_Reach = k_PassingDistance * k_PassingDistance;
	return x87::DotZyx(nowAway, nowAway) < k_Reach && x87::DotZyx(beforeAway, beforeAway) > k_Reach &&
	       x87::DotZyx(velocity, velocity) > k_PassingSpeed * k_PassingSpeed;
}
