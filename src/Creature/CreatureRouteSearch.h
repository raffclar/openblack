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

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "CreatureRoute.h"

/// How a creature finds its way round what stands in its way.
///
/// What it walks round is a set of circles, gathered block by block (8 by 8 cells, 80 units a side) the first time the
/// search reaches a block. A route is a chain of straight legs and arcs: from a point it heads straight for where it is
/// going, and on meeting a circle it goes round it, clockwise or anticlockwise, leaving it along the tangent that heads
/// on. Partial routes are kept side by side and the one with the least cost so far plus the straight distance left is
/// taken further, one step at a time. Each time a route is taken further, a shorter way back to an earlier part of it is
/// looked for, so routes don't keep the detours of their first guess.
namespace openblack::creature_route
{
/// A circle met while a route is planned, and what it belongs to (nothing for the land's own)
struct Obstacle
{
	Circle circle;
	std::optional<uint32_t> thing;
	/// A later circle covering this one wholly puts it out of use
	bool active {true};
};

/// Where a route's way round a circle leaves it, heading for a point: the tangent from the point, or where the point lies
/// if it is inside the circle (tangent false)
struct SidePoint
{
	glm::vec2 point;
	bool tangent;
};
[[nodiscard]] SidePoint SidePointFrom(const Circle& circle, glm::vec2 from, Way way);

/// The tangent that leaves one circle going round it one way and meets another going round it another way: where it
/// leaves the first and where it meets the second. None when the circles are too close for it.
struct Tangent
{
	glm::vec2 leaving;
	glm::vec2 arriving;
};
[[nodiscard]] std::optional<Tangent> TangentBetween(const Circle& from, Way fromWay, const Circle& to, Way toWay);

/// What a creature walks round on one walk, kept for the whole walk: circles are only ever added. Circles are filed in
/// the blocks their square overlaps.
class Obstacles
{
public:
	static constexpr int32_t k_BlocksPerSide = 64;
	static constexpr float k_BlockSize = 80.0f;
	static constexpr int32_t k_CellsPerBlock = 8;
	/// The most circles kept, and the most times they are filed in blocks
	static constexpr size_t k_MostCircles = 16384;
	static constexpr size_t k_MostFilings = 16384;

	/// Called once for each of a block's cells the first time a search reaches the block, to add what stands there
	using Gatherer = std::function<void(glm::ivec2 cell, Obstacles& obstacles)>;

	/// Starts afresh for a walk from a point, keeping clear of things by a margin
	void Reset(glm::vec2 walker, float margin, Gatherer gatherer = {});
	/// Forgets every circle, keeping how blocks are gathered
	void Clear();
	/// Where the creature walking is now: circles added later are kept clear of it
	void SetWalker(glm::vec2 walker) { _walker = walker; }
	[[nodiscard]] glm::vec2 GetWalker() const { return _walker; }
	[[nodiscard]] float GetMargin() const { return _margin; }

	/// Adds a circle. One about a point within a tenth of the walker is left out and one the walker stands in reaches
	/// just to it; one inside a circle already kept is left out, and one already kept wholly inside the new one is put out
	/// of use. The land's circles are kept once at each centre. Returns whether it was kept.
	bool Add(std::optional<uint32_t> thing, glm::vec2 centre, float radius);
	/// Whether a thing's circles are already filed in the block of a cell
	[[nodiscard]] bool BlockHas(uint32_t thing, glm::ivec2 cell) const;
	[[nodiscard]] const std::vector<Obstacle>& All() const { return _obstacles; }
	[[nodiscard]] const Obstacle& At(uint32_t index) const { return _obstacles[index]; }

	/// The first circle a straight line from a point meets, where and which way round it, gathering the blocks the line
	/// crosses as it goes. The line stops short of its end by a distance, which shortens the end given.
	struct Hit
	{
		uint32_t obstacle;
		glm::vec2 point;
		Way way;
	};
	[[nodiscard]] std::optional<Hit> FirstInWay(glm::vec2 from, glm::vec2& to, std::optional<uint32_t> ignore, float stopShort);
	/// Going round a circle one way from where the route met it to where it would leave, the circle it runs into first,
	/// moving the leaving point back to where it does. None when the way round is clear, or when it can't be worked out.
	[[nodiscard]] std::optional<uint32_t> RunsInto(uint32_t round, Way way, glm::vec2& leaving, glm::vec2 met);

private:
	void Gather(int32_t x, int32_t z);
	[[nodiscard]] size_t BlockIndex(int32_t x, int32_t z) const;

	glm::vec2 _walker {0.0f};
	float _margin {0.0f};
	Gatherer _gatherer;
	std::vector<Obstacle> _obstacles;
	/// Each block's circles, in the order added; searched newest first
	std::vector<std::vector<uint32_t>> _blocks;
	std::vector<bool> _gathered;
	size_t _filings {0};
};

/// A leg's length: an arc's round the circle the leg before heads for, else straight
[[nodiscard]] float LengthOf(const std::vector<Leg>& legs, size_t index, const Obstacles& obstacles);
/// Gives each arc of a route the circle it goes round, so it can be followed without the obstacles
void SetRounds(std::vector<Leg>& legs, const Obstacles& obstacles);
/// Sets a leg's cost from the leg before and its own length
void SetCost(std::vector<Leg>& legs, size_t index, const Obstacles& obstacles);

/// A route being searched for, from a start to a goal, taken one step further at a time
class Search
{
public:
	enum class Status : uint8_t
	{
		Searching,
		Found,
		/// The goal's whole arrival ring is inside a circle
		Enclosed,
		/// Every way tried has come to nothing
		Failed,
	};

	/// Where a search starts, and the circles it may start on (searching back along a route), gone round the given way
	struct Start
	{
		glm::vec2 point;
		std::optional<uint32_t> circle {};
		std::optional<uint32_t> second {};
		Way way {Way::None};
	};
	/// Where a search goes: a point, stopping short of it by a distance and arriving anywhere within another, or (searching
	/// back along a route) a circle met at a point, gone round a way. Routes costing more than the most are dropped.
	struct Goal
	{
		glm::vec2 point;
		float stopShort {0.0f};
		float arriveWithin {0.0f};
		float startCost {0.0f};
		std::optional<uint32_t> circle {};
		Way way {Way::None};
		float mostCost {0.0f};
		glm::vec2 metAt {0.0f};
	};

	/// Starts a search. When recordMet is set, the things whose circles are met are noted.
	Search(Obstacles& obstacles, const Start& start, const Goal& goal, bool recordMet = false);

	/// Takes the most promising route one step further. Routes are improved by searching back along them, which nests
	/// up to a depth.
	void Step(Obstacles& obstacles, int32_t depth = 0);

	[[nodiscard]] Status GetStatus() const { return _status; }
	/// The best route's cost so far plus the straight distance it has left
	[[nodiscard]] float GetEstimate() const { return _estimate; }
	/// The route found
	[[nodiscard]] const std::vector<Leg>& GetBest() const;
	[[nodiscard]] std::vector<Leg> TakeBest();
	[[nodiscard]] size_t GetRouteCount() const { return _routes.size(); }
	[[nodiscard]] glm::vec2 GetStart() const { return _start.point; }
	[[nodiscard]] glm::vec2 GetGoal() const { return _goal.point; }
	/// The things whose circles were met, and the most a route coming back to a point already reached had cost
	[[nodiscard]] const std::vector<uint32_t>& GetMet() const { return _met; }
	[[nodiscard]] float GetMostRevisitCost() const { return _mostRevisitCost; }

private:
	struct Route
	{
		std::vector<Leg> legs;
		/// No better way back along it is looked for
		bool settled {true};
	};

	void Begin(Obstacles& obstacles);
	/// Whether a point has been reached before or the cost is too much; notes it as reached if not
	[[nodiscard]] bool Reject(glm::vec2 point, float cost);
	void Met(const Obstacles& obstacles, uint32_t index);
	[[nodiscard]] float EstimateOf(const Route& route) const;
	void Remove(size_t index);
	void Succeed(size_t index);
	void CheckArrived(size_t index);
	void Improve(Obstacles& obstacles, size_t index, int32_t depth);
	void Splice(Obstacles& obstacles, Route& route, size_t candidate, const std::vector<Leg>& back);

	Start _start;
	Goal _goal;
	bool _recordMet;
	Status _status {Status::Searching};
	/// Routes side by side; the newest is at the back and wins ties
	std::vector<Route> _routes;
	std::optional<size_t> _best;
	float _estimate {0.0f};
	std::vector<glm::vec2> _reached;
	std::vector<uint32_t> _met;
	float _mostRevisitCost {0.0f};
};

/// The land's circles round a cell: one at the centre of each of the cell and its eight neighbours that is blocked
void AddLandCircles(const WalkableLand& land, glm::ivec2 cell, Obstacles& obstacles);
} // namespace openblack::creature_route
