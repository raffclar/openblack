/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureRouteSearch.h"

#include <cmath>

#include <algorithm>
#include <ranges>
#include <utility>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::creature_route;

namespace
{
/// Points this close are one point, and costs or distances this close are equal
constexpr float k_Allowance = 0.001f;
/// A line shorter than this goes nowhere
constexpr float k_ShortestLineSquared = 1e-4f;
/// A line stops this far short of where it was cut short to
constexpr float k_StopShortMargin = 0.005f;
/// A circle is met by a line passing this far inside its edge
constexpr float k_GrazeMargin = 0.002f;
/// A circle this close to the walker's position is left out
constexpr float k_TooCloseToWalker = 0.1f;
/// Two circles whose centres are this close (squared) are the same circle
constexpr float k_SameCentreSquared = 1e-6f;
/// Searching back along a route, each try is given this many steps, and routes are searched back at most this deep
constexpr int32_t k_ImproveSteps = 16;
constexpr int32_t k_DeepestImprove = 2;
/// Searching back along a route tries the leg found and up to this many earlier ones
constexpr int32_t k_ImproveRetries = 3;

float Cross(glm::vec2 a, glm::vec2 b)
{
	return (a.x * b.y) - (a.y * b.x);
}

float LengthSquared(glm::vec2 v)
{
	return glm::dot(v, v);
}

/// The block holding a coordinate, cut towards nothing as the game turns them to whole numbers
int32_t BlockOf(float coordinate)
{
	return static_cast<int32_t>(coordinate / Obstacles::k_BlockSize);
}

bool Overlap(const Circle& a, const Circle& b, float allowance)
{
	const auto reach = a.radius + b.radius;
	return LengthSquared(a.centre - b.centre) < (reach * reach) + allowance;
}
} // namespace

SidePoint creature_route::SidePointFrom(const Circle& circle, glm::vec2 from, Way way)
{
	const auto toCentre = circle.centre - from;
	const auto distanceSquared = LengthSquared(toCentre);
	if (distanceSquared < k_ShortestLineSquared)
	{
		return {.point = {circle.centre.x, circle.centre.y + circle.radius}, .tangent = false};
	}
	const auto radiusSquared = circle.radius * circle.radius;
	if (distanceSquared < radiusSquared + k_Allowance)
	{
		// Inside the circle: the point on its edge nearest
		const auto out = from - circle.centre;
		return {.point = circle.centre + (out * (circle.radius / std::sqrt(LengthSquared(out)))), .tangent = false};
	}
	const auto ratio = std::max(radiusSquared / (distanceSquared - radiusSquared), 0.0f);
	const auto along = 1.0f / (ratio + 1.0f);
	const auto across = std::sqrt(ratio) * along;
	const glm::vec2 left {-toCentre.y, toCentre.x};
	const auto side = way == Way::Anticlockwise ? left : -left;
	return {.point = from + (toCentre * along) + (side * across), .tangent = true};
}

std::optional<Tangent> creature_route::TangentBetween(const Circle& from, Way fromWay, const Circle& to, Way toWay)
{
	const auto between = to.centre - from.centre;
	const auto distance = glm::length(between);
	const bool sameWay = fromWay == toWay;
	const auto ratio = (sameWay ? from.radius - to.radius : from.radius + to.radius) / distance;
	const auto rest = 1.0f - (ratio * ratio);
	if (!(rest > 0.0f))
	{
		return std::nullopt;
	}
	const auto direction = between / distance;
	const glm::vec2 left {-direction.y, direction.x};
	const auto across = std::sqrt(rest) * (fromWay == Way::Clockwise ? 1.0f : -1.0f);
	const auto normal = (direction * ratio) + (left * across);
	return Tangent {.leaving = from.centre + (normal * from.radius),
	                .arriving = sameWay ? to.centre + (normal * to.radius) : to.centre - (normal * to.radius)};
}

void Obstacles::Reset(glm::vec2 walker, float margin, Gatherer gatherer)
{
	_walker = walker;
	_margin = margin;
	_gatherer = std::move(gatherer);
	_obstacles.clear();
	_blocks.assign(static_cast<size_t>(k_BlocksPerSide * k_BlocksPerSide), {});
	_gathered.assign(static_cast<size_t>(k_BlocksPerSide * k_BlocksPerSide), false);
	_filings = 0;
}

void Obstacles::Clear()
{
	_margin = 0.0f;
	_obstacles.clear();
	_blocks.assign(static_cast<size_t>(k_BlocksPerSide * k_BlocksPerSide), {});
	_gathered.assign(static_cast<size_t>(k_BlocksPerSide * k_BlocksPerSide), false);
	_filings = 0;
}

size_t Obstacles::BlockIndex(int32_t x, int32_t z) const
{
	return static_cast<size_t>((z * k_BlocksPerSide) + x);
}

bool Obstacles::Add(std::optional<uint32_t> thing, glm::vec2 centre, float radius)
{
	if (_obstacles.size() >= k_MostCircles || _blocks.empty())
	{
		return false;
	}
	// One about a point right under the walker is no use; one it stands in reaches just to it so it can walk out
	const auto fromWalker = glm::distance(_walker, centre);
	if (fromWalker < k_TooCloseToWalker)
	{
		return false;
	}
	radius = std::min(radius, fromWalker);

	const auto lowX = BlockOf(centre.x - radius);
	const auto highX = BlockOf(centre.x + radius);
	const auto lowZ = BlockOf(centre.y - radius);
	const auto highZ = BlockOf(centre.y + radius);
	const auto filings = static_cast<size_t>(std::max((highX - lowX + 1) * (highZ - lowZ + 1), 0));
	if (filings + _filings > k_MostFilings || lowX < 0 || highX >= k_BlocksPerSide || lowZ < 0 || highZ >= k_BlocksPerSide)
	{
		return false;
	}

	for (auto z = lowZ; z <= highZ; ++z)
	{
		for (auto x = lowX; x <= highX; ++x)
		{
			for (const auto index : _blocks[BlockIndex(x, z)] | std::views::reverse)
			{
				auto& kept = _obstacles[index];
				if (!kept.thing.has_value() && !thing.has_value() && kept.circle.centre == centre)
				{
					return false;
				}
				const auto apart = glm::distance(kept.circle.centre, centre);
				if (apart + radius - k_Allowance < kept.circle.radius)
				{
					return false;
				}
				if (apart + kept.circle.radius - k_Allowance < radius)
				{
					kept.active = false;
				}
			}
		}
	}

	const auto index = static_cast<uint32_t>(_obstacles.size());
	_obstacles.push_back({.circle = {.centre = centre, .radius = radius}, .thing = thing, .active = true});
	for (auto z = lowZ; z <= highZ; ++z)
	{
		for (auto x = lowX; x <= highX; ++x)
		{
			_blocks[BlockIndex(x, z)].push_back(index);
			++_filings;
		}
	}
	return true;
}

bool Obstacles::BlockHas(uint32_t thing, glm::ivec2 cell) const
{
	const auto x = cell.x / k_CellsPerBlock;
	const auto z = cell.y / k_CellsPerBlock;
	if (_blocks.empty() || x < 0 || z < 0 || x >= k_BlocksPerSide || z >= k_BlocksPerSide)
	{
		return false;
	}
	return std::ranges::any_of(_blocks[BlockIndex(x, z)],
	                           [this, thing](uint32_t index) { return _obstacles[index].thing == thing; });
}

void Obstacles::Gather(int32_t x, int32_t z)
{
	_gathered[BlockIndex(x, z)] = true;
	if (!_gatherer)
	{
		return;
	}
	for (int32_t cz = 0; cz < k_CellsPerBlock; ++cz)
	{
		for (int32_t cx = 0; cx < k_CellsPerBlock; ++cx)
		{
			_gatherer({(x * k_CellsPerBlock) + cx, (z * k_CellsPerBlock) + cz}, *this);
		}
	}
}

std::optional<Obstacles::Hit> Obstacles::FirstInWay(glm::vec2 from, glm::vec2& to, std::optional<uint32_t> ignore,
                                                    float stopShort)
{
	const auto line = to - from;
	if (LengthSquared(line) < k_ShortestLineSquared || _blocks.empty())
	{
		return std::nullopt;
	}
	auto length = glm::length(line);
	const auto direction = line / length;
	if (stopShort > 0.0f)
	{
		// It stops short of the end, and goes nowhere when the end is no further than that
		length -= stopShort;
		if (length < k_Allowance)
		{
			to = from;
			return std::nullopt;
		}
		to = from + (direction * (length - k_StopShortMargin));
	}

	// The blocks the line crosses, in order: the first holding a circle it meets gives the circle nearest in it
	auto x = BlockOf(from.x);
	auto z = BlockOf(from.y);
	const auto endX = BlockOf(to.x);
	const auto endZ = BlockOf(to.y);
	const auto stepX = endX > x ? 1 : (endX < x ? -1 : 0);
	const auto stepZ = endZ > z ? 1 : (endZ < z ? -1 : 0);
	glm::vec2 at = from;
	// The land's first row and column of blocks are never searched
	while (x > 0 && x < k_BlocksPerSide && z > 0 && z < k_BlocksPerSide)
	{
		if (!_gathered[BlockIndex(x, z)])
		{
			Gather(x, z);
		}
		std::optional<Hit> nearest;
		float nearestDistance = 0.0f;
		for (const auto index : _blocks[BlockIndex(x, z)] | std::views::reverse)
		{
			const auto& obstacle = _obstacles[index];
			if (!obstacle.active || index == ignore)
			{
				continue;
			}
			const auto offset = obstacle.circle.centre - from;
			const auto along = glm::dot(offset, direction);
			if (!(along > 0.0f && along < length + obstacle.circle.radius))
			{
				continue;
			}
			const auto across = Cross(direction, offset);
			const auto reach = obstacle.circle.radius - k_GrazeMargin;
			if (!(-reach < across && across < reach))
			{
				continue;
			}
			const auto twice = along * -2.0f;
			const auto discriminant =
			    (twice * twice) - ((LengthSquared(offset) - (obstacle.circle.radius * obstacle.circle.radius)) * 4.0f);
			if (discriminant < 0.0f)
			{
				continue;
			}
			const auto meets = (std::sqrt(discriminant) + twice) * -0.5f;
			if (meets > -k_Allowance && meets < length - k_Allowance && (!nearest.has_value() || meets < nearestDistance))
			{
				nearest = Hit {.obstacle = index, .point = {}, .way = across > 0.0f ? Way::Anticlockwise : Way::Clockwise};
				nearestDistance = meets;
			}
		}
		if (nearest.has_value())
		{
			nearest->point = from + (direction * nearestDistance);
			return nearest;
		}

		const bool doneX = stepX == 0 || (stepX > 0 ? x >= endX : x <= endX);
		const bool doneZ = stepZ == 0 || (stepZ > 0 ? z >= endZ : z <= endZ);
		if (doneX && doneZ)
		{
			break;
		}
		if (stepZ == 0 || doneZ)
		{
			x += stepX;
		}
		else if (stepX == 0 || doneX)
		{
			z += stepZ;
		}
		else
		{
			// On to whichever block's edge the line reaches first, across x on a tie
			const auto edgeX = static_cast<float>(stepX > 0 ? x + 1 : x) * k_BlockSize;
			const auto edgeZ = static_cast<float>(stepZ > 0 ? z + 1 : z) * k_BlockSize;
			const auto toEdgeX = (edgeX - at.x) / direction.x;
			const auto toEdgeZ = (edgeZ - at.y) / direction.y;
			if (toEdgeX <= toEdgeZ)
			{
				at = {edgeX, at.y + (toEdgeX * direction.y)};
				x += stepX;
			}
			else
			{
				at = {at.x + (toEdgeZ * direction.x), edgeZ};
				z += stepZ;
			}
		}
	}
	return std::nullopt;
}

std::optional<uint32_t> Obstacles::RunsInto(uint32_t round, Way way, glm::vec2& leaving, glm::vec2 met)
{
	if (_blocks.empty())
	{
		return std::nullopt;
	}
	const auto circle = _obstacles[round].circle;
	auto point = leaving - circle.centre;
	const auto target = met - circle.centre;
	const auto lowX = std::clamp(BlockOf(circle.centre.x - circle.radius), 0, k_BlocksPerSide - 1);
	const auto highX = std::clamp(BlockOf(circle.centre.x + circle.radius), 0, k_BlocksPerSide - 1);
	const auto lowZ = std::clamp(BlockOf(circle.centre.y - circle.radius), 0, k_BlocksPerSide - 1);
	const auto highZ = std::clamp(BlockOf(circle.centre.y + circle.radius), 0, k_BlocksPerSide - 1);
	std::optional<uint32_t> found;
	for (auto z = lowZ; z <= highZ; ++z)
	{
		for (auto x = lowX; x <= highX; ++x)
		{
			if (!_gathered[BlockIndex(x, z)])
			{
				Gather(x, z);
			}
			for (const auto index : _blocks[BlockIndex(x, z)] | std::views::reverse)
			{
				const auto& other = _obstacles[index];
				if (index == round || !other.active)
				{
					continue;
				}
				const auto apart = other.circle.centre - circle.centre;
				const auto apartSquared = LengthSquared(apart);
				const auto reach = circle.radius + other.circle.radius;
				if (!(apartSquared < reach * reach))
				{
					continue;
				}
				if (apartSquared < k_SameCentreSquared)
				{
					return std::nullopt;
				}
				// Whether the other circle's edge crosses this one within the stretch still to go round
				const auto targetSide = Cross(target, apart);
				const auto pointSide = Cross(point, apart);
				const auto turn = Cross(target, point);
				const auto otherRadiusSquared = other.circle.radius * other.circle.radius;
				const auto fromPoint = LengthSquared(other.circle.centre - (circle.centre + point));
				bool crosses = false;
				if (way == Way::Clockwise)
				{
					const bool pointInside = fromPoint < otherRadiusSquared;
					crosses = turn >= 0.0f ? (targetSide > 0.0f ? (pointSide >= 0.0f || pointInside) : true)
					                       : (targetSide <= 0.0f ? (pointSide >= 0.0f || pointInside) : pointInside);
				}
				else
				{
					const bool pointInside = fromPoint <= otherRadiusSquared;
					crosses = turn <= 0.0f ? (targetSide < 0.0f ? (pointSide < 0.0f || pointInside) : true)
					                       : (targetSide >= 0.0f ? (pointSide < 0.0f || pointInside) : pointInside);
				}
				if (!crosses)
				{
					continue;
				}
				const auto radiusSquared = circle.radius * circle.radius;
				const auto distance = std::sqrt(apartSquared);
				const auto difference = otherRadiusSquared - radiusSquared;
				const auto cosine = (difference - apartSquared) / ((distance * circle.radius) + (distance * circle.radius));
				const auto acrossSquared = (1.0f - (cosine * cosine)) * radiusSquared;
				if (!(acrossSquared > 0.0f))
				{
					return std::nullopt;
				}
				const auto acrossLength = std::sqrt(acrossSquared);
				auto alongLength = std::sqrt(radiusSquared - acrossSquared);
				if (apartSquared < difference)
				{
					alongLength = -alongLength;
				}
				const auto direction = apart / distance;
				const glm::vec2 left {-direction.y, direction.x};
				point = (direction * alongLength) + (left * (way == Way::Clockwise ? acrossLength : -acrossLength));
				leaving = circle.centre + point;
				found = index;
			}
		}
	}
	return found;
}

float creature_route::LengthOf(const std::vector<Leg>& legs, size_t index, const Obstacles& obstacles)
{
	const auto& leg = legs[index];
	if (!leg.arc)
	{
		return glm::distance(leg.from, leg.to);
	}
	if (index == 0 || !legs[index - 1].heading.has_value())
	{
		return 0.0f;
	}
	const auto& before = legs[index - 1];
	return ArcLength(obstacles.At(*before.heading).circle, before.way, leg.from, leg.to);
}

void creature_route::SetCost(std::vector<Leg>& legs, size_t index, const Obstacles& obstacles)
{
	legs[index].cost = (index > 0 ? legs[index - 1].cost : 0.0f) + LengthOf(legs, index, obstacles);
}

void creature_route::SetRounds(std::vector<Leg>& legs, const Obstacles& obstacles)
{
	for (size_t i = 0; i < legs.size(); ++i)
	{
		auto& leg = legs[i];
		leg.round.reset();
		leg.roundWay = Way::None;
		if (leg.arc && i > 0 && legs[i - 1].heading.has_value())
		{
			leg.round = obstacles.At(*legs[i - 1].heading).circle;
			leg.roundWay = legs[i - 1].way;
		}
	}
}

Search::Search(Obstacles& obstacles, const Start& start, const Goal& goal, bool recordMet)
    : _start(start)
    , _goal(goal)
    , _recordMet(recordMet)
{
	Begin(obstacles);
}

void Search::Begin(Obstacles& obstacles)
{
	_routes.push_back({});
	glm::vec2 target = _goal.point;
	if (!_goal.circle.has_value())
	{
		// A goal whose whole arrival ring lies inside a circle met already can't be reached
		for (const auto& obstacle : obstacles.All())
		{
			const auto inside = obstacle.circle.radius - _goal.arriveWithin - k_Allowance;
			if (inside >= 0.0f && LengthSquared(_goal.point - obstacle.circle.centre) < inside * inside)
			{
				_status = Status::Enclosed;
				return;
			}
		}
	}
	else
	{
		const auto side = SidePointFrom(obstacles.At(*_goal.circle).circle, _start.point, Flip(_goal.way));
		if (!side.tangent)
		{
			_status = Status::Failed;
			return;
		}
		target = side.point;
	}

	const auto hit = obstacles.FirstInWay(_start.point, target, std::nullopt, _goal.stopShort);
	auto& first = _routes.back().legs;
	if (!hit.has_value())
	{
		if (_goal.circle.has_value())
		{
			const auto& circle = obstacles.At(*_goal.circle).circle;
			if (glm::dot(_start.point - _goal.metAt, circle.centre - _goal.metAt) / circle.radius < k_Allowance)
			{
				_status = Status::Failed;
				return;
			}
		}
		_status = Status::Found;
		first.push_back({.from = _start.point, .to = target});
		first.back().cost = glm::distance(_start.point, target) + _goal.startCost;
		_estimate = EstimateOf(_routes.back());
		return;
	}

	Met(obstacles, hit->obstacle);
	if (hit->obstacle == _start.circle || hit->obstacle == _start.second)
	{
		// Starting on a circle: round it the other way back, or round the second the way given
		first.push_back({.from = _start.point,
		                 .to = _start.point,
		                 .heading = _start.circle,
		                 .way = Flip(_start.way),
		                 .onStartCircle = true,
		                 .cost = _goal.startCost});
		if (_start.second.has_value())
		{
			Route second;
			second.legs.push_back({.from = _start.point,
			                       .to = _start.point,
			                       .heading = _start.second,
			                       .way = _start.way,
			                       .onStartCircle = true,
			                       .cost = _goal.startCost});
			_routes.push_back(std::move(second));
		}
		_estimate = EstimateOf(_routes.back());
		return;
	}

	// Round the circle met both ways, the way the line passes it last so it wins a tie
	const auto cost = glm::distance(_start.point, hit->point) + _goal.startCost;
	first.push_back({.from = _start.point, .to = hit->point, .heading = hit->obstacle, .way = Flip(hit->way), .cost = cost});
	if (glm::distance(hit->point, target) >= _goal.arriveWithin)
	{
		Route other;
		other.legs.push_back({.from = _start.point, .to = hit->point, .heading = hit->obstacle, .way = hit->way, .cost = cost});
		_routes.push_back(std::move(other));
	}
	else
	{
		_status = Status::Found;
	}
	_estimate = EstimateOf(_routes.back());
}

float Search::EstimateOf(const Route& route) const
{
	const auto& last = route.legs.back();
	return glm::distance(last.to, _goal.point) + last.cost;
}

bool Search::Reject(glm::vec2 point, float cost)
{
	if (_goal.mostCost < cost)
	{
		return true;
	}
	const auto same = [point](glm::vec2 reached) {
		return point.x - k_Allowance < reached.x && reached.x < point.x + k_Allowance && point.y - k_Allowance < reached.y &&
		       reached.y < point.y + k_Allowance;
	};
	if (std::ranges::any_of(_reached, same))
	{
		if (_recordMet && _mostRevisitCost < cost)
		{
			_mostRevisitCost = cost;
		}
		return true;
	}
	_reached.push_back(point);
	return false;
}

void Search::Met(const Obstacles& obstacles, uint32_t index)
{
	const auto& thing = obstacles.At(index).thing;
	if (_recordMet && thing.has_value() && std::ranges::find(_met, *thing) == _met.end())
	{
		_met.push_back(*thing);
	}
}

void Search::Remove(size_t index)
{
	if (_best == index)
	{
		_best.reset();
	}
	else if (_best.has_value() && *_best > index)
	{
		--*_best;
	}
	_routes.erase(_routes.begin() + static_cast<std::ptrdiff_t>(index));
	if (_routes.empty())
	{
		_status = Status::Failed;
		_reached.clear();
	}
}

void Search::Succeed(size_t index)
{
	auto kept = std::move(_routes[index]);
	_routes.clear();
	_routes.push_back(std::move(kept));
	_best = 0;
	_estimate = _routes.back().legs.back().cost;
	_status = Status::Found;
	_reached.clear();
}

void Search::CheckArrived(size_t index)
{
	if (glm::distance(_routes[index].legs.back().to, _goal.point) < _goal.arriveWithin)
	{
		Succeed(index);
	}
}

const std::vector<Leg>& Search::GetBest() const
{
	return _routes[_best.value_or(_routes.size() - 1)].legs;
}

std::vector<Leg> Search::TakeBest()
{
	return std::move(_routes[_best.value_or(_routes.size() - 1)].legs);
}

void Search::Step(Obstacles& obstacles, int32_t depth)
{
	if (_status != Status::Searching || _routes.empty())
	{
		return;
	}
	// The most promising route: the least cost so far plus the straight distance left, the newest on a near tie
	size_t best = _routes.size() - 1;
	_estimate = EstimateOf(_routes[best]);
	for (size_t i = best; i-- > 0;)
	{
		const auto estimate = EstimateOf(_routes[i]);
		if (estimate < _estimate - k_Allowance)
		{
			_estimate = estimate;
			best = i;
		}
	}
	_best = best;

	const auto last = _routes[best].legs.back();
	if (!last.heading.has_value() || !obstacles.At(*last.heading).active)
	{
		Remove(best);
		return;
	}
	const auto round = *last.heading;
	const auto circle = obstacles.At(round).circle;

	// Where it would leave the circle, heading for the goal
	glm::vec2 leaving;
	glm::vec2 target;
	bool headsOn = true;
	if (!_goal.circle.has_value())
	{
		const auto side = SidePointFrom(circle, _goal.point, last.way);
		leaving = side.point;
		target = _goal.point;
		headsOn = side.tangent;
	}
	else
	{
		const auto& goalCircle = obstacles.At(*_goal.circle).circle;
		const auto tangent = round == *_goal.circle || (last.way != _goal.way && Overlap(circle, goalCircle, k_Allowance))
		                         ? std::nullopt
		                         : TangentBetween(circle, last.way, goalCircle, _goal.way);
		if (!tangent.has_value() ||
		    glm::dot(circle.centre - last.to, tangent->arriving - last.to) / circle.radius < k_Allowance)
		{
			Remove(best);
			return;
		}
		leaving = tangent->leaving;
		target = tangent->arriving;
	}

	const auto addArc = [&](size_t index, glm::vec2 to, std::optional<uint32_t> heading, Way way) {
		auto& legs = _routes[index].legs;
		legs.push_back({.from = last.to, .to = to, .arc = true, .heading = heading, .way = way});
		SetCost(legs, legs.size() - 1, obstacles);
	};
	const auto addLine = [&](size_t index, glm::vec2 from, glm::vec2 to, std::optional<uint32_t> heading, Way way) {
		auto& legs = _routes[index].legs;
		legs.push_back({.from = from, .to = to, .heading = heading, .way = way});
		legs.back().cost = legs[legs.size() - 2].cost + glm::distance(from, to);
	};

	if (const auto blocked = obstacles.RunsInto(round, last.way, leaving, last.to))
	{
		// Going round, it runs into another circle: round that one next, unless that point is no use
		Met(obstacles, *blocked);
		addArc(best, leaving, *blocked, last.way);
		if (Reject(leaving, _routes[best].legs.back().cost))
		{
			Remove(best);
			return;
		}
		_routes[best].settled = last.onStartCircle;
		Improve(obstacles, best, depth);
		CheckArrived(best);
		return;
	}

	const auto hit = obstacles.FirstInWay(leaving, target, round, _goal.stopShort);
	if (!hit.has_value())
	{
		if (_goal.circle.has_value())
		{
			const auto& goalCircle = obstacles.At(*_goal.circle).circle;
			auto met = _goal.metAt;
			if (glm::dot(leaving - _goal.metAt, goalCircle.centre - _goal.metAt) / goalCircle.radius < k_Allowance ||
			    obstacles.RunsInto(*_goal.circle, _goal.way, met, target).has_value())
			{
				Remove(best);
				return;
			}
		}
		// Clear to the goal: round to the tangent and on
		_routes[best].settled = last.onStartCircle;
		addArc(best, leaving, std::nullopt, Way::None);
		if (headsOn)
		{
			addLine(best, leaving, target, std::nullopt, Way::None);
		}
		Improve(obstacles, best, depth);
		Succeed(best);
		return;
	}

	Met(obstacles, hit->obstacle);
	const auto& met = obstacles.At(hit->obstacle).circle;
	if (Overlap(met, circle, 0.0f))
	{
		// The circle met overlaps this one: it carries on round it the same way
		_routes[best].settled = last.onStartCircle;
		addArc(best, leaving, std::nullopt, Way::None);
		addLine(best, leaving, hit->point, hit->obstacle, last.way);
		Improve(obstacles, best, depth);
		CheckArrived(best);
		return;
	}

	// It may also carry on round this circle to where it runs into another, as a route of its own
	auto further = last.to;
	if (const auto other = obstacles.RunsInto(round, last.way, further, last.to))
	{
		Met(obstacles, *other);
		auto copy = _routes[best];
		copy.settled = last.onStartCircle;
		_routes.push_back(std::move(copy));
		const auto added = _routes.size() - 1;
		addArc(added, further, *other, last.way);
		if (Reject(further, _routes[added].legs.back().cost))
		{
			Remove(added);
		}
		else
		{
			Improve(obstacles, added, depth);
		}
	}

	// Leave the circle for the circle met, round it both ways, the way the line passes it in the newest route
	_routes[best].settled = last.onStartCircle;
	addArc(best, leaving, std::nullopt, Way::None);
	addLine(best, leaving, hit->point, hit->obstacle, Flip(hit->way));
	Improve(obstacles, best, depth);
	auto otherWay = _routes[best];
	otherWay.legs.back().way = Flip(otherWay.legs.back().way);
	_routes.push_back(std::move(otherWay));
	CheckArrived(best);
}

void Search::Improve(Obstacles& obstacles, size_t index, int32_t depth)
{
	auto& route = _routes[index];
	if (route.settled)
	{
		return;
	}
	const auto settle = [&route]() { route.settled = true; };
	if (depth > k_DeepestImprove || route.legs.empty())
	{
		settle();
		return;
	}
	const auto& legs = route.legs;
	// The last arc, and the leg before it
	auto arc = static_cast<int64_t>(legs.size()) - 1;
	if (!legs[static_cast<size_t>(arc)].arc)
	{
		--arc;
	}
	if (arc <= 0 || !legs[static_cast<size_t>(arc)].arc)
	{
		settle();
		return;
	}
	const auto arcIndex = static_cast<size_t>(arc);

	// Back from there to the nearest arc, or the first leg if it heads for a circle
	auto candidate = arcIndex - 1;
	while (candidate != 0 && !legs[candidate].arc)
	{
		--candidate;
	}
	if (!legs[candidate].arc && legs[candidate].way == Way::None)
	{
		settle();
		return;
	}

	for (int32_t retry = 0;; ++retry)
	{
		Start start {.point = legs[arcIndex].to,
		             .circle = legs[arcIndex - 1].heading,
		             .second = legs[arcIndex].heading,
		             .way = legs[arcIndex - 1].way};
		Goal goal {};
		if (!legs[candidate].arc)
		{
			goal.point = legs[candidate].from;
			goal.mostCost = legs[arcIndex].cost;
		}
		else
		{
			if (candidate == 0 || !legs[candidate - 1].heading.has_value())
			{
				settle();
				return;
			}
			// Back to the circle the earlier leg went round, where it met it; the point aimed at stays at the land's
			// corner, as the game has it
			const auto& before = legs[candidate - 1];
			goal.circle = before.heading;
			goal.way = Flip(before.way);
			goal.metAt = before.to;
			goal.mostCost = legs[arcIndex].cost - before.cost;
		}
		Search back(obstacles, start, goal, _recordMet);
		for (int32_t step = 0; step < k_ImproveSteps && back.GetStatus() == Status::Searching; ++step)
		{
			back.Step(obstacles, depth + 1);
		}
		for (const auto thing : back.GetMet())
		{
			if (std::ranges::find(_met, thing) == _met.end())
			{
				_met.push_back(thing);
			}
		}
		if (back.GetStatus() == Status::Found)
		{
			Splice(obstacles, route, candidate, back.GetBest());
			settle();
			return;
		}

		// Try an earlier leg: an arc, or the first leg
		if (retry >= k_ImproveRetries || candidate == 0)
		{
			settle();
			return;
		}
		--candidate;
		if (!legs[candidate].arc && candidate != 0)
		{
			--candidate;
			if (!legs[candidate].arc)
			{
				settle();
				return;
			}
		}
	}
}

void Search::Splice(Obstacles& obstacles, Route& route, size_t candidate, const std::vector<Leg>& back)
{
	const auto& legs = route.legs;
	const auto lastTo = legs.back().to;
	const auto lastHeading = legs.back().heading;
	const auto lastWay = legs.back().way;
	const auto& end = back.back();

	std::vector<Leg> joined;
	const auto addFirst = [&](Leg leg) {
		leg.cost = glm::distance(leg.from, leg.to) + _goal.startCost;
		joined.push_back(leg);
	};
	if (!legs[candidate].arc)
	{
		// Back to the start: the route is the way found, reversed
		if (end.arc && back.size() >= 2)
		{
			const auto& before = back[back.size() - 2];
			addFirst({.from = end.to, .to = end.to, .heading = before.heading, .way = Flip(before.way)});
		}
	}
	else
	{
		// Back to an earlier arc: it goes round only as far as where the way found reaches it
		joined.assign(legs.begin(), legs.begin() + static_cast<std::ptrdiff_t>(candidate) + 1);
		auto& kept = joined.back();
		kept.to = end.to;
		if (end.arc && back.size() >= 2)
		{
			kept.heading = back[back.size() - 2].heading;
			kept.way = Flip(back[back.size() - 2].way);
		}
		else
		{
			kept.heading.reset();
			kept.way = Way::None;
		}
		SetCost(joined, joined.size() - 1, obstacles);
	}

	for (auto i = back.size(); i-- > 0;)
	{
		const auto& leg = back[i];
		if (i == 0)
		{
			// The way found began where the route's last arc ended: on from there to where the route ended
			if (leg.to == lastTo && !joined.empty())
			{
				joined.back().heading = lastHeading;
				joined.back().way = lastWay;
			}
			else
			{
				joined.push_back({.from = leg.to, .to = lastTo, .arc = leg.arc, .heading = lastHeading, .way = lastWay});
				SetCost(joined, joined.size() - 1, obstacles);
			}
			break;
		}
		std::optional<uint32_t> heading;
		auto way = Way::None;
		if ((!leg.arc || back[i - 1].arc) && i >= 2)
		{
			heading = back[i - 2].heading;
			way = Opposite(back[i - 2].way);
		}
		Leg reversed {.from = leg.to, .to = leg.from, .arc = leg.arc, .heading = heading, .way = way};
		if (joined.empty())
		{
			addFirst(reversed);
		}
		else
		{
			joined.push_back(reversed);
			SetCost(joined, joined.size() - 1, obstacles);
		}
	}
	route.legs = std::move(joined);
}

void creature_route::AddLandCircles(const WalkableLand& land, glm::ivec2 cell, Obstacles& obstacles)
{
	const auto inLand = [](int32_t i) { return i >= 0 && i < k_CellsPerSide; };
	if (!inLand(cell.x) || !inLand(cell.y))
	{
		return;
	}
	for (auto x = cell.x - 1; x <= cell.x + 1; ++x)
	{
		for (auto z = cell.y - 1; z <= cell.y + 1; ++z)
		{
			if (inLand(x) && inLand(z) && land.At(x, z) == Ground::Blocked)
			{
				obstacles.Add(std::nullopt,
				              {(static_cast<float>(x) * k_CellSize) + (0.5f * k_CellSize),
				               (static_cast<float>(z) * k_CellSize) + (0.5f * k_CellSize)},
				              k_DestinationClearance);
			}
		}
	}
}
