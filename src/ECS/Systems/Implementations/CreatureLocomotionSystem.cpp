/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureLocomotionSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat3x3.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandAvoid.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "RoutePlanner/RouteFollower.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
namespace locomotion = openblack::creature_locomotion;
using Motion = CreatureLocomotion::Motion;
using FollowerState = openblack::route_planner::RouteFollower::State;

namespace
{
/// A game turn's milliseconds and seconds, as the game's tick time sets them
float TurnMs()
{
	return static_cast<float>(game_clock::MsPerTurn());
}
float TurnSeconds()
{
	return TurnMs() / 1000.0f;
}
/// The follower moves a tenth of its speed at each step along its route
constexpr float k_FollowerStepSeconds = 0.1f;
/// Search turns the route follower takes a game turn
constexpr int32_t k_SearchTurnsPerUpdate = 64;
/// The longest route the follower plans, as many times the way there (plus one) as the footpaths allow theirs
constexpr float k_PlanLengthFactor = 5.0f;
/// A new destination this close to the current one carries on the same walk
constexpr float k_SameDestination = 0.1f;
/// Fidgets while waiting for a route: the first comes after this long, a look about is followed by a longer wait, and
/// a puzzled face is held this long
constexpr float k_FidgetMs = 3000.0f;
constexpr float k_LookAboutFidgetMs = 6000.0f;
constexpr float k_PuzzledMs = 2000.0f;
constexpr size_t k_PuzzledFace = creature_layers::animations::k_FirstFace + 6;
/// Following something, the walk is pointed anew once it has moved this far from where the walk heads
constexpr float k_FollowRetarget = 1.0f;
/// Running away, a creature goes up to this much further than its species' distance, and looks this far round the
/// point for somewhere to stand
constexpr uint32_t k_RunAwayExtra = 40;
constexpr float k_RunAwaySearch = 30.0f;
/// Arriving where it ran away to, anywhere within this
constexpr float k_RunAwayArrival = 5.0f;
/// Looking for somewhere to stand after straying off where it can
constexpr float k_StraySearch = 1000.0f;
/// What is known of a species without the game's creature tables: the ape's speeds
constexpr float k_DefaultSlowFraction = 0.5f;
constexpr float k_DefaultWalkFraction = 0.7f;
constexpr float k_DefaultRunFraction = 0.9f;
constexpr float k_DefaultRunAwayDistance = 50.0f;
/// Turning or stepping, the steps never move it less than this a turn
constexpr float k_Tiny = 1e-4f;

struct Fractions
{
	float slow;
	float walk;
	float run;
	float runAway;
};

Fractions FractionsOf(CreatureType species)
{
	if (Locator::infoConstants::has_value())
	{
		const auto& creatures = Locator::infoConstants::value().creature;
		const auto row = creature::InfoRow(species);
		if (row < creatures.size())
		{
			const auto& info = creatures.at(row);
			return {.slow = info.slowSpeed, .walk = info.walkSpeed, .run = info.runSpeed, .runAway = info.runAwayDistance};
		}
	}
	return {.slow = k_DefaultSlowFraction,
	        .walk = k_DefaultWalkFraction,
	        .run = k_DefaultRunFraction,
	        .runAway = k_DefaultRunAwayDistance};
}

/// The species' animations, as its base mesh has them: the blends follow the base's lengths and moves
struct Moves
{
	const CreatureRig* rig {nullptr};

	[[nodiscard]] const skeletal_animation::Animation* Get(size_t index) const
	{
		if (rig == nullptr)
		{
			return nullptr;
		}
		const auto* animation = rig->GetAnimation(CreatureRig::Mesh::Base, index);
		return animation != nullptr && !animation->frames.empty() ? animation : nullptr;
	}
	[[nodiscard]] bool Has(size_t index) const { return Get(index) != nullptr; }
	[[nodiscard]] float Duration(size_t index) const
	{
		const auto* animation = Get(index);
		return animation != nullptr ? static_cast<float>(std::max(animation->duration, 1u)) : 1.0f;
	}
	[[nodiscard]] glm::vec2 Displacement(size_t index) const
	{
		const auto* animation = Get(index);
		return animation != nullptr ? glm::vec2(animation->displacement.x, animation->displacement.z) : glm::vec2(0.0f);
	}
	[[nodiscard]] locomotion::Cycle Cycle(size_t index) const
	{
		return {.durationMs = Duration(index), .stride = glm::length(Displacement(index))};
	}
	[[nodiscard]] bool CanWalk() const { return Has(locomotion::animations::k_Walk) && Has(locomotion::animations::k_Run); }
	[[nodiscard]] bool HasAll(size_t first) const { return Has(first) && Has(first + 1) && Has(first + 2); }
	[[nodiscard]] float PairDuration(const locomotion::Pair& pair) const
	{
		return Duration(pair.from) + ((Duration(pair.to) - Duration(pair.from)) * pair.weight);
	}
	[[nodiscard]] glm::vec2 PairDisplacement(const locomotion::Pair& pair) const
	{
		return Displacement(pair.from) + ((Displacement(pair.to) - Displacement(pair.from)) * pair.weight);
	}
};

Moves MovesOf(CreatureType species)
{
	if (!Locator::resources::has_value())
	{
		return {};
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(species);
	return {.rig = rigs.Contains(id) ? &*rigs.Handle(id) : nullptr};
}

float HeightAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

/// Whether a creature can stand at a point: k_CreatureRadius for a destination, k_CreatureTurningRadius for where it
/// is as it walks
bool IsValid(glm::vec2 point, float radius)
{
	return land_avoid::IsPosValid(glm::vec3(point.x, 0.0f, point.y), radius);
}

/// The creature's size and speeds this turn
void Measure(CreatureLocomotion& self, const Creature& creature, const Transform& transform)
{
	self.speeds = locomotion::SpeedsFor(creature.size);
	self.scale = std::abs(transform.scale.x);
	if (!Locator::resources::has_value())
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto meshId = creature::GetIdFromType(creature.species, creature::CreatureBody::Appearance::Base);
	if (meshes.Contains(meshId))
	{
		// (approximate) its radius is the mean of its mesh's half width and half length; the game's own radius is not
		// read yet
		const auto size = meshes.Handle(meshId)->GetBoundingBox().Size();
		self.radius = self.scale * (size.x + size.z) * 0.25f;
	}
}

void SetIdle(CreatureLocomotion& self)
{
	self.motion = Motion::Standing;
	self.speed = 0.0f;
	self.move.reset();
	self.tracks.clear();
}

/// The follower is kept while it is idle, for the next move; one that is planning or following a route is dropped, so
/// the next move starts with a fresh one
void DropRoute(CreatureLocomotion& self)
{
	if (self.follower != nullptr && self.follower->GetState() != FollowerState::Idle)
	{
		self.follower.reset();
	}
	self.routeReady = false;
	self.planEnd = -1;
}

void ClearWalk(CreatureLocomotion& self)
{
	SetIdle(self);
	DropRoute(self);
	self.destination.reset();
	self.facingOnly = false;
}

glm::vec2 PositionOf(const CreatureLocomotion& self)
{
	return glm::xz(self.toPosition);
}

void PlaceAt(CreatureLocomotion& self, glm::vec2 point)
{
	self.toPosition = glm::vec3(point.x, HeightAt(point), point.y);
}

/// Put back at a point outside a walk: its Transform goes there at once and it is drawn there with no slide
void SnapTo(entt::entity creature, CreatureLocomotion& self, glm::vec2 point)
{
	PlaceAt(self, point);
	self.fromPosition = self.toPosition;
	ecs::creature_pose::CommitTurnPose(creature, self.toPosition, self.heading);
	ecs::NotifyTeleported(creature);
}

/// The follower reports how its plan ended to the creature it walks
void RecordPlanEnd(int32_t context, int32_t code)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = static_cast<entt::entity>(context);
	if (registry.Valid(creature) && std::as_const(registry).AllOf<CreatureLocomotion>(creature))
	{
		registry.Get<CreatureLocomotion>(creature).planEnd = code;
	}
}

/// How far round the follower keeps clear of things ahead: the creature's radius
float FollowerRadius(int32_t context)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return 0.0f;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto creature = static_cast<entt::entity>(context);
	const auto* self = registry.Valid(creature) ? registry.TryGet<const CreatureLocomotion>(creature) : nullptr;
	return self != nullptr ? self->radius : 0.0f;
}

route_planner::RouteFollower& FreshFollower(entt::entity creature, CreatureLocomotion& self)
{
	self.follower = std::make_unique<route_planner::RouteFollower>();
	self.follower->Init(static_cast<int32_t>(entt::to_integral(creature)), &RecordPlanEnd, {}, &FollowerRadius,
	                    k_SearchTurnsPerUpdate);
	self.follower->SetIdle();
	return *self.follower;
}

bool FollowsRoute(const CreatureLocomotion& self)
{
	if (self.follower == nullptr)
	{
		return false;
	}
	const auto state = self.follower->GetState();
	return state == FollowerState::Following || state == FollowerState::Replanning;
}

/// The next point of the route the creature heads for: the end of the first node that is not where it stands, or its
/// destination
std::optional<glm::vec2> NextPoint(const CreatureLocomotion& self)
{
	const auto position = PositionOf(self);
	if (self.follower != nullptr && self.follower->HasRoute())
	{
		for (const auto& node : self.follower->GetRouteAhead())
		{
			const glm::vec2 point {node.to.x, node.to.z};
			if (glm::distance(point, position) > k_Tiny)
			{
				return point;
			}
		}
	}
	return self.destination;
}

bool RouteFinished(const CreatureLocomotion& self)
{
	return self.follower == nullptr || !self.follower->HasRoute();
}

/// Where the follower is after moving some distance along the route, which way it went and whether the route ended
struct Advanced
{
	glm::vec2 position;
	float heading;
	bool finished;
};

Advanced AdvanceAlongRoute(CreatureLocomotion& self, float distance)
{
	const auto before = PositionOf(self);
	if (self.follower == nullptr)
	{
		return {.position = before, .heading = self.heading, .finished = true};
	}
	auto& follower = *self.follower;
	follower.SetSpeed(distance / k_FollowerStepSeconds);
	follower.MoveAlongRoute();
	const glm::vec2 after {follower.GetPosition().x, follower.GetPosition().z};
	return {.position = after,
	        .heading = ecs::creature_pose::HeadingFromFollower(before, after, self.heading),
	        .finished = !follower.HasRoute()};
}

/// The two animations of a turn or step, through the same fraction of each
void TrackPair(CreatureLocomotion& self, const Moves& moves, float fromMs, float advanceMs)
{
	if (!self.move.has_value() || !self.move->animations.has_value())
	{
		return;
	}
	const auto& pair = *self.move->animations;
	const auto duration = self.move->durationMs;
	for (const auto& [animation, weight] : {std::pair {pair.from, 1.0f - pair.weight}, std::pair {pair.to, pair.weight}})
	{
		const auto own = moves.Duration(animation);
		const auto ratio = duration > 0.0f ? own / duration : 0.0f;
		self.tracks.push_back({.animation = animation,
		                       .fromMs = fromMs * ratio,
		                       .advanceMs = advanceMs * ratio,
		                       .durationMs = own,
		                       .weight = weight,
		                       .looping = false,
		                       .breathing = false});
	}
}

/// Turns on the spot or steps off towards the next point of the route, or walks straight on, by how far round it is
void BeginLeg(CreatureLocomotion& self, const Moves& moves)
{
	const auto position = PositionOf(self);
	const auto next = NextPoint(self);
	const auto offset = next.has_value() ? *next - position : glm::vec2(0.0f);
	if (glm::length(offset) <= k_Tiny)
	{
		self.motion = Motion::Walking;
		return;
	}
	self.targetHeading = locomotion::HeadingOf(offset);
	const auto angle = locomotion::WrapAngle(self.targetHeading - self.heading);
	const auto side = locomotion::SideOf(angle);
	const auto firstStep =
	    side == locomotion::Side::Right ? locomotion::animations::k_RightStep : locomotion::animations::k_LeftStep;
	const auto firstSpin =
	    side == locomotion::Side::Right ? locomotion::animations::k_RightSpin : locomotion::animations::k_LeftSpin;
	locomotion::StartOptions options {.stepStrides = std::nullopt, .hasSpins = moves.HasAll(firstSpin), .moving = true};
	if (moves.HasAll(firstStep))
	{
		options.stepStrides =
		    std::array {glm::length(moves.Displacement(firstStep)), glm::length(moves.Displacement(firstStep + 1)),
		                glm::length(moves.Displacement(firstStep + 2))};
	}
	const auto distanceInMesh = self.scale > 0.0f ? glm::length(offset) / self.scale : 0.0f;
	const auto start = locomotion::ChooseStart(angle, distanceInMesh, options);
	self.speed = 0.0f;
	switch (start.kind)
	{
	case locomotion::Start::Kind::Walk:
		self.motion = Motion::Walking;
		self.move.reset();
		break;
	case locomotion::Start::Kind::Step:
		self.motion = Motion::Stepping;
		self.move = CreatureLocomotion::Move {.animations = start.animations,
		                                      .timeMs = 0.0f,
		                                      .durationMs = moves.PairDuration(*start.animations),
		                                      .displacement = moves.PairDisplacement(*start.animations),
		                                      .startHeading = self.heading,
		                                      .startRouteHeading = self.targetHeading};
		break;
	case locomotion::Start::Kind::Turn:
		self.motion = Motion::Turning;
		self.move = CreatureLocomotion::Move {.animations = start.animations,
		                                      .timeMs = 0.0f,
		                                      .durationMs = start.animations.has_value()
		                                                        ? moves.PairDuration(*start.animations)
		                                                        : moves.Duration(locomotion::animations::k_Stand),
		                                      .displacement = glm::vec2(0.0f),
		                                      .startHeading = self.heading,
		                                      .startRouteHeading = self.targetHeading};
		break;
	}
}

void Arrive(CreatureLocomotion& self)
{
	ClearWalk(self);
	self.failed = false;
}

void TurnStep(CreatureLocomotion& self, const Moves& moves, float playbackRate)
{
	auto& move = *self.move;
	const auto advance = TurnMs() * playbackRate;
	const auto from = move.timeMs;
	move.timeMs = std::min(move.timeMs + advance, move.durationMs);
	if (move.animations.has_value())
	{
		// The spin's own root turns the body; its heading changes once the turn is over
		TrackPair(self, moves, from, move.timeMs - from);
	}
	else
	{
		self.heading = locomotion::LerpHeading(move.startHeading, self.targetHeading, move.timeMs, move.durationMs);
	}
	if (move.timeMs < move.durationMs)
	{
		return;
	}
	self.heading = self.targetHeading;
	self.move.reset();
	self.tracks.clear();
	if (self.facingOnly || RouteFinished(self))
	{
		ClearWalk(self);
		return;
	}
	BeginLeg(self, moves);
}

void StepOff(CreatureLocomotion& self, const Moves& moves)
{
	auto& move = *self.move;
	const auto from = move.timeMs;
	move.timeMs = std::min(move.timeMs + TurnMs(), move.durationMs);
	const auto fraction = move.durationMs > 0.0f ? (move.timeMs - from) / move.durationMs : 1.0f;
	// The step's move carries it along the route
	const auto distance = glm::length(move.displacement) * self.scale * fraction;
	self.speed = distance / TurnSeconds();
	self.distance = distance;
	const auto advanced = AdvanceAlongRoute(self, distance);
	PlaceAt(self, advanced.position);
	if (advanced.finished)
	{
		Arrive(self);
		return;
	}
	// The step's own root turns the body round towards the route as it steps; its heading follows once the step ends
	TrackPair(self, moves, from, move.timeMs - from);

	if (std::abs(locomotion::WrapAngle(advanced.heading - move.startRouteHeading)) > locomotion::k_CornerAngle)
	{
		self.tracks.clear();
		self.move.reset();
		BeginLeg(self, moves);
		return;
	}
	if (move.timeMs >= move.durationMs)
	{
		// It leaves the step already walking, along the route
		self.heading = advanced.heading;
		self.move.reset();
		self.motion = Motion::Walking;
		self.speed = self.speeds.walk;
	}
}

void Walk(CreatureLocomotion& self, const Moves& moves, float targetSpeed, float breathPhase)
{
	if (RouteFinished(self))
	{
		Arrive(self);
		return;
	}
	const auto seconds = TurnSeconds();
	// It slows to stop at the route's end. The follower's routes round their corners into arcs, so there is no corner
	// to slow for.
	const auto cap = locomotion::StopCap(self.follower->RemainingLength());
	const auto position = PositionOf(self);
	const auto ahead = position + (locomotion::DirectionOf(self.heading) * self.radius);
	const auto slope = locomotion::SlopeFactor(HeightAt(ahead), HeightAt(position), self.radius);
	self.speed = std::min(locomotion::Accelerate(self.speed, slope * targetSpeed, seconds), cap);

	const auto distance = self.speed * seconds;
	self.distance = distance;
	const auto advanced = AdvanceAlongRoute(self, distance);
	PlaceAt(self, advanced.position);
	if (advanced.finished)
	{
		Arrive(self);
		return;
	}
	if (std::abs(locomotion::WrapAngle(advanced.heading - self.heading)) >= locomotion::k_CornerAngle)
	{
		self.speed = 0.0f;
		BeginLeg(self, moves);
		return;
	}
	self.heading = advanced.heading;

	const auto walkFrom = self.walkTimeMs;
	const auto stand = moves.Cycle(locomotion::animations::k_Stand);
	const auto walk = moves.Cycle(locomotion::animations::k_Walk);
	const auto run = moves.Cycle(locomotion::animations::k_Run);
	const auto gait =
	    locomotion::BlendGait(self.speed, self.speeds, distance, self.scale, stand, walk, run, self.walkTimeMs, breathPhase);
	self.walkTimeMs = gait.walkTimeMs;
	for (const auto& slot : gait.slots)
	{
		CreatureLocomotion::Track track {.animation = slot.animation,
		                                 .fromMs = walkFrom,
		                                 .advanceMs = gait.walkAdvanceMs,
		                                 .durationMs = walk.durationMs,
		                                 .weight = slot.weight,
		                                 .looping = true,
		                                 .breathing = false};
		if (slot.animation == locomotion::animations::k_Stand)
		{
			track.durationMs = stand.durationMs;
			track.breathing = true;
		}
		else if (slot.animation == locomotion::animations::k_Run)
		{
			// The run keeps in step with the walk
			const auto ratio = walk.durationMs > 0.0f ? run.durationMs / walk.durationMs : 0.0f;
			track.fromMs = walkFrom * ratio;
			track.advanceMs = gait.walkAdvanceMs * ratio;
			track.durationMs = run.durationMs;
		}
		self.tracks.push_back(track);
	}
}

/// How far round something reaches, for walking up to it
float RadiusOf(const ecs::Registry& registry, entt::entity entity)
{
	if (const auto* other = registry.TryGet<const CreatureLocomotion>(entity))
	{
		return other->radius;
	}
	if (const auto* fixed = registry.TryGet<const Fixed>(entity))
	{
		return fixed->boundingRadius;
	}
	return 1.0f;
}

bool IsMovingMotion(Motion motion)
{
	return motion != Motion::Standing;
}

/// The planning of a route a turn on: whether it is ready, has failed or waits
void StepPlanning(CreatureLocomotion& self)
{
	if (self.follower == nullptr)
	{
		return;
	}
	self.follower->Update(nullptr, 0);
	if ((self.motion != Motion::Planning && self.motion != Motion::Confused) || self.routeReady)
	{
		return;
	}
	self.planningMs -= TurnMs();
	const auto state = self.follower->GetState();
	const bool ready = state == FollowerState::Following || state == FollowerState::Replanning || self.planEnd == 0;
	const bool failed = state == FollowerState::GivenUp || self.planEnd == 1 || self.planEnd == 3;
	if (ready)
	{
		self.routeReady = true;
	}
	else if (failed || self.planningMs <= 0.0f)
	{
		// No way there: it stands where it is
		ClearWalk(self);
		self.failed = true;
	}
}
} // namespace

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::StartMove(entt::entity creature, CreatureLocomotion& self,
                                                                         glm::vec2 point, float fraction, float minDistance,
                                                                         float maxDistance)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || !MovesOf(body->species).CanWalk())
	{
		return MoveResult::Busy;
	}
	if (!IsValid(point, land_avoid::k_CreatureRadius))
	{
		return MoveResult::InvalidDestination;
	}
	self.fraction = fraction;
	self.failed = false;
	self.facingOnly = false;
	if (IsMovingMotion(self.motion) && self.destination.has_value() &&
	    glm::distance(*self.destination, point) <= k_SameDestination)
	{
		return MoveResult::Started;
	}

	// Straying somewhere it can't stand, it is put back on the nearest place it can first
	auto position = PositionOf(self);
	if (!IsValid(position, land_avoid::k_CreatureTurningRadius))
	{
		if (const auto valid = land_avoid::NearestValid(position, land_avoid::k_CreatureTurningRadius, k_StraySearch))
		{
			position = *valid;
			SnapTo(creature, self, position);
		}
	}
	const auto distance = glm::distance(position, point);
	self.ring = locomotion::MoveRing(distance, minDistance, maxDistance);
	self.destination = point;
	self.planningMs = locomotion::TimeLimitMs(distance);
	self.fidgetMs = k_FidgetMs;
	self.fidgeted = false;

	const route_planner::Point2D destination {point.x, point.y};
	const auto planLength = (distance + 1.0f) * k_PlanLengthFactor;
	// Already walking a route, it carries on along it while the way ahead is planned again
	if (self.motion == Motion::Walking && FollowsRoute(self))
	{
		self.follower->SetDest(destination, self.ring.max, self.ring.min, self.radius, planLength);
		return MoveResult::Started;
	}
	if (self.follower == nullptr || self.follower->GetState() != FollowerState::Idle)
	{
		FreshFollower(creature, self);
	}
	auto& follower = *self.follower;
	const route_planner::Point2D start {position.x, position.y};
	follower.SetPosition(start);
	follower.SetPlanStart(start);
	self.routeReady = false;
	self.planEnd = -1;
	follower.SetDest(destination, self.ring.max, self.ring.min, self.radius, planLength);
	SetIdle(self);
	self.motion = Motion::Planning;
	return MoveResult::Started;
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::MoveTo(entt::entity creature, glm::vec2 point, Pace pace,
                                                                      float minDistance, float maxDistance)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<CreatureLocomotion, Creature>(creature))
	{
		return MoveResult::Busy;
	}
	auto& self = registry.Get<CreatureLocomotion>(creature);
	const auto& body = std::as_const(registry).Get<Creature>(creature);
	if (!self.started)
	{
		return MoveResult::Busy;
	}
	self.following.reset();
	const auto fractions = FractionsOf(body.species);
	return StartMove(creature, self, point, pace == Pace::Run ? fractions.run : fractions.walk, minDistance, maxDistance);
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::LeadTo(entt::entity creature, glm::vec2 point, float pull,
                                                                      float maxDistance)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<CreatureLocomotion, Creature>(creature))
	{
		return MoveResult::Busy;
	}
	auto& self = registry.Get<CreatureLocomotion>(creature);
	const auto& body = std::as_const(registry).Get<Creature>(creature);
	if (!self.started)
	{
		return MoveResult::Busy;
	}
	self.following.reset();
	const auto fractions = FractionsOf(body.species);
	return StartMove(creature, self, point, locomotion::LeashFraction(fractions.walk, fractions.run, pull), 0.0f, maxDistance);
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::MoveToObject(entt::entity creature, entt::entity target,
                                                                            Pace pace, float extra)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature) || !ecs::IsAvailable(target))
	{
		return MoveResult::Busy;
	}
	const auto* self = registry.TryGet<const CreatureLocomotion>(creature);
	const auto* at = registry.TryGet<const Transform>(target);
	if (self == nullptr || at == nullptr)
	{
		return MoveResult::Busy;
	}
	const auto ring = locomotion::ObjectRing(self->radius, RadiusOf(registry, target), extra);
	return MoveTo(creature, glm::xz(at->position), pace, ring.min, ring.max);
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::Follow(entt::entity creature, entt::entity target,
                                                                      float distance, Pace pace)
{
	const auto result = MoveToObject(creature, target, pace, distance);
	auto& registry = Locator::entitiesRegistry::value();
	if (result != MoveResult::Busy && std::as_const(registry).AllOf<CreatureLocomotion>(creature))
	{
		auto& self = registry.Get<CreatureLocomotion>(creature);
		self.following = target;
		self.followDistance = distance;
	}
	return result;
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::FleeFrom(entt::entity creature, glm::vec2 threat)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<CreatureLocomotion, Creature>(creature))
	{
		return MoveResult::Busy;
	}
	auto& self = registry.Get<CreatureLocomotion>(creature);
	const auto& body = std::as_const(registry).Get<Creature>(creature);
	if (!self.started)
	{
		return MoveResult::Busy;
	}
	const auto fractions = FractionsOf(body.species);
	const auto distance = static_cast<float>(game_random::GameRand(k_RunAwayExtra)) + fractions.runAway;
	auto point = locomotion::RunAwayPoint(PositionOf(self), threat, distance);
	if (!IsValid(point, land_avoid::k_CreatureRadius))
	{
		const auto valid = land_avoid::NearestValid(point, land_avoid::k_CreatureRadius, k_RunAwaySearch);
		if (!valid.has_value())
		{
			return MoveResult::InvalidDestination;
		}
		point = *valid;
	}
	self.following.reset();
	return StartMove(creature, self, point, fractions.run, 0.0f, k_RunAwayArrival);
}

bool CreatureLocomotionSystem::TurnToFace(entt::entity creature, glm::vec2 point)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<CreatureLocomotion, Creature>(creature))
	{
		return false;
	}
	auto& self = registry.Get<CreatureLocomotion>(creature);
	const auto& body = std::as_const(registry).Get<Creature>(creature);
	if (!self.started)
	{
		return false;
	}
	const auto offset = point - PositionOf(self);
	if (glm::length(offset) <= k_Tiny)
	{
		return false;
	}
	ClearWalk(self);
	self.following.reset();
	self.facingOnly = true;
	self.targetHeading = locomotion::HeadingOf(offset);
	const auto angle = locomotion::WrapAngle(self.targetHeading - self.heading);
	const auto moves = MovesOf(body.species);
	const auto firstSpin = locomotion::SideOf(angle) == locomotion::Side::Right ? locomotion::animations::k_RightSpin
	                                                                            : locomotion::animations::k_LeftSpin;
	const auto start = locomotion::ChooseStart(
	    angle, 0.0f, {.stepStrides = std::nullopt, .hasSpins = moves.HasAll(firstSpin), .moving = false});
	if (start.kind != locomotion::Start::Kind::Turn)
	{
		// Nearly facing it already
		self.heading = self.targetHeading;
		self.facingOnly = false;
		return true;
	}
	self.motion = Motion::Turning;
	self.move =
	    CreatureLocomotion::Move {.animations = start.animations,
	                              .timeMs = 0.0f,
	                              .durationMs = start.animations.has_value() ? moves.PairDuration(*start.animations)
	                                                                         : moves.Duration(locomotion::animations::k_Stand),
	                              .displacement = glm::vec2(0.0f),
	                              .startHeading = self.heading,
	                              .startRouteHeading = self.heading};
	return true;
}

void CreatureLocomotionSystem::Stop(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(creature) && std::as_const(registry).AllOf<CreatureLocomotion>(creature))
	{
		auto& self = registry.Get<CreatureLocomotion>(creature);
		ClearWalk(self);
		self.following.reset();
	}
}

bool CreatureLocomotionSystem::IsMoving(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* self = registry.Valid(creature) ? registry.TryGet<const CreatureLocomotion>(creature) : nullptr;
	return self != nullptr && IsMovingMotion(self->motion);
}

bool CreatureLocomotionSystem::IsValidPosition(glm::vec2 point, float radius) const
{
	return IsValid(point, radius);
}

void CreatureLocomotionSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	const auto turnMs = TurnMs();

	// The creatures, in the registry's order, found without making any storage
	std::vector<entt::entity> creatures;
	lookup.Each<const Creature, const CreatureLocomotion, const Transform, const CreatureAnimation>(
	    [&creatures](entt::entity entity, const auto&...) { creatures.push_back(entity); });

	// Following something, the walk is pointed anew as it moves away
	std::vector<std::pair<entt::entity, entt::entity>> retarget;
	for (const auto entity : creatures)
	{
		auto& self = registry.Get<CreatureLocomotion>(entity);
		if (!self.following.has_value() || !self.started)
		{
			continue;
		}
		const auto* at = ecs::IsAvailable(*self.following) ? lookup.TryGet<const Transform>(*self.following) : nullptr;
		if (at == nullptr)
		{
			self.following.reset();
			continue;
		}
		const auto target = glm::xz(at->position);
		const bool moved = IsMovingMotion(self.motion) && self.destination.has_value() &&
		                   glm::distance(target, *self.destination) > k_FollowRetarget;
		const bool left =
		    !IsMovingMotion(self.motion) && glm::distance(target, PositionOf(self)) > self.ring.max + k_FollowRetarget;
		if (moved || left)
		{
			retarget.emplace_back(entity, *self.following);
		}
	}
	for (const auto& [entity, target] : retarget)
	{
		const auto& self = lookup.Get<CreatureLocomotion>(entity);
		const auto& body = lookup.Get<Creature>(entity);
		const auto distance = self.followDistance;
		const auto fractions = FractionsOf(body.species);
		Follow(entity, target, distance, self.fraction >= fractions.run ? Pace::Run : Pace::Walk);
	}

	for (const auto entity : creatures)
	{
		const auto& creature = lookup.Get<Creature>(entity);
		const auto& transform = lookup.Get<Transform>(entity);
		auto& self = registry.Get<CreatureLocomotion>(entity);
		auto& animation = registry.Get<CreatureAnimation>(entity);
		if (!self.started)
		{
			self.started = true;
			self.heading = ecs::creature_pose::ReadHeading(transform.rotation);
			self.toPosition = transform.position;
			self.toHeading = self.heading;
		}
		self.fromPosition = self.toPosition;
		self.fromHeading = self.toHeading;
		self.tracks.clear();
		self.distance = 0.0f;
		Measure(self, creature, transform);
		const auto moves = MovesOf(creature.species);

		// The route is planned a little each turn
		StepPlanning(self);

		const auto* needs = lookup.TryGet<const CreatureNeeds>(entity);
		const auto fractions = FractionsOf(creature.species);
		const auto fraction =
		    locomotion::RequiredFraction(self.fraction, fractions.slow, needs != nullptr ? needs->needs.exhaustion : 0.0f);
		const auto targetSpeed = locomotion::TargetSpeed(fraction, self.speeds.run);

		switch (self.motion)
		{
		case Motion::Standing:
			self.speed = 0.0f;
			break;
		case Motion::Planning:
			if (self.routeReady)
			{
				// Sitting or playing an action, it gets up and finishes first
				if (creature_layers::IsLooping(animation.body))
				{
					animation.body = creature_layers::EndLoop(animation.body);
				}
				if (!creature_layers::IsPlaying(animation.body))
				{
					self.routeReady = false;
					BeginLeg(self, moves);
				}
				break;
			}
			self.fidgetMs -= turnMs;
			if (self.fidgetMs <= 0.0f)
			{
				// The first fidget is always a look about
				const auto choice = self.fidgeted ? game_random::GameRand(3) : 0u;
				self.fidgeted = true;
				self.fidgetMs = k_FidgetMs;
				if (choice == 0)
				{
					self.fidgetMs = k_LookAboutFidgetMs;
				}
				else if (choice == 1)
				{
					animation.face =
					    creature_layers::PullFace(animation.face, k_PuzzledFace, k_PuzzledMs, creature_face::Cue::Lost);
				}
				else
				{
					const bool mirrored = game_random::GameRand(2) != 0;
					if (auto confused =
					        creature_layers::PlayOnce(animation.body, creature_layers::animations::k_Confused, mirrored))
					{
						animation.body = *confused;
						self.motion = Motion::Confused;
					}
				}
			}
			break;
		case Motion::Confused:
			if (!creature_layers::IsPlaying(animation.body))
			{
				self.motion = Motion::Planning;
			}
			break;
		case Motion::Turning:
			TurnStep(self, moves, creature_layers::PlaybackRate(creature.size));
			break;
		case Motion::Stepping:
			StepOff(self, moves);
			break;
		case Motion::Walking:
			Walk(self, moves, targetSpeed, animation.breathPhase);
			break;
		}

		// Strayed somewhere it can't stand, it is put back where it can and sets off again
		if ((self.motion == Motion::Walking || self.motion == Motion::Stepping) && self.destination.has_value() &&
		    !IsValid(PositionOf(self), land_avoid::k_CreatureTurningRadius))
		{
			if (const auto valid =
			        land_avoid::NearestValid(PositionOf(self), land_avoid::k_CreatureTurningRadius, k_StraySearch))
			{
				SnapTo(entity, self, *valid);
				const auto destination = *self.destination;
				const auto ring = self.ring;
				SetIdle(self);
				DropRoute(self);
				StartMove(entity, self, destination, self.fraction, ring.min, ring.max);
			}
		}
		self.toHeading = self.heading;
		// The Transform moves once a turn, to where the turn ends
		ecs::creature_pose::CommitTurnPose(entity, self.toPosition, self.toHeading);
	}
}

void CreatureLocomotionSystem::Update(float turnFraction)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	const auto f = std::clamp(turnFraction, 0.0f, 1.0f);
	std::vector<entt::entity> creatures;
	lookup.Each<const CreatureLocomotion, const CreatureAnimation>(
	    [&creatures](entt::entity entity, const auto&...) { creatures.push_back(entity); });
	bool moved = false;
	for (const auto entity : creatures)
	{
		const auto& self = lookup.Get<CreatureLocomotion>(entity);
		if (!self.started)
		{
			continue;
		}
		// Only the drawn pose moves between turns; the Transform stays where the turn put it
		if (lookup.AllOf<CreatureDrawPose>(entity))
		{
			auto& pose = registry.Get<CreatureDrawPose>(entity);
			const auto position = self.fromPosition + ((self.toPosition - self.fromPosition) * f);
			const auto heading = self.fromHeading + (locomotion::WrapAngle(self.toHeading - self.fromHeading) * f);
			const auto rotation = affine::AngleY(-heading);
			if (position != pose.position || rotation != pose.rotation)
			{
				pose.position = position;
				pose.rotation = rotation;
				moved = true;
			}
		}
		auto& animation = registry.Get<CreatureAnimation>(entity);
		animation.slots.clear();
		for (const auto& track : self.tracks)
		{
			auto time = track.fromMs + (track.advanceMs * f);
			if (track.breathing)
			{
				time = static_cast<float>(creature_animation::BreathTime(
				    animation.breathPhase, static_cast<uint32_t>(std::max(track.durationMs, 1.0f))));
			}
			else if (track.looping)
			{
				time = track.durationMs > 0.0f ? std::fmod(time, track.durationMs) : 0.0f;
			}
			else
			{
				time = std::clamp(time, 0.0f, std::max(track.durationMs - 1.0f, 0.0f));
			}
			animation.slots.push_back(
			    {.animation = track.animation, .timeMs = time, .weight = track.weight, .mirrored = false});
		}
	}
	if (moved)
	{
		registry.SetDirty();
	}
}
