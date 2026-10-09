/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WallHugRules.h"

#include <cmath>

#include <algorithm>
#include <array>

namespace openblack::ecs::wall_hug
{

namespace
{

constexpr float k_Pi = 3.1415927f;
constexpr float k_FullTurn = 6.2831855f;
/// A tenth of a degree, by which the walk round to a goal inside the circle ends short of facing it
constexpr float k_TenthDegreeCos = 0.99999845f;
constexpr float k_TenthDegreeSin = 0.0017453284f;
/// The look ahead's limits on the turns it finds
constexpr double k_MostTurns = 255.0;
constexpr double k_TooCloseTurns = 1.0;
constexpr double k_SoonTurns = 4.0;
/// How many circles a walker can be looked along in a turn
constexpr int k_MostSweeps = 3;
/// The walker faces a quarter turn round from the circle's middle, less an eighth of that for each radius it is
/// away from the circle
constexpr float k_QuarterTurn = k_Pi / 2.0f;
constexpr float k_OutwardsPerRadius = k_QuarterTurn / 8.0f;

/// The game's two dimensional cross product: positive when b is clockwise of a with x right and y down
[[nodiscard]] float Cross(glm::vec2 a, glm::vec2 b)
{
	return a.y * b.x - b.y * a.x;
}

[[nodiscard]] float Dot(glm::vec2 a, glm::vec2 b)
{
	return b.y * a.y + b.x * a.x;
}

/// Normalises a vector in place, leaving a zero vector be, and gives its length before
float Normalise(glm::vec2& v)
{
	if (v.x == 0.0f && v.y == 0.0f)
	{
		return 0.0f;
	}
	const float length = std::sqrt(v.x * v.x + v.y * v.y);
	v /= length;
	return length;
}

/// A direction from the hugged circle's middle, with which side of the walker's own direction it is on: the ones the
/// walker reaches going round before it has gone half way round come first
struct Bearing
{
	glm::vec2 point {0.0f, 0.0f};
	bool ahead {false};
};

void Resolve(Bearing& bearing, glm::vec2 origin, bool clockwise)
{
	const float cross = Cross(bearing.point, origin);
	bearing.ahead = clockwise ? cross <= 0.0f : cross >= 0.0f;
}

/// Whether the walker reaches a before b going round
[[nodiscard]] bool Before(const Bearing& a, const Bearing& b, bool clockwise)
{
	if (a.ahead == b.ahead)
	{
		const float cross = Cross(b.point, a.point);
		return clockwise ? cross < 0.0f : cross > 0.0f;
	}
	return a.ahead;
}

/// The arc of the hugged circle that another circle covers, from where the walker would meet it
struct Interval
{
	std::array<Bearing, 2> bounds {};
	float chordProjection {0.0f};
	float halfChordLengthSq {0.0f};
	float halfChordLength {0.0f};
	bool landscapeOrFence {false};
	std::optional<size_t> blocker;
};

void ResolveInterval(Interval& interval, glm::vec2 origin, bool clockwise)
{
	auto& [first, second] = interval.bounds;
	if (first.point == second.point)
	{
		return;
	}
	interval.halfChordLength = std::sqrt(interval.halfChordLengthSq);
	const float p = interval.chordProjection;
	const float h = interval.halfChordLength;
	if (clockwise)
	{
		first.point = {p * second.point.x - h * second.point.y, p * second.point.y + h * second.point.x};
	}
	else
	{
		first.point = {h * second.point.y + p * second.point.x, p * second.point.y - h * second.point.x};
	}
	Resolve(first, origin, clockwise);
	second = first;
}

void InitInterval(Interval& interval, const BlockingCircle& circle, size_t index, float radius, glm::vec2 centre,
                  glm::vec2 origin, bool clockwise)
{
	auto& [first, second] = interval.bounds;
	interval.halfChordLength = 0.0f;
	interval.landscapeOrFence = circle.landscapeOrFence;
	interval.blocker = index;
	second.point = circle.centre - centre;
	first = second;
	const glm::vec2 side = clockwise ? glm::vec2(-second.point.y, second.point.x) : glm::vec2(second.point.y, -second.point.x);
	first.point += circle.radius > radius ? side : side * (circle.radius / radius);
	Resolve(first, origin, clockwise);
	Resolve(second, origin, clockwise);
	const float distanceSq = second.point.x * second.point.x + second.point.y * second.point.y;
	interval.chordProjection = (distanceSq + radius * radius - circle.radius * circle.radius) * 0.5f / radius;
	interval.halfChordLengthSq = distanceSq - interval.chordProjection * interval.chordProjection;
	if (interval.halfChordLengthSq > 0.0f && Before(second, first, clockwise))
	{
		ResolveInterval(interval, origin, clockwise);
	}
}

/// Whether the walker meets interval a before b, settling both where they overlap
[[nodiscard]] bool Before(Interval& a, Interval& b, glm::vec2 origin, bool clockwise)
{
	if (Before(a.bounds[1], b.bounds[0], clockwise))
	{
		return true;
	}
	if (Before(b.bounds[1], a.bounds[0], clockwise))
	{
		return false;
	}
	ResolveInterval(a, origin, clockwise);
	ResolveInterval(b, origin, clockwise);
	return Before(a.bounds[1], b.bounds[0], clockwise);
}

/// The look along one circle
struct SingleSweep
{
	enum class Kind
	{
		Turns,
		StepThrough,
		HandOver,
	};
	Kind kind;
	uint8_t turns;
	std::optional<size_t> blocker;
	bool landscapeOrFence;
};

SingleSweep SweepOnce(const CircleSweepInput& input, glm::vec2 centre, float radius, float& overlap)
{
	const bool clockwise = input.clockwise;
	const auto blockers = input.blockers;
	glm::vec2 dir = input.position - centre;
	const glm::vec2 origin = dir;
	const float length = Normalise(dir);
	overlap = static_cast<float>(static_cast<double>(length / radius) - 0.9);
	const glm::vec2 toGoal = input.goal - centre;
	const bool goalInside = toGoal.x * toGoal.x + toGoal.y * toGoal.y < radius * radius;

	if (blockers.empty() && !goalInside)
	{
		return {.kind = SingleSweep::Kind::Turns, .turns = k_NoObstacleInReach};
	}

	Interval nearest;
	Interval current;
	Bearing start;
	size_t i = 0;
	if (goalInside)
	{
		auto& [first, second] = nearest.bounds;
		second.point = toGoal;
		if (second.point == glm::vec2(0.0f, 0.0f))
		{
			first.point = dir;
		}
		else if (clockwise)
		{
			first.point = {second.point.x * k_TenthDegreeCos - second.point.y * k_TenthDegreeSin,
			               second.point.y * k_TenthDegreeCos + second.point.x * k_TenthDegreeSin};
		}
		else
		{
			first.point = {second.point.y * k_TenthDegreeSin + second.point.x * k_TenthDegreeCos,
			               second.point.y * k_TenthDegreeCos - second.point.x * k_TenthDegreeSin};
		}
		second = first;
		Resolve(second, origin, clockwise);
		first.ahead = second.ahead;
		nearest.blocker.reset();
		nearest.halfChordLengthSq = 1.0f;
		start = second;
	}
	else
	{
		InitInterval(nearest, blockers[i], i, radius, centre, origin, clockwise);
		while (nearest.halfChordLengthSq <= 0.0f)
		{
			if (++i >= blockers.size())
			{
				break;
			}
			InitInterval(nearest, blockers[i], i, radius, centre, origin, clockwise);
		}
		if (i < blockers.size())
		{
			++i;
		}
	}
	for (; i < blockers.size(); ++i)
	{
		InitInterval(current, blockers[i], i, radius, centre, origin, clockwise);
		while (current.halfChordLengthSq <= 0.0f)
		{
			if (++i >= blockers.size())
			{
				break;
			}
			InitInterval(current, blockers[i], i, radius, centre, origin, clockwise);
		}
		if (i >= blockers.size())
		{
			break;
		}
		// Water and the land's edge never take the place of the end of the walk round to a goal inside the circle
		if (Before(current, nearest, origin, clockwise) && (!blockers[i].landscape || nearest.blocker.has_value()))
		{
			std::swap(nearest, current);
		}
	}

	if (nearest.halfChordLengthSq <= 0.0f)
	{
		return {.kind = SingleSweep::Kind::Turns, .turns = k_NoObstacleInReach};
	}

	ResolveInterval(nearest, origin, clockwise);
	if (goalInside)
	{
		// The goal's point ends the walk round when it comes before the end of the arc the blocking circle covers
		const glm::vec2 point = nearest.bounds[1].point;
		const float p = nearest.chordProjection;
		const float h = nearest.halfChordLength;
		Bearing arcEnd;
		if (clockwise)
		{
			const glm::vec2 mid {p * point.x + point.y * h, p * point.y - h * point.x};
			arcEnd.point = {mid.y * h + mid.x * p, mid.y * p - mid.x * h};
		}
		else
		{
			const glm::vec2 mid {p * point.x - point.y * h, h * point.x + p * point.y};
			arcEnd.point = {mid.x * p - mid.y * h, mid.x * h + mid.y * p};
		}
		Resolve(arcEnd, origin, clockwise);
		if (Before(start, arcEnd, clockwise))
		{
			nearest.bounds[0] = nearest.bounds[1] = start;
			nearest.blocker.reset();
		}
	}

	auto& end = nearest.bounds[1].point;
	Normalise(end);
	float angle = std::acos(Dot(end, dir));
	if (clockwise ? Cross(end, dir) > 0.0f : Cross(end, dir) < 0.0f)
	{
		angle = k_FullTurn - angle;
	}
	const auto speed = static_cast<float>(WholeSpeed(input.speed));
	const double turns = static_cast<double>(angle * radius * 65536.0f) / (static_cast<double>(speed * 10.0f) * 1.5);
	if (turns > k_MostTurns)
	{
		return {.kind = SingleSweep::Kind::Turns, .turns = k_NoObstacleInReach};
	}
	if (turns < k_TooCloseTurns)
	{
		if (!nearest.blocker.has_value())
		{
			return {.kind = SingleSweep::Kind::StepThrough};
		}
		return {.kind = SingleSweep::Kind::HandOver, .blocker = nearest.blocker, .landscapeOrFence = nearest.landscapeOrFence};
	}
	if (turns < k_SoonTurns)
	{
		return {.kind = SingleSweep::Kind::Turns, .turns = 0};
	}
	// Turns that are not a number (from rounding past a full cosine) come out as none, as the game's conversion does
	return {.kind = SingleSweep::Kind::Turns, .turns = std::isnan(turns) ? uint8_t {0} : static_cast<uint8_t>(turns)};
}

} // namespace

int32_t WholeSpeed(float metresPerSecond)
{
	const auto whole = std::lround(metresPerSecond * k_WholePerMetre / k_TurnsPerSecond);
	return static_cast<int32_t>(std::clamp<long>(whole, 0, k_MaxWholeSpeed));
}

float StepMetres(float metresPerSecond)
{
	return static_cast<float>(WholeSpeed(metresPerSecond)) / k_WholePerMetre;
}

float OrbitTurn(float metresPerSecond, float radius)
{
	return StepMetres(metresPerSecond) / radius;
}

uint32_t EntryDistance(float metresFromGoal)
{
	return static_cast<uint32_t>(std::max(static_cast<double>(metresFromGoal) * 128.0 - 1.0, 0.0));
}

bool LeavesCircle(glm::vec2 position, glm::vec2 goal, glm::vec2 step, uint32_t entryDistance, bool clockwise)
{
	const auto turns = static_cast<float>(entryDistance);
	const float limit = turns * (1.0f / 0x4000) * turns;
	const glm::vec2 d = position - goal;
	if (!(d.x * d.x + d.y * d.y < limit))
	{
		return false;
	}
	const float side = step.x * d.y - step.y * d.x;
	const bool goalAhead = d.x * step.x + d.y * step.y < 0.0f;
	return (clockwise ? side < 0.0f : side > 0.0f) && goalAhead;
}

CircleSweep SweepCircle(const CircleSweepInput& input)
{
	CircleSweep result {.outcome = CircleSweep::Outcome::Hug, .turnsToObstacle = k_NoObstacleInReach, .needsLookahead = false};
	glm::vec2 centre = input.centre;
	float radius = input.radius;
	for (int sweep = 1;; ++sweep)
	{
		float overlap = 0.0f;
		const auto once = SweepOnce(input, centre, radius, overlap);
		switch (once.kind)
		{
		case SingleSweep::Kind::StepThrough:
			result.outcome = CircleSweep::Outcome::StepThrough;
			result.turnsToObstacle = k_NoObstacleInReach;
			return result;
		case SingleSweep::Kind::HandOver:
		{
			const auto& next = input.blockers[*once.blocker];
			result.hugged = once.blocker;
			result.needsLookahead = once.landscapeOrFence;
			if (sweep >= k_MostSweeps)
			{
				result.turnsToObstacle = k_TurnsAfterTooManyHandovers;
				return result;
			}
			centre = next.centre;
			radius = next.radius;
			continue;
		}
		case SingleSweep::Kind::Turns:
		{
			result.turnsToObstacle = once.turns;
			const glm::vec2 toCentre = centre - input.position;
			const float offset = k_QuarterTurn - k_OutwardsPerRadius * overlap;
			// In double precision, so every platform's arctangent agrees
			const auto towards =
			    static_cast<float>(std::atan2(static_cast<double>(toCentre.y), static_cast<double>(toCentre.x)));
			result.heading = towards + (input.clockwise ? offset : -offset);
			return result;
		}
		}
	}
}

} // namespace openblack::ecs::wall_hug
