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
#include <iterator>
#include <limits>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "ECS/MapCells.h"

namespace openblack::ecs::wall_hug
{

namespace
{

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
/// away from the circle, in game angles
constexpr int32_t k_QuarterTurn = 0x200;
constexpr float k_OutwardsPerRadius = 64.0f;

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
	glm::vec2 dir = ToPoint(input.position) - centre;
	const glm::vec2 origin = dir;
	const float length = Normalise(dir);
	overlap = static_cast<float>(static_cast<double>(length / radius) - 0.9);
	const glm::vec2 toGoal = ToPoint(input.goal) - centre;
	// Whether the goal is inside is measured between map positions: the circle's middle made one first
	const bool goalInside = MetresDistanceSq(input.goal, ToWhole(centre)) < radius * radius;

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
	const auto speed = static_cast<float>(input.wholeSpeed);
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

glm::vec2 ToPoint(glm::ivec2 whole)
{
	// The whole number times a tenth of 65536, rounded once to a float, as the game's integer multiply does
	return {map_coords::ToMetres(whole.x), map_coords::ToMetres(whole.y)};
}

glm::ivec2 ToWhole(glm::vec2 metres)
{
	return {map_coords::ToFixed(metres.x), map_coords::ToFixed(metres.y)};
}

float MetresDistanceSq(glm::ivec2 a, glm::ivec2 b)
{
	const glm::vec2 d = ToPoint(b - a);
	return d.x * d.x + d.y * d.y;
}

glm::ivec2 StepAlong(uint16_t angle, int32_t wholeSpeed)
{
	const int32_t sixteenth = wholeSpeed >> 4;
	return {(sixteenth * gutils::Cos(angle)) >> 12, (sixteenth * gutils::Sin(angle)) >> 12};
}

bool WithinStep(glm::ivec2 position, glm::ivec2 point, int32_t wholeSpeed)
{
	const auto dx = static_cast<float>(position.x - point.x);
	const auto dz = static_cast<float>(position.y - point.y);
	const auto r = static_cast<float>(wholeSpeed);
	return dx * dx + dz * dz < r * r;
}

uint16_t OrbitTurn(int32_t wholeSpeed, float radius)
{
	// The step over the radius is the turn in radians; each operation rounds to a float
	const float radians = static_cast<float>(wholeSpeed) / radius;
	const float angle = radians * 2048.0f / k_FullTurn / k_WholePerMetre;
	return static_cast<uint16_t>(static_cast<uint16_t>(map_coords::FtoL(angle)) + 1);
}

bool GoesRoundClockwise(glm::ivec2 position, glm::ivec2 centre, glm::ivec2 step)
{
	return Cross(ToPoint(position) - ToPoint(centre), ToPoint(step)) > 0.0f;
}

uint32_t EntryDistance(glm::ivec2 position, glm::ivec2 goal)
{
	const double metres = std::sqrt(static_cast<double>(MetresDistanceSq(position, goal)));
	return static_cast<uint32_t>(std::max(metres * 128.0 - 1.0, 0.0));
}

bool LeavesCircle(glm::ivec2 position, glm::ivec2 goal, glm::ivec2 step, uint32_t entryDistance, bool clockwise)
{
	const auto turns = static_cast<float>(entryDistance);
	const float limit = turns * (1.0f / 0x4000) * turns;
	if (!(MetresDistanceSq(position, goal) < limit))
	{
		return false;
	}
	// In whole units, each coordinate made a float on its own first
	const float dx = static_cast<float>(static_cast<uint32_t>(position.x)) - static_cast<float>(static_cast<uint32_t>(goal.x));
	const float dz = static_cast<float>(static_cast<uint32_t>(position.y)) - static_cast<float>(static_cast<uint32_t>(goal.y));
	const auto sx = static_cast<float>(step.x);
	const auto sz = static_cast<float>(step.y);
	const float side = sx * dz - sz * dx;
	const bool goalAhead = dx * sx + dz * sz < 0.0f;
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
			// A quarter turn round from facing the circle's middle, less a little outwards for each radius away
			const int32_t towards = gutils::GetAngleFromXZ(input.position, ToWhole(centre));
			const int32_t outwards = map_coords::FtoL(overlap * (input.clockwise ? k_OutwardsPerRadius : -k_OutwardsPerRadius));
			const int32_t angle = towards + (input.clockwise ? k_QuarterTurn : -k_QuarterTurn) - outwards;
			result.heading = static_cast<uint16_t>(angle & gutils::k_GameAngleMask);
			return result;
		}
		}
	}
}

namespace
{

/// How far into a circle the walker may already be and still count it as ahead, in metres
constexpr double k_AlreadyInside = -0.2;
/// Out of reach along the line
constexpr float k_Unreachable = std::numeric_limits<float>::max();

/// Where along the walker's line a circle is: `near` is where the line comes into it, or until worked out only as near
/// as the circle's radius short of `closest`, the point of the line nearest the circle's middle
struct LineInterval
{
	float near;
	float closest;
	/// The radius squared less the square of the middle's distance from the line: above 0 when the line meets it
	float meets;
	size_t circle;

	/// Works out where the line comes into the circle, once
	void Resolve()
	{
		if (near != closest)
		{
			near = closest = closest - std::sqrt(meets);
			if (static_cast<double>(near) < k_AlreadyInside)
			{
				near = closest = k_Unreachable;
			}
		}
	}

	/// Whether this circle comes before another along the line
	[[nodiscard]] bool Before(LineInterval& other)
	{
		if (closest < other.near)
		{
			return true;
		}
		if (near > other.closest)
		{
			return false;
		}
		Resolve();
		other.Resolve();
		return closest < other.near;
	}
};

[[nodiscard]] LineInterval IntervalOf(const BlockingCircle& circle, size_t index, glm::vec2 origin, glm::vec2 direction)
{
	const glm::vec2 offset = circle.centre - origin;
	LineInterval interval {};
	interval.closest = offset.x * direction.x + offset.y * direction.y;
	interval.near = interval.closest - circle.radius;
	interval.meets =
	    interval.closest * interval.closest + (circle.radius * circle.radius - (offset.x * offset.x + offset.y * offset.y));
	interval.circle = index;
	if (static_cast<double>(interval.near) < k_AlreadyInside && interval.meets > 0.0f)
	{
		interval.Resolve();
	}
	return interval;
}

} // namespace

LineScan ScanLine(glm::ivec2 position, glm::ivec2 step, std::span<const BlockingCircle> circles)
{
	constexpr LineScan k_Nothing {.turnsToObstacle = k_NoObstacleInReach, .circle = std::nullopt};
	const glm::vec2 origin = ToPoint(position);
	glm::vec2 direction = ToPoint(step);
	float length = 0.0f;
	if (direction != glm::vec2(0.0f))
	{
		length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
		direction *= 1.0f / length;
	}

	// The first circle the line meets, then each later one that comes before it
	std::optional<LineInterval> nearest;
	for (size_t i = 0; i < circles.size(); ++i)
	{
		auto interval = IntervalOf(circles[i], i, origin, direction);
		if (interval.meets <= 0.0f)
		{
			continue;
		}
		if (!nearest.has_value() || interval.Before(*nearest))
		{
			nearest = interval;
		}
	}
	if (!nearest.has_value())
	{
		return k_Nothing;
	}
	nearest->Resolve();
	if (nearest->near == k_Unreachable)
	{
		return k_Nothing;
	}
	const float turns = nearest->near / length;
	if (!(turns <= 255.0f))
	{
		return k_Nothing;
	}
	// Truncated towards nothing; a line with no length right inside a circle meets it at once
	const auto whole = turns < static_cast<float>(std::numeric_limits<int32_t>::min()) ? 0 : static_cast<int32_t>(turns);
	const auto turnsToObstacle = static_cast<uint8_t>(whole & 0xff);
	if (turnsToObstacle == k_NoObstacleInReach)
	{
		return k_Nothing;
	}
	return {.turnsToObstacle = turnsToObstacle, .circle = nearest->circle};
}

std::vector<BlockingCircle> CirclesOf(const ThingOnMap& thing)
{
	const auto blocking = [&thing](const map_cells::Circle& circle) {
		return BlockingCircle {
		    .centre = circle.centre, .radius = circle.radius, .landscape = false, .landscapeOrFence = thing.fence};
	};
	switch (thing.shape)
	{
	case ThingShape::None:
		return {};
	case ThingShape::Trunk:
		// At the tree's position on the map
		return {blocking({.centre = {map_coords::Quantise(thing.position.x), map_coords::Quantise(thing.position.z)},
		                  .radius = k_TreeTrunkRadius})};
	case ThingShape::TempleRing:
	{
		// At the temple's place on the map, turned as its model is
		const glm::vec3 xAxis = thing.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
		return TempleRingCircles({map_coords::Quantise(thing.position.x), map_coords::Quantise(thing.position.z)},
		                         std::atan2(xAxis.z, xAxis.x));
	}
	case ThingShape::ModelBox:
		break;
	}
	const auto outline = map_cells::CirclesOf(
	    map_cells::OutlineOfBox(thing.position, thing.rotation, thing.scale, thing.boxCentre, thing.boxHalfSize));
	// A long box's own bounding circle is never in the way: only the row along it
	if (outline.row.empty())
	{
		return {blocking(outline.bounds)};
	}
	std::vector<BlockingCircle> circles;
	circles.reserve(outline.row.size());
	std::ranges::transform(outline.row, std::back_inserter(circles), blocking);
	return circles;
}

std::vector<BlockingCircle> TempleRingCircles(glm::vec2 centre, float yAngle)
{
	// Every length is a share of the temple's reach, whatever the temple's size
	constexpr float k_Reach = 21.5f;
	constexpr int k_Spokes = 7;
	// The two spokes either side of the way in have six circles, the others four
	constexpr int k_BentSpokes = 2;
	constexpr int k_StraightCircles = 3;
	constexpr int k_PlainSpokeCircles = 4;
	constexpr float k_SpokeAngle = 0.8975979f; // a seventh of a turn
	constexpr float k_FirstSpokeTurn = 3.83f;
	constexpr float k_SpokeCircleRadius = k_Reach * 0.11f;
	// The bent spokes' last three circles: how far round each is turned (towards the other bent spoke), and how much
	// further out the next one is
	constexpr std::array<float, 3> k_BendTurn {0.06f, 0.16f, 0.27f};
	constexpr std::array<float, 3> k_BendStep {0.1f, 0.07f, 0.0f};

	const auto blocking = [](glm::vec2 at, float radius) {
		return BlockingCircle {.centre = at, .radius = radius, .landscape = false, .landscapeOrFence = false};
	};
	std::vector<BlockingCircle> circles;
	circles.reserve(1 + k_BentSpokes * 6 + (k_Spokes - k_BentSpokes) * k_PlainSpokeCircles);
	circles.push_back(blocking(centre, k_Reach * 0.7f));

	// Worked out as the game does, each sum and product in double before it is kept as a float
	const auto at = [&centre](double distance, double cosine, double sine) {
		return glm::vec2 {static_cast<float>(distance * cosine + centre.x), static_cast<float>(distance * sine + centre.y)};
	};
	const float radius = 1.4f * k_SpokeCircleRadius;
	float angle = yAngle + k_FirstSpokeTurn;
	for (int spoke = 0; spoke < k_Spokes; ++spoke)
	{
		const auto sine = static_cast<float>(std::sin(static_cast<double>(angle)));
		const auto cosine = static_cast<float>(std::cos(static_cast<double>(angle)));
		float distance = k_Reach * 0.74f;
		const int straight = spoke < k_BentSpokes ? k_StraightCircles : k_PlainSpokeCircles;
		for (int i = 0; i < straight; ++i)
		{
			circles.push_back(blocking(at(distance, cosine, sine), radius));
			distance = static_cast<float>(static_cast<double>(k_Reach) * 0.2f + distance);
		}
		if (spoke < k_BentSpokes)
		{
			// The first bends one way round, the second the other
			const double side = spoke == 0 ? 1.0 : -1.0;
			for (size_t i = 0; i < k_BendTurn.size(); ++i)
			{
				const double turned = side * k_BendTurn.at(i) + angle;
				circles.push_back(blocking(at(distance, std::cos(turned), std::sin(turned)), radius));
				distance = static_cast<float>(static_cast<double>(k_Reach) * k_BendStep.at(i) + distance);
			}
		}
		angle += k_SpokeAngle;
	}
	return circles;
}

bool IsFenceModel(MeshId model)
{
	return model == MeshId::BuildingAmericanFence || model == MeshId::BuildingCelticFenceShort ||
	       model == MeshId::BuildingCelticFenceTall;
}

} // namespace openblack::ecs::wall_hug
