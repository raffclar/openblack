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
#include <vector>

#include <LNDFile.h>
#include <entt/entity/entity.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/ThingCircles.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerNeeds.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/VillagerWalkRules.h"
#include "ECS/WallHugRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

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

/// Whether the circle a walker heads for or hugs is still there: water and the land's edge always are; a thing's is
/// while the thing stands on the map where it stood, with that circle (a thing that is picked up or moved has a new
/// shape when it is put down)
bool CircleStillThere(const ecs::Registry& registry, const WallHugObjectReference& reference)
{
	if (reference.entity == entt::null)
	{
		return true;
	}
	if (!registry.Valid(reference.entity))
	{
		return false;
	}
	const auto* resident = registry.TryGet<const MapCellResident>(reference.entity);
	if (resident == nullptr || resident->cells.empty())
	{
		return false;
	}
	const auto circles = thing_circles::CirclesOf(registry, reference.entity);
	return std::ranges::any_of(circles, [&reference](const wall_hug::BlockingCircle& circle) {
		return circle.centre == reference.centre && circle.radius == reference.radius;
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

/// The circles that may block a walker, looked for in the cell of a map position as the game comes across them: the
/// circles of the things in it, newest first, then a circle in the middle of each of that cell and the eight round it
/// that is water or off the land
struct Blockers
{
	std::vector<wall_hug::BlockingCircle> circles;
	std::vector<entt::entity> entities;
};

Blockers BlockersAround(const ecs::Registry& registry, glm::ivec2 coords)
{
	Blockers blockers;
	const glm::ivec2 cell {map_coords::CellOf(coords.x), map_coords::CellOf(coords.y)};
	if (Locator::entitiesMap::has_value())
	{
		for (const auto thing : Locator::entitiesMap::value().GetFixedInGridCell(MapInterface::CellId(cell)))
		{
			for (const auto& circle : thing_circles::CirclesOf(registry, thing))
			{
				blockers.circles.push_back(circle);
				blockers.entities.push_back(thing);
			}
		}
	}
	constexpr std::array<glm::ivec2, 9> k_Around {
	    {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}}};
	constexpr float k_CellMetres = 10.0f;
	for (const auto& offset : k_Around)
	{
		const glm::ivec2 around = cell + offset;
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

/// Every circle that may stand round a map position: those of the things in its cell and the eight round it, and the
/// water and the land's edge round its cell
std::vector<wall_hug::BlockingCircle> CirclesNear(const ecs::Registry& registry, glm::ivec2 coords)
{
	auto circles = BlockersAround(registry, coords).circles;
	std::erase_if(circles, [](const wall_hug::BlockingCircle& circle) { return !circle.landscape; });
	if (!Locator::entitiesMap::has_value())
	{
		return circles;
	}
	const glm::ivec2 cell {map_coords::CellOf(coords.x), map_coords::CellOf(coords.y)};
	std::vector<entt::entity> seen;
	for (int dz = -1; dz <= 1; ++dz)
	{
		for (int dx = -1; dx <= 1; ++dx)
		{
			const glm::ivec2 around = cell + glm::ivec2(dx, dz);
			if (around.x < 0 || around.y < 0 || around.x >= MapInterface::k_GridSize.x ||
			    around.y >= MapInterface::k_GridSize.y)
			{
				continue;
			}
			for (const auto thing : Locator::entitiesMap::value().GetFixedInGridCell(MapInterface::CellId(around)))
			{
				if (std::ranges::find(seen, thing) != seen.end())
				{
					continue;
				}
				seen.push_back(thing);
				const auto shape = thing_circles::CirclesOf(registry, thing);
				circles.insert(circles.end(), shape.begin(), shape.end());
			}
		}
	}
	return circles;
}

/// A walker heading straight on looks along its step for the nearest circle in the cell of a map position (see
/// WallHugRules.h), and heads for it, or for nothing in reach
void ScanLine(ecs::Registry& registry, entt::entity entity, glm::ivec2 position, glm::ivec2 step, glm::ivec2 coords)
{
	registry.Remove<WallHugObjectReference>(entity);
	const auto blockers = BlockersAround(registry, coords);
	const auto scan = wall_hug::ScanLine(position, step, blockers.circles);
	if (!scan.circle.has_value())
	{
		return;
	}
	const auto& circle = blockers.circles[*scan.circle];
	registry.Assign<WallHugObjectReference>(entity, WallHugObjectReference {
	                                                    .stepsAway = scan.turnsToObstacle,
	                                                    .entity = blockers.entities[*scan.circle],
	                                                    .centre = circle.centre,
	                                                    .radius = circle.radius,
	                                                });
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
	/// Whether it came up against water, the land's edge or a fence going round this turn
	bool needsLookahead {false};
	/// A stand-in on a trial walk, never seen: it isn't put on the ground
	bool standIn {false};
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
	const float altitude = Locator::terrainSystem::has_value() && !walker.standIn
	                           ? Locator::terrainSystem::value().GetHeightAt(wallHug.placedAt)
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

/// Looks along the walker's straight way for the circle it will meet, among those in the cell of a map position: where
/// it stands, unless it is about to step into another cell
void ScanAhead(ecs::Registry& registry, Walker& walker, std::optional<glm::ivec2> coords = std::nullopt)
{
	ScanLine(registry, walker.entity, walker.wallHug.position, walker.wallHug.step, coords.value_or(walker.wallHug.position));
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
	if (sweep.hugged.has_value())
	{
		walker.needsLookahead = sweep.needsLookahead;
	}
	if (sweep.outcome == wall_hug::CircleSweep::Outcome::StepThrough)
	{
		FaceGoal(walker);
		walker.wallHug.turnsUntilStepRebuild = k_TurnsToReaimAfterHugging;
		// It no longer hugs anything, so it no longer waits for or keeps a trial of both ways round
		registry.Remove<WallHugObjectReference, WallHugLookahead>(walker.entity);
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
		ScanAhead(registry, walker, next);
	}

	if (auto* reference = registry.TryGet<WallHugObjectReference>(walker.entity);
	    reference != nullptr && TurnsRunOut(*reference))
	{
		// Reaching water or the land's edge, it will try both ways round
		if (reference->entity == entt::null)
		{
			walker.needsLookahead = true;
		}
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
		// Leaving the circle, it may try both ways round again at the next
		if (const auto* lookahead = registry.TryGet<const WallHugLookahead>(walker.entity);
		    lookahead != nullptr && lookahead->tried)
		{
			registry.Remove<WallHugLookahead>(walker.entity);
		}
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

/// The walker's walk this turn; true when it has got there, standing on its goal
bool Walk(ecs::Registry& registry, Walker& walker)
{
	switch (walker.state)
	{
	case MoveState::FinalStep:
		MoveWalker(walker, walker.goal);
		return true;
	case MoveState::Arrived:
		if (AtGoal(walker))
		{
			MoveWalker(walker, walker.goal);
			return true;
		}
		// Moved off its goal: it steps back to it
		walker.wallHug.turnsUntilStepRebuild = k_TurnsToReaimAfterHugging;
		walker.state = MoveState::StepThrough;
		StepThrough(walker);
		return false;
	case MoveState::StepThrough:
		StepThrough(walker);
		return false;
	case MoveState::Linear:
		WalkStraight(registry, walker);
		return false;
	case MoveState::Orbit:
		WalkRound(registry, walker);
		return false;
	case MoveState::ExitCircle:
		WalkOut(registry, walker);
		return false;
	}
	return false;
}

template <MoveState S>
void CollectWalkers(ecs::Registry& registry, std::vector<Walker>& walkers)
{
	registry.Each<const MoveStateTagComponent<S>, WallHug, Transform>(
	    [&walkers, &registry](entt::entity entity, const MoveStateTagComponent<S>& tag, WallHug& wallHug,
	                          Transform& transform) {
		    // A villager's walk waits while it is in a state that doesn't walk it
		    if (const auto* action = registry.TryGet<const LivingAction>(entity);
		        action != nullptr && registry.AllOf<Villager>(entity) &&
		        !villager_walk::Walks(
		            static_cast<VillagerStates>(action->states.at(static_cast<size_t>(LivingAction::Index::Top)))))
		    {
			    return;
		    }
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

/// The walker's walk as it stands, from the walk state it is in; one not walking is taken as there
Walker WalkerOf(ecs::Registry& registry, entt::entity entity, WallHug& wallHug, Transform& transform)
{
	Walker walker {.entity = entity,
	               .transform = transform,
	               .wallHug = wallHug,
	               .state = MoveState::Arrived,
	               .clockwise = MoveStateClockwise::Undefined,
	               .speed = wall_hug::WholeSpeed(wallHug.speed),
	               .goal = wall_hug::ToWhole(wallHug.goal)};
	const auto take = [&registry, &walker]<MoveState S>() {
		if (const auto* tag = registry.TryGet<const MoveStateTagComponent<S>>(walker.entity); tag != nullptr)
		{
			walker.state = S;
			walker.clockwise = tag->clockwise;
			return true;
		}
		return false;
	};
	[[maybe_unused]] const bool walking =
	    take.template operator()<MoveState::Linear>() || take.template operator()<MoveState::Orbit>() ||
	    take.template operator()<MoveState::ExitCircle>() || take.template operator()<MoveState::StepThrough>() ||
	    take.template operator()<MoveState::FinalStep>() || take.template operator()<MoveState::Arrived>();
	return walker;
}

/// Turns a walker round, the other way round what it goes round or heads past: it jumps three steps on and faces back
/// (see WallHugRules.h); going round a circle it looks along it again the new way. A walker heading straight for its
/// goal with no side to keep, or not going round anything, stays as it is.
void TurnRound(ecs::Registry& registry, Walker& walker)
{
	switch (walker.state)
	{
	case MoveState::Linear:
		if (walker.clockwise == MoveStateClockwise::Undefined)
		{
			return;
		}
		break;
	case MoveState::Orbit:
	case MoveState::ExitCircle:
		break;
	case MoveState::StepThrough:
	case MoveState::FinalStep:
	case MoveState::Arrived:
		return;
	}
	walker.clockwise = walker.clockwise == MoveStateClockwise::Clockwise ? MoveStateClockwise::CounterClockwise
	                                                                     : MoveStateClockwise::Clockwise;
	auto& wallHug = walker.wallHug;
	const auto turned = wall_hug::TurnRound(wallHug.position, wallHug.step, wallHug.gameAngle);
	MoveWalker(walker, turned.position);
	wallHug.step = turned.step;
	// Only its walking direction turns: it is drawn facing as before until it is next turned
	wallHug.gameAngle = turned.gameAngle;
	if (walker.state == MoveState::Orbit)
	{
		if (auto* reference = HuggedCircle(registry, walker.entity); reference != nullptr)
		{
			SweepAroundCircle(registry, walker, *reference, wallHug.position);
		}
	}
}

/// A trial of the walker's walk by a stand-in villager made where it stands, as the game makes one (with the drawing of
/// lots, the counting of things made and the name that making any villager may take): a grown-up woman of the first
/// kind of villager, going four times the walker's speed at her own pace. She walks the walker's walk, as it goes or
/// turned round, until she gets there or her turns run out, then is taken away.
wall_hug::TrialWalk TrialWalk(ecs::Registry& registry, const Walker& walker, bool turnedRound)
{
	constexpr auto k_StandInKind = VillagerInfo::CelticHousewifeFemale;
	const auto standIn =
	    archetypes::VillagerArchetype::Create(walker.transform.position, walker.transform.position, k_StandInKind,
	                                          wall_hug::k_TrialWalkerAge, archetypes::VillagerArchetype::Joins::Nobody);
	const auto& person = registry.Get<const Villager>(standIn);
	const auto& info = Locator::infoConstants::value().villager.at(static_cast<size_t>(k_StandInKind));
	const float pace = villager_speed::PersonalFactor({
	    .creationIndex = person.creationIndex,
	    .age = villager_age::AgeNow(person),
	    .grownUpAge = info.grownUpAge,
	    .oldAge = info.oldAge,
	    .desireForFood = villager_needs::DesireForFood(person.food),
	    .life = person.life,
	    .female = info.sex == SexType::Female,
	});

	auto& wallHug = registry.Get<WallHug>(standIn);
	wallHug.goal = walker.wallHug.goal;
	wallHug.step = walker.wallHug.step;
	wallHug.gameAngle = walker.wallHug.gameAngle;
	wallHug.turnsUntilStepRebuild = 0;
	wallHug.position = walker.wallHug.position;
	wallHug.placedAt = walker.wallHug.placedAt;
	if (const auto* reference = registry.TryGet<const WallHugObjectReference>(walker.entity); reference != nullptr)
	{
		registry.Assign<WallHugObjectReference>(standIn, *reference);
	}
	Walker trial {.entity = standIn,
	              .transform = registry.Get<Transform>(standIn),
	              .wallHug = wallHug,
	              .state = walker.state,
	              .clockwise = walker.clockwise,
	              .speed = villager_speed::FinalSpeed(walker.speed * wall_hug::k_TrialSpeedUp, pace),
	              .goal = walker.goal,
	              .standIn = true};

	wall_hug::TrialWalk result {.turnsToSpare = 0, .distanceSq = std::numeric_limits<float>::max()};
	if (turnedRound)
	{
		TurnRound(registry, trial);
	}
	// Turned round straight towards its goal, it no longer goes round anything: no trial
	if (!turnedRound || trial.state != MoveState::StepThrough)
	{
		result.turnsToSpare = wall_hug::TurnsToSpare([&registry, &trial]() { return Walk(registry, trial); });
		result.distanceSq = wall_hug::MetresDistanceSq(trial.goal, trial.wallHug.position);
	}
	registry.Destroy(standIn);
	return result;
}

} // namespace

void PathfindingSystem::Update()
{
	auto& registry = Locator::entitiesRegistry::value();

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
		// One that came up against water, the land's edge or a fence waits for its trial of both ways round, unless it
		// has had it
		if (walker.needsLookahead && !registry.AllOf<WallHugLookahead>(walker.entity))
		{
			registry.Assign<WallHugLookahead>(walker.entity);
		}
	}
}

void PathfindingSystem::ProcessLookahead()
{
	auto& registry = Locator::entitiesRegistry::value();
	// The waiting walker that comes first
	std::optional<entt::entity> next;
	registry.Each<const WallHugLookahead>([&next](entt::entity entity, const WallHugLookahead& lookahead) {
		if (!lookahead.tried && (!next.has_value() || entt::to_integral(entity) < entt::to_integral(*next)))
		{
			next = entity;
		}
	});
	if (!next.has_value())
	{
		return;
	}
	auto* lookahead = &registry.Get<WallHugLookahead>(*next);
	lookahead->tried = true;
	auto* wallHug = registry.TryGet<WallHug>(*next);
	auto* transform = registry.TryGet<Transform>(*next);
	if (wallHug == nullptr || transform == nullptr || !Locator::infoConstants::has_value())
	{
		return;
	}
	auto walker = WalkerOf(registry, *next, *wallHug, *transform);
	const auto asItGoes = TrialWalk(registry, walker, false);
	const auto turnedRound = TrialWalk(registry, walker, true);
	// The stand-ins came and went: look the walker's trial up again
	lookahead = &registry.Get<WallHugLookahead>(*next);
	lookahead->spareAsItGoes = asItGoes.turnsToSpare;
	lookahead->spareTurnedRound = turnedRound.turnsToSpare;
	// Turned round, it would jump to where it lands: never into a ring of overlapping circles that shuts it in, where
	// it would loop for good (a deviation from the game, which turns it round all the same)
	const auto landing = wall_hug::TurnRound(wallHug->position, wallHug->step, wallHug->gameAngle).position;
	lookahead->landingShutIn = wall_hug::ShutInByCircles(wall_hug::ToPoint(landing), CirclesNear(registry, landing));
	lookahead->turnedRound = wall_hug::TurnedRoundIsBetter(turnedRound, asItGoes) && !lookahead->landingShutIn;
	if (lookahead->turnedRound)
	{
		TurnRound(registry, walker);
		Retag(registry, walker);
		return;
	}
	// Going round the way it goes, it faces round its circle afresh
	if (const auto* reference = registry.TryGet<const WallHugObjectReference>(*next);
	    walker.state == MoveState::Orbit && reference != nullptr)
	{
		FaceAngle(walker, wall_hug::HeadingRound(wallHug->position, wall_hug::ToWhole(reference->centre),
		                                         walker.clockwise == MoveStateClockwise::Clockwise));
	}
}
