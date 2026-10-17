/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureEyes.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>

#include <glm/geometric.hpp>

#include "Common/ModelInverseSquareRoot.h"
#include "Common/X87.h"
#include "Common/X87Vector.h"

using namespace openblack;
using namespace openblack::creature_eyes;

namespace
{
/// An eye is a tenth of the creature's size, by its species' eye size
constexpr float k_EyeSizePerSize = 0.1f;
/// Sizes above this make the eyes and head no smaller for their size
constexpr float k_MaxSizeEffect = 2.0f;
/// How much bigger an evil (smaller for good) creature's eyes are
constexpr float k_EvilGoodEyeEffect = 0.05f;
/// The darting eyes of a stoned creature are a tenth bigger and take five times as long to turn
constexpr float k_StonedEyeSize = 1.1f;
constexpr float k_StonedLookSlowdown = 5.0f;
/// How long the eyes take to turn, by how open they are
constexpr float k_LookSeconds = 0.3f;
constexpr float k_LookOpennessOffset = 2.0f;
/// The least the back of the eye looks into the head along its normal
constexpr float k_MinInwardLook = 0.1f;
/// How much a calm lid opens further for how open the eyes are, in radians
constexpr float k_LidOpennessAngle = 0.3f;
/// The step of a stoned creature's darting look
constexpr float k_DartStep = 0.009f;
/// The share of a blink's half that each millisecond is
constexpr float k_BlinkShare = 0.005f;
/// The game reads the frame time in milliseconds
constexpr float k_SecondsPerMillisecond = 0.001f;

/// The vector made one long with the 3D engine's inverse square root, its squares added in the given order. A zero
/// vector stays zero.
template <x87::SumOrder Order>
glm::vec3 Unit(const glm::vec3& v)
{
	return v * gutils::ModelInverseSquareRoot(x87::Dot<Order>(v, v));
}

/// The arcsine as the game's library makes it: the arctangent of x over the square root of (1 + x)(1 - x), each step
/// rounded to a float, the arctangent kept wide. Past one, the result is not a number, as in the game.
double ArcSine(float x)
{
	const float across = std::sqrt((1.0f + x) * (1.0f - x));
	return x87::Atan2(x, across);
}
} // namespace

SurfacePoint creature_eyes::PointOnTriangle(const glm::vec3& first, const glm::vec3& second, const glm::vec3& third, float u,
                                            float v)
{
	const auto towardsSecond = second - first;
	const auto towardsThird = third - first;
	return {
	    .position = first + (towardsSecond * u) + (towardsThird * v),
	    .normal = glm::cross(towardsThird, towardsSecond),
	};
}

float creature_eyes::EyeSize(float size, float speciesEyeScale, float evilGood, Mode mode)
{
	const auto limited = std::clamp(size, 0.0f, k_MaxSizeEffect);
	const auto eyes = (1.0f + (k_EvilGoodEyeEffect * evilGood)) * (1.1f - (0.1f * limited));
	const auto head = 1.8f - (0.5f * limited);
	const auto eyeSize = head * eyes * speciesEyeScale * size * k_EyeSizePerSize;
	return mode == Mode::Stoned ? eyeSize * k_StonedEyeSize : eyeSize;
}

float creature_eyes::RestedOpenness(float exhaustion)
{
	return std::clamp(1.0f - (exhaustion + exhaustion), k_RestedOpennessLeast, k_RestedOpennessMost);
}

float creature_eyes::LookSeconds(float openness, Mode mode)
{
	const auto seconds = k_LookSeconds / (openness + k_LookOpennessOffset);
	return mode == Mode::Stoned ? seconds * k_StonedLookSlowdown : seconds;
}

glm::vec3 creature_eyes::UnitNormal(const glm::vec3& normal)
{
	return Unit<x87::SumOrder::Yzx>(normal);
}

glm::vec3 creature_eyes::AwayFrom(const glm::vec3& surface, const glm::vec3& target)
{
	return Unit<x87::SumOrder::Xyz>(surface - target);
}

glm::vec3 creature_eyes::Darted(const glm::vec3& away, const std::array<int32_t, 3>& offsets)
{
	return {
	    (static_cast<float>(offsets[0]) * k_DartStep) + away.x,
	    (static_cast<float>(offsets[1]) * k_DartStep) + away.y,
	    (static_cast<float>(offsets[2]) * k_DartStep) + away.z,
	};
}

glm::vec3 creature_eyes::ClampLook(const glm::vec3& away, const glm::vec3& inwardNormal)
{
	auto look = away;
	const auto along = x87::Dot<x87::SumOrder::Zyx>(inwardNormal, look);
	if (along < k_MinInwardLook)
	{
		look += (k_MinInwardLook - along) * inwardNormal;
	}
	return Unit<x87::SumOrder::Zyx>(look);
}

Frame creature_eyes::EyeballFrame(const glm::vec3& centre, const glm::vec3& away)
{
	const auto z = Unit<x87::SumOrder::Xyz>(away);
	const auto x = Unit<x87::SumOrder::Xyz>(glm::vec3(0.0f - z.z, 0.0f, z.x - 0.0f));
	return {.x = x, .y = glm::cross(z, x), .z = z, .origin = centre};
}

Frame creature_eyes::EyelidFrame(const glm::vec3& centre, const glm::vec3& inwardNormal, const glm::vec3& anchor, bool rightEye)
{
	const auto z = Unit<x87::SumOrder::Xyz>(-inwardNormal);
	const auto towards = rightEye ? centre - anchor : anchor - centre;
	const auto y = Unit<x87::SumOrder::Xyz>(glm::cross(towards, z));
	return {.x = glm::cross(y, z), .y = y, .z = z, .origin = centre};
}

float creature_eyes::LidPitch(const Frame& lid, const Frame& sizedEyeball, float size)
{
	return x87::Dot<x87::SumOrder::Zyx>(lid.y, sizedEyeball.z) / size;
}

float creature_eyes::EyelidAngle(Mode mode, const LidAngles& angles, float openness, float lidPitch, const Blink& blink)
{
	constexpr auto k_Pi = std::numbers::pi_v<float>;
	switch (mode)
	{
	case Mode::Wide:
	case Mode::Stoned:
		return angles.open * k_Pi;
	case Mode::Closed:
		return angles.closed * k_Pi;
	case Mode::Calm:
	case Mode::Ahead:
		break;
	}
	const auto calm = x87::Sub(angles.calm * k_Pi, ArcSine(lidPitch)) + (openness * k_LidOpennessAngle);
	switch (blink.state)
	{
	case Blink::State::Closing:
		return (static_cast<float>(k_BlinkMs - blink.timerMs) * k_BlinkShare * ((angles.closed * k_Pi) - calm)) + calm;
	case Blink::State::Opening:
		return (((angles.closed * k_Pi) - calm) * (static_cast<float>(blink.timerMs) * k_BlinkShare)) + calm;
	case Blink::State::Open:
		break;
	}
	return calm;
}

Frame creature_eyes::Sized(const Frame& frame, float size)
{
	return {.x = size * frame.x, .y = size * frame.y, .z = size * frame.z, .origin = frame.origin};
}

Frame creature_eyes::Turned(const Frame& frame, float angle)
{
	// The cosine and sine stay wide, and each product rounds one time
	const auto c = x87::Cos(angle);
	const auto s = x87::Sin(angle);
	const auto turn = [c, s](float along, float across) { return x87::Mul(c, along) - x87::Mul(s, across); };
	const auto back = [c, s](float along, float across) { return x87::Mul(c, along) + x87::Mul(s, across); };
	return {
	    .x = frame.x,
	    .y = {turn(frame.y.x, frame.z.x), turn(frame.y.y, frame.z.y), turn(frame.y.z, frame.z.z)},
	    .z = {back(frame.z.x, frame.y.x), back(frame.z.y, frame.y.y), back(frame.z.z, frame.y.z)},
	    .origin = frame.origin,
	};
}

glm::mat4 creature_eyes::ToMatrix(const Frame& frame)
{
	// The game's rows are the axes the mesh's x, y and z go along
	return {
	    glm::vec4(frame.x, 0.0f),
	    glm::vec4(frame.y, 0.0f),
	    glm::vec4(frame.z, 0.0f),
	    glm::vec4(frame.origin, 1.0f),
	};
}

glm::vec3 creature_eyes::LookDestination(const SurfacePoint& point, const Look& look)
{
	auto away = look.ahead;
	if (look.mode != Mode::Ahead && look.lookAt.has_value())
	{
		away = AwayFrom(point.position, *look.lookAt);
	}
	if (look.mode == Mode::Stoned)
	{
		away = Darted(away, look.darts);
	}
	return ClampLook(away, UnitNormal(point.normal));
}

PlacedEye creature_eyes::PlaceEye(const SurfacePoint& point, const std::optional<glm::vec3>& anchor, float depth, bool rightEye,
                                  const Look& look, const LidAngles& lids, float eyeSize, Zoomer3& turn, int32_t milliseconds)
{
	const auto normal = UnitNormal(point.normal);
	turn.SetDestination(LookDestination(point, look),
	                    LookSeconds(look.openness, look.mode == Mode::Stoned ? Mode::Stoned : Mode::Calm));
	turn.Update(static_cast<float>(milliseconds) * k_SecondsPerMillisecond);

	const auto centre = point.position - (depth * normal);
	const auto eyeball = Sized(EyeballFrame(centre, turn.GetValue()), eyeSize);
	PlacedEye placed {.eyeball = ToMatrix(eyeball), .eyelid = std::nullopt};
	if (!anchor)
	{
		return placed;
	}
	const auto lid = EyelidFrame(centre, normal, *anchor, rightEye);
	const auto angle = EyelidAngle(look.mode, lids, look.openness, LidPitch(lid, eyeball, eyeSize), look.blink);
	placed.eyelid = ToMatrix(Turned(Sized(lid, eyeSize), angle));
	return placed;
}
