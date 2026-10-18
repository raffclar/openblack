/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlanetNavigation.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/mat3x3.hpp>

#include "Graphics/Atmosphere.h"

using namespace openblack;

namespace
{
// The curved sea drag measures again this many times after the first move
constexpr int k_DragRefines = 3;

glm::dvec3 FromCentre(const planet_nav::Ball& ball, glm::vec3 point)
{
	return glm::dvec3(point) - glm::dvec3(ball.centre);
}

// A unit direction square to `direction`
glm::vec3 AnySquareTo(glm::vec3 direction)
{
	const glm::vec3 guide = std::abs(direction.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
	return glm::normalize(glm::cross(glm::cross(direction, guide), direction));
}

// The way that a camera faces, as a turn from the camera's own axes: x to the right, y up and z back
glm::quat Facing(glm::vec3 forward, glm::vec3 up)
{
	const glm::vec3 right = glm::normalize(glm::cross(forward, up));
	const glm::vec3 square = glm::cross(right, forward);
	return glm::normalize(glm::quat_cast(glm::mat3(right, square, -forward)));
}
} // namespace

planet_nav::Ball planet_nav::WorldBall(float seaLevel, glm::vec2 mapMiddle, float radius)
{
	return {.centre = {mapMiddle.x, seaLevel - radius, mapMiddle.y}, .radius = radius};
}

float planet_nav::HeightOver(const Ball& ball, glm::vec3 point)
{
	const auto offset = FromCentre(ball, point);
	const double distance = glm::length(offset);
	const auto radius = static_cast<double>(ball.radius);
	// distance - radius, with no loss of the small terms
	return static_cast<float>((glm::dot(offset, offset) - radius * radius) / (distance + radius));
}

float planet_nav::OrbitShare(float height)
{
	const float t = std::clamp((height - k_OrbitFrom) / (k_OrbitFull - k_OrbitFrom), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

std::optional<float> planet_nav::BallDistance(const Ball& ball, glm::vec3 eye, glm::vec3 direction)
{
	const auto offset = FromCentre(ball, eye);
	const auto ray = glm::normalize(glm::dvec3(direction));
	const auto radius = static_cast<double>(ball.radius);
	const double distance = glm::length(offset);
	const double b = glm::dot(offset, ray);
	// The square of the distance to the middle less the square of the radius
	const double c = (distance - radius) * (distance + radius);
	if (c < 0.0)
	{
		return std::nullopt;
	}
	const double disc = b * b - c;
	if (disc < 0.0 || b >= 0.0)
	{
		return std::nullopt;
	}
	// The near root of t^2 + 2 b t + c, kept exact for a small c
	const double near = c / (-b + std::sqrt(disc));
	return static_cast<float>(near);
}

glm::vec3 planet_nav::GripOnBall(const Ball& ball, glm::vec3 eye, glm::vec3 direction)
{
	const auto offset = FromCentre(ball, eye);
	const auto ray = glm::normalize(glm::dvec3(direction));
	if (const auto along = BallDistance(ball, eye, direction))
	{
		return glm::vec3(glm::normalize(offset + ray * static_cast<double>(*along)));
	}
	const double along = std::max(-glm::dot(offset, ray), 0.0);
	return glm::vec3(glm::normalize(offset + ray * along));
}

glm::quat planet_nav::GreatCircleTurn(glm::vec3 from, glm::vec3 to)
{
	return glm::normalize(glm::rotation(glm::normalize(from), glm::normalize(to)));
}

planet_nav::Pose planet_nav::TurnAbout(const Ball& ball, const glm::quat& turn, const Pose& pose)
{
	const glm::dquat wide(turn);
	const glm::dvec3 centre(ball.centre);
	return {
	    .eye = glm::vec3(centre + wide * FromCentre(ball, pose.eye)),
	    .focus = glm::vec3(centre + wide * FromCentre(ball, pose.focus)),
	    .up = glm::normalize(turn * pose.up),
	};
}

glm::quat planet_nav::GlobeDrag(const Ball& ball, glm::vec3 eyeAtGrip, glm::vec3 gripped, glm::vec3 rayNow)
{
	return GreatCircleTurn(GripOnBall(ball, eyeAtGrip, rayNow), gripped);
}

glm::quat planet_nav::SpinAbout(const Ball& ball, glm::vec3 eye, float angle)
{
	return glm::angleAxis(angle, glm::vec3(glm::normalize(FromCentre(ball, eye))));
}

glm::vec3 planet_nav::ScreenUp(glm::vec3 eye, glm::vec3 focus, glm::vec3 guide)
{
	const glm::vec3 forward = glm::vec3(glm::normalize(glm::dvec3(focus) - glm::dvec3(eye)));
	const glm::vec3 side = glm::cross(forward, guide);
	if (glm::dot(side, side) < 1e-12f)
	{
		return AnySquareTo(forward);
	}
	return glm::cross(glm::normalize(side), forward);
}

planet_nav::Pose planet_nav::OrbitPose(const Ball& ball, const Pose& ground)
{
	const glm::vec3 forward = glm::vec3(glm::normalize(-FromCentre(ball, ground.eye)));
	const glm::vec3 top = ScreenUp(ground.eye, ground.focus, ground.up);
	const glm::vec3 square = top - forward * glm::dot(top, forward);
	if (glm::dot(square, square) < 1e-10f)
	{
		// The camera looked straight at the middle: its forward is the nearest top
		const glm::vec3 ahead = glm::vec3(glm::normalize(glm::dvec3(ground.focus) - glm::dvec3(ground.eye)));
		return {.eye = ground.eye, .focus = ball.centre, .up = glm::dot(ahead, forward) > 0.999f ? top : ahead};
	}
	return {.eye = ground.eye, .focus = ball.centre, .up = glm::normalize(square)};
}

planet_nav::Pose planet_nav::Blend(const Pose& ground, const Pose& orbit, float share)
{
	if (share <= 0.0f)
	{
		return ground;
	}
	if (share >= 1.0f)
	{
		return orbit;
	}
	const auto lineOf = [](const Pose& pose) { return glm::dvec3(pose.focus) - glm::dvec3(pose.eye); };
	const auto groundLine = lineOf(ground);
	const auto orbitLine = lineOf(orbit);
	const glm::vec3 groundForward = glm::vec3(glm::normalize(groundLine));
	const glm::vec3 orbitForward = glm::vec3(glm::normalize(orbitLine));
	const auto from = Facing(groundForward, ScreenUp(ground.eye, ground.focus, ground.up));
	auto to = Facing(orbitForward, ScreenUp(orbit.eye, orbit.focus, orbit.up));
	const auto facing = glm::normalize(glm::slerp(from, to, share));
	const double distance = glm::mix(glm::length(groundLine), glm::length(orbitLine), static_cast<double>(share));
	const glm::vec3 forward = facing * glm::vec3(0.0f, 0.0f, -1.0f);
	return {
	    .eye = ground.eye,
	    .focus = glm::vec3(glm::dvec3(ground.eye) + glm::dvec3(forward) * distance),
	    .up = facing * glm::vec3(0.0f, 1.0f, 0.0f),
	};
}

float planet_nav::ArcFromMap(const Ball& ball, glm::vec3 eye)
{
	const auto up = glm::normalize(FromCentre(ball, eye));
	return static_cast<float>(std::acos(std::clamp(up.y, -1.0, 1.0)) * static_cast<double>(ball.radius));
}

planet_nav::Settled planet_nav::Settle(const Ball& ball, const glm::quat& turn, const Pose& local)
{
	constexpr glm::vec3 k_Up {0.0f, 1.0f, 0.0f};
	const Pose world = TurnAbout(ball, turn, local);
	if (ArcFromMap(ball, world.eye) <= k_FarSeaLimit)
	{
		return {.local = {.eye = world.eye, .focus = world.focus, .up = k_Up}};
	}
	// The direction across the ground from the map's middle towards the point. Exactly opposite the island any direction
	// will do: the local camera's own direction then.
	const auto under = glm::normalize(FromCentre(ball, world.eye));
	glm::dvec2 bearing {under.x, under.z};
	if (glm::length(bearing) < 1e-9)
	{
		bearing = glm::dvec2(FromCentre(ball, local.eye).x, FromCentre(ball, local.eye).z);
	}
	bearing = glm::length(bearing) > 1e-12 ? glm::normalize(bearing) : glm::dvec2(1.0, 0.0);
	const glm::dvec2 offset = bearing * static_cast<double>(k_AnchorSpotDistance);
	const auto spotDirection = glm::normalize(glm::dvec3(offset.x, ball.radius, offset.y));
	const auto localDirection = glm::normalize(FromCentre(ball, local.eye));
	const glm::quat move = GreatCircleTurn(glm::vec3(localDirection), glm::vec3(spotDirection));
	Pose moved = TurnAbout(ball, move, local);
	moved.up = k_Up;
	return {
	    .local = moved,
	    .turn = glm::normalize(turn * glm::conjugate(move)),
	    .spot = glm::vec2(glm::dvec2(ball.centre.x, ball.centre.z) + offset),
	};
}

std::optional<glm::vec2> planet_nav::CurvedSeaDrag(const SeaDistance& sea, glm::vec3 eyeAtGrip, glm::vec3 gripped,
                                                   glm::vec3 ray)
{
	const glm::vec3 direction = glm::normalize(ray);
	const glm::vec2 toGrip {gripped.x - eyeAtGrip.x, gripped.z - eyeAtGrip.z};
	glm::vec2 move {0.0f};
	for (int pass = 0; pass <= k_DragRefines; ++pass)
	{
		const auto along = sea(eyeAtGrip + glm::vec3(move.x, 0.0f, move.y), direction);
		if (!along.has_value())
		{
			if (pass == 0)
			{
				return std::nullopt;
			}
			break;
		}
		// Where the ray meets the sea, across the ground from the eye at the grip
		const glm::vec2 hit = move + glm::vec2(direction.x, direction.z) * *along;
		move += toGrip - hit;
	}
	return move;
}

std::optional<float> planet_nav::DrawnSeaDistance(glm::vec3 eye, glm::vec3 direction, float seaLevel, float mapSize,
                                                  bool islandFar)
{
	namespace atmosphere = graphics::atmosphere;
	const float height = eye.y - seaLevel;
	const auto cap = atmosphere::MapCap({eye.x, eye.z}, height, mapSize, islandFar);
	return atmosphere::CurvedSeaDistance(cap, height, glm::normalize(direction));
}

std::optional<planet_nav::Line> planet_nav::LineNearMap(glm::vec3 from, glm::vec3 to, glm::vec3 mapMiddle, float reach)
{
	const glm::dvec3 offset = glm::dvec3(from) - glm::dvec3(mapMiddle);
	const auto radius = static_cast<double>(reach);
	// A line with a part that is not a number has no place to start
	if (!std::isfinite(glm::dot(offset, offset)) || !std::isfinite(glm::dot(glm::dvec3(to), glm::dvec3(to))))
	{
		return std::nullopt;
	}
	if (glm::dot(offset, offset) <= radius * radius)
	{
		return Line {.from = from, .to = to};
	}
	const glm::dvec3 line = glm::dvec3(to) - glm::dvec3(from);
	const double length = glm::length(line);
	if (length <= 0.0)
	{
		return std::nullopt;
	}
	const glm::dvec3 ray = line / length;
	const double b = glm::dot(offset, ray);
	const double disc = b * b - (glm::dot(offset, offset) - radius * radius);
	if (disc < 0.0 || b >= 0.0)
	{
		return std::nullopt;
	}
	const glm::dvec3 entry = glm::dvec3(mapMiddle) + offset + ray * (-b - std::sqrt(disc));
	return Line {.from = glm::vec3(entry), .to = glm::vec3(entry + ray * std::max(length, 1.0))};
}

float planet_nav::OrbitZoomHeight(float height, float zoom)
{
	constexpr float k_WheelNotch = 60.0f;
	return height * std::pow(k_OrbitZoomPerNotch, zoom / k_WheelNotch);
}

std::optional<glm::vec3> planet_nav::KeepUnderOrbitTop(const Ball& ball, glm::vec3 eye)
{
	if (!(HeightOver(ball, eye) > k_OrbitHighest))
	{
		return std::nullopt;
	}
	const auto offset = FromCentre(ball, eye);
	const double top = static_cast<double>(ball.radius) + k_OrbitHighest;
	return glm::vec3(glm::dvec3(ball.centre) + offset * (top / glm::length(offset)));
}

std::optional<glm::vec2> planet_nav::KeepInsideDisc(glm::vec2 point, glm::vec2 middle, float radius)
{
	const glm::vec2 across = point - middle;
	const float distance = glm::length(across);
	if (!(distance > radius))
	{
		return std::nullopt;
	}
	return middle + across * (radius / distance);
}
