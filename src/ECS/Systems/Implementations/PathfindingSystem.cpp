/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PathfindingSystem.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <unordered_set>
#include <vector>

#include <LNDFile.h>
#include <entt/entity/entity.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/WallHugRules.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{

/// Turns until a walker stepping straight to its goal aims at it again: soon after it stops hugging a circle, then
/// seldom
constexpr int8_t k_TurnsToReaimAfterHugging = 16;
constexpr int8_t k_TurnsToReaimStepping = 127;

/// Whether the circle a walker heads for or hugs is still there: water and the land's edge always are
bool CircleStillThere(const ecs::Registry& registry, const WallHugObjectReference& reference)
{
	return reference.entity == entt::null || (registry.Valid(reference.entity) && registry.AllOf<Fixed>(reference.entity));
}

/// Get neighbouring cells in this order for traversal
/// +-----+-----+-----+
/// | [5] | [4] | [7] |
/// +-----+-----+-----+
/// | [2] | [0] | [1] |
/// +-----+-----+-----+
/// | [6] | [3] | [8] |
/// +-----+-----+-----+
std::array<ecs::MapInterface::CellId, 9> GetNeighboringCells(const glm::vec2& pos)
{
	const auto cellIndex = MapInterface::GetGridCell(pos);
	assert(glm::compMin(cellIndex) > 0 && glm::all(glm::lessThan(cellIndex, MapInterface::k_GridSize - glm::u16vec2(1))));
	return {
	    cellIndex,                          // Current
	    {cellIndex.x + 1, cellIndex.y},     // Right
	    {cellIndex.x - 1, cellIndex.y},     // Left
	    {cellIndex.x, cellIndex.y + 1},     // Under
	    {cellIndex.x, cellIndex.y - 1},     // Over
	    {cellIndex.x - 1, cellIndex.y - 1}, // Over and left
	    {cellIndex.x - 1, cellIndex.y + 1}, // Under and left
	    {cellIndex.x + 1, cellIndex.y - 1}, // Over and right
	    {cellIndex.x + 1, cellIndex.y + 1}, // Under and right
	};
}

/// The walkers' obstacles in a cell, by their bounding circles
const std::unordered_set<entt::entity>& ObstaclesIn(const ecs::Registry& registry, const MapInterface::CellId& cell)
{
	static const std::unordered_set<entt::entity> k_None;
	const auto& obstacles = registry.Context().wallHugObstacles;
	const auto found = obstacles.find(static_cast<uint32_t>(cell.x + cell.y * MapInterface::k_GridSize.x));
	return found != obstacles.end() ? found->second : k_None;
}

/// Files every fixed thing in the cells whose middles its bounding circle (a unit wider) takes in
void FileObstacles(ecs::Registry& registry)
{
	auto& obstacles = registry.Context().wallHugObstacles;
	obstacles.clear();
	registry.Each<const Fixed, const Transform>(
	    [&obstacles](entt::entity entity, const Fixed& fixed, const Transform& transform) {
		    // TODO(bwrsandman): This is only in the case of a square bb underling the bounding circle (x/z) <= 1.4
		    const float radius = fixed.boundingRadius * glm::compMax(transform.scale) + 1.0f;
		    const auto min = MapInterface::GetGridCell(fixed.boundingCenter - radius);
		    const auto max = MapInterface::GetGridCell(fixed.boundingCenter + radius);
		    for (uint16_t x = min.x; x < max.x + 1; ++x)
		    {
			    for (uint16_t y = min.y; y < max.y + 1; ++y)
			    {
				    const auto cellId = MapInterface::CellId(x, y);
				    if (glm::distance2(MapInterface::GetCellCenter(cellId), fixed.boundingCenter) < radius * radius)
				    {
					    obstacles[static_cast<uint32_t>(cellId.x + cellId.y * MapInterface::k_GridSize.x)].insert(entity);
				    }
			    }
		    }
	    });
}

/// Whether a walker may go into a map cell: on the map, on land and not in water
bool ValidForTravel(int32_t x, int32_t z)
{
	const auto size = MapInterface::k_GridSize;
	if (x < 0 || z < 0 || x >= size.x || z >= size.y)
	{
		return false;
	}
	if (!Locator::terrainSystem::has_value())
	{
		return true;
	}
	const auto* cell = Locator::terrainSystem::value().FindCell({static_cast<uint16_t>(x), static_cast<uint16_t>(z)});
	return cell != nullptr && cell->properties.hasWater == 0;
}

/// The circles that may block a walker going round a circle, looked for in the cell of a point: the things in it, then
/// a circle in the middle of each of that cell and the eight round it that is water or off the land
struct Blockers
{
	std::vector<wall_hug::BlockingCircle> circles;
	std::vector<entt::entity> entities;
};

Blockers BlockersAround(const ecs::Registry& registry, glm::ivec2 coords)
{
	Blockers blockers;
	const MapInterface::CellId cell {map_coords::CellOf(coords.x), map_coords::CellOf(coords.y)};
	const auto& filed = ObstaclesIn(registry, cell);
	// In a fixed order, so every machine looks at them alike
	std::vector<entt::entity> things(filed.begin(), filed.end());
	std::ranges::sort(things);
	for (const auto thing : things)
	{
		if (registry.AnyOf<Field>(thing))
		{
			continue;
		}
		const auto& fixed = registry.Get<const Fixed>(thing);
		blockers.circles.push_back(
		    {.centre = fixed.boundingCenter, .radius = fixed.boundingRadius, .landscape = false, .landscapeOrFence = false});
		blockers.entities.push_back(thing);
	}
	constexpr std::array<glm::ivec2, 9> k_Around {
	    {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}}};
	constexpr float k_CellMetres = 10.0f;
	for (const auto& offset : k_Around)
	{
		const glm::ivec2 around = glm::ivec2(cell) + offset;
		if (!ValidForTravel(around.x, around.y))
		{
			blockers.circles.push_back({.centre = (glm::vec2(around) + 0.5f) * k_CellMetres,
			                            .radius = wall_hug::k_LandscapeBlockerRadius,
			                            .landscape = true,
			                            .landscapeOrFence = true});
			blockers.entities.push_back(entt::null);
		}
	}
	return blockers;
}

/// Iterate between all adjacent grids and find closest object that the ray (step) intersects with (circle)
/// If that object is in front (and we are not in it) and less than 256 steps away, set as target and store steps
bool LinearScanForObstacle(ecs::Registry& registry, entt::entity entity, const glm::vec2& pos, const glm::vec2& step)
{
	// Reference will be updated or removed
	registry.Remove<WallHugObjectReference>(entity);

	// FIXME(bwrsandman): This gets first, not closest
	std::optional<entt::entity> fixedEntity = std::nullopt;
	for (const auto& c : GetNeighboringCells(pos + step))
	{
		// TODO(bwrsandman): Skip if out of bounds or in water
		const auto& fixed = ObstaclesIn(registry, c);
		if (!fixed.empty())
		{
			auto iter = std::find_if(fixed.cbegin(), fixed.cend(), [&registry](const auto& f) {
				return !registry.AnyOf<Field>(f); // TODO(bwrsandman): && registry.AllOf<CollideData>();
			});
			if (iter != fixed.cend())
			{
				fixedEntity = std::make_optional(*iter);
				break;
			}
		}
	}
	if (!fixedEntity.has_value())
	{
		return false;
	}

	// Do ray-circle intersection with all objects found
	const auto& fixed = registry.Get<Fixed>(*fixedEntity);
	const auto stepSize = glm::length(step);
	if (stepSize == 0.0f)
	{
		return false;
	}
	const auto direction = step / stepSize;
	// Do a ray-circle intersection in 2d with ray = {pos, normal}, circle = {fixed.c, fixed.r} (same as ray-sphere)
	const auto oc = pos - fixed.boundingCenter;
	const auto halfB = glm::dot(oc, direction);
	const auto c = glm::length2(oc) - fixed.boundingRadius * fixed.boundingRadius;
	const float discriminant = halfB * halfB - c;
	const bool hit = discriminant > 0;

	// TODO(bwrsandman): if no hit, go through neighbouring cells, if still none, remove WallHugReference and return 1
	if (!hit)
	{
		return false;
	}

	const float t = hit ? -halfB - glm::sqrt(discriminant) : -1.0f;
	const bool inFront = t > 0.0f;

	if (!inFront)
	{
		return false;
	}

	const auto numSteps = t / stepSize;
	assert(numSteps >= 0); // t wouldn't be positive here and size should always be positive

	// Too far
	if (numSteps >= std::numeric_limits<decltype(WallHugObjectReference::stepsAway)>::max())
	{
		return false;
	}

	// Store object and number of steps away
	registry.Assign<WallHugObjectReference>(entity, WallHugObjectReference {
	                                                    .stepsAway = static_cast<uint8_t>(numSteps),
	                                                    .entity = *fixedEntity,
	                                                    .centre = fixed.boundingCenter,
	                                                    .radius = fixed.boundingRadius,
	                                                });
	return true;
}

/// The walker's walk this turn: where the walk holds it, its speed and goal in whole map units
struct Walker
{
	entt::entity entity;
	Transform& transform;
	WallHug& wallHug;
	MoveState state;
	MoveStateClockwise clockwise;
	int32_t speed;
	glm::ivec2 goal;
};

/// Where the walk holds a walker: its whole map position, taken up afresh when something else has moved it
glm::ivec2 HeldPosition(WallHug& wallHug, const Transform& transform)
{
	const auto metres = glm::xz(transform.position);
	if (metres != wallHug.placedAt)
	{
		wallHug.position = wall_hug::ToWhole(metres);
		wallHug.placedAt = metres;
	}
	return wallHug.position;
}

/// Puts the walker at a map position, on the ground
void MoveWalker(Walker& walker, glm::ivec2 position)
{
	auto& wallHug = walker.wallHug;
	wallHug.position = position;
	wallHug.placedAt = wall_hug::ToPoint(position);
	const float altitude = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(wallHug.placedAt)
	                                                           : walker.transform.position.y;
	walker.transform.position = glm::xzy(glm::vec3(wallHug.placedAt, altitude));
}

/// Faces the walker along a game angle and makes its step that way
void FaceAngle(Walker& walker, uint16_t angle)
{
	auto& wallHug = walker.wallHug;
	wallHug.gameAngle = static_cast<uint16_t>(angle & gutils::k_GameAngleMask);
	wallHug.yAngle = gutils::ConvertGameAngleTo3D(wallHug.gameAngle);
	walker.transform.rotation = glm::eulerAngleY(-wallHug.yAngle - glm::radians(90.0f));
	wallHug.step = wall_hug::StepAlong(wallHug.gameAngle, walker.speed);
}

/// Faces the walker towards its goal
void FaceGoal(Walker& walker)
{
	FaceAngle(walker, gutils::GetAngleFromXZ(walker.wallHug.position, walker.goal));
}

[[nodiscard]] bool AtGoal(const Walker& walker)
{
	return wall_hug::WithinStep(walker.wallHug.position, walker.goal, walker.speed);
}

/// Within a step of its goal the walker takes its last step onto it on the next turn
void StartFinalStep(Walker& walker)
{
	walker.state = MoveState::FinalStep;
	walker.clockwise = MoveStateClockwise::Undefined;
	walker.wallHug.step = walker.wallHug.position - walker.goal;
}

/// Looks along the walker's straight way for the circle it will meet
void ScanAhead(ecs::Registry& registry, Walker& walker)
{
	LinearScanForObstacle(registry, walker.entity, wall_hug::ToPoint(walker.wallHug.position),
	                      wall_hug::ToPoint(walker.wallHug.step));
}

/// A walker going round its circle looks along it for what blocks it, from the cell of a map position (see
/// WallHugRules.h): it may be handed over to another circle, keep going round for some turns facing round the circle,
/// or stop hugging and face its goal to step straight to it
void SweepAroundCircle(ecs::Registry& registry, Walker& walker, WallHugObjectReference& reference, glm::ivec2 coords)
{
	const auto blockers = BlockersAround(registry, coords);
	const auto sweep = wall_hug::SweepCircle({
	    .position = walker.wallHug.position,
	    .goal = walker.goal,
	    .centre = reference.centre,
	    .radius = reference.radius,
	    .clockwise = walker.clockwise == MoveStateClockwise::Clockwise,
	    .wholeSpeed = walker.speed,
	    .blockers = blockers.circles,
	});
	if (sweep.outcome == wall_hug::CircleSweep::Outcome::StepThrough)
	{
		FaceGoal(walker);
		walker.wallHug.turnsUntilStepRebuild = k_TurnsToReaimAfterHugging;
		registry.Remove<WallHugObjectReference>(walker.entity);
		walker.state = MoveState::StepThrough;
		walker.clockwise = MoveStateClockwise::Undefined;
		return;
	}
	if (sweep.hugged.has_value())
	{
		const auto& circle = blockers.circles[*sweep.hugged];
		reference.entity = blockers.entities[*sweep.hugged];
		reference.centre = circle.centre;
		reference.radius = circle.radius;
	}
	reference.stepsAway = sweep.turnsToObstacle;
	if (sweep.heading.has_value())
	{
		FaceAngle(walker, *sweep.heading);
	}
}

/// Counts down the turns to the circle ahead; true when they have run out this turn
[[nodiscard]] bool TurnsRunOut(WallHugObjectReference& reference)
{
	if (reference.stepsAway == wall_hug::k_NoObstacleInReach)
	{
		return false;
	}
	return reference.stepsAway-- == 0;
}

/// The circle a walker hugs, if it is still there
WallHugObjectReference* HuggedCircle(ecs::Registry& registry, entt::entity entity)
{
	auto* reference = registry.TryGet<WallHugObjectReference>(entity);
	return reference != nullptr && CircleStillThere(registry, *reference) ? reference : nullptr;
}

/// A walker whose circle has gone heads for its goal again, round the same way, and stands still this turn
void LoseCircle(ecs::Registry& registry, Walker& walker)
{
	FaceGoal(walker);
	walker.state = MoveState::Linear;
	ScanAhead(registry, walker);
}

/// Heading straight for its goal: on stepping into another map cell it faces its goal again and looks along its way;
/// when the turns to the circle ahead run out it starts round it
void WalkStraight(ecs::Registry& registry, Walker& walker)
{
	auto& wallHug = walker.wallHug;
	if (wallHug.step == glm::ivec2(0, 0))
	{
		// A walk just set up faces its goal and looks along its way, or is there already
		FaceGoal(walker);
		if (AtGoal(walker))
		{
			walker.state = MoveState::Arrived;
			MoveWalker(walker, walker.goal);
			return;
		}
		ScanAhead(registry, walker);
	}
	else if (auto* reference = registry.TryGet<WallHugObjectReference>(walker.entity);
	         reference != nullptr && reference->stepsAway != wall_hug::k_NoObstacleInReach &&
	         !CircleStillThere(registry, *reference))
	{
		// The circle it was heading for went away
		ScanAhead(registry, walker);
	}

	const glm::ivec2 position = wallHug.position;
	const glm::ivec2 next = position + wallHug.step;
	if (map_coords::CellOf(next.x) != map_coords::CellOf(position.x) ||
	    map_coords::CellOf(next.y) != map_coords::CellOf(position.y))
	{
		FaceGoal(walker);
		ScanAhead(registry, walker);
	}

	if (auto* reference = registry.TryGet<WallHugObjectReference>(walker.entity);
	    reference != nullptr && TurnsRunOut(*reference))
	{
		if (walker.clockwise == MoveStateClockwise::Undefined)
		{
			const bool clockwise = wall_hug::GoesRoundClockwise(position, wall_hug::ToWhole(reference->centre), wallHug.step);
			walker.clockwise = clockwise ? MoveStateClockwise::Clockwise : MoveStateClockwise::CounterClockwise;
		}
		walker.state = MoveState::Orbit;
		SweepAroundCircle(registry, walker, *reference, position);
		if (auto* hugged = registry.TryGet<WallHugObjectReference>(walker.entity))
		{
			hugged->entryDistance = wall_hug::EntryDistance(position, walker.goal);
		}
	}

	// This turn's step is the one it had before it looked
	MoveWalker(walker, next);
	if (AtGoal(walker))
	{
		StartFinalStep(walker);
	}
}

/// Going round a circle: it turns by its step over the radius, looks along the circle on stepping into another map
/// cell and when the turns of the last look run out, steps, and leaves the circle once nearer its goal than on
/// starting round it, with the goal ahead, facing straight out from the circle's middle
void WalkRound(ecs::Registry& registry, Walker& walker)
{
	auto* reference = HuggedCircle(registry, walker.entity);
	if (reference == nullptr)
	{
		LoseCircle(registry, walker);
		return;
	}
	auto& wallHug = walker.wallHug;
	const bool clockwise = walker.clockwise == MoveStateClockwise::Clockwise;
	const int32_t turn = wall_hug::OrbitTurn(walker.speed, reference->radius);
	FaceAngle(walker, static_cast<uint16_t>((wallHug.gameAngle + (clockwise ? -turn : turn)) & gutils::k_GameAngleMask));

	const glm::ivec2 position = wallHug.position;
	const glm::ivec2 next = position + wallHug.step;
	if (map_coords::CellOf(next.x) != map_coords::CellOf(position.x) ||
	    map_coords::CellOf(next.y) != map_coords::CellOf(position.y))
	{
		SweepAroundCircle(registry, walker, *reference, next);
	}
	if (auto* hugged = registry.TryGet<WallHugObjectReference>(walker.entity); hugged != nullptr && TurnsRunOut(*hugged))
	{
		SweepAroundCircle(registry, walker, *hugged, next);
	}
	MoveWalker(walker, next);

	const auto* hugged = registry.TryGet<WallHugObjectReference>(walker.entity);
	const uint32_t entryDistance = hugged != nullptr ? hugged->entryDistance : 0;
	if (AtGoal(walker))
	{
		StartFinalStep(walker);
		return;
	}
	if (hugged != nullptr && wall_hug::LeavesCircle(wallHug.position, walker.goal, wallHug.step, entryDistance, clockwise))
	{
		FaceAngle(walker, gutils::GetAngleFromXZ(wall_hug::ToWhole(hugged->centre), wallHug.position));
		walker.state = MoveState::ExitCircle;
	}
}

/// Leaving a circle: it walks straight out until outside it, then heads for its goal again
void WalkOut(ecs::Registry& registry, Walker& walker)
{
	const auto* reference = HuggedCircle(registry, walker.entity);
	if (reference == nullptr)
	{
		LoseCircle(registry, walker);
		return;
	}
	const auto centre = wall_hug::ToWhole(reference->centre);
	const float radius = reference->radius;
	MoveWalker(walker, walker.wallHug.position + walker.wallHug.step);
	if (AtGoal(walker))
	{
		StartFinalStep(walker);
		return;
	}
	if (radius * radius < wall_hug::MetresDistanceSq(centre, walker.wallHug.position))
	{
		FaceGoal(walker);
		walker.state = MoveState::Linear;
		ScanAhead(registry, walker);
	}
}

/// Stepping straight to its goal, aiming at it again now and then
void StepThrough(Walker& walker)
{
	auto& wallHug = walker.wallHug;
	// A small signed count, as the game keeps it: from 0 it goes round through -128 to 127
	wallHug.turnsUntilStepRebuild = static_cast<int8_t>(static_cast<uint8_t>(wallHug.turnsUntilStepRebuild) - 1U);
	if (wallHug.turnsUntilStepRebuild == 0)
	{
		wallHug.turnsUntilStepRebuild = k_TurnsToReaimStepping;
		FaceGoal(walker);
	}
	MoveWalker(walker, wallHug.position + wallHug.step);
	if (AtGoal(walker))
	{
		StartFinalStep(walker);
	}
}

void Walk(ecs::Registry& registry, Walker& walker)
{
	switch (walker.state)
	{
	case MoveState::FinalStep:
		MoveWalker(walker, walker.goal);
		return;
	case MoveState::Arrived:
		if (AtGoal(walker))
		{
			MoveWalker(walker, walker.goal);
			return;
		}
		// Moved off its goal: it steps back to it
		walker.wallHug.turnsUntilStepRebuild = k_TurnsToReaimAfterHugging;
		walker.state = MoveState::StepThrough;
		StepThrough(walker);
		return;
	case MoveState::StepThrough:
		StepThrough(walker);
		return;
	case MoveState::Linear:
		WalkStraight(registry, walker);
		return;
	case MoveState::Orbit:
		WalkRound(registry, walker);
		return;
	case MoveState::ExitCircle:
		WalkOut(registry, walker);
		return;
	}
}

template <MoveState S>
void CollectWalkers(ecs::Registry& registry, std::vector<Walker>& walkers)
{
	registry.Each<const MoveStateTagComponent<S>, WallHug, Transform>(
	    [&walkers](entt::entity entity, const MoveStateTagComponent<S>& tag, WallHug& wallHug, Transform& transform) {
		    walkers.push_back({
		        .entity = entity,
		        .transform = transform,
		        .wallHug = wallHug,
		        .state = S,
		        .clockwise = tag.clockwise,
		        .speed = wall_hug::WholeSpeed(wallHug.speed),
		        .goal = wall_hug::ToWhole(wallHug.goal),
		    });
	    });
}

template <MoveState S>
void SetTag(ecs::Registry& registry, const Walker& walker)
{
	registry.AssignOrReplace<MoveStateTagComponent<S>>(walker.entity, walker.clockwise,
	                                                   wall_hug::ToPoint(walker.wallHug.position));
}

/// Gives the walker the tag of the state it is now in, and takes the others off
void Retag(ecs::Registry& registry, const Walker& walker)
{
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(walker.entity);
	switch (walker.state)
	{
	case MoveState::Linear:
		SetTag<MoveState::Linear>(registry, walker);
		break;
	case MoveState::Orbit:
		SetTag<MoveState::Orbit>(registry, walker);
		break;
	case MoveState::ExitCircle:
		SetTag<MoveState::ExitCircle>(registry, walker);
		break;
	case MoveState::StepThrough:
		SetTag<MoveState::StepThrough>(registry, walker);
		break;
	case MoveState::FinalStep:
		SetTag<MoveState::FinalStep>(registry, walker);
		break;
	case MoveState::Arrived:
		SetTag<MoveState::Arrived>(registry, walker);
		break;
	}
}

} // namespace

void PathfindingSystem::Update()
{
	auto& registry = Locator::entitiesRegistry::value();
	FileObstacles(registry);

	// Every walker takes its turn's walk on its own, in the state it is in
	std::vector<Walker> walkers;
	CollectWalkers<MoveState::Arrived>(registry, walkers);
	CollectWalkers<MoveState::FinalStep>(registry, walkers);
	CollectWalkers<MoveState::StepThrough>(registry, walkers);
	CollectWalkers<MoveState::Linear>(registry, walkers);
	CollectWalkers<MoveState::Orbit>(registry, walkers);
	CollectWalkers<MoveState::ExitCircle>(registry, walkers);
	for (auto& walker : walkers)
	{
		HeldPosition(walker.wallHug, walker.transform);
		Walk(registry, walker);
		Retag(registry, walker);
	}
}
