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
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <entt/entity/entity.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
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

/// A walker whose hugged circle went away walks no further this turn; it heads for its goal again on the next
struct HugLostThisTurn
{
};

void InitializeStep(Transform& transform, WallHug& wallHug, float angle)
{
	transform.rotation = glm::eulerAngleY(-angle - glm::radians(90.0f));
	wallHug.step = glm::vec2(glm::cos(angle), glm::sin(angle)) * wall_hug::StepMetres(wallHug.speed);
	wallHug.yAngle = angle;
}

void InitializeStepToGoal(Transform& transform, WallHug& wallHug)
{
	const auto diff = wallHug.goal - glm::xz(transform.position);
	const auto angle = glm::atan(diff.y, diff.x);
	InitializeStep(transform, wallHug, angle);
}

/// Going round a circle, the walker turns by its step over the radius each turn
void IterateStepAroundObstacle(Transform& transform, WallHug& wallHug, float radius, bool clockwise)
{
	const float turn = wall_hug::OrbitTurn(wallHug.speed, radius);
	InitializeStep(transform, wallHug, wallHug.yAngle + (clockwise ? -turn : turn));
}

/// Within a step of a point (the walker's step a turn, plus any more)
bool AreWeThere(const glm::vec2& pos, const glm::vec2& goal, float threshold)
{
	return glm::distance2(pos, goal) < threshold * threshold;
}

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

Blockers BlockersAround(const ecs::Registry& registry, glm::vec2 point)
{
	Blockers blockers;
	const auto cell = MapInterface::GetGridCell(point);
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
bool LinearScanForObstacle(entt::entity entity, const glm::vec2& pos, const glm::vec2& step)
{
	auto& registry = Locator::entitiesRegistry::value();

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

/// A walker going round its circle looks along it for what blocks it, from the cell of a point (see WallHugRules.h):
/// it may be handed over to another circle, keep going round for some turns facing round the circle, or stop hugging
/// and step straight towards its goal (then it carries on to the point it was stepping to this turn)
void SweepAroundCircle(ecs::Registry& registry, entt::entity entity, bool clockwise, Transform& transform, WallHug& wallHug,
                       WallHugObjectReference& reference, glm::vec2 point)
{
	const auto blockers = BlockersAround(registry, point);
	const auto sweep = wall_hug::SweepCircle({
	    .position = glm::xz(transform.position),
	    .goal = wallHug.goal,
	    .centre = reference.centre,
	    .radius = reference.radius,
	    .clockwise = clockwise,
	    .speed = wallHug.speed,
	    .blockers = blockers.circles,
	});
	if (sweep.outcome == wall_hug::CircleSweep::Outcome::StepThrough)
	{
		InitializeStepToGoal(transform, wallHug);
		registry.Remove<WallHugObjectReference>(entity);
		registry.AssignOrReplace<MoveStateStepThroughTag>(entity, MoveStateClockwise::Undefined, glm::vec2(0.0f));
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
		InitializeStep(transform, wallHug, *sweep.heading);
	}
}

template <MoveState S, typename... Exclude>
void StepForward(ecs::Registry& registry, Exclude... exclude)
{
	registry.Each<MoveStateTagComponent<S>, const WallHug, Transform>(
	    [](MoveStateTagComponent<S>& state, const WallHug& wallHug, const Transform& transform) {
		    const auto goal = glm::xz(transform.position) + wallHug.step;
		    state.stepGoal = goal;
	    },
	    exclude...);
}

/// Moving into another map cell, a walker heading straight for its goal looks along its way again; one going round a
/// circle looks along the circle from the cell it steps into
template <MoveState S>
void HandleCellTransition(ecs::Registry& registry)
{
	registry.Each<const MoveStateTagComponent<S>, WallHug, Transform>(
	    [&registry](entt::entity entity, const MoveStateTagComponent<S>& state, WallHug& wallHug, Transform& transform) {
		    const auto position = glm::xz(transform.position);
		    if (MapInterface::GetGridCell(position) == MapInterface::GetGridCell(state.stepGoal))
		    {
			    return;
		    }
		    if constexpr (S == MoveState::Linear)
		    {
			    InitializeStepToGoal(transform, wallHug);
			    LinearScanForObstacle(entity, position, wallHug.step);
		    }
		    else
		    {
			    if (auto* reference = registry.TryGet<WallHugObjectReference>(entity))
			    {
				    SweepAroundCircle(registry, entity, state.clockwise == MoveStateClockwise::Clockwise, transform, wallHug,
				                      *reference, state.stepGoal);
			    }
		    }
	    },
	    entt::exclude<HugLostThisTurn>);
}

// TODO(bwrsandman): Vanilla is more complex than this. Update to the map might be needed when transitioning from one block to
// the other.
template <MoveState S, typename... Exclude>
void ApplyStepGoal(ecs::Registry& registry, Exclude... exclude)
{
	registry.Each<const MoveStateTagComponent<S>, Transform>(
	    [](const MoveStateTagComponent<S>& state, Transform& transform) {
		    const float altitude = Locator::terrainSystem::value().GetHeightAt(state.stepGoal);
		    transform.position = glm::xzy(glm::vec3(state.stepGoal, altitude));
	    },
	    exclude...);
}

} // namespace

void PathfindingSystem::Update()
{
	auto& registry = Locator::entitiesRegistry::value();
	FileObstacles(registry);

	// 1.  ARRIVED:
	//         If AreWeThere is false, set to STEP_THROUGH (and it will trigger following steps)
	registry.Each<const MoveStateArrivedTag, const Transform, const WallHug>(
	    [&registry](entt::entity entity, const MoveStateArrivedTag& state, const Transform& transform, const WallHug& wallHug) {
		    if (!AreWeThere(glm::xz(transform.position), wallHug.goal, wall_hug::StepMetres(wallHug.speed)))
		    {
			    registry.SwapComponents<MoveStateStepThroughTag>(entity, state, state.clockwise);
		    }
	    });

	// 2.  LINEAR, LINEAR_CW, LINEAR_CCW
	//         If this is the first turn and there is step size defined
	registry.Each<const MoveStateLinearTag, Transform, WallHug>(
	    [](entt::entity entity, const MoveStateLinearTag&, Transform& transform, WallHug& wallHug) {
		    if (wallHug.step == glm::vec2(0.0f, 0.0))
		    {
			    InitializeStepToGoal(transform, wallHug);
			    LinearScanForObstacle(entity, glm::xz(transform.position), wallHug.step);
		    }
	    },
	    entt::exclude<WallHugObjectReference>);

	// 3.  ORBIT_CW, ORBIT_CCW, EXIT_CIRCLE_CW, EXIT_CIRCLE_CCW:
	//         A walker whose circle has gone heads for its goal again, round the same way, and stands still this turn.
	//         Collect first, then change, so we never mutate the pool being iterated for a non-current entity.
	{
		std::vector<std::pair<entt::entity, MoveStateClockwise>> lost;
		const auto collectIfLost = [&registry, &lost](entt::entity entity, MoveStateClockwise clockwise) {
			const auto* reference = registry.TryGet<WallHugObjectReference>(entity);
			if (reference == nullptr || !CircleStillThere(registry, *reference))
			{
				lost.emplace_back(entity, clockwise);
			}
		};
		registry.Each<const MoveStateOrbitTag>(
		    [&collectIfLost](entt::entity entity, const MoveStateOrbitTag& state) { collectIfLost(entity, state.clockwise); });
		registry.Each<const MoveStateExitCircleTag>([&collectIfLost](entt::entity entity, const MoveStateExitCircleTag& state) {
			collectIfLost(entity, state.clockwise);
		});
		for (const auto& [entity, clockwise] : lost)
		{
			auto& transform = registry.Get<Transform>(entity);
			auto& wallHug = registry.Get<WallHug>(entity);
			registry.Remove<MoveStateOrbitTag, MoveStateExitCircleTag>(entity);
			registry.AssignOrReplace<MoveStateLinearTag>(entity, clockwise, glm::xz(transform.position));
			registry.AssignOrReplace<HugLostThisTurn>(entity);
			InitializeStepToGoal(transform, wallHug);
			LinearScanForObstacle(entity, glm::xz(transform.position), wallHug.step);
		}
	}

	// 4a. STEP_THROUGH, EXIT_CIRCLE_CW, EXIT_CIRCLE_CCW, LINEAR without obstacles:
	//         Do StepForward and ApplyStepGoal for the step distance -> no change to state
	StepForward<MoveState::StepThrough>(registry);
	StepForward<MoveState::ExitCircle>(registry);
	ApplyStepGoal<MoveState::StepThrough>(registry);
	ApplyStepGoal<MoveState::ExitCircle>(registry);

	// 4b. FINAL_STEP, ARRIVED:
	//         Do ApplyStepGoal for the remaining distance to the goal and return a message to change LIVING STATE
	//         exclude from next parts -> no change to state
	ApplyStepGoal<MoveState::FinalStep>(registry);
	ApplyStepGoal<MoveState::Arrived>(registry);

	// 4c. ORBIT_CW, ORBIT_CCW: turn round the circle, step, and look along it again on reaching another cell or when the
	//     turns of the last look have run out. A walker that stops hugging still makes this turn's step.
	registry.Each<const MoveStateOrbitTag, const WallHugObjectReference, WallHug, Transform>(
	    [](const MoveStateOrbitTag& state, const WallHugObjectReference& reference, WallHug& wallHug, Transform& transform) {
		    IterateStepAroundObstacle(transform, wallHug, reference.radius, state.clockwise == MoveStateClockwise::Clockwise);
	    });
	StepForward<MoveState::Orbit>(registry);
	HandleCellTransition<MoveState::Orbit>(registry);
	{
		std::vector<entt::entity> runOut;
		registry.Each<const MoveStateOrbitTag, WallHugObjectReference>(
		    [&runOut](entt::entity entity, const MoveStateOrbitTag&, WallHugObjectReference& reference) {
			    if (reference.stepsAway == wall_hug::k_NoObstacleInReach)
			    {
				    return;
			    }
			    if (reference.stepsAway-- == 0)
			    {
				    runOut.push_back(entity);
			    }
		    });
		for (const auto entity : runOut)
		{
			const auto& state = registry.Get<const MoveStateOrbitTag>(entity);
			SweepAroundCircle(registry, entity, state.clockwise == MoveStateClockwise::Clockwise,
			                  registry.Get<Transform>(entity), registry.Get<WallHug>(entity),
			                  registry.Get<WallHugObjectReference>(entity), state.stepGoal);
		}
	}
	ApplyStepGoal<MoveState::Orbit>(registry);
	// Leave the circle once nearer the goal than on starting round it, with the goal ahead: head straight out from its
	// middle
	registry.Each<const MoveStateOrbitTag, WallHug, Transform, const WallHugObjectReference>(
	    [&registry](entt::entity entity, const MoveStateOrbitTag& state, WallHug& wallHug, Transform& transform,
	                const WallHugObjectReference& reference) {
		    const auto pos = glm::xz(transform.position);
		    if (AreWeThere(pos, wallHug.goal, wall_hug::StepMetres(wallHug.speed)))
		    {
			    return;
		    }
		    if (!wall_hug::LeavesCircle(pos, wallHug.goal, wallHug.step, reference.entryDistance,
		                                state.clockwise == MoveStateClockwise::Clockwise))
		    {
			    return;
		    }
		    const auto outwards = pos - reference.centre;
		    InitializeStep(transform, wallHug, glm::atan(outwards.y, outwards.x));
		    // Add exit tag, current tag stay to avoid 6. and is removed after
		    registry.Assign<MoveStateExitCircleTag>(entity, state.clockwise, state.stepGoal);
	    },
	    entt::exclude<MoveStateStepThroughTag>);

	// 4d. LINEAR, LINEAR_CW, LINEAR_CCW:
	//         Do move_to_circle_hug (complex) -> can change state to ORBIT*
	StepForward<MoveState::Linear>(registry, entt::exclude<HugLostThisTurn>);
	HandleCellTransition<MoveState::Linear>(registry);
	// Decrement turns to object, transition to orbit at 0
	registry.Each<const MoveStateLinearTag, Transform, WallHug, WallHugObjectReference>(
	    [&registry](entt::entity entity, const MoveStateLinearTag& state, Transform& transform, WallHug& wallHug,
	                WallHugObjectReference& reference) {
		    if (reference.stepsAway == wall_hug::k_NoObstacleInReach)
		    {
			    return;
		    }
		    if (reference.stepsAway-- != 0)
		    {
			    return;
		    }
		    auto clockwise = state.clockwise;
		    const auto position = glm::xz(transform.position);
		    if (clockwise == MoveStateClockwise::Undefined)
		    {
			    const auto diff = position - reference.centre;
			    // 2D cross product gives the sin between both vectors
			    const float sin = glm::cross(glm::vec3(wallHug.step, 0.0f), glm::vec3(diff, 0.0f)).z;
			    // Positive is 180 degrees clockwise, negative is 180 degrees counter-clockwise
			    clockwise = sin > 0.0f ? MoveStateClockwise::Clockwise : MoveStateClockwise::CounterClockwise;
		    }
		    // Add orbit, remove linear later; this turn's step is still the straight one
		    registry.Assign<MoveStateOrbitTag>(entity, clockwise, state.stepGoal);
		    SweepAroundCircle(registry, entity, clockwise == MoveStateClockwise::Clockwise, transform, wallHug, reference,
		                      position);
		    if (auto* hugged = registry.TryGet<WallHugObjectReference>(entity))
		    {
			    hugged->entryDistance = wall_hug::EntryDistance(glm::distance(position, wallHug.goal));
		    }
	    },
	    entt::exclude<HugLostThisTurn>);

	ApplyStepGoal<MoveState::Linear>(registry, entt::exclude<HugLostThisTurn>);
	// Clean-up: Remove those which have been transitioned
	registry.Each<const MoveStateLinearTag, const MoveStateOrbitTag>(
	    [&registry](entt::entity entity, const MoveStateLinearTag, const MoveStateOrbitTag) {
		    registry.Remove<MoveStateLinearTag>(entity);
	    });
	// Those that stopped hugging step straight towards their goal from now on
	registry.Each<const MoveStateStepThroughTag>([&registry](entt::entity entity, const MoveStateStepThroughTag&) {
		registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag>(entity);
	});

	// 5.  NOT(FINAL_STEP, ARRIVED): ** PRIOR TO ANY CHANGE OF THE ABOVE STEPS (4c):
	//         if AreWeThere(): sets to FINAL_STEP
	registry.Each<WallHug, const Transform>(
	    [&registry](entt::entity entity, WallHug& wallHug, const Transform& transform) {
		    if (AreWeThere(glm::xz(transform.position), wallHug.goal, wall_hug::StepMetres(wallHug.speed)))
		    {
			    registry.Assign<MoveStateFinalStepTag>(entity, MoveStateClockwise::Undefined, wallHug.goal);
			    registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag>(entity);
		    }
	    },
	    entt::exclude<MoveStateFinalStepTag, MoveStateArrivedTag, HugLostThisTurn>);

	// 6.  EXIT_CIRCLE_CW, EXIT_CIRCLE_CCW ** PRIOR TO ANY CHANGE OF THE ABOVE STEPS (4c):
	//         once out of the circle: set to LINEAR_(C)CW, head for the goal and look along the way again
	{
		std::vector<entt::entity> outside;
		registry.Each<const MoveStateExitCircleTag, const WallHugObjectReference, const Transform>(
		    [&registry, &outside](entt::entity entity, const MoveStateExitCircleTag&, const WallHugObjectReference& reference,
		                          const Transform& transform) {
			    if (registry.AnyOf<MoveStateOrbitTag>(entity))
			    {
				    return;
			    }
			    if (glm::distance2(glm::xz(transform.position), reference.centre) > reference.radius * reference.radius)
			    {
				    outside.push_back(entity);
			    }
		    });
		for (const auto entity : outside)
		{
			auto& transform = registry.Get<Transform>(entity);
			auto& wallHug = registry.Get<WallHug>(entity);
			const auto state = registry.Get<const MoveStateExitCircleTag>(entity);
			InitializeStepToGoal(transform, wallHug);
			registry.SwapComponents<MoveStateLinearTag>(entity, state, state.clockwise, state.stepGoal);
			LinearScanForObstacle(entity, glm::xz(transform.position), wallHug.step);
		}
	}

	// Remove leftover tag from orbit to exit circle transition
	registry.Each<const MoveStateExitCircleTag, const MoveStateOrbitTag>(
	    [&registry](entt::entity entity, const MoveStateExitCircleTag, const MoveStateOrbitTag) {
		    registry.Remove<MoveStateOrbitTag>(entity);
	    });
	std::vector<entt::entity> held;
	registry.Each<const HugLostThisTurn>([&held](entt::entity entity) { held.push_back(entity); });
	for (const auto entity : held)
	{
		registry.Remove<HugLostThisTurn>(entity);
	}
}
