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
constexpr float k_HalfSize = 0.5f;
constexpr float k_SizeSlope = 1.75f;
constexpr float k_SizeOffset = 0.25f;
/// The arrival ring never reaches past the destination by less than this, and is never thinner than this
constexpr float k_RingInside = 0.05f;
constexpr float k_RingThickness = 0.001f;
/// A walk is given this long and this much longer per unit of distance before it is given up on
constexpr float k_TimeLimitBaseMs = 5000.0f;
constexpr float k_TimeLimitPerUnitMs = 2.0f;

} // namespace

Speeds creature_locomotion::SpeedsFor(float size)
{
	// The speeds come from the size as it is given, before the size is kept within its limits
	const auto factor = ((size * k_HalfSize) * k_SizeSlope) + k_SizeOffset;
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

GameHeading creature_locomotion::HeadingOf(glm::vec2 direction)
{
	return GameHeading::Of(direction.x, direction.y);
}

glm::vec2 creature_locomotion::DirectionOf(GameHeading heading)
{
	// The FPU's sine and cosine, each rounded to a float
	return {x87::RoundToFloat(x87::Sin(heading.Radians())), -x87::RoundToFloat(x87::Cos(heading.Radians()))};
}

creature_locomotion::DrawnTurn creature_locomotion::DrawnAfterStop(const DrawnTurn& drawn, GameHeading heading)
{
	if (!drawn.snaps)
	{
		return drawn;
	}
	return {.from = heading, .to = heading, .snaps = false};
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

Side creature_locomotion::SpinSideOf(float angle)
{
	return angle <= 0.0f ? Side::Right : Side::Left;
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

std::optional<GameHeading> creature_locomotion::TurnThatStartsAtRest(std::optional<GameHeading> waiting, bool bodyPlaysAction)
{
	return bodyPlaysAction ? std::nullopt : waiting;
}
