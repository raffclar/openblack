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
#include <span>
#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureRoute.h"
#include "Creature/CreatureRouteSearch.h"

/// How the game lays a footpath in play. The creatures' route search finds a way round what stands between the two
/// ends, and the route is turned into the path's nodes: one at the end of each straight leg, and nodes stepped round
/// each circle it goes round. Places are whole map units (a 65536th of a ten metre cell); the search works in metres.
/// The maths is in floats, in the game's order. Pure.
namespace openblack::ecs::footpath_planning
{

/// A path planned at once is given this many steps of the search, and one planned over turns this many steps a turn
constexpr int32_t k_MostStepsAtOnce = 128;
constexpr int32_t k_StepsPerTurn = 20;
/// A path's end is moved out of its building's circles grown by this margin, and then by a little more
constexpr float k_EndClearMargin = 0.5f;
constexpr float k_EndClearExtra = 0.1f;
/// An end exactly on its building's place first moves this far towards the other end
constexpr float k_OffMiddleStep = 0.01f;

/// A place in metres, and a point in whole map units (cut towards nothing)
[[nodiscard]] glm::vec2 ToPoint(glm::ivec2 place);
[[nodiscard]] glm::ivec2 ToPlace(glm::vec2 point);

/// The straight distance between two points
[[nodiscard]] float Range(glm::vec2 from, glm::vec2 to);
/// The most a route between two points may cost: the straight distance and one, five times
[[nodiscard]] float MostCost(glm::vec2 from, glm::vec2 to);

/// The places of the nodes of a route found from a start to a goal, from the start to the goal. Each straight leg gives a
/// node at its end, except the last leg, whose end is the goal. Each arc gives a node where it begins, nodes stepped
/// round its circle a tenth of a half turn at a time while still farther from where it leaves than about a third of the
/// radius, and a node where it leaves.
[[nodiscard]] std::vector<glm::ivec2> RoutePlaces(std::span<const creature_route::Leg> legs,
                                                  std::span<const creature_route::Obstacle> circles, glm::vec2 start,
                                                  glm::vec2 goal);

/// A node as the planning sees it
struct PlanNode
{
	glm::ivec2 place {0, 0};
	/// Laid by a plan, and may be planned again
	bool planned {false};
	/// Under something put down on the path
	bool hidden {false};
	/// The place of the thing that a stretch of the path was sent round
	bool marker {false};

	bool operator==(const PlanNode&) const = default;
};

/// A path with the nodes between two of its nodes laid again along a route: the nodes between that are hidden or are
/// markers are kept, after the route, and the others go. The place of the thing that the stretch is sent round, if
/// given, comes right after the route as a marker.
[[nodiscard]] std::vector<PlanNode> LayBetween(std::span<const PlanNode> nodes, size_t first, size_t last,
                                               std::span<const glm::ivec2> route, std::optional<glm::ivec2> marker);

/// A path laid between two things is settled: no node of it that walkers use may be planned again
void Settle(std::span<PlanNode> nodes);

/// Where a path's end is put when its building stands there: an end exactly on the building's place first moves a little
/// towards the other end; then, circle by circle, an end inside one (grown by a little more) moves out along the line
/// from the building's place through it to the circle's edge
[[nodiscard]] glm::vec2 ClearOfBuilding(glm::vec2 end, glm::vec2 buildingPlace, glm::vec2 otherEnd,
                                        std::span<const creature_route::Circle> circles);

} // namespace openblack::ecs::footpath_planning
