/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureRoute.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <limits>
#include <numbers>
#include <ranges>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

using namespace openblack;
using namespace openblack::creature_route;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
/// Searching outwards for somewhere to stand: in this many directions, this much further each time
constexpr int32_t k_SearchDirections = 16;
constexpr float k_SearchStep = 0.5f;
constexpr float k_ShortestLeg = 0.001f;

bool Standable(Ground ground)
{
	return ground == Ground::Open || ground == Ground::Water;
}

float HeadingBetween(glm::vec2 from, glm::vec2 to)
{
	const auto d = to - from;
	return std::atan2(-d.x, -d.y);
}
} // namespace

WalkableLand::WalkableLand()
    : _cells(static_cast<size_t>(k_CellsPerSide * k_CellsPerSide), Ground::Open)
{
}

WalkableLand WalkableLand::Build(const CornerHeight& height, const CellWater& water)
{
	WalkableLand land;
	const auto index = [](int32_t x, int32_t z) { return static_cast<size_t>((z * k_CellsPerSide) + x); };
	for (int32_t z = 0; z < k_CellsPerSide; ++z)
	{
		for (int32_t x = 0; x < k_CellsPerSide; ++x)
		{
			const std::array corners {height(x, z), height(x + 1, z), height(x, z + 1), height(x + 1, z + 1)};
			const auto [low, high] = std::ranges::minmax(corners);
			const bool deep = high <= 0.0f;
			land._cells[index(x, z)] = (high - low > k_SteepRise || deep) ? Ground::Blocked : Ground::CutOff;
		}
	}

	// The largest stretch of walkable cells joined side by side is the land; the rest is cut off from it
	std::vector<int32_t> labels(land._cells.size(), -1);
	std::vector<size_t> sizes;
	std::vector<size_t> stack;
	for (size_t start = 0; start < land._cells.size(); ++start)
	{
		if (land._cells[start] == Ground::Blocked || labels[start] >= 0)
		{
			continue;
		}
		const auto label = static_cast<int32_t>(sizes.size());
		size_t count = 0;
		stack.push_back(start);
		labels[start] = label;
		while (!stack.empty())
		{
			const auto cell = stack.back();
			stack.pop_back();
			++count;
			const auto x = static_cast<int32_t>(cell % k_CellsPerSide);
			const auto z = static_cast<int32_t>(cell / k_CellsPerSide);
			for (const auto [dx, dz] : std::array<std::array<int32_t, 2>, 4> {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}})
			{
				const auto nx = x + dx;
				const auto nz = z + dz;
				if (nx < 0 || nz < 0 || nx >= k_CellsPerSide || nz >= k_CellsPerSide)
				{
					continue;
				}
				const auto next = index(nx, nz);
				if (land._cells[next] != Ground::Blocked && labels[next] < 0)
				{
					labels[next] = label;
					stack.push_back(next);
				}
			}
		}
		sizes.push_back(count);
	}
	if (sizes.empty())
	{
		return land;
	}
	const auto largest = static_cast<int32_t>(std::distance(sizes.begin(), std::ranges::max_element(sizes)));
	for (size_t i = 0; i < land._cells.size(); ++i)
	{
		if (labels[i] != largest)
		{
			continue;
		}
		const auto wet = water(static_cast<int32_t>(i % k_CellsPerSide), static_cast<int32_t>(i / k_CellsPerSide));
		land._cells[i] = !wet.has_value() || *wet ? Ground::Water : Ground::Open;
	}
	return land;
}

Ground WalkableLand::At(int32_t x, int32_t z) const
{
	if (x < 0 || z < 0 || x >= k_CellsPerSide || z >= k_CellsPerSide)
	{
		return Ground::Blocked;
	}
	return _cells[static_cast<size_t>((z * k_CellsPerSide) + x)];
}

Ground WalkableLand::AtPoint(glm::vec2 point) const
{
	return At(static_cast<int32_t>(std::floor(point.x / k_CellSize)), static_cast<int32_t>(std::floor(point.y / k_CellSize)));
}

bool WalkableLand::IsValid(glm::vec2 point, float radius) const
{
	const auto cx = static_cast<int32_t>(std::floor(point.x / k_CellSize));
	const auto cz = static_cast<int32_t>(std::floor(point.y / k_CellSize));
	if (!Standable(At(cx, cz)))
	{
		return false;
	}
	const auto reach = static_cast<int32_t>(std::ceil(radius / k_CellSize));
	for (int32_t dz = -reach; dz <= reach; ++dz)
	{
		for (int32_t dx = -reach; dx <= reach; ++dx)
		{
			if (Standable(At(cx + dx, cz + dz)))
			{
				continue;
			}
			const glm::vec2 centre {(static_cast<float>(cx + dx) + 0.5f) * k_CellSize,
			                        (static_cast<float>(cz + dz) + 0.5f) * k_CellSize};
			if (glm::distance(point, centre) < radius)
			{
				return false;
			}
		}
	}
	return true;
}

std::optional<glm::vec2> WalkableLand::NearestValid(glm::vec2 point, float radius, float maxDistance) const
{
	if (IsValid(point, radius))
	{
		return point;
	}
	for (float distance = k_SearchStep; distance <= maxDistance; distance += k_SearchStep)
	{
		for (int32_t i = 0; i < k_SearchDirections; ++i)
		{
			const auto angle = 2.0f * k_Pi * static_cast<float>(i) / static_cast<float>(k_SearchDirections);
			const auto candidate = point + (distance * glm::vec2(std::cos(angle), std::sin(angle)));
			if (IsValid(candidate, radius))
			{
				return candidate;
			}
		}
	}
	return std::nullopt;
}

float creature_route::DistanceToSegment(glm::vec2 point, glm::vec2 from, glm::vec2 to)
{
	const auto along = to - from;
	const auto lengthSquared = glm::dot(along, along);
	if (lengthSquared <= 0.0f)
	{
		return glm::distance(point, from);
	}
	const auto t = std::clamp(glm::dot(point - from, along) / lengthSquared, 0.0f, 1.0f);
	return glm::distance(point, from + (t * along));
}

float creature_route::ArcLength(const Circle& circle, Way way, glm::vec2 from, glm::vec2 to)
{
	auto start = std::atan2(from.y - circle.centre.y, from.x - circle.centre.x);
	auto end = std::atan2(to.y - circle.centre.y, to.x - circle.centre.x);
	if (way == Way::Anticlockwise)
	{
		if (end < start)
		{
			end += 2.0f * k_Pi;
		}
		return (end - start) * circle.radius;
	}
	if (start < end)
	{
		start += 2.0f * k_Pi;
	}
	return (start - end) * circle.radius;
}

float creature_route::LengthOf(const Leg& leg)
{
	if (leg.arc && leg.round.has_value())
	{
		return ArcLength(*leg.round, leg.roundWay, leg.from, leg.to);
	}
	return glm::distance(leg.from, leg.to);
}

namespace
{
/// The angles round an arc's circle where it starts and ends, the end taken round past the start the way it goes
std::array<float, 2> ArcAngles(const Leg& leg)
{
	const auto& circle = *leg.round;
	auto start = std::atan2(leg.from.y - circle.centre.y, leg.from.x - circle.centre.x);
	auto end = std::atan2(leg.to.y - circle.centre.y, leg.to.x - circle.centre.x);
	if (leg.roundWay == Way::Anticlockwise)
	{
		if (end < start)
		{
			end += 2.0f * k_Pi;
		}
	}
	else if (start < end)
	{
		start += 2.0f * k_Pi;
	}
	return {start, end};
}

float ArcAngleAt(const Leg& leg, float distance)
{
	const auto length = LengthOf(leg);
	const auto [start, end] = ArcAngles(leg);
	return length > 0.0f ? start + ((distance / length) * (end - start)) : start;
}
} // namespace

glm::vec2 creature_route::PointAlong(const Leg& leg, float distance)
{
	if (leg.arc && leg.round.has_value())
	{
		const auto angle = ArcAngleAt(leg, distance);
		return leg.round->centre + (leg.round->radius * glm::vec2(std::cos(angle), std::sin(angle)));
	}
	const auto length = glm::distance(leg.from, leg.to);
	// A straight leg too short to measure keeps the creature at its start
	return length > k_ShortestLeg ? leg.from + ((leg.to - leg.from) * (distance / length)) : leg.from;
}

float creature_route::HeadingAlong(const Leg& leg, float distance)
{
	if (leg.arc && leg.round.has_value())
	{
		const auto angle = ArcAngleAt(leg, distance);
		const auto tangent = leg.roundWay == Way::Anticlockwise ? glm::vec2(-std::sin(angle), std::cos(angle))
		                                                        : glm::vec2(std::sin(angle), -std::cos(angle));
		return std::atan2(-tangent.x, -tangent.y);
	}
	return HeadingBetween(leg.from, leg.to);
}

float Route::SegmentLength() const
{
	return Finished() ? 0.0f : LengthOf(legs[segment]);
}

float Route::Remaining() const
{
	return std::max(SegmentLength() - travelled, 0.0f);
}

float Route::RemainingTotal() const
{
	auto total = Remaining();
	for (auto i = segment + 1; i < legs.size(); ++i)
	{
		total += LengthOf(legs[i]);
	}
	return total;
}

glm::vec2 Route::Position() const
{
	if (legs.empty())
	{
		return {};
	}
	if (Finished())
	{
		return legs.back().to;
	}
	return PointAlong(legs[segment], travelled);
}

glm::vec2 Route::End() const
{
	return legs.empty() ? glm::vec2(0.0f) : legs.back().to;
}

float Route::Heading() const
{
	if (legs.empty())
	{
		return 0.0f;
	}
	if (Finished())
	{
		return HeadingAlong(legs.back(), LengthOf(legs.back()));
	}
	return HeadingAlong(legs[segment], travelled);
}

float Route::TurnAtEnd() const
{
	if (OnLastSegment())
	{
		return 0.0f;
	}
	const auto here = HeadingAlong(legs[segment], LengthOf(legs[segment]));
	const auto next = HeadingAlong(legs[segment + 1], 0.0f);
	auto turn = std::fmod(next - here + k_Pi, 2.0f * k_Pi);
	if (turn < 0.0f)
	{
		turn += 2.0f * k_Pi;
	}
	return turn - k_Pi;
}

std::vector<glm::vec2> Route::Points(float maxStepAngle) const
{
	std::vector<glm::vec2> points;
	for (const auto& leg : legs)
	{
		if (points.empty())
		{
			points.push_back(leg.from);
		}
		if (leg.arc && leg.round.has_value() && maxStepAngle > 0.0f)
		{
			const auto [start, end] = ArcAngles(leg);
			const auto steps = std::max(static_cast<int32_t>(std::ceil(std::abs(end - start) / maxStepAngle)), 1);
			const auto length = LengthOf(leg);
			for (int32_t k = 1; k <= steps; ++k)
			{
				points.push_back(PointAlong(leg, length * static_cast<float>(k) / static_cast<float>(steps)));
			}
			continue;
		}
		points.push_back(leg.to);
	}
	return points;
}

Advanced creature_route::Advance(Route& route, float distance)
{
	bool changed = false;
	while (!route.Finished())
	{
		const auto length = route.SegmentLength();
		if (route.travelled + distance < length)
		{
			route.travelled += distance;
			break;
		}
		distance -= length - route.travelled;
		++route.segment;
		route.travelled = 0.0f;
		changed = true;
	}
	return {.position = route.Position(), .heading = route.Heading(), .segmentChanged = changed, .finished = route.Finished()};
}

glm::ivec2 creature_route::StepBlock(glm::vec3 end)
{
	// Truncated towards nothing, as the game turns them to whole numbers
	return {static_cast<int32_t>(end.x / k_StepBlockSize), static_cast<int32_t>(end.y / k_StepBlockSize)};
}

std::vector<creature_route::Circle> creature_route::StepBlockCircles(const WalkableLand& land, glm::ivec2 block,
                                                                     glm::vec2 standing)
{
	// The follower is never closer to a circle's centre than this
	constexpr float k_LeastRange = 0.1f;
	// Circles are kept in 80-unit squares, the land's 64 a side: one reaching past them is left out
	constexpr int32_t k_Squares = 64;
	const auto inSquares = [](float low, float high) {
		return static_cast<int32_t>(low / k_StepBlockSize) >= 0 && static_cast<int32_t>(high / k_StepBlockSize) < k_Squares;
	};
	std::vector<Circle> circles;
	const auto add = [&](glm::vec2 centre, float radius) {
		const float range = glm::distance(standing, centre);
		if (range < k_LeastRange)
		{
			return;
		}
		radius = std::min(radius, range);
		if (inSquares(centre.x - radius, centre.x + radius) && inSquares(centre.y - radius, centre.y + radius))
		{
			circles.push_back({.centre = centre, .radius = radius});
		}
	};
	const auto inLand = [](int32_t i) { return i >= 0 && i < k_CellsPerSide; };
	for (int32_t z = block.y * k_StepBlockCells; z < (block.y + 1) * k_StepBlockCells; ++z)
	{
		for (int32_t x = block.x * k_StepBlockCells; x < (block.x + 1) * k_StepBlockCells; ++x)
		{
			if (!inLand(x) || !inLand(z))
			{
				continue;
			}
			for (int32_t nx = x - 1; nx <= x + 1; ++nx)
			{
				for (int32_t nz = z - 1; nz <= z + 1; ++nz)
				{
					// The neighbour's sort is read from the cell a row further on. No land stands high enough for that to
					// pass the last row.
					if (!inLand(nx) || !inLand(nz) || !inLand(nz + 1) || land.At(nx, nz + 1) != Ground::Blocked)
					{
						continue;
					}
					add({(static_cast<float>(nx) * k_CellSize) + (0.5f * k_CellSize),
					     (static_cast<float>(nz) * k_CellSize) + (0.5f * k_CellSize)},
					    k_DestinationClearance);
				}
			}
		}
	}
	return circles;
}

bool creature_route::InsideAny(glm::vec2 point, const std::vector<Circle>& circles)
{
	return std::ranges::any_of(circles, [point](const Circle& circle) {
		const auto d = point - circle.centre;
		return glm::dot(d, d) < circle.radius * circle.radius;
	});
}
