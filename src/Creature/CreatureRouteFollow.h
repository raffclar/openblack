/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "CreatureRoute.h"
#include "CreatureRouteSearch.h"

namespace openblack::creature_route
{
/// A creature's walk: what it walks round, the searches for its route and the route it follows.
///
/// Standing, a new destination starts a search from where it stands. Walking, a new destination starts searches from
/// up to five points ahead on the route it is following (the end of the leg it is on, points spread along the rest and
/// the route's end), the creature walking on meanwhile; the cheapest that finds a way is joined onto the route there,
/// and the stretch from the creature to the next few legs is searched again for a shorter join. A search started from
/// a point the creature has walked past is dropped. Asked to go somewhere else while still searching, it carries on
/// with the old search if the new destination is near the old one (no further from it than half its distance from the
/// creature), and searches again once that one ends.
class Follower
{
public:
	enum class State : uint8_t
	{
		Idle,
		Standing,
		Planning,
		Walking,
		/// Walking on along its route while searching for a way to a new destination
		Replanning,
		/// Gave up on its first try: things in the way left it nowhere near
		GaveUp,
	};

	struct Destination
	{
		glm::vec2 point {0.0f};
		/// Lines head for a point this far short of it, and arriving within this distance is arriving
		float stopShort {0.0f};
		float arriveWithin {0.0f};
		/// Routes costing more than this are given up
		float mostCost {0.0f};
	};

	enum class Outcome : uint8_t
	{
		None,
		/// A route was found and is being followed
		Found,
		/// No route could be found
		Failed,
		/// Its first search came to nothing far short of the destination, among the things it met
		GaveUp,
	};

	/// Starts afresh, standing at a point and keeping clear of things by a margin
	void Reset(glm::vec2 position, float margin, Obstacles::Gatherer gatherer = {});
	/// Sets where to go, as its state allows (see the class)
	void SetDestination(const Destination& destination);
	/// Takes its searches up to a number of steps further. When recordMet is set, the things met are noted, and a first
	/// search that comes to nothing gives up rather than fails when the destination is further away than any route
	/// coming back on itself had cost. A route found while walking is joined onto the route followed, searched again
	/// from a point a distance ahead of the creature.
	Outcome Update(int32_t steps, bool recordMet, float joinAhead);
	/// Moves a distance along the route followed
	Advanced Advance(float distance);

	[[nodiscard]] State GetState() const { return _state; }
	[[nodiscard]] bool IsFollowing() const { return _state == State::Walking || _state == State::Replanning; }
	[[nodiscard]] bool IsSearching() const { return _state == State::Planning || _state == State::Replanning; }
	[[nodiscard]] const Route& GetRoute() const { return _route; }
	[[nodiscard]] Obstacles& GetObstacles() { return _obstacles; }
	[[nodiscard]] const Obstacles& GetObstacles() const { return _obstacles; }
	[[nodiscard]] const Destination& GetDestination() const { return _destination; }
	[[nodiscard]] glm::vec2 GetPosition() const { return _position; }
	[[nodiscard]] size_t GetSearchCount() const { return _searches.size(); }
	/// The things met by a first search that gave up
	[[nodiscard]] const std::vector<uint32_t>& GetMet() const { return _met; }

private:
	struct Ahead
	{
		Search search;
		/// The leg of the route followed it starts from the end of, none for a search from where it stands
		std::optional<size_t> fromLeg;
	};

	void Succeed(size_t chosen, float joinAhead);
	Outcome Fail(size_t chosen);
	void Join(size_t chosen, float joinAhead);

	Obstacles _obstacles;
	State _state {State::Idle};
	Route _route;
	glm::vec2 _position {0.0f};
	/// Where it will be once the route followed ends
	glm::vec2 _routeEnd {0.0f};
	Destination _destination;
	/// A new destination came while searching: searched for once the search ends
	bool _pending {false};
	std::vector<Ahead> _searches;
	std::vector<uint32_t> _met;
};
} // namespace openblack::creature_route
