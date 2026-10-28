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
#include <chrono>
#include <iterator>
#include <limits>
#include <memory>
#include <unordered_set>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat3x3.hpp>

#include "3D/AllMeshes.h"
#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "Common/PortableRotations.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureObstacles.h"
#include "Creature/CreaturePointing.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureRouteFollow.h"
#include "Creature/CreatureRouteSearch.h"
#include "Creature/CreatureWalkPlanner.h"
#include "ECS/BuildingSiteList.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureDrawnWalk.h"
#include "ECS/CreatureObstacleThings.h"
#include "ECS/Map.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/ThingModel.h"
#include "ECS/WallHugRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/ResourcePiles.h"
#include "Magic/ShieldRules.h"
#include "Magic/VortexPullRules.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
namespace locomotion = openblack::creature_locomotion;
namespace route = openblack::creature_route;
namespace planner = openblack::creature_walk_planner;
namespace obstacles = openblack::creature_obstacles;
namespace things = openblack::ecs::creature_obstacle_things;
namespace drawn_walk = openblack::ecs::creature_drawn_walk;
using Motion = CreatureLocomotion::Motion;

namespace
{
constexpr float k_TurnMs = std::chrono::duration<float, std::milli>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnSeconds = k_TurnMs / 1000.0f;
/// Steps of the route search on a new walk's first turn. In the other turns, the search gets the turn's share.
constexpr int32_t k_FirstTurnSearchSteps = 256;
/// A route costing more than this much a unit of the straight distance, and this much besides, is given up
constexpr float k_CostPerUnit = 2.0f;
constexpr float k_BaseCost = 5000.0f;
/// Waiting for a route, a puzzled face is held this long
constexpr float k_PuzzledMs = 2000.0f;
constexpr size_t k_PuzzledFace = creature_layers::animations::k_FirstFace + 6;

/// A number from 0 up to the range, not including it, from the game's random numbers. Without them it is 0.
uint32_t GameRand(uint32_t range)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameRand(range) : 0u;
}

/// Following something, the walk is pointed anew once it has moved this far from where the walk heads
constexpr float k_FollowRetarget = 1.0f;
/// What is known of a species without the game's creature tables: the ape's speeds
constexpr float k_DefaultSlowFraction = 0.5f;
constexpr float k_DefaultWalkFraction = 0.7f;
constexpr float k_DefaultRunFraction = 0.9f;
constexpr float k_DefaultRunAwayDistance = 50.0f;
/// Turning or stepping, the steps never move it less than this a turn
constexpr float k_Tiny = 1e-4f;
/// A body whose mesh isn't loaded reaches this far round at a drawn scale of one
constexpr float k_UnknownBodyRadius = 0.0f;

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

/// The fraction of its top speed for a walk at a pace. The kept pace is the speed that the creature got last.
float PaceFraction(CreatureLocomotionSystemInterface::Pace pace, float kept, const Fractions& fractions)
{
	switch (pace)
	{
	case CreatureLocomotionSystemInterface::Pace::Run:
		return fractions.run;
	case CreatureLocomotionSystemInterface::Pace::Keep:
		return locomotion::KeptFraction(kept, fractions.walk);
	case CreatureLocomotionSystemInterface::Pace::Walk:
		break;
	}
	return fractions.walk;
}

/// A walk that starts makes the creature breathe faster for the rest of the game turn: faster still at the run speed
void BreatheForWalk(entt::entity creature, const Creature& body, float fraction, const Fractions& fractions)
{
	if (auto* animation = Locator::entitiesRegistry::value().TryGet<CreatureAnimation>(creature))
	{
		animation->breathTarget = creature_animation::WalkBreathPeriod(ShownSize(body), fraction == fractions.run);
	}
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
	[[nodiscard]] bool CanWalk() const { return Has(locomotion::animations::k_Walk) && Has(locomotion::animations::k_Run); }
	[[nodiscard]] bool HasAll(size_t first) const { return Has(first) && Has(first + 1) && Has(first + 2); }
	/// The pair blended into one animation lasts between their lengths, to the nearest millisecond
	[[nodiscard]] float PairDuration(const locomotion::Pair& pair) const
	{
		return std::nearbyint(Duration(pair.from) + ((Duration(pair.to) - Duration(pair.from)) * pair.weight));
	}
};

Moves MovesOf(CreatureType species)
{
	auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(species);
	return {.rig = rigs.Contains(id) ? &*rigs.Handle(id) : nullptr};
}

float HeightAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

GameHeading HeadingOfRotation(const glm::mat3& rotation)
{
	// The drawn body is turned about y by a yaw; its mesh looks back along +z
	return GameHeading::FromYaw(portable_math::Atan2(rotation[2].x, rotation[2].z));
}

/// How far round its middle a species' body reaches at its drawn scale of one: the furthest any of its base mesh's bones
/// stands from its middle across the ground, at the start of its stand
std::optional<float> MeasureBodyRadius(CreatureType species)
{
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto meshId = creature::GetIdFromType(species, creature::CreatureBody::Appearance::Base);
	if (!meshes.Contains(meshId))
	{
		return std::nullopt;
	}
	const auto mesh = meshes.Handle(meshId);
	const auto& rest = mesh->GetBoneMatrices();
	auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(species);
	const auto* stand = rigs.Contains(rigId)
	                        ? rigs.Handle(rigId)->GetAnimation(CreatureRig::Mesh::Base, locomotion::animations::k_Stand)
	                        : nullptr;
	if (stand == nullptr || stand->frames.empty() || rest.empty())
	{
		return obstacles::BodyRadius(rest);
	}
	const auto skeleton = skeletal_animation::Skeleton::FromRestMatrices(mesh->GetBoneParents(), rest);
	const auto poses = skeletal_animation::SampleCycle(*stand, *stand, 0, skeleton);
	return obstacles::BodyRadius(skeletal_animation::ComposeBoneMatrices(poses, skeleton.parents));
}

/// The creature's size and speeds this turn
void Measure(CreatureLocomotion& self, const Creature& creature, const Transform& transform, float bodyRadius)
{
	self.speeds = locomotion::SpeedsFor(ShownSize(creature));
	self.scale = std::abs(transform.scale.x);
	// Drawn at another size than its own in a temple's pen, it keeps clear of things by its own size
	self.shownRadius = bodyRadius * self.scale;
	const auto shown = ShownSize(creature);
	self.radius = shown > 0.0f ? self.shownRadius * creature_morph::ClampScale(creature.size) / shown : self.shownRadius;
	// While it clears a thing out of its way, it keeps clear of things by its drawn size
	if (self.clearingWay)
	{
		self.radius = self.shownRadius;
	}
}

void SetIdle(CreatureLocomotion& self)
{
	self.motion = Motion::Standing;
	self.speed = 0.0f;
	self.move.reset();
	self.tracks.clear();
	self.routePoint.reset();
}

void ClearWalk(CreatureLocomotion& self)
{
	SetIdle(self);
	self.follower.Reset(glm::xz(self.toPosition), self.radius);
	self.routeWalker.reset();
	self.firstSearchTurn = false;
	self.routeReady = false;
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

/// What a creature's walk needs this turn besides its own state
struct WalkTurn
{
	const Moves& moves;
	/// The top speed that it is asked for
	float topSpeed;
	float breathPhase;
	/// The whole milliseconds that the creature lives this turn: fewer when it is frozen. A step plays them.
	int32_t turnMs;
	/// The whole milliseconds that its animations play this turn. A turn on the spot and the wait for a route use them.
	int32_t spinMs;
	/// What its legs played last turn, which they keep when it is too slow to move them
	const std::vector<CreatureLocomotion::Track>& lastTracks;
	/// The land where creatures can stand
	const route::WalkableLand& land;
	/// Set when a step or the walk puts the creature where it cannot stand. It is put back after its move this turn.
	mutable bool strayed {false};
};

/// Whether the creature can stand where its step or walk put it this turn. When it cannot, the turn notes it.
bool CheckStand(const CreatureLocomotion& self, const WalkTurn& turn)
{
	if (turn.land.IsValid(PositionOf(self), route::k_Clearance))
	{
		return true;
	}
	turn.strayed = true;
	return false;
}

/// A step or a turn on the spot: its two animations, blended keyframe by keyframe into one. The step's blend plays
/// over the step's length, so its time is scaled to the blend's own length.
void TrackPair(CreatureLocomotion& self, float fromMs, float advanceMs)
{
	if (!self.move.has_value() || !self.move->animations.has_value())
	{
		return;
	}
	const auto& move = *self.move;
	const auto scale = move.blendDurationMs > 0.0f && move.durationMs > 0.0f ? move.blendDurationMs / move.durationMs : 1.0f;
	const auto duration = move.blendDurationMs > 0.0f ? move.blendDurationMs : move.durationMs;
	self.tracks.push_back({.animation = move.animations->from,
	                       .fromMs = fromMs * scale,
	                       .advanceMs = advanceMs * scale,
	                       .durationMs = duration,
	                       .weight = 1.0f,
	                       .looping = false,
	                       .breathing = false,
	                       .blendedWith = move.animations->to,
	                       .blendWeight = move.animations->weight});
}

/// The legs stand still, at the creature's point of its breath
void StandPose(CreatureLocomotion& self, const Moves& moves)
{
	self.tracks.clear();
	self.tracks.push_back({.animation = locomotion::animations::k_Stand,
	                       .fromMs = 0.0f,
	                       .advanceMs = 0.0f,
	                       .durationMs = moves.Duration(locomotion::animations::k_Stand),
	                       .weight = 1.0f,
	                       .looping = false,
	                       .breathing = true});
}

/// The species' three steps of a side, when it has all three
planner::StepSet StepsOf(const Moves& moves, size_t first)
{
	if (!moves.HasAll(first))
	{
		return std::nullopt;
	}
	std::array<planner::StepClip, 3> steps {};
	for (size_t i = 0; i < steps.size(); ++i)
	{
		const auto* animation = moves.Get(first + i);
		steps.at(i) = {.displacement = animation->displacement, .frames = static_cast<int32_t>(animation->frames.size())};
	}
	return steps;
}

planner::Clip ClipOf(const Moves& moves, size_t index)
{
	const auto* animation = moves.Get(index);
	if (animation == nullptr)
	{
		return {.durationMs = 1, .stride = 0.0f};
	}
	return {.durationMs = static_cast<int32_t>(std::max(animation->duration, 1u)), .stride = animation->strideLength};
}

/// Starts a turn on the spot to a heading. With all three spins of the side, it plays their blend. Without them, it
/// eases round over the stand's length.
void BeginTurn(CreatureLocomotion& self, const Moves& moves, GameHeading target)
{
	self.targetHeading = target;
	const auto angle = target.TurnFrom(self.heading);
	const auto firstSpin = locomotion::SpinSideOf(angle) == locomotion::Side::Right ? locomotion::animations::k_RightSpin
	                                                                                : locomotion::animations::k_LeftSpin;
	const auto animations = moves.HasAll(firstSpin) ? std::optional(locomotion::SpinPairFor(firstSpin, angle)) : std::nullopt;
	self.motion = Motion::Turning;
	self.move =
	    CreatureLocomotion::Move {.animations = animations,
	                              .timeMs = 0.0f,
	                              .durationMs = animations.has_value() ? moves.PairDuration(*animations)
	                                                                   : moves.Duration(locomotion::animations::k_Stand)};
}

void Arrive(CreatureLocomotion& self)
{
	ClearWalk(self);
	self.failed = false;
	self.gaveUp.reset();
}

/// The end of the route: arrived, or (a new destination having come while it walked) waiting there for the search
void RouteEnded(CreatureLocomotion& self)
{
	if (self.follower.GetState() == route::Follower::State::Planning)
	{
		SetIdle(self);
		self.motion = Motion::Planning;
		return;
	}
	Arrive(self);
}

/// How far ahead of the creature a route found while walking is joined from: a turn on the spot's little way, or what
/// is left of a step
float JoinAhead(const CreatureLocomotion& self)
{
	if (self.motion == Motion::Turning)
	{
		return planner::k_TurnJoinAhead;
	}
	if (self.motion == Motion::Stepping && self.move.has_value())
	{
		return planner::StepJoinAhead(self.move->displacement, self.scale, self.follower.GetRoute().travelled);
	}
	return 0.0f;
}

void TurnStep(CreatureLocomotion& self, const WalkTurn& turn);
void StepOff(CreatureLocomotion& self, const WalkTurn& turn);
void Walk(CreatureLocomotion& self, const WalkTurn& turn);

/// Starts the creature along its route from where it stands: it looks at a point ahead along the route and steps off,
/// walks straight on or turns on the spot. When the route has just been found, the first turn of the start plays at
/// once.
void StartLeg(CreatureLocomotion& self, const WalkTurn& turn, bool playNow)
{
	const auto& route = self.follower.GetRoute();
	if (route.Finished())
	{
		RouteEnded(self);
		return;
	}
	const auto from = PositionOf(self);
	const auto point = planner::LookAhead(from, self.follower.GetHeading(), route::LengthOf(route.legs[route.segment]));
	const auto start = planner::ChooseStart(from, point, self.heading, self.scale, self.speeds.walk,
	                                        StepsOf(turn.moves, locomotion::animations::k_RightStep),
	                                        StepsOf(turn.moves, locomotion::animations::k_LeftStep));
	self.targetHeading = start.heading;
	switch (start.phase)
	{
	case planner::Phase::Walk:
		self.motion = Motion::Walking;
		self.move.reset();
		self.walkTimeMs = 0;
		if (playNow)
		{
			Walk(self, turn);
		}
		break;
	case planner::Phase::Step:
		self.motion = Motion::Stepping;
		self.move = CreatureLocomotion::Move {
		    .animations = start.steps,
		    .timeMs = 0.0f,
		    .durationMs = static_cast<float>(start.stepMs),
		    .displacement = start.stepDisplacement,
		    .blendDurationMs = turn.moves.PairDuration(*start.steps),
		    .side = start.side,
		    .routeHeading = start.heading,
		};
		if (playNow)
		{
			StepOff(self, turn);
		}
		break;
	case planner::Phase::Turn:
		BeginTurn(self, turn.moves, start.heading);
		if (playNow)
		{
			TurnStep(self, turn);
		}
		break;
	}
}

/// Stops the creature where a sharp turn of its route caught it, and starts it again from there
void Restart(CreatureLocomotion& self, const WalkTurn& turn)
{
	self.speed = 0.0f;
	self.move.reset();
	PlaceAt(self, self.follower.GetPosition());
	StandPose(self, turn.moves);
	StartLeg(self, turn, false);
}

void TurnStep(CreatureLocomotion& self, const WalkTurn& turn)
{
	auto& move = *self.move;
	const auto from = move.timeMs;
	move.timeMs = std::min(move.timeMs + static_cast<float>(turn.spinMs), move.durationMs);
	if (move.animations.has_value())
	{
		// The spin's own root turns the body; its heading changes once the turn is over
		TrackPair(self, from, move.timeMs - from);
	}
	else
	{
		// On each game turn, the heading closes the part of the gap that the turn's time before this step gives
		self.heading = locomotion::LerpHeading(self.heading, self.targetHeading, from, move.durationMs);
	}
	if (move.timeMs < move.durationMs)
	{
		return;
	}
	self.heading = self.targetHeading;
	self.move.reset();
	if (self.facingOnly || !self.follower.IsFollowing())
	{
		// The spin plays out over this turn, ending with the body turned to face the new way, which its heading takes up
		// at once as the next turn starts
		self.headingSnaps = true;
		auto lastPart = std::move(self.tracks);
		ClearWalk(self);
		self.tracks = std::move(lastPart);
		return;
	}
	// The walk starts in the same turn, from the start of its animations
	self.motion = Motion::Walking;
	self.walkTimeMs = 0;
	self.tracks.clear();
	Walk(self, turn);
}

void StepOff(CreatureLocomotion& self, const WalkTurn& turn)
{
	auto& move = *self.move;
	auto& follower = self.follower;
	if (planner::StepTurnsTooFar(move.routeHeading, follower.GetHeading()))
	{
		// The route turned too far while it stepped: it goes back to the start of the leg and starts again
		self.heading = move.routeHeading;
		follower.RestartLeg();
		Restart(self, turn);
		return;
	}
	// Its heading turns with the route
	self.heading = planner::StepHeading(self.heading, move.routeHeading, follower.GetHeading());
	move.routeHeading = follower.GetHeading();
	self.targetHeading = move.routeHeading;

	const auto timeMs = static_cast<int32_t>(move.timeMs);
	const auto stepMs = static_cast<int32_t>(move.durationMs);
	const auto turnMs = turn.turnMs;
	if (timeMs + turnMs >= stepMs)
	{
		// The step ends and the walk starts in the same turn, part way through its cycle
		self.heading = move.routeHeading;
		self.walkTimeMs = planner::WalkTimeAfterStep(timeMs, stepMs,
		                                             ClipOf(turn.moves, locomotion::animations::k_Walk).durationMs, move.side);
		self.move.reset();
		self.motion = Motion::Walking;
		self.tracks.clear();
		Walk(self, turn);
		return;
	}
	TrackPair(self, static_cast<float>(timeMs), static_cast<float>(turnMs));
	move.timeMs = static_cast<float>(timeMs + turnMs);
	if (!follower.IsFollowing())
	{
		RouteEnded(self);
		return;
	}
	// The creature stands where the route was at the start of this turn; the step's move then takes the route on
	PlaceAt(self, follower.GetPosition());
	if (!CheckStand(self, turn))
	{
		return;
	}
	self.speed =
	    planner::StepSpeed(glm::vec3(move.displacement.x, 0.0f, move.displacement.z), turnMs, self.scale, stepMs, self.heading);
	self.distance = planner::TurnDistance(self.speed);
	if (follower.Advance(self.distance).finished)
	{
		// It arrives facing the way that the route went
		self.heading = follower.GetHeading();
		RouteEnded(self);
	}
}

void Walk(CreatureLocomotion& self, const WalkTurn& turn)
{
	auto& follower = self.follower;
	if (!follower.IsFollowing())
	{
		StandPose(self, turn.moves);
		RouteEnded(self);
		return;
	}
	const auto& route = follower.GetRoute();
	const auto segment = route.segment;
	const auto& leg = route.legs[segment];
	planner::NextLeg next {.kind = planner::NextLeg::Kind::None};
	if (segment + 1 < route.legs.size())
	{
		const auto& after = route.legs[segment + 1];
		next = after.arc && after.round.has_value()
		           ? planner::NextLeg {.kind = planner::NextLeg::Kind::Arc, .radius = after.round->radius}
		           : planner::NextLeg {.kind = planner::NextLeg::Kind::Straight};
	}
	const auto cap = planner::SpeedCap(turn.topSpeed, route::LengthOf(leg), route.travelled, next);
	const auto ahead = planner::SlopePoint(PositionOf(self), self.heading, self.radius);
	const auto slope = planner::SlopeFactor(HeightAt(ahead), self.toPosition.y, self.radius);
	self.speed = planner::NextSpeed(self.speed, slope * turn.topSpeed, cap);

	const auto distance = planner::TurnDistance(self.speed);
	self.distance = distance;
	const auto advanced = follower.Advance(distance);
	if (advanced.finished)
	{
		PlaceAt(self, follower.GetPosition());
		StandPose(self, turn.moves);
		RouteEnded(self);
		return;
	}
	if (planner::WalkTurnsTooFar(self.heading, follower.GetHeading()))
	{
		// Too sharp a turn to walk round: a new leg starts again from its start
		if (follower.GetRoute().segment != segment)
		{
			follower.RestartLeg();
		}
		Restart(self, turn);
		CheckStand(self, turn);
		return;
	}
	PlaceAt(self, follower.GetPosition());
	self.heading = follower.GetHeading();
	CheckStand(self, turn);

	const auto stand = ClipOf(turn.moves, locomotion::animations::k_Stand);
	const auto walk = ClipOf(turn.moves, locomotion::animations::k_Walk);
	const auto run = ClipOf(turn.moves, locomotion::animations::k_Run);
	const auto walkFrom = self.walkTimeMs;
	const auto gait = planner::BlendGait(self.speed, self.speeds.walk, self.speeds.run, distance, self.scale, stand, walk, run,
	                                     self.walkTimeMs, turn.breathPhase);
	if (!gait.has_value())
	{
		// Too slow to move its legs, it keeps what they played
		self.tracks = turn.lastTracks;
		for (auto& track : self.tracks)
		{
			track.fromMs += track.advanceMs;
			track.advanceMs = 0.0f;
		}
		return;
	}
	self.walkTimeMs = gait->timesMs[1];
	// The walk moves on by its advance over the turn, round its loop
	auto walkAdvance = self.walkTimeMs - walkFrom;
	if (walkAdvance < 0)
	{
		walkAdvance += walk.durationMs;
	}
	for (size_t i = 0; i < gait->animations.size(); ++i)
	{
		const auto animation = gait->animations.at(i);
		CreatureLocomotion::Track track {.animation = animation,
		                                 .fromMs = static_cast<float>(walkFrom),
		                                 .advanceMs = static_cast<float>(walkAdvance),
		                                 .durationMs = static_cast<float>(walk.durationMs),
		                                 .weight = gait->weights.at(i),
		                                 .looping = true,
		                                 .breathing = false};
		if (animation == locomotion::animations::k_Stand)
		{
			track.durationMs = static_cast<float>(stand.durationMs);
			track.breathing = true;
		}
		else if (animation == locomotion::animations::k_Run)
		{
			// The run keeps in step with the walk
			const auto ratio = static_cast<float>(run.durationMs) / static_cast<float>(walk.durationMs);
			track.fromMs *= ratio;
			track.advanceMs *= ratio;
			track.durationMs = static_cast<float>(run.durationMs);
		}
		self.tracks.push_back(track);
	}
}

/// The circles of a thing that the creature walks round, none for a thing it walks through
std::vector<route::Circle> CirclesToAvoid(const ecs::Registry& registry, entt::entity entity, const obstacles::Walker& walker)
{
	const auto thing = things::ThingOf(registry, entity);
	if (!thing.has_value() || !obstacles::MustAvoid(*thing, walker))
	{
		return {};
	}
	// The Norse gate is a row of small circles across its width, with a way through the middle while it stands open
	if (registry.AnyOf<AnimatedStatic>(entity) && Locator::animatedStaticSystem::has_value())
	{
		if (const auto gate = Locator::animatedStaticSystem::value().RouteCircles(entity))
		{
			std::vector<route::Circle> circles;
			circles.reserve(gate->size());
			std::ranges::transform(*gate, std::back_inserter(circles), [](const auto& circle) {
				return route::Circle {.centre = circle.centre, .radius = circle.radius};
			});
			return circles;
		}
	}
	return obstacles::CirclesOf(*thing, walker);
}

/// What stands in a cell, added to what a creature walks round the first time its route search reaches the cell's
/// block: the land it can't walk on, then each thing it must avoid whose circles aren't filed in the block already
route::Obstacles::Gatherer GathererFor(entt::entity self, const obstacles::Walker& walker)
{
	return [self, walker](glm::ivec2 cell, route::Obstacles& into) {
		if (Locator::creatureLocomotionSystem::has_value())
		{
			route::AddLandCircles(Locator::creatureLocomotionSystem::value().GetWalkableLand(), cell, into);
		}
		if (!Locator::entitiesMap::has_value())
		{
			return;
		}
		const auto& registry = Locator::entitiesRegistry::value();
		for (const auto entity : Locator::entitiesMap::value().GetAllInCell(cell))
		{
			const auto key = entt::to_integral(entity);
			if (entity == self || into.BlockHas(key, cell))
			{
				continue;
			}
			for (const auto& circle : CirclesToAvoid(registry, entity, walker))
			{
				into.Add(key, circle.centre, circle.radius);
			}
		}
	};
}

/// How far from something's middle a creature walking up to it stops short, before its own radius
float ReachOf(const ecs::Registry& registry, entt::entity entity, const obstacles::Walker& walker)
{
	if (const auto thing = things::ThingOf(registry, entity))
	{
		return obstacles::ReachRadius(*thing, walker);
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

/// Whether it is on its way somewhere: waiting for its route, or turning, stepping or walking along it. A turn on the
/// spot to face a point is not.
bool IsOnItsWay(const CreatureLocomotion& self)
{
	return InMove(self);
}

/// Stops it at once where it is, as a new body action or a placement does
void Halt(CreatureLocomotion& self)
{
	// A turn on the spot that ended in this turn has turned the body: without the rest of its spin, the body shows at
	// the new heading at once
	const auto drawn =
	    locomotion::DrawnAfterStop({.from = self.fromHeading, .to = self.toHeading, .snaps = self.headingSnaps}, self.heading);
	self.fromHeading = drawn.from;
	self.toHeading = drawn.to;
	self.headingSnaps = drawn.snaps;
	ClearWalk(self);
	self.following.reset();
}

/// Where the head of the creature is in the pose of its newest game turn
glm::vec3 HeadAt(const Moves& moves, const CreatureAnimation& animation, const Transform& transform)
{
	const auto* points = moves.rig != nullptr && moves.rig->actionPoints.has_value() ? &*moves.rig->actionPoints : nullptr;
	if (points == nullptr || points->head >= animation.boneMatrices.size())
	{
		return transform.position;
	}
	const auto placement = creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
	const auto pose = creature::TurnPose(animation.newerTurnPose, animation.boneMatrices, placement);
	return glm::vec3(creature::PosedBone(points->head, pose.bones, pose.placement)[3]);
}

/// The hand that holds a thing, when the creature holds one: true for the left hand
std::optional<bool> HeldInLeft(entt::entity creature)
{
	const auto* held = Locator::entitiesRegistry::value().TryGet<const CreatureHeldObject>(creature);
	return held != nullptr ? std::optional(held->mirrored) : std::nullopt;
}

/// The look ahead along the search for the route: the point where the search has got to, at most once in a while
void StepRouteLook(CreatureLocomotion& self, int32_t stepMs)
{
	if (!self.destination.has_value())
	{
		return;
	}
	self.routeLook = creature_head_look::StepRouteLook(
	    self.routeLook, stepMs, {.searchHead = self.follower.GetSearchHead(), .destination = *self.destination}, HeightAt);
}

/// The first fidget while the creature waits for its route: it points at the route ahead. It turns on the spot first
/// when the point is too far round from its head. Nothing happens when the species has no pointing animations.
void StartRoutePoint(entt::entity creature, CreatureLocomotion& self, const WalkTurn& turn, const CreatureAnimation& animation,
                     const Transform& transform)
{
	StepRouteLook(self, turn.spinMs);
	if (!std::ranges::all_of(creature_pointing::k_Animations, [&turn](size_t clip) { return turn.moves.Has(clip); }))
	{
		return;
	}
	CreatureLocomotion::RoutePoint routePoint {.point = self.routeLook.point};
	const auto head = HeadAt(turn.moves, animation, transform);
	const auto angle = creature_pointing::TurnAngle(head, self.heading, routePoint.point);
	if (std::abs(angle) >= creature_pointing::k_TurnLimit)
	{
		BeginTurn(self, turn.moves, GameHeading::Of(routePoint.point.x - head.x, routePoint.point.z - head.z));
	}
	else
	{
		routePoint.ready = true;
		routePoint.mirrored = creature_pointing::PointsWithLeft(angle, HeldInLeft(creature));
	}
	self.motion = Motion::Pointing;
	self.routePoint = routePoint;
}

/// One turn of the arm that points at the route: it turns on the spot until it faces the point, then the four pointing
/// animations blend by where the point is from the top of the creature
void AimRoutePoint(entt::entity creature, CreatureLocomotion& self, const WalkTurn& turn, float size)
{
	auto& routePoint = *self.routePoint;
	bool turned = false;
	if (!routePoint.ready)
	{
		auto& move = *self.move;
		const auto from = move.timeMs;
		move.timeMs = std::min(move.timeMs + static_cast<float>(turn.spinMs), move.durationMs);
		if (move.timeMs < move.durationMs)
		{
			if (move.animations.has_value())
			{
				// The spin's own root turns the body; its heading changes once the turn is over
				TrackPair(self, from, move.timeMs - from);
			}
			else
			{
				self.heading = locomotion::LerpHeading(self.heading, self.targetHeading, from, move.durationMs);
			}
			return;
		}
		// It faces the point: the arm starts to point in the same turn, at the new heading
		self.heading = self.targetHeading;
		self.fromHeading = self.heading;
		self.move.reset();
		routePoint.ready = true;
		routePoint.timeMs = 0.0f;
		turned = true;
	}
	const auto position = self.toPosition;
	const auto origin = position + glm::vec3(0.0f, creature_pointing::k_OriginHeightPerSize * size, 0.0f);
	const auto aim = creature_pointing::AimAt(origin, self.heading, routePoint.point);
	if (turned)
	{
		routePoint.mirrored = creature_pointing::PointsWithLeftAfterTurn(aim, HeldInLeft(creature));
	}
	const auto weights = creature_pointing::Weights(aim, routePoint.mirrored);
	const auto pointMs =
	    turn.moves.rig != nullptr && turn.moves.rig->actionPoints.has_value() ? turn.moves.rig->actionPoints->pointMs : 0.0f;
	const auto from = routePoint.timeMs;
	routePoint.timeMs =
	    creature_pointing::RoutePointTime(from, turn.spinMs, pointMs, routePoint.ending,
	                                      static_cast<int32_t>(turn.moves.Duration(creature_pointing::k_Animations.front())));
	self.tracks.clear();
	for (size_t i = 0; i < creature_pointing::k_Animations.size(); ++i)
	{
		const auto clip = creature_pointing::k_Animations.at(i);
		self.tracks.push_back({.animation = clip,
		                       .fromMs = from,
		                       .advanceMs = std::max(routePoint.timeMs - from, 0.0f),
		                       .durationMs = turn.moves.Duration(clip),
		                       .weight = weights.at(i),
		                       .looping = false,
		                       .breathing = false,
		                       .mirrored = routePoint.mirrored});
	}
}

/// One turn of the point at the route. While it points, the wait for its next fidget counts down by the clip step,
/// and the point ends below zero. Each turn the creature looks ahead along the search again and points there. Once the
/// point ends and the arm gets to the end of its animations, it waits for its route again.
void RoutePointTurn(entt::entity creature, CreatureLocomotion& self, const WalkTurn& turn, float size)
{
	auto& routePoint = *self.routePoint;
	if (!routePoint.ending)
	{
		self.fidgetMs -= static_cast<float>(turn.spinMs);
		if (self.fidgetMs < 0.0f)
		{
			routePoint.ending = true;
		}
		else
		{
			StepRouteLook(self, turn.spinMs);
			routePoint.point = self.routeLook.point;
		}
	}
	else if (creature_pointing::RoutePointOver(routePoint.ready, routePoint.ending, routePoint.timeMs, turn.spinMs,
	                                           turn.moves.Duration(creature_pointing::k_Animations.at(2))))
	{
		// The events of the high animation to the left sound as the arm gets to its end
		const auto end = creature_pointing::k_Animations.at(2);
		if (turn.moves.rig != nullptr && end < turn.moves.rig->soundEvents.size() && Locator::creatureAudioSystem::has_value())
		{
			const auto durationMs = turn.moves.Duration(end);
			const auto [fromMs, toMs] = creature_pointing::RoutePointEndSounds(routePoint.timeMs, turn.spinMs, durationMs);
			for (const auto& fired :
			     creature_audio::EventsPassed(turn.moves.rig->soundEvents.at(end), static_cast<float>(fromMs),
			                                  static_cast<float>(toMs), static_cast<uint32_t>(durationMs), false))
			{
				Locator::creatureAudioSystem::value().PlayAction(creature, fired.event);
			}
		}
		self.motion = Motion::Planning;
		self.routePoint.reset();
		self.tracks.clear();
		self.fidgetMs = locomotion::k_FidgetMs;
		// The head looks ahead along the search again at once
		self.routeLook.countdownMs = 0;
		return;
	}
	AimRoutePoint(creature, self, turn, size);
}
} // namespace

void CreatureLocomotionSystem::SaveState(reflection::BinaryWriter& /*out*/) const
{
	// Each creature's walk is in its component, with the creature as its route search saw it
}

bool CreatureLocomotionSystem::LoadState(reflection::BinaryReader& /*in*/)
{
	// A loaded walk finds the things in its way again, from the creature as its route search saw it
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<CreatureLocomotion>([](entt::entity creature, CreatureLocomotion& self) {
		if (self.routeWalker.has_value())
		{
			self.follower.GetObstacles().SetGatherer(GathererFor(creature, *self.routeWalker));
		}
	});
	return true;
}

float CreatureLocomotionSystem::BodyRadius(CreatureType species)
{
	if (const auto found = _bodyRadii.find(species); found != _bodyRadii.end())
	{
		return found->second;
	}
	const auto measured = MeasureBodyRadius(species);
	if (!measured.has_value())
	{
		return k_UnknownBodyRadius;
	}
	_bodyRadii.emplace(species, *measured);
	return *measured;
}

const route::WalkableLand& CreatureLocomotionSystem::GetWalkableLand()
{
	if (!_land.has_value())
	{
		if (!Locator::terrainSystem::has_value())
		{
			_land.emplace();
			return *_land;
		}
		const auto& land = Locator::terrainSystem::value();
		// The corners' own altitudes rather than the drawn heights, which put everything at sea level at the bottom: only
		// corners right at the bottom make deep water, and shallows a little above it can be waded
		_land = route::WalkableLand::Build(
		    [&land](int32_t x, int32_t z) {
			    const auto* cell = land.FindCell(glm::u16vec2(static_cast<uint16_t>(x), static_cast<uint16_t>(z)));
			    return cell != nullptr ? static_cast<float>(cell->altitude) * LandIslandInterface::k_HeightUnit : 0.0f;
		    },
		    [&land](int32_t x, int32_t z) -> std::optional<bool> {
			    const auto* cell = land.FindCell(glm::u16vec2(static_cast<uint16_t>(x), static_cast<uint16_t>(z)));
			    if (cell == nullptr)
			    {
				    return std::nullopt;
			    }
			    return cell->properties.hasWater != 0;
		    });
	}
	return *_land;
}

std::vector<route::Circle> CreatureLocomotionSystem::ThingCirclesIn(entt::entity creature, glm::ivec2 cell)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* self = registry.TryGet<const CreatureLocomotion>(creature);
	if (body == nullptr || self == nullptr || !Locator::entitiesMap::has_value())
	{
		return {};
	}
	const auto walker = things::WalkerOf(registry, creature, *body, self->radius, PositionOf(*self));
	std::vector<route::Circle> circles;
	for (const auto entity : Locator::entitiesMap::value().GetAllInCell(cell))
	{
		if (entity != creature)
		{
			std::ranges::copy(CirclesToAvoid(registry, entity, walker), std::back_inserter(circles));
		}
	}
	return circles;
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::StartMove(entt::entity creature, CreatureLocomotion& self,
                                                                         glm::vec2 point, float fraction, float minDistance,
                                                                         float maxDistance, bool ontoDestination)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || !MovesOf(body->species).CanWalk())
	{
		return MoveResult::Busy;
	}
	const auto& land = GetWalkableLand();
	if (!land.IsValid(point, route::k_DestinationClearance))
	{
		return MoveResult::InvalidDestination;
	}
	self.fraction = fraction;
	self.failed = false;
	self.gaveUp.reset();
	self.facingOnly = false;
	if (IsMovingMotion(self.motion) && self.destination.has_value() &&
	    creature_cast_moves::SameDestination(*self.destination, point))
	{
		return MoveResult::Started;
	}

	// Not on a walk, it starts afresh from where it stands; straying somewhere it can't stand, it is put back on the
	// nearest place it can first
	auto position = PositionOf(self);
	const bool onAWalk = self.follower.IsSearching() || self.follower.IsFollowing();
	if (!onAWalk)
	{
		if (!land.IsValid(position, route::k_Clearance))
		{
			if (const auto valid = land.NearestStandable(position))
			{
				position = *valid;
				PlaceAt(self, position);
				self.fromPosition = self.toPosition;
			}
		}
		self.routeWalker = things::WalkerOf(registry, creature, *body, self.radius, position);
		self.follower.Reset(position, self.radius, GathererFor(creature, *self.routeWalker));
		// While it clears a thing out of its way, its first turn searches no further and notes nothing that it meets
		self.firstSearchTurn = !self.clearingWay;
	}
	const auto distance = glm::distance(position, point);
	self.ring = locomotion::MoveRing(distance, minDistance, maxDistance);
	self.ontoDestination = ontoDestination;
	self.destination = point;
	self.fidgetMs = locomotion::k_FidgetMs;
	self.fidgeted = false;
	// The head looks ahead along the new search at once
	self.routeLook.countdownMs = 0;
	self.follower.SetDestination({.point = point,
	                              .stopShort = self.ring.min,
	                              .arriveWithin = self.ring.max,
	                              .mostCost = (k_CostPerUnit * distance) + k_BaseCost});
	if (self.follower.GetState() == route::Follower::State::Planning && self.motion != Motion::Planning)
	{
		SetIdle(self);
		self.routeReady = false;
		self.motion = Motion::Planning;
	}
	return MoveResult::Started;
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::MoveTo(entt::entity creature, glm::vec2 point, Pace pace,
                                                                      float minDistance, float maxDistance)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (self == nullptr || body == nullptr || !self->started)
	{
		return MoveResult::Busy;
	}
	self->following.reset();
	const auto fractions = FractionsOf(body->species);
	const auto fraction = PaceFraction(pace, self->fraction, fractions);
	BreatheForWalk(creature, *body, fraction, fractions);
	return StartMove(creature, *self, point, fraction, minDistance, maxDistance, true);
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::LeadTo(entt::entity creature, glm::vec2 point, float extraSpeed,
                                                                      float maxDistance)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (self == nullptr || body == nullptr || !self->started)
	{
		return MoveResult::Busy;
	}
	self->following.reset();
	return StartMove(creature, *self, point, FractionsOf(body->species).walk + extraSpeed, 0.0f, maxDistance, true);
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::MoveToObject(entt::entity creature, entt::entity target,
                                                                            Pace pace, float extra)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	const auto* at = registry.TryGet<const Transform>(target);
	if (self == nullptr || at == nullptr)
	{
		return MoveResult::Busy;
	}
	const auto* body = Locator::entitiesRegistry::value().TryGet<const Creature>(creature);
	if (body == nullptr || !self->started)
	{
		return MoveResult::Busy;
	}
	// It stops short of the thing by how far round it the thing reaches, as the creature plans its route round it
	const auto walker = things::WalkerOf(registry, creature, *body, self->radius, PositionOf(*self));
	const auto ring = locomotion::ObjectRing(self->radius, ReachOf(registry, target, walker), extra);
	self->following.reset();
	const auto fractions = FractionsOf(body->species);
	const auto fraction = PaceFraction(pace, self->fraction, fractions);
	BreatheForWalk(creature, *body, fraction, fractions);
	return StartMove(creature, *self, glm::xz(at->position), fraction, ring.min, ring.max, false);
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::Follow(entt::entity creature, entt::entity target,
                                                                      float distance, Pace pace)
{
	const auto result = MoveToObject(creature, target, pace, distance);
	if (auto* self = Locator::entitiesRegistry::value().TryGet<CreatureLocomotion>(creature);
	    self != nullptr && result != MoveResult::Busy)
	{
		self->following = target;
		self->followDistance = distance;
	}
	return result;
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::FleeFrom(entt::entity creature, glm::vec2 threat, Pace pace)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (self == nullptr || body == nullptr || !self->started)
	{
		return MoveResult::Busy;
	}
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr || !Locator::terrainSystem::has_value() || !Locator::gameRandom::has_value())
	{
		return MoveResult::InvalidDestination;
	}
	const auto fractions = FractionsOf(body->species);
	const auto& island = Locator::terrainSystem::value();
	const auto& land = GetWalkableLand();
	const locomotion::RunAwayLand runAwayLand {
	    .cellClear =
	        [creature](int32_t x, int32_t z) {
		        return Locator::creatureMindSystem::has_value() &&
		               Locator::creatureMindSystem::value().IsCellClearFor(creature, x, z, false);
	        },
	    .ground = [&island](const map_coords::MapCoords& coords) { return island.GetAltitude(coords.x, coords.z); },
	    .canStand = [&land](glm::vec2 at) { return land.IsValid(at, route::k_DestinationClearance); },
	    .nearestStand = [&land](glm::vec2 at) { return land.NearestStandable(at); },
	};
	const auto draw = Locator::gameRandom::value().GameRand(locomotion::k_RunAwayExtra);
	const auto point = glm::xz(locomotion::RunAwayPoint(map_coords::FromWorld(island, transform->position), threat, draw,
	                                                    fractions.runAway, runAwayLand));
	self->following.reset();
	const auto fraction = PaceFraction(pace, self->fraction, fractions);
	BreatheForWalk(creature, *body, fraction, fractions);
	const float height = ShownSize(*body) * locomotion::k_RunAwayHeightOfSizeOne;
	return StartMove(creature, *self, point, fraction, 0.0f, height * locomotion::k_RunAwayArrivalHeights, false);
}

bool CreatureLocomotionSystem::TurnToFace(entt::entity creature, glm::vec2 point)
{
	const auto heading = HeadingToFace(creature, point);
	return heading.has_value() && TurnToHeading(creature, *heading);
}

bool CreatureLocomotionSystem::TurnToFaceAtOnce(entt::entity creature, glm::vec2 point)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* self = registry.TryGet<CreatureLocomotion>(creature); self != nullptr && self->started)
	{
		Halt(*self);
	}
	const auto heading = HeadingToFace(creature, point);
	return heading.has_value() && TurnToHeadingAtOnce(creature, *heading);
}

std::optional<GameHeading> CreatureLocomotionSystem::HeadingToFace(entt::entity creature, glm::vec2 point)
{
	const auto* self = Locator::entitiesRegistry::value().TryGet<const CreatureLocomotion>(creature);
	if (self == nullptr)
	{
		return std::nullopt;
	}
	const auto offset = point - PositionOf(*self);
	if (glm::length(offset) <= k_Tiny)
	{
		return std::nullopt;
	}
	// It always turns on the spot, also when it nearly faces the point already
	return locomotion::HeadingOf(offset);
}

bool CreatureLocomotionSystem::TurnToHeading(entt::entity creature, GameHeading heading)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	if (self == nullptr || !self->started)
	{
		return false;
	}
	// It stops first. While it is on its way or turns, the turn does not start: it walks on to its stop. While its
	// body plays an action, the turn does not start too.
	Stop(creature);
	if (!planner::MayTurnOnTheSpot(IsMovingMotion(self->motion), PlaysBodyAction(creature)))
	{
		return false;
	}
	return StartTurnOnTheSpot(creature, heading);
}

bool CreatureLocomotionSystem::TurnToHeadingAtOnce(entt::entity creature, GameHeading heading)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	if (self == nullptr || !self->started)
	{
		return false;
	}
	Halt(*self);
	return StartTurnOnTheSpot(creature, heading);
}

bool CreatureLocomotionSystem::PlaysBodyAction(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	    animation != nullptr && creature_layers::IsPlaying(animation->body))
	{
		return true;
	}
	// An action with a thing, such as a throw or a catch, plays its animations on the body too
	const auto* action = registry.TryGet<const CreatureObjectAction>(creature);
	return action != nullptr && action->phase == CreatureObjectAction::Phase::Playing &&
	       (action->status == creature_object_actions::Status::Running ||
	        action->status == creature_object_actions::Status::Contact);
}

bool CreatureLocomotionSystem::StartTurnOnTheSpot(entt::entity creature, GameHeading heading)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (self == nullptr || body == nullptr)
	{
		return false;
	}
	ClearWalk(*self);
	self->facingOnly = true;
	// The target is kept as it is given, and the turn to it is wrapped once
	BeginTurn(*self, MovesOf(body->species), heading);
	return true;
}

void CreatureLocomotionSystem::Place(entt::entity creature, glm::vec3 position)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* self = registry.TryGet<CreatureLocomotion>(creature))
	{
		self->fromPosition = position;
		self->toPosition = position;
	}
	if (auto* transform = registry.TryGet<Transform>(creature))
	{
		transform->position = position;
		registry.SetDirty();
	}
	// The frames draw it at the new point at once
	if (auto* animation = registry.TryGet<CreatureAnimation>(creature);
	    animation != nullptr && animation->drawnPlace.has_value())
	{
		animation->drawnPlace->position = position;
	}
}

void CreatureLocomotionSystem::PlaceWithoutWalking(entt::entity creature, glm::vec3 position)
{
	Place(creature, position);
	// The body keeps its last pose as both poses, at its old place, until its next turn
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().HoldTurnPose(creature);
	}
	auto& registry = Locator::entitiesRegistry::value();
	// The route starts again at the new place, and the walk or turn stops. A walk step of the plan is then complete.
	if (auto* self = registry.TryGet<CreatureLocomotion>(creature))
	{
		ClearWalk(*self);
	}
	// The body stands, no face shows, and the head looks straight ahead at nothing
	if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
	{
		animation->body = {};
		animation->face = creature_layers::DropFace(animation->face);
		animation->lookAt.reset();
		animation->yaw = {};
		animation->pitch = {};
	}
}

void CreatureLocomotionSystem::Stop(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	BreatheAtRest(creature);
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	if (self == nullptr)
	{
		return;
	}
	self->following.reset();
	if (!IsOnItsWay(*self))
	{
		return;
	}
	// Its route ends where its speed lets it slow to a stand, or where its turn or step ends. Its walk then slows to
	// that end, as at the end of any route.
	self->follower.Stop(planner::StopAhead(self->speed, JoinAhead(*self)));
	if (self->follower.IsFollowing())
	{
		self->destination = self->follower.GetRouteEnd();
	}
}

void CreatureLocomotionSystem::StopAtOnce(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* self = registry.TryGet<CreatureLocomotion>(creature))
	{
		Halt(*self);
	}
	BreatheAtRest(creature);
}

void CreatureLocomotionSystem::BreatheAtRest(entt::entity creature)
{
	// Stopped, it breathes at its resting period again
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* animation = registry.TryGet<CreatureAnimation>(creature); animation != nullptr)
	{
		if (const auto* body = registry.TryGet<const Creature>(creature))
		{
			animation->breathTarget = creature_animation::BreathPeriod(ShownSize(*body));
		}
	}
}

void CreatureLocomotionSystem::ResetPace(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (self != nullptr && body != nullptr)
	{
		self->fraction = FractionsOf(body->species).walk;
	}
}

float CreatureLocomotionSystem::KeptFraction(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* self = registry.TryGet<const CreatureLocomotion>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (self == nullptr || body == nullptr)
	{
		return 0.0f;
	}
	return locomotion::KeptFraction(self->fraction, FractionsOf(body->species).walk);
}

bool CreatureLocomotionSystem::IsMoving(entt::entity creature) const
{
	const auto* self = Locator::entitiesRegistry::value().TryGet<const CreatureLocomotion>(creature);
	return self != nullptr && IsMovingMotion(self->motion);
}

void CreatureLocomotionSystem::ShareSearchSteps()
{
	size_t searching = 0;
	Locator::entitiesRegistry::value().Each<const CreatureLocomotion>(
	    [&searching](entt::entity /*entity*/, const CreatureLocomotion& self) {
		    if (self.follower.IsSearching())
		    {
			    ++searching;
		    }
	    });
	_searchShare = route::TurnSearchShare(searching);
}

void CreatureLocomotionSystem::ProcessTurn()
{
	ShareSearchSteps();
	std::vector<entt::entity> walkers;
	Locator::entitiesRegistry::value().Each<CreatureLocomotion>(
	    [&walkers](entt::entity entity, CreatureLocomotion& /*self*/) { walkers.push_back(entity); });
	for (const auto entity : walkers)
	{
		CreatureTurn(entity);
	}
}

void CreatureLocomotionSystem::CreatureTurn(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<CreatureLocomotion>(entity))
	{
		return;
	}
	const auto& land = GetWalkableLand();

	// Following something, the walk is pointed anew as it moves away
	[&]() {
		auto& self = registry.Get<CreatureLocomotion>(entity);
		if (!self.following.has_value() || !self.started)
		{
			return;
		}
		const auto* at = registry.Valid(*self.following) ? registry.TryGet<const Transform>(*self.following) : nullptr;
		if (at == nullptr)
		{
			self.following.reset();
			return;
		}
		const auto target = glm::xz(at->position);
		const bool moved = IsMovingMotion(self.motion) && self.destination.has_value() &&
		                   glm::distance(target, *self.destination) > k_FollowRetarget;
		const bool left =
		    !IsMovingMotion(self.motion) && glm::distance(target, PositionOf(self)) > self.ring.max + k_FollowRetarget;
		if (moved || left)
		{
			// The walk that it points anew keeps the speed that it got last
			const auto followed = *self.following;
			const auto distance = self.followDistance;
			Follow(entity, followed, distance, Pace::Keep);
		}
	}();

	if (!registry.AllOf<Creature, CreatureLocomotion, Transform, CreatureAnimation>(entity))
	{
		return;
	}
	bool stopped = false;
	{
		const auto& creature = registry.Get<const Creature>(entity);
		auto& self = registry.Get<CreatureLocomotion>(entity);
		const auto& transform = registry.Get<const Transform>(entity);
		auto& animation = registry.Get<CreatureAnimation>(entity);
		if (!self.started)
		{
			self.started = true;
			self.heading = HeadingOfRotation(transform.rotation);
			self.toPosition = transform.position;
			self.toHeading = self.heading;
		}
		self.fromPosition = self.toPosition;
		self.fromHeading = self.headingSnaps ? self.heading : self.toHeading;
		self.toHeading = self.fromHeading;
		self.headingSnaps = false;
		const auto lastTracks = std::exchange(self.tracks, {});
		self.distance = 0.0f;
		Measure(self, creature, transform, BodyRadius(creature.species));
		const auto moves = MovesOf(creature.species);

		// The route is searched for a little each turn, further on a walk's first turn
		const auto standing = [&self]() {
			return self.follower.GetState() == route::Follower::State::Standing ||
			       self.follower.GetState() == route::Follower::State::Planning;
		};
		const bool wasStanding = standing();
		if (self.follower.IsSearching() && !self.searchHeld)
		{
			const auto first = std::exchange(self.firstSearchTurn, false);
			switch (self.follower.Update(first ? k_FirstTurnSearchSteps : _searchShare, first, JoinAhead(self)))
			{
			case route::Follower::Outcome::Found:
				if (self.motion == Motion::Planning || self.motion == Motion::Confused || self.motion == Motion::Pointing)
				{
					self.routeReady = true;
				}
				break;
			case route::Follower::Outcome::GaveUp:
				// It is shut in. It stands where it is, and its walk ends as if it arrived. The mind can clear one of
				// the things that the search met, then walk on.
				if (self.destination.has_value())
				{
					self.gaveUp = CreatureLocomotion::GaveUp {.destination = *self.destination, .met = self.follower.GetMet()};
				}
				ClearWalk(self);
				break;
			case route::Follower::Outcome::Failed:
				// No way there: it stands where it is
				ClearWalk(self);
				self.failed = true;
				break;
			case route::Follower::Outcome::None:
				break;
			}
		}
		const auto* needs = registry.TryGet<const CreatureNeeds>(entity);
		const auto fractions = FractionsOf(creature.species);
		// Exhausted, the creature gets its slow speed and keeps it until a new action or a run sets a different speed
		self.fraction = locomotion::RequiredFraction(
		    self.fraction, fractions.slow, needs != nullptr ? needs->needs.exhaustion : 0.0f, registry.AnyOf<InHand>(entity));
		// A frozen creature lives fewer milliseconds in each turn: its steps, turns and waits slow down. Its walk along
		// the route does not.
		const auto turnMs = planner::TurnMs(animation.playbackScale, static_cast<int32_t>(k_TurnMs));
		const WalkTurn walkTurn {.moves = moves,
		                         .topSpeed = locomotion::TargetSpeed(self.fraction, self.speeds.run),
		                         .breathPhase = animation.breathPhase,
		                         .turnMs = turnMs,
		                         .spinMs = animation.realTime ? turnMs : planner::ClipStepMs(turnMs, ShownSize(creature)),
		                         .lastTracks = lastTracks,
		                         .land = land};

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
					StartLeg(self, walkTurn, true);
				}
				break;
			}
			{
				// The wait counts down by the milliseconds that the animations play
				locomotion::FidgetWait wait {.ms = self.fidgetMs, .fidgeted = self.fidgeted};
				const auto turn = locomotion::WaitTurn(wait, static_cast<float>(walkTurn.spinMs),
				                                       !creature_layers::IsPlaying(animation.body), GameRand);
				self.fidgetMs = wait.ms;
				self.fidgeted = wait.fidgeted;
				if (turn.fidget == locomotion::Fidget::Puzzled)
				{
					animation.face =
					    creature_layers::PullFace(animation.face, k_PuzzledFace, k_PuzzledMs, creature_face::Cue::Lost);
				}
				else if (turn.fidget == locomotion::Fidget::Confused)
				{
					if (auto confused =
					        creature_layers::PlayOnce(animation.body, creature_layers::animations::k_Confused, turn.mirrored))
					{
						// The confused look is a part of the walk, and keeps its pace when the creature is told to hurry
						confused->keepsPace = true;
						animation.body = *confused;
						self.motion = Motion::Confused;
					}
				}
				else if (turn.fidget == locomotion::Fidget::LookAbout)
				{
					// It points at the route ahead, from this turn on
					StartRoutePoint(entity, self, walkTurn, animation, transform);
					if (self.motion == Motion::Pointing)
					{
						StepRouteLook(self, walkTurn.spinMs);
						self.routePoint->point = self.routeLook.point;
						AimRoutePoint(entity, self, walkTurn, ShownSize(creature));
					}
				}
			}
			break;
		case Motion::Pointing:
			RoutePointTurn(entity, self, walkTurn, ShownSize(creature));
			break;
		case Motion::Confused:
			if (!creature_layers::IsPlaying(animation.body))
			{
				self.motion = Motion::Planning;
				self.fidgetMs = locomotion::k_FidgetMs;
			}
			break;
		case Motion::Turning:
			TurnStep(self, walkTurn);
			break;
		case Motion::Stepping:
			StepOff(self, walkTurn);
			break;
		case Motion::Walking:
			Walk(self, walkTurn);
			break;
		}
		// A stop while it waited for its route ended the search: it stands
		if (self.motion == Motion::Planning && self.follower.GetState() == route::Follower::State::Standing)
		{
			StandPose(self, moves);
			Arrive(self);
		}
		// A stop while it pointed at the route ends the point
		if (self.motion == Motion::Pointing && self.follower.GetState() == route::Follower::State::Standing)
		{
			self.routePoint->ending = true;
		}

		// A step or the walk put it somewhere it cannot stand: it is put back where it can, or stays where it is when
		// there is no such place, and sets off again. When it cannot set off again, it cannot find a route.
		if (walkTurn.strayed && self.destination.has_value())
		{
			PlaceAt(self, land.NearestStandable(PositionOf(self)).value_or(PositionOf(self)));
			self.fromPosition = self.toPosition;
			const auto destination = *self.destination;
			const auto ring = self.ring;
			ClearWalk(self);
			if (StartMove(entity, self, destination, self.fraction, ring.min, ring.max, self.ontoDestination) !=
			    MoveResult::Started)
			{
				self.failed = true;
			}
		}
		self.toHeading = self.headingSnaps ? self.fromHeading : self.heading;
		stopped = !wasStanding && standing();
	}

	// A creature that has just stopped is now in the way of the others still on their walks
	if (!stopped)
	{
		return;
	}
	[&]() {
		const auto thing = things::ThingOf(registry, entity);
		if (!thing.has_value())
		{
			return;
		}
		registry.Each<const Creature, CreatureLocomotion>(
		    [&](entt::entity other, const Creature& body, CreatureLocomotion& walking) {
			    if (other == entity || !(walking.follower.IsSearching() || walking.follower.IsFollowing()))
			    {
				    return;
			    }
			    const auto walker = things::WalkerOf(registry, other, body, walking.radius, PositionOf(walking));
			    if (!obstacles::MustAvoid(*thing, walker))
			    {
				    return;
			    }
			    for (const auto& circle : obstacles::CirclesOf(*thing, walker))
			    {
				    walking.follower.GetObstacles().Add(entt::to_integral(entity), circle.centre, circle.radius);
			    }
		    });
	}();
}

namespace
{
/// Places a creature and poses its legs at the end of its turn. The frames draw it there until the next frame. It is
/// true when the place or the heading changed.
bool ShowTurnEndOf(const CreatureLocomotion& self, Transform& transform, CreatureAnimation& animation)
{
	auto place = drawn_walk::PlaceAt(self, animation.breathPhase, 1.0f);
	const bool moved = place.position != transform.position || place.rotation != transform.rotation;
	transform.position = place.position;
	transform.rotation = place.rotation;
	animation.slots = place.slots;
	animation.drawnPlace = std::move(place);
	return moved;
}

} // namespace

void CreatureLocomotionSystem::Update(float turnFraction)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto f = std::clamp(turnFraction, 0.0f, 1.0f);
	bool moved = false;
	// The frame only draws: each walking creature is drawn between the places of its last two turns. The god's hand
	// draws a creature that it holds where the hand is.
	registry.Each<const CreatureLocomotion, const Transform, CreatureAnimation>(
	    [&](entt::entity entity, const CreatureLocomotion& self, const Transform& /*transform*/, CreatureAnimation& animation) {
		    if (!self.started || registry.AllOf<InHand>(entity))
		    {
			    if (animation.drawnPlace.has_value())
			    {
				    animation.drawnPlace.reset();
				    moved = true;
			    }
			    return;
		    }
		    auto place = drawn_walk::PlaceAt(self, animation.breathPhase, f);
		    if (drawn_walk::Changed(animation.drawnPlace, place))
		    {
			    animation.drawnPlace = std::move(place);
			    moved = true;
		    }
	    });
	if (moved)
	{
		registry.SetDirty();
	}
}

void CreatureLocomotionSystem::ShowTurnEnds()
{
	auto& registry = Locator::entitiesRegistry::value();
	bool moved = false;
	registry.Each<CreatureLocomotion, Transform, CreatureAnimation>(
	    [&](CreatureLocomotion& self, Transform& transform, CreatureAnimation& animation) {
		    if (self.started && ShowTurnEndOf(self, transform, animation))
		    {
			    moved = true;
		    }
	    });
	if (moved)
	{
		registry.SetDirty();
	}
}

void CreatureLocomotionSystem::ShowTurnEnd(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	auto* transform = registry.TryGet<Transform>(creature);
	auto* animation = registry.TryGet<CreatureAnimation>(creature);
	if (self != nullptr && transform != nullptr && animation != nullptr && self->started &&
	    ShowTurnEndOf(*self, *transform, *animation))
	{
		registry.SetDirty();
	}
}
