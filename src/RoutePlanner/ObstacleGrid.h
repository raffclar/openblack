/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>

#include "Point2D.h"

// The LH route planner's obstacle set. A 64 x 64 grid of 80 m squares over the 512 x 512 cell map; each square keeps
// a chain of the obstacle circles that touch it. The world fills
// a square the first time a search reads it (the FillSquareFn installed with InstallCallbacks), so the chains' order
// follows the search: it must stay lazy. No ECS here: ecs::route_plan_world is the world side.

namespace openblack::route_planner
{
class ObstacleGrid;
class RouteFollower;

/// One obstacle: the object's id (-1 anonymous), active (0 once a later circle swallowed it), the centre and the
/// radius
struct RouteObstacle
{
	int32_t id {-1};
	int32_t active {0};
	Point2D c;
	float r {0.0f};

	/// k = (this.r - r) - 0.001; k >= 0 and k^2 > |p - c|^2
	[[nodiscard]] bool ContainsCircle(const Point2D& p, float radius) const;
};

/// The square grid's two callbacks (InstallCallbacks): the world's check for each map cell of a square, and the
/// special objects (the creature's)
using FillSquareFn = void (*)(int32_t cellX, int32_t cellZ, ObstacleGrid& holder);
using SpecialObjectsFn = void (*)(ObstacleGrid& holder);
/// The search's obstacle hook: installed by RouteFollower's update (with the creature as its context) for that update's
/// search turns only, and cleared at its end
using ObstacleHook = void (*)(void* context, int32_t objectId);

class ObstacleGrid
{
public:
	static constexpr int32_t k_MaxObjects = 0x4000;
	static constexpr int32_t k_MaxChains = 0x4000;
	static constexpr int32_t k_Squares = 64;
	static constexpr float k_SquareSize = 80.0f;
	static constexpr int16_t k_End = -1; ///< 0xFFFF

	/// Installs the square grid's callbacks for every holder
	static void InstallCallbacks(FillSquareFn fill, SpecialObjectsFn special);
	static void SetObstacleHook(ObstacleHook hook, void* context);
	/// The hook, when installed: hook(context, id) for an object id != -1 (SetDest, StepSearch)
	static void NotifyObstacle(int32_t objectId);
	[[nodiscard]] static bool HasObstacleHook();

	/// No plan, then Empty
	ObstacleGrid();
	/// No object, no chain, no dest / start, margin 0, every square unfilled with an empty chain
	void Empty();

	/// The margin an object adds to every circle it puts into the route plan (0 after Empty)
	void SetMargin(float margin) { _margin = margin; }
	[[nodiscard]] float GetMargin() const { return _margin; }
	/// SetDest's last argument: the plan's dest and start, which AddObject keeps outside
	void SetEnds(const Point2D* dest, const Point2D* start)
	{
		_dest = dest;
		_start = start;
	}
	/// The follower this holder is (RouteFollower's ctor); nullptr for the footpaths' holders
	void SetPlan(RouteFollower* plan) { _plan = plan; }
	[[nodiscard]] RouteFollower* GetPlan() const { return _plan; }

	/// Adds an obstacle circle
	void AddObject(int32_t id, const Point2D& c, float r, int32_t notify);
	/// True when no chain entry of the cell's square is `id`
	[[nodiscard]] bool IsNotInSquare(int32_t id, int32_t cellX, int32_t cellZ) const;

	/// The tangent point from p to obj's circle
	int32_t StartObstacleSidePoint(int32_t obj, const Point2D& p, Point2D& out, int32_t side) const;
	/// The tangent between two circles
	int32_t GetTangent(int32_t objA, int32_t sideA, Point2D& outA, int32_t objB, int32_t sideB, Point2D& outB) const;
	/// The first obstacle on from -> to, or -1
	int32_t GetFirstObject(const Point2D& from, Point2D& to, int32_t ignore, Point2D& hit, int32_t& side, float r);
	/// 1 when another circle cuts the arc start -> end round obj
	int32_t ArcBlocked(int32_t obj, int32_t side, Point2D& end, const Point2D& start, int32_t& hit);

	[[nodiscard]] const RouteObstacle& Object(int32_t index) const { return _objects.at(static_cast<size_t>(index)); }
	[[nodiscard]] int32_t ObjectCount() const { return _objectCount; }

private:
	struct Square
	{
		uint16_t filled {0};
		int16_t head {k_End};
	};
	struct Chain
	{
		int16_t object {0};
		int16_t next {k_End};
	};

	/// The square marked filled, then the fill callback for its 8 x 8 cells (z outer, x inner)
	void FillSquare(int32_t squareX, int32_t squareZ);
	/// The new entry into the chains of every square its circle's box covers
	void Link(const RouteObstacle& entry, int32_t index);
	Square& SquareAt(int32_t x, int32_t z) { return _squares.at(static_cast<size_t>(z * k_Squares + x)); }

	std::array<RouteObstacle, k_MaxObjects> _objects {};
	int32_t _objectCount {0};
	const Point2D* _dest {nullptr};
	const Point2D* _start {nullptr};
	float _margin {0.0f};
	RouteFollower* _plan {nullptr};
	std::array<Square, k_Squares * k_Squares> _squares {};
	std::array<Chain, k_MaxChains> _chains {};
	int32_t _chainCount {0};
};
} // namespace openblack::route_planner
