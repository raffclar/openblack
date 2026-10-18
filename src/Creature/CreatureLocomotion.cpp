/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureLocomotion.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <bit>

#include <glm/geometric.hpp>

#include "Common/X87.h"
#include "Common/X87Vector.h"

using namespace openblack;
using namespace openblack::creature_locomotion;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
constexpr float k_HalfPi = k_Pi / 2.0f;
/// The share of a quarter turn that one radian is, which the spin blend multiplies by
constexpr float k_SpinShare = std::bit_cast<float>(0x3F22F983u);
constexpr uint32_t k_SignBit = 0x80000000u;
/// Walking speed is this many units a second for each unit of the size factor, running this many
constexpr float k_WalkPerSize = 8.0f;
constexpr float k_RunPerSize = 20.0f;
/// The size factor is size times this plus the offset
constexpr float k_SizeSlope = 0.875f;
constexpr float k_SizeOffset = 0.25f;
/// A creature going slower than this isn't moving its feet
constexpr float k_Still = 1e-4f;
/// The arrival ring never reaches past the destination by less than this, and is never thinner than this
constexpr float k_RingInside = 0.05f;
constexpr float k_RingThickness = 0.001f;
/// A walk is given this long and this much longer per unit of distance before it is given up on
constexpr float k_TimeLimitBaseMs = 5000.0f;
constexpr float k_TimeLimitPerUnitMs = 2.0f;

float Loop(float time, float duration)
{
	if (duration <= 0.0f)
	{
		return 0.0f;
	}
	const auto looped = std::fmod(time, duration);
	return looped < 0.0f ? looped + duration : looped;
}
} // namespace

Speeds creature_locomotion::SpeedsFor(float size)
{
	const auto factor = (k_SizeSlope * std::clamp(size, k_MinSize, k_MaxSize)) + k_SizeOffset;
	return {.walk = k_WalkPerSize * factor, .run = k_RunPerSize * factor};
}

float creature_locomotion::TargetSpeed(float fraction, float runSpeed)
{
	return std::clamp(fraction, 0.0f, 1.0f) * runSpeed * k_TopSpeedMargin;
}

float creature_locomotion::KeptFraction(float kept, float walkFraction)
{
	return kept > 0.0f ? kept : walkFraction;
}

float creature_locomotion::RequiredFraction(float kept, float slowFraction, float exhaustion, bool held)
{
	return !held && exhaustion >= k_ExhaustedAt ? slowFraction : kept;
}

float creature_locomotion::StopCap(float remaining)
{
	return std::max(std::sqrt(2.0f * k_Acceleration * std::max(remaining, 0.0f)), k_MinSpeed);
}

float creature_locomotion::CornerCap(float remaining, float cornerAllowance)
{
	return std::max(
	    std::sqrt((k_Acceleration * std::max(cornerAllowance, 0.0f)) + (2.0f * k_Acceleration * std::max(remaining, 0.0f))),
	    k_MinSpeed);
}

float creature_locomotion::SlopeFactor(float altitudeAhead, float altitude, float radius)
{
	if (radius <= 0.0f)
	{
		return 1.0f;
	}
	return std::clamp(1.0f - (k_SlopeSlowing * (altitudeAhead - altitude) / radius), k_MinSlopeFactor, k_MaxSlopeFactor);
}

float creature_locomotion::Accelerate(float speed, float target, float seconds)
{
	if (speed < target)
	{
		return std::min(target, speed + (k_Acceleration * seconds));
	}
	return speed;
}

GameHeading creature_locomotion::HeadingOf(glm::vec2 direction)
{
	return GameHeading::Of(direction.x, direction.y);
}

glm::vec2 creature_locomotion::DirectionOf(GameHeading heading)
{
	// The FPU's sine and cosine, each rounded to a float
	return {x87::RoundToFloat(x87::Sin(heading.Radians())), -x87::RoundToFloat(x87::Cos(heading.Radians()))};
}

GameHeading creature_locomotion::LerpHeading(GameHeading heading, GameHeading target, float timeMs, float durationMs)
{
	if (durationMs <= 0.0f || timeMs >= durationMs)
	{
		return target;
	}
	// The gap is wrapped once, scaled by the time and then divided by the length, and the result is wrapped once more
	const auto eased = ((target.TurnFrom(heading) * std::max(timeMs, 0.0f)) / durationMs) + heading.Radians();
	return GameHeading(GameHeading::Wrap(eased));
}

Gait creature_locomotion::BlendGait(float speed, const Speeds& speeds, float distance, float scale, const Cycle& stand,
                                    const Cycle& walk, const Cycle& run, float walkTimeMs, float breathPhase)
{
	const auto standTime = stand.durationMs * breathPhase;
	if (speed <= k_Still || speeds.walk <= 0.0f)
	{
		return {.slots = {{{.animation = animations::k_Stand, .timeMs = standTime, .weight = 1.0f},
		                   {.animation = animations::k_Walk, .timeMs = walkTimeMs, .weight = 0.0f}}},
		        .walkTimeMs = walkTimeMs,
		        .walkAdvanceMs = 0.0f};
	}

	if (speed < speeds.walk)
	{
		const auto walkWeight = speed / speeds.walk;
		// The blend's stride shrinks with the walk's weight, so the feet don't slide
		const auto stride = walkWeight * walk.stride * scale;
		const auto advance = stride > 0.0f ? walk.durationMs * distance / stride : 0.0f;
		const auto time = Loop(walkTimeMs + advance, walk.durationMs);
		return {.slots = {{{.animation = animations::k_Stand, .timeMs = standTime, .weight = 1.0f - walkWeight},
		                   {.animation = animations::k_Walk, .timeMs = time, .weight = walkWeight}}},
		        .walkTimeMs = time,
		        .walkAdvanceMs = advance};
	}

	const auto range = speeds.run - speeds.walk;
	const auto runWeight = range > 0.0f ? std::clamp((speed - speeds.walk) / range, 0.0f, 1.0f) : 1.0f;
	const auto walkWeight = 1.0f - runWeight;
	const auto stride = ((walkWeight * walk.stride) + (runWeight * run.stride)) * scale;
	const auto advance = stride > 0.0f ? walk.durationMs * distance / stride : 0.0f;
	const auto time = Loop(walkTimeMs + advance, walk.durationMs);
	// The run keeps in step with the walk
	const auto runTime = walk.durationMs > 0.0f ? run.durationMs * time / walk.durationMs : 0.0f;
	return {.slots = {{{.animation = animations::k_Run, .timeMs = runTime, .weight = runWeight},
	                   {.animation = animations::k_Walk, .timeMs = time, .weight = walkWeight}}},
	        .walkTimeMs = time,
	        .walkAdvanceMs = advance};
}

Side creature_locomotion::SideOf(float angle)
{
	return angle < 0.0f ? Side::Right : Side::Left;
}

Side creature_locomotion::SpinSideOf(float angle)
{
	return angle <= 0.0f ? Side::Right : Side::Left;
}

Pair creature_locomotion::PairFor(size_t first, float angle)
{
	const auto turn = std::min(std::abs(angle), k_Pi);
	if (turn < k_HalfPi)
	{
		return {.from = first, .to = first + 1, .weight = turn / k_HalfPi};
	}
	return {.from = first + 1, .to = first + 2, .weight = std::clamp((turn - k_HalfPi) / k_HalfPi, 0.0f, 1.0f)};
}

Pair creature_locomotion::SpinPairFor(size_t first, float angle)
{
	// A turn of 0 or less changes its sign, so that a turn of +0 becomes -0 as in the game. The sign bit is flipped
	// directly: an optimising compiler can make the plain negation an absolute value.
	const auto turn = angle <= 0.0f ? std::bit_cast<float>(std::bit_cast<uint32_t>(angle) ^ k_SignBit) : angle;
	if (turn < k_HalfPi)
	{
		return {.from = first, .to = first + 1, .weight = turn * k_SpinShare};
	}
	return {.from = first + 1, .to = first + 2, .weight = (turn - k_HalfPi) * k_SpinShare};
}

Start creature_locomotion::ChooseStart(float angle, float distanceInMesh, const StartOptions& options)
{
	const auto side = SideOf(angle);
	if (options.moving && options.stepStrides.has_value() &&
	    std::ranges::all_of(*options.stepStrides, [distanceInMesh](float stride) { return distanceInMesh > stride; }))
	{
		const auto first = side == Side::Right ? animations::k_RightStep : animations::k_LeftStep;
		return {.kind = Start::Kind::Step, .side = side, .animations = PairFor(first, angle)};
	}
	if (std::abs(angle) < k_WalkStraightAngle)
	{
		return {.kind = Start::Kind::Walk, .side = side, .animations = std::nullopt};
	}
	if (options.hasSpins)
	{
		const auto first = side == Side::Right ? animations::k_RightSpin : animations::k_LeftSpin;
		return {.kind = Start::Kind::Turn, .side = side, .animations = SpinPairFor(first, angle)};
	}
	return {.kind = Start::Kind::Turn, .side = side, .animations = std::nullopt};
}

Ring creature_locomotion::MoveRing(float distance, float minDistance, float maxDistance)
{
	const auto min = std::max(std::min(minDistance, distance - k_RingInside), 0.0f);
	return {.min = min, .max = std::max(maxDistance, min + k_RingThickness)};
}

Ring creature_locomotion::ObjectRing(float creatureRadius, float objectRadius, float extra)
{
	const auto min = creatureRadius + objectRadius;
	return {.min = min, .max = min + std::max(extra, k_RingThickness)};
}

bool creature_locomotion::Arrived(float distance, const Ring& ring)
{
	return distance <= ring.max;
}

float creature_locomotion::TimeLimitMs(float distance)
{
	return (k_TimeLimitPerUnitMs * distance) + k_TimeLimitBaseMs;
}

glm::vec3 creature_locomotion::RunAwayPoint(const map_coords::MapCoords& creature, glm::vec2 threat, uint32_t draw,
                                            float runAwayDistance, const RunAwayLand& land)
{
	constexpr float k_CellSize = 10.0f;
	constexpr float k_TooShort = 0.001f;
	const float distance = static_cast<float>(draw) + runAwayDistance;
	const auto cellX = map_coords::CellX(creature);
	const auto cellZ = map_coords::CellZ(creature);
	float awayX = (static_cast<float>(cellX) * k_CellSize) - threat.x;
	float awayZ = (static_cast<float>(cellZ) * k_CellSize) - threat.y;
	if (!(std::abs(awayX) > k_TooShort) && !(std::abs(awayZ) > k_TooShort))
	{
		awayX = 1.0f;
		awayZ = 0.0f;
	}
	else if (awayX != 0.0f || awayZ != 0.0f)
	{
		const float share = 1.0f / std::sqrt((awayZ * awayZ) + (awayX * awayX));
		awayX = share * awayX;
		awayZ = share * awayZ;
	}
	const float offsetX = awayX * distance;
	const float offsetZ = awayZ * distance;

	// The aimed cell replaces the cell in each map value; the place in the cell stays
	const auto withCell = [](int32_t value, int32_t cell) {
		return static_cast<int32_t>((static_cast<uint32_t>(static_cast<uint16_t>(cell)) << 16u) |
		                            (static_cast<uint32_t>(value) & 0xFFFFu));
	};
	auto at = creature;
	at.x = withCell(creature.x, map_coords::FtoL(((static_cast<float>(cellX) * k_CellSize) + offsetX) / k_CellSize));
	at.z = withCell(creature.z, map_coords::FtoL(((static_cast<float>(cellZ) * k_CellSize) + offsetZ) / k_CellSize));

	std::optional<map_coords::MapCoords> found;
	map_coords::Spiral spiral;
	for (int32_t left = k_RunAwaySearchCells; left != 0; --left)
	{
		if (map_coords::InBounds(at) && land.cellClear(map_coords::CellX(at), map_coords::CellZ(at)))
		{
			found = map_coords::MapCoords {
			    .x = withCell(0, map_coords::CellX(at)), .z = withCell(0, map_coords::CellZ(at)), .altitude = 0.0f};
			break;
		}
		map_coords::AddCells(at, spiral.Next());
	}
	const auto& place = found.value_or(creature);
	const glm::vec3 point {map_coords::ToMetres(place.x), land.ground(place) + place.altitude, map_coords::ToMetres(place.z)};
	if (land.canStand({point.x, point.z}))
	{
		return point;
	}

	// The search gives a point on no height. When it finds none, the test reads what the game left in its place: the
	// creature's cell across as the bits of a float, and the run's distance along z.
	const auto nearest = land.nearestStand({point.x, point.z});
	const glm::vec3 other = nearest.has_value() ? glm::vec3(nearest->x, 0.0f, nearest->y)
	                                            : glm::vec3(std::bit_cast<float>(static_cast<uint32_t>(cellX)), 0.0f, offsetZ);
	if (land.canStand({other.x, other.z}) && x87::Distance<x87::SumOrder::Zyx>(point, other) < k_RunAwayStandReach)
	{
		return other;
	}
	return point;
}

creature_locomotion::FidgetTurn creature_locomotion::WaitTurn(FidgetWait& wait, float stepMs, bool bodyFree,
                                                              const Random& gameRandom)
{
	wait.ms -= stepMs;
	if (wait.ms >= 0.0f)
	{
		return {};
	}
	const auto choice = wait.fidgeted ? gameRandom(3) : 0u;
	wait.fidgeted = true;
	switch (choice)
	{
	case 0:
		wait.ms = k_LookAboutFidgetMs;
		return {.fidget = Fidget::LookAbout};
	case 1:
		wait.ms = k_FidgetMs;
		return {.fidget = Fidget::Puzzled};
	default:
		// The wait starts again at its first length when the clip is over. It does not count down while the clip plays.
		wait.ms = k_FidgetMs;
		if (!bodyFree)
		{
			return {};
		}
		return {.fidget = Fidget::Confused, .mirrored = gameRandom(2) == 1};
	}
}
