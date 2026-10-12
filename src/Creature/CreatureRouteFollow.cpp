/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureRouteFollow.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::creature_route;

namespace
{
/// Walking, searches start from at most this many points spread along the route, and from its end
constexpr size_t k_MostSpreadSearches = 4;
/// Searches after the first are spread at this fraction of the route's cost left
constexpr float k_SearchSpread = 0.25f;
/// A new destination no further from the old one than this fraction of the old one's distance doesn't restart a search
constexpr float k_NearDestination = 0.5f;
/// Joining a route found while walking: searches for a shorter join try this many legs of it, each this many steps
constexpr int32_t k_JoinTries = 4;
constexpr int32_t k_JoinSteps = 32;
/// A join searched for arrives within this distance of a leg's end
constexpr float k_JoinArrival = 0.001f;
/// Distances this short are nothing, joining
constexpr float k_JoinAllowance = 0.01f;
} // namespace

void Follower::Reset(glm::vec2 position, float margin, Obstacles::Gatherer gatherer)
{
	_searches.clear();
	_obstacles.Reset(position, margin, std::move(gatherer));
	_position = position;
	_routeEnd = position;
	_route = {};
	_state = State::Standing;
	_pending = false;
	_met.clear();
}

void Follower::SetDestination(const Destination& destination)
{
	_destination.mostCost = destination.mostCost;
	while (true)
	{
		switch (_state)
		{
		case State::Standing:
			_destination = destination;
			_searches.clear();
			_searches.push_back({.search = Search(_obstacles, {.point = _routeEnd},
			                                      {.point = destination.point,
			                                       .stopShort = destination.stopShort,
			                                       .arriveWithin = destination.arriveWithin,
			                                       .mostCost = destination.mostCost}),
			                     .fromLeg = std::nullopt});
			_state = State::Planning;
			return;
		case State::Walking:
		{
			_destination = destination;
			auto& legs = _route.legs;
			// The route's costs, counted from the start of the leg it is on
			if (_route.segment > 0)
			{
				legs[_route.segment - 1].cost = 0.0f;
			}
			for (auto i = _route.segment; i < legs.size(); ++i)
			{
				legs[i].cost = (i > 0 ? legs[i - 1].cost : 0.0f) + LengthOf(legs[i]);
			}
			auto spreadFrom = legs[_route.segment].cost;
			const auto spread = (legs.back().cost - spreadFrom) * k_SearchSpread;
			_searches.clear();
			for (auto i = _route.segment; i < legs.size(); ++i)
			{
				if (i + 1 == legs.size() || (_searches.size() < k_MostSpreadSearches && spreadFrom <= legs[i].cost))
				{
					_searches.push_back({.search = Search(_obstacles, {.point = legs[i].to},
					                                      {.point = destination.point,
					                                       .stopShort = destination.stopShort,
					                                       .arriveWithin = destination.arriveWithin,
					                                       .startCost = legs[i].cost,
					                                       .mostCost = destination.mostCost}),
					                     .fromLeg = i});
					spreadFrom += spread;
				}
			}
			_state = State::Replanning;
			return;
		}
		case State::Planning:
		case State::Replanning:
		{
			const auto mostCost = _destination.mostCost;
			_destination = destination;
			_destination.mostCost = mostCost;
			_pending = true;
			const auto old = _searches.front().search.GetGoal();
			if (glm::distance(old, destination.point) <= glm::distance(old, _position) * k_NearDestination)
			{
				return;
			}
			_state = _state == State::Planning ? State::Standing : State::Walking;
			_searches.clear();
			_pending = false;
			break;
		}
		default:
			return;
		}
	}
}

Follower::Outcome Follower::Update(int32_t steps, bool recordMet, float joinAhead)
{
	if (_searches.empty())
	{
		return Outcome::None;
	}
	for (int32_t step = 0; step < steps; ++step)
	{
		// The search whose best route looks cheapest goes on
		size_t chosen = 0;
		for (size_t i = 1; i < _searches.size(); ++i)
		{
			if (_searches[i].search.GetEstimate() < _searches[chosen].search.GetEstimate())
			{
				chosen = i;
			}
		}
		auto& search = _searches[chosen].search;
		if (search.GetStatus() == Search::Status::Found)
		{
			Succeed(chosen, joinAhead);
			return Outcome::Found;
		}
		search.Step(_obstacles);
		switch (search.GetStatus())
		{
		case Search::Status::Searching:
			break;
		case Search::Status::Enclosed:
			return Fail(chosen);
		case Search::Status::Failed:
			if (recordMet && glm::distance(_destination.point, _position) > search.GetMostRevisitCost())
			{
				_met = search.GetMet();
				_state = State::GaveUp;
				_searches.clear();
				return Outcome::GaveUp;
			}
			return Fail(chosen);
		case Search::Status::Found:
			// Walking, the found route is taken only once it is the cheapest of the searches
			if (_state != State::Replanning)
			{
				Succeed(chosen, joinAhead);
				return Outcome::Found;
			}
			break;
		}
	}
	return Outcome::None;
}

void Follower::Succeed(size_t chosen, float joinAhead)
{
	if (_state == State::Planning)
	{
		_route = {.legs = _searches.front().search.TakeBest(), .segment = 0, .travelled = 0.0f};
		SetRounds(_route.legs, _obstacles);
		_routeEnd = _route.End();
		_position = _route.Position();
		_searches.clear();
		_state = State::Walking;
	}
	else if (_state == State::Replanning)
	{
		Join(chosen, joinAhead);
		_state = State::Walking;
	}
	if (_searches.empty() && _pending)
	{
		_pending = false;
		SetDestination(_destination);
	}
}

Follower::Outcome Follower::Fail(size_t /*chosen*/)
{
	if (_state == State::Planning)
	{
		_state = State::Standing;
	}
	else if (_state == State::Replanning)
	{
		_state = State::Walking;
	}
	_searches.clear();
	if (_pending)
	{
		_pending = false;
		SetDestination(_destination);
		return Outcome::None;
	}
	return Outcome::Failed;
}

void Follower::Join(size_t chosen, float joinAhead)
{
	auto& legs = _route.legs;
	const auto from = *_searches[chosen].fromLeg;
	auto found = _searches[chosen].search.TakeBest();
	legs.resize(from + 1);
	std::ranges::move(found, std::back_inserter(legs));
	_routeEnd = legs.back().to;
	_searches.clear();
	SetRounds(legs, _obstacles);

	// The join is searched again from a point ahead of the creature on the leg it is on
	auto& current = legs[_route.segment];
	const auto length = LengthOf(current);
	const auto left = length - _route.travelled;
	glm::vec2 joinFrom = _position;
	bool stepAhead = false;
	if (joinAhead > k_JoinAllowance && left > k_JoinAllowance)
	{
		joinFrom = left <= joinAhead + k_JoinAllowance ? current.to : PointAlong(current, _route.travelled + joinAhead);
		stepAhead = true;
	}
	current.from = _position;
	const auto currentCost = current.cost;
	_route.travelled = 0.0f;
	if (_route.segment > 0)
	{
		legs[_route.segment - 1].from = _position;
		legs[_route.segment - 1].to = _position;
	}
	SetRounds(legs, _obstacles);

	auto next = from + 1 < legs.size() ? std::optional<size_t>(from + 1) : std::nullopt;
	for (int32_t tries = 0; tries < k_JoinTries && next.has_value(); ++tries)
	{
		const auto index = *next;
		next = index + 1 < legs.size() ? std::optional<size_t>(index + 1) : std::nullopt;
		Search join(
		    _obstacles, {.point = joinFrom},
		    {.point = legs[index].to, .arriveWithin = k_JoinArrival, .mostCost = legs[index].cost - currentCost + length});
		for (int32_t step = 0; step < k_JoinSteps && join.GetStatus() == Search::Status::Searching; ++step)
		{
			join.Step(_obstacles);
		}
		if (join.GetStatus() != Search::Status::Found)
		{
			continue;
		}
		const auto& end = join.GetBest().back();
		if (end.heading.has_value() && end.heading != legs[index].heading)
		{
			continue;
		}
		auto shorter = join.TakeBest();
		shorter.back().heading = legs[index].heading;
		shorter.back().way = legs[index].way;
		std::vector<Leg> joined;
		if (stepAhead)
		{
			joined.push_back({.from = _position, .to = joinFrom});
		}
		std::ranges::move(shorter, std::back_inserter(joined));
		const auto resume = joined.size();
		joined.insert(joined.end(), legs.begin() + static_cast<std::ptrdiff_t>(index) + 1, legs.end());
		legs = std::move(joined);
		_route.segment = 0;
		_route.travelled = 0.0f;
		SetRounds(legs, _obstacles);
		next = resume < legs.size() ? std::optional<size_t>(resume) : std::nullopt;
	}
}

Advanced Follower::Advance(float distance)
{
	bool changed = false;
	_route.travelled += distance;
	while (!_route.Finished() && LengthOf(_route.legs[_route.segment]) < _route.travelled)
	{
		_route.travelled -= LengthOf(_route.legs[_route.segment]);
		// A search from the end of the leg just walked is too late to join
		if (!_searches.empty() && _searches.front().fromLeg == _route.segment && _route.segment + 1 < _route.legs.size())
		{
			_searches.erase(_searches.begin());
			if (_searches.empty())
			{
				_state = State::Walking;
				if (_pending)
				{
					_pending = false;
					SetDestination(_destination);
				}
			}
		}
		++_route.segment;
		changed = true;
	}
	if (_route.Finished())
	{
		_position = _routeEnd;
		_route.travelled = 0.0f;
		if (_state == State::Walking)
		{
			_obstacles.Clear();
			_state = State::Standing;
		}
		else if (_state == State::Replanning)
		{
			_state = State::Planning;
		}
	}
	else
	{
		_position = _route.Position();
	}
	_obstacles.SetWalker(_position);
	return {.position = _position, .heading = _route.Heading(), .segmentChanged = changed, .finished = _route.Finished()};
}
