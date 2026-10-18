/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureWalkPlanner.h"

#include <cmath>

#include "3D/MapCoords.h"
#include "Common/X87.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureAnimation.h"

using namespace openblack;
using namespace openblack::creature_walk_planner;

namespace
{
constexpr float k_HalfPi = std::bit_cast<float>(0x3FC90FDBu);
/// The share of a quarter turn that one radian is, which the step blend multiplies by
constexpr float k_StepShare = std::bit_cast<float>(0x3F22F983u);
/// The slope ahead slows the creature by this much for each unit of rise over its radius, within these limits
constexpr float k_SlopeSlowing = std::bit_cast<float>(0x3F19999Au);
constexpr float k_LeastSlopeFactor = std::bit_cast<float>(0x3E99999Au);
constexpr float k_MostSlopeFactor = std::bit_cast<float>(0x3F8CCCCDu);
constexpr float k_MsPerSecond = 1000.0f;

/// The length of a move with the squares added from z to x, as the step chooser measures each step
float LengthZyx(const glm::vec3& v)
{
	return std::sqrt(((v.z * v.z) + (v.y * v.y)) + (v.x * v.x));
}

/// The cap that lets the creature slow down to a speed in the distance left: the square root of the squared speed
/// and twice the slowing over the distance, never below k_LeastCapSpeed. A NaN stays.
float SlowingCap(float squaredSpeed, float left)
{
	const float slowing = left * k_Acceleration;
	const float cap = std::sqrt(squaredSpeed + (slowing + slowing));
	return !(k_LeastCapSpeed > cap) ? cap : k_LeastCapSpeed;
}
} // namespace

glm::vec2 creature_walk_planner::LookAhead(glm::vec2 position, GameHeading routeHeading, float legLength)
{
	const float distance = legLength < k_LeastLookAhead ? k_LeastLookAhead : legLength;
	const double h = routeHeading.Radians();
	return {x87::Add(x87::Mul(x87::Sin(h), distance), position.x), x87::Add(-x87::Mul(x87::Cos(h), distance), position.y)};
}

Start creature_walk_planner::ChooseStart(glm::vec2 from, glm::vec2 to, GameHeading heading, float scale, float walkSpeed,
                                         const StepSet& rightSteps, const StepSet& leftSteps)
{
	const float dx = to.x - from.x;
	const float dz = to.y - from.y;
	// The way that the creature faces, each part rounded to a float
	const double h = heading.Radians();
	const float facingX = x87::RoundToFloat(x87::Sin(h));
	const float facingZ = -x87::RoundToFloat(x87::Cos(h));
	Start start {.phase = Phase::Turn, .heading = GameHeading::Of(dx, dz), .side = creature_locomotion::Side::Right};

	// The point is on the left of the creature when the cross product of the facing and the way is below zero
	const float cross = (facingZ * dx) - (dz * facingX);
	start.side = cross < 0.0f ? creature_locomotion::Side::Left : creature_locomotion::Side::Right;
	const auto& steps = start.side == creature_locomotion::Side::Left ? leftSteps : rightSteps;
	if (!steps.has_value())
	{
		return start;
	}
	const std::array strides {LengthZyx((*steps)[0].displacement), LengthZyx((*steps)[1].displacement),
	                          LengthZyx((*steps)[2].displacement)};
	const float distance = std::sqrt((dz * dz) + (dx * dx)) / scale;
	const float turn = start.heading.TurnFrom(heading);
	if (distance > strides[0] && distance > strides[1] && distance > strides[2])
	{
		const float size = turn < 0.0f ? -turn : turn;
		const auto first = start.side == creature_locomotion::Side::Left ? creature_locomotion::animations::k_LeftStep
		                                                                 : creature_locomotion::animations::k_RightStep;
		const auto pair =
		    size < k_HalfPi
		        ? creature_locomotion::Pair {.from = first, .to = first + 1, .weight = size * k_StepShare}
		        : creature_locomotion::Pair {.from = first + 1, .to = first + 2, .weight = (size - k_HalfPi) * k_StepShare};
		const auto& fromClip = (*steps)[pair.from - first];
		const auto& toClip = (*steps)[pair.to - first];
		start.phase = Phase::Step;
		start.steps = pair;
		start.stepDisplacement = creature_animation::BlendMoves(fromClip.displacement, toClip.displacement, pair.weight);

		// The step lasts as long as the part of its move along the new heading takes at walking speed. The move is
		// taken a frame at a time and the frames are counted back in.
		const auto frames = static_cast<float>(fromClip.frames - 1);
		const float perFrame = 1.0f / frames;
		const float sine = x87::RoundToFloat(x87::Sin(turn));
		const double minusCosine = -x87::Cos(turn);
		const float x = perFrame * start.stepDisplacement.x;
		const float z = perFrame * start.stepDisplacement.z;
		const float along = x87::Add(x87::Mul(minusCosine, z), x * sine);
		start.stepMs = map_coords::FtoL((((along * scale) / walkSpeed) * frames) * k_MsPerSecond);
		return start;
	}
	if (std::abs(turn) < k_StraightTurn)
	{
		start.phase = Phase::Walk;
	}
	return start;
}

float creature_walk_planner::SpeedCap(float topSpeed, float legLength, float travelled, const NextLeg& next)
{
	switch (next.kind)
	{
	case NextLeg::Kind::None:
		return SlowingCap(0.0f, legLength - travelled);
	case NextLeg::Kind::Arc:
	{
		// The arc allows the speed whose square is the slowing times its radius
		const float arcSpeed = std::sqrt(next.radius * k_Acceleration);
		return SlowingCap(arcSpeed * arcSpeed, legLength - travelled);
	}
	case NextLeg::Kind::Straight:
		break;
	}
	return topSpeed;
}

glm::vec2 creature_walk_planner::SlopePoint(glm::vec2 position, GameHeading heading, float radius)
{
	const double h = heading.Radians();
	return {x87::Add(x87::Mul(x87::Sin(h), radius), position.x), x87::Add(-x87::Mul(x87::Cos(h), radius), position.y)};
}

float creature_walk_planner::SlopeFactor(float altitudeAhead, float altitude, float radius)
{
	const float factor = 1.0f - (((altitudeAhead - altitude) / radius) * k_SlopeSlowing);
	if (!(factor > k_LeastSlopeFactor))
	{
		return k_LeastSlopeFactor;
	}
	return factor < k_MostSlopeFactor ? factor : k_MostSlopeFactor;
}

float creature_walk_planner::NextSpeed(float speed, float target, float cap)
{
	if (speed < target)
	{
		const float faster = (k_TurnSeconds * k_Acceleration) + speed;
		speed = target < faster ? target : faster;
	}
	return speed > cap ? cap : speed;
}

float creature_walk_planner::TurnDistance(float speed)
{
	return k_TurnSeconds * speed;
}

bool creature_walk_planner::WalkTurnsTooFar(GameHeading heading, GameHeading routeHeading)
{
	return !(std::abs(heading.TurnFrom(routeHeading)) < k_CornerTurn);
}

bool creature_walk_planner::StepTurnsTooFar(GameHeading stepHeading, GameHeading routeHeading)
{
	return std::abs(stepHeading.TurnFrom(routeHeading)) > k_CornerTurn;
}

GameHeading creature_walk_planner::StepHeading(GameHeading heading, GameHeading lastRouteHeading, GameHeading routeHeading)
{
	return GameHeading(GameHeading::Wrap(routeHeading.TurnFrom(lastRouteHeading) + heading.Radians()));
}

float creature_walk_planner::StepSpeed(const glm::vec3& displacement, int32_t turnMs, float scale, int32_t stepMs,
                                       GameHeading heading)
{
	const double h = heading.Radians();
	// The cosine is kept as a float, the sine wide
	const float cosine = x87::RoundToFloat(x87::Cos(h));
	const double sine = x87::Sin(h);
	const float share = (static_cast<float>(turnMs) * scale) / static_cast<float>(stepMs);
	const float x = share * displacement.x;
	const float z = share * displacement.z;
	// The move turned to the creature's heading, on the ground
	const float turnedX = x87::Add(x87::Mul(cosine, x), x87::Mul(-sine, z));
	const float turnedZ = x87::Add(x87::Mul(sine, x), x87::Mul(cosine, z));
	return std::sqrt((turnedX * turnedX) + (turnedZ * turnedZ)) * k_TurnsPerSecond;
}

float creature_walk_planner::StepJoinAhead(const glm::vec3& displacement, float scale, float travelled)
{
	return (LengthZyx(displacement) * scale) - travelled;
}

int32_t creature_walk_planner::WalkTimeAfterStep(int32_t stepTimeMs, int32_t stepMs, int32_t walkMs,
                                                 creature_locomotion::Side side)
{
	const float share = static_cast<float>(stepTimeMs) / static_cast<float>(stepMs);
	if (side == creature_locomotion::Side::Right)
	{
		return map_coords::FtoL(static_cast<float>(walkMs) * share);
	}
	return map_coords::FtoL((share - 0.5f) * static_cast<float>(walkMs));
}

int32_t creature_walk_planner::AdvanceLoop(int32_t timeMs, int32_t advanceMs, int32_t durationMs)
{
	if (advanceMs == 0)
	{
		return timeMs;
	}
	auto time = timeMs + advanceMs;
	if (durationMs <= time)
	{
		time -= durationMs;
	}
	return durationMs <= time ? durationMs - 1 : time;
}

int32_t creature_walk_planner::StandTime(int32_t standMs, float breathPhase)
{
	const auto time = map_coords::FtoL(static_cast<float>(standMs) * breathPhase);
	return time < standMs ? time : standMs - 1;
}

std::optional<Gait> creature_walk_planner::BlendGait(float speed, float walkSpeed, float runSpeed, float distance, float scale,
                                                     const Clip& stand, const Clip& walk, const Clip& run, int32_t walkTimeMs,
                                                     float breathPhase)
{
	if (!(speed > k_StillSpeed))
	{
		return std::nullopt;
	}
	using creature_locomotion::animations::k_Run;
	using creature_locomotion::animations::k_Stand;
	using creature_locomotion::animations::k_Walk;
	if (speed < walkSpeed)
	{
		const float walkWeight = speed / walkSpeed;
		const float stride = (walk.stride * scale) * walkWeight;
		const float cycles = distance / stride;
		const auto standTime = map_coords::FtoL(static_cast<float>(stand.durationMs) * breathPhase);
		const auto advance = map_coords::FtoL(static_cast<float>(walk.durationMs) * cycles);
		return Gait {.animations = {k_Stand, k_Walk},
		             .weights = {1.0f - walkWeight, walkWeight},
		             .timesMs = {standTime, AdvanceLoop(walkTimeMs, advance, walk.durationMs)}};
	}
	const float runWeight = (speed - walkSpeed) / (runSpeed - walkSpeed);
	const float walkWeight = 1.0f - runWeight;
	const float stride = ((walkWeight * walk.stride) + (runWeight * run.stride)) * scale;
	const float cycles = distance / stride;
	const auto advance = map_coords::FtoL(static_cast<float>(walk.durationMs) * cycles);
	const auto walkTime = AdvanceLoop(walkTimeMs, advance, walk.durationMs);
	return Gait {.animations = {k_Run, k_Walk},
	             .weights = {runWeight, walkWeight},
	             .timesMs = {(run.durationMs * walkTime) / walk.durationMs, walkTime}};
}
