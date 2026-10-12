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
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureObstacles.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureRouteFollow.h"
#include "Creature/CreatureRouteSearch.h"
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
#include "ECS/Components/WorshipSite.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/WallHugRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/ResourcePiles.h"
#include "Magic/ShieldRules.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
namespace locomotion = openblack::creature_locomotion;
namespace route = openblack::creature_route;
namespace obstacles = openblack::creature_obstacles;
using Motion = CreatureLocomotion::Motion;

namespace
{
constexpr float k_TurnMs = std::chrono::duration<float, std::milli>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnSeconds = k_TurnMs / 1000.0f;
/// Steps of the route search each turn: more on a new walk's first turn
constexpr int32_t k_FirstTurnSearchSteps = 256;
constexpr int32_t k_TurnSearchSteps = 16;
/// A route costing more than this much a unit of the straight distance, and this much besides, is given up
constexpr float k_CostPerUnit = 2.0f;
constexpr float k_BaseCost = 5000.0f;
/// Turning on the spot as a route is joined while walking, the join is searched from this far ahead
constexpr float k_TurningJoinAhead = 0.05f;
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
/// Routes' corners are rounded at the creature's radius, and no tighter than this
constexpr float k_MinCornerRadius = 6.0f;
/// What is known of a species without the game's creature tables: the ape's speeds
constexpr float k_DefaultSlowFraction = 0.5f;
constexpr float k_DefaultWalkFraction = 0.7f;
constexpr float k_DefaultRunFraction = 0.9f;
constexpr float k_DefaultRunAwayDistance = 50.0f;
/// Turning or stepping, the steps never move it less than this a turn
constexpr float k_Tiny = 1e-4f;
/// A building no more built than this on its site is only just begun
constexpr float k_JustBegun = 0.01f;
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
	/// The pair blended into one animation lasts between their lengths, to the nearest millisecond
	[[nodiscard]] float PairDuration(const locomotion::Pair& pair) const
	{
		return std::nearbyint(Duration(pair.from) + ((Duration(pair.to) - Duration(pair.from)) * pair.weight));
	}
	/// The pair blended into one animation moves the way between theirs, as far as between their moves' lengths
	[[nodiscard]] glm::vec2 PairDisplacement(const locomotion::Pair& pair) const
	{
		const auto from = Displacement(pair.from);
		const auto to = Displacement(pair.to);
		const auto move = creature_animation::BlendMoves(glm::vec3(from, 0.0f), glm::vec3(to, 0.0f), pair.weight);
		return {move.x, move.y};
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

float HeadingOfRotation(const glm::mat3& rotation)
{
	// The body is turned about y; its mesh looks back along +z
	return std::atan2(rotation[2].x, rotation[2].z);
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
}

void SetIdle(CreatureLocomotion& self)
{
	self.motion = Motion::Standing;
	self.speed = 0.0f;
	self.move.reset();
	self.tracks.clear();
}

void ClearWalk(CreatureLocomotion& self)
{
	SetIdle(self);
	self.follower.Reset(glm::xz(self.toPosition), self.radius);
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

/// A turn's or step's two animations, blended keyframe by keyframe into one that plays over the move
void TrackPair(CreatureLocomotion& self, float fromMs, float advanceMs)
{
	if (!self.move.has_value() || !self.move->animations.has_value())
	{
		return;
	}
	const auto& pair = *self.move->animations;
	self.tracks.push_back({.animation = pair.from,
	                       .fromMs = fromMs,
	                       .advanceMs = advanceMs,
	                       .durationMs = self.move->durationMs,
	                       .weight = 1.0f,
	                       .looping = false,
	                       .breathing = false,
	                       .blendedWith = pair.to,
	                       .blendWeight = pair.weight});
}

/// Turns on the spot or steps off towards the end of the route's segment, or walks straight on, by how far round it is
void BeginLeg(CreatureLocomotion& self, const Moves& moves)
{
	const auto& route = self.follower.GetRoute();
	const auto remaining = route.Remaining();
	if (route.Finished() || remaining <= k_Tiny)
	{
		self.motion = Motion::Walking;
		return;
	}
	self.targetHeading = route.Heading();
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
	const auto distanceInMesh = self.scale > 0.0f ? remaining / self.scale : 0.0f;
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
		                                      .startRouteHeading = route.Heading()};
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
		                                      .startRouteHeading = route.Heading()};
		break;
	}
}

void Arrive(CreatureLocomotion& self)
{
	ClearWalk(self);
	self.failed = false;
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
		return k_TurningJoinAhead;
	}
	if (self.motion == Motion::Stepping && self.move.has_value())
	{
		return (glm::length(self.move->displacement) * self.scale) - self.follower.GetRoute().travelled;
	}
	return 0.0f;
}

void TurnStep(CreatureLocomotion& self, const Moves& moves, float playbackRate)
{
	auto& move = *self.move;
	const auto advance = k_TurnMs * playbackRate;
	const auto from = move.timeMs;
	move.timeMs = std::min(move.timeMs + advance, move.durationMs);
	if (move.animations.has_value())
	{
		// The spin's own root turns the body; its heading changes once the turn is over
		TrackPair(self, from, move.timeMs - from);
	}
	else
	{
		self.heading = locomotion::LerpHeading(move.startHeading, self.targetHeading, move.timeMs, move.durationMs);
	}
	if (move.timeMs < move.durationMs)
	{
		return;
	}
	// The spin plays out over this turn, ending with the body turned to face the new way, which its heading takes up at
	// once as the next turn starts
	self.heading = self.targetHeading;
	self.headingSnaps = true;
	self.move.reset();
	auto lastPart = std::move(self.tracks);
	if (self.facingOnly || self.follower.GetRoute().legs.empty())
	{
		ClearWalk(self);
	}
	else
	{
		BeginLeg(self, moves);
	}
	self.tracks = std::move(lastPart);
}

void StepOff(CreatureLocomotion& self, const Moves& moves)
{
	auto& move = *self.move;
	const auto from = move.timeMs;
	move.timeMs = std::min(move.timeMs + k_TurnMs, move.durationMs);
	const auto fraction = move.durationMs > 0.0f ? (move.timeMs - from) / move.durationMs : 1.0f;
	// The step's move carries it along the route
	const auto distance = glm::length(move.displacement) * self.scale * fraction;
	self.speed = distance / k_TurnSeconds;
	self.distance = distance;
	const auto advanced = self.follower.Advance(distance);
	PlaceAt(self, advanced.position);
	if (advanced.finished)
	{
		RouteEnded(self);
		return;
	}
	// The step's own root turns the body round towards the route as it steps; its heading follows once the step ends
	TrackPair(self, from, move.timeMs - from);

	if (std::abs(locomotion::WrapAngle(advanced.heading - move.startRouteHeading)) > locomotion::k_CornerAngle)
	{
		self.tracks.clear();
		self.move.reset();
		BeginLeg(self, moves);
		return;
	}
	if (move.timeMs >= move.durationMs)
	{
		// It leaves the step already walking, along the route: the step plays out over this turn, ending with the body
		// turned towards the route, which its heading takes up at once as the next turn starts
		self.heading = advanced.heading;
		self.headingSnaps = true;
		self.move.reset();
		self.motion = Motion::Walking;
		self.speed = self.speeds.walk;
	}
}

void Walk(CreatureLocomotion& self, const Moves& moves, float targetSpeed, float breathPhase)
{
	const auto& route = self.follower.GetRoute();
	if (route.Finished())
	{
		RouteEnded(self);
		return;
	}
	auto cap = locomotion::StopCap(route.RemainingTotal());
	if (std::abs(route.TurnAtEnd()) >= locomotion::k_CornerAngle)
	{
		// Too sharp a corner to walk round: it slows to stop there
		cap = std::min(cap, locomotion::CornerCap(route.Remaining(), 0.0f));
	}
	const auto position = PositionOf(self);
	const auto ahead = position + (locomotion::DirectionOf(self.heading) * self.radius);
	const auto slope = locomotion::SlopeFactor(HeightAt(ahead), HeightAt(position), self.radius);
	self.speed = std::min(locomotion::Accelerate(self.speed, slope * targetSpeed, k_TurnSeconds), cap);

	const auto distance = self.speed * k_TurnSeconds;
	self.distance = distance;
	const auto advanced = self.follower.Advance(distance);
	PlaceAt(self, advanced.position);
	if (advanced.finished)
	{
		RouteEnded(self);
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

/// What a thing on the map is to a creature planning its route round it, none for what it never meets
std::optional<obstacles::Thing> ThingOf(const ecs::Registry& registry, entt::entity entity)
{
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr || registry.AnyOf<InHand>(entity))
	{
		return std::nullopt;
	}
	obstacles::Thing thing {.avoidance = obstacles::Avoidance::Object,
	                        .shape = obstacles::Shape::ModelBox,
	                        .position = glm::xz(transform->position),
	                        .yAngle = obstacles::YAngleOf(transform->rotation),
	                        .scale = std::abs(transform->scale.x),
	                        .boxCentre = glm::vec3(0.0f),
	                        .boxHalfSize = glm::vec3(0.0f),
	                        .height = 0.0f,
	                        .width = 0.0f,
	                        .burning = false,
	                        .hasFire = registry.AnyOf<Fire>(entity),
	                        .justBegun = false,
	                        .walking = false,
	                        .playerAlignment = 0.0f,
	                        .worshipSites = {},
	                        .owner = PlayerNames::NEUTRAL};
	thing.burning = thing.hasFire && Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(entity);

	if (const auto* other = registry.TryGet<const CreatureLocomotion>(entity))
	{
		const auto* body = registry.TryGet<const Creature>(entity);
		thing.avoidance = obstacles::Avoidance::Creature;
		thing.shape = obstacles::Shape::Creature;
		thing.width = other->shownRadius;
		thing.height = body != nullptr ? obstacles::k_HeightPerSize * ShownSize(*body) : 0.0f;
		// Walking along a route of its own: setting off on it, or on its way
		thing.walking = other->follower.IsFollowing();
		return thing;
	}
	if (registry.AnyOf<Villager, Animal, FishFarm, StreetLantern, TownDesireFlag>(entity))
	{
		return std::nullopt;
	}
	if (const auto* shield = registry.TryGet<const MagicShield>(entity))
	{
		// Every shield's object is the solid shield's model's box, the spiritual one at its radius's scale
		thing.avoidance = obstacles::Avoidance::Shield;
		thing.owner = shield->player;
		const auto* dome = registry.TryGet<const ShieldDome>(entity);
		thing.scale = dome != nullptr ? dome->solidScale : shield->radius * magic::shield::k_DomeScalePerRadius;
		const auto meshId = resources::HashIdentifier(MeshId::SpellSolidShield);
		if (Locator::resources::has_value() && Locator::resources::value().GetMeshes().Contains(meshId))
		{
			const auto box = Locator::resources::value().GetMeshes().Handle(meshId)->GetBoundingBox();
			thing.boxCentre = box.Center();
			thing.boxHalfSize = box.Size() * 0.5f;
			thing.height = obstacles::ModelHeight(thing.boxHalfSize, thing.scale);
			thing.width = obstacles::ModelWidth(thing.boxHalfSize, thing.scale);
		}
		return thing;
	}

	// The model's box gives most things their size
	if (const auto* mesh = registry.TryGet<const Mesh>(entity);
	    mesh != nullptr && Locator::resources::has_value() && Locator::resources::value().GetMeshes().Contains(mesh->id))
	{
		const auto box = Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox();
		thing.boxCentre = box.Center();
		thing.boxHalfSize = box.Size() * 0.5f;
		thing.height = obstacles::ModelHeight(thing.boxHalfSize, thing.scale);
		thing.width = obstacles::ModelWidth(thing.boxHalfSize, thing.scale);
	}

	if (const auto* temple = registry.TryGet<const Temple>(entity))
	{
		thing.avoidance = obstacles::Avoidance::Building;
		thing.shape = obstacles::Shape::Temple;
		thing.yAngle = temple->yAngle;
		thing.playerAlignment =
		    Locator::alignmentSystem::has_value() ? Locator::alignmentSystem::value().GetPlayerAlignment(temple->owner) : 0.0f;
		if (const auto* worship = registry.TryGet<const CitadelWorship>(entity))
		{
			std::ranges::transform(worship->sites, thing.worshipSites.begin(),
			                       [](entt::entity site) { return site != entt::null; });
		}
	}
	else if (registry.AnyOf<Tree>(entity))
	{
		thing.avoidance = obstacles::Avoidance::Tree;
		thing.shape = obstacles::Shape::Trunk;
	}
	else if (registry.AnyOf<BigForest>(entity))
	{
		thing.avoidance = obstacles::Avoidance::Forest;
	}
	else if (registry.AnyOf<Field>(entity))
	{
		thing.avoidance = obstacles::Avoidance::BurningUnlessScripted;
		thing.width = obstacles::k_FieldWidth;
	}
	else if (registry.AnyOf<DeadTree>(entity))
	{
		thing.avoidance = obstacles::Avoidance::BurningUnlessScripted;
		thing.shape = obstacles::Shape::AtPlace;
	}
	else if (registry.AnyOf<AnimatedStatic>(entity))
	{
		thing.avoidance = obstacles::Avoidance::Always;
		thing.shape = obstacles::Shape::Scenery;
	}
	else if (const auto* mobileStatic = registry.TryGet<const MobileStatic>(entity))
	{
		// Fences are the outline of their boxes, rocks and the like one circle at their place
		const auto fence = Locator::infoConstants::has_value() &&
		                   ecs::wall_hug::IsFenceModel(
		                       Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(mobileStatic->type)).meshId);
		thing.shape = fence ? obstacles::Shape::ModelBox : obstacles::Shape::AtPlace;
	}
	else if (registry.AnyOf<TeleportStone>(entity))
	{
		thing.shape = obstacles::Shape::AtPlace;
		thing.width = obstacles::k_TeleportStoneWidth;
	}
	else if (const auto* pot = registry.TryGet<const Pot>(entity))
	{
		thing.shape = obstacles::Shape::AtPlace;
		if (Locator::infoConstants::has_value())
		{
			const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
			if (info.potType == PotType::PileFood)
			{
				// A food pile is as wide as it is raised by what it holds
				thing.avoidance = obstacles::Avoidance::BurningUnlessScripted;
				thing.width *= magic::piles::ProportionRaised(ResourceType::Food, pot->amount, info.maxAmountInPot);
			}
		}
	}
	else if (registry.AnyOf<Mobile, Ball, ScriptHighlight>(entity))
	{
		thing.shape = obstacles::Shape::AtPlace;
	}
	else if (registry.AnyOf<Abode, Feature, Flowers, SpellDispenser, VillageTotem, WorshipSite>(entity))
	{
		thing.avoidance = obstacles::Avoidance::Building;
	}

	if (thing.avoidance == obstacles::Avoidance::Building)
	{
		// A building only just begun, with a site taking wood brought to it, isn't walked round yet
		const auto* progress = registry.TryGet<const BuildProgress>(entity);
		const auto* sites = registry.TryGet<const BuildingSites>(entity);
		thing.justBegun = sites != nullptr && ecs::building_site_list::TakingBroughtWood(*sites) != nullptr &&
		                  progress != nullptr && progress->built <= k_JustBegun;
	}
	return thing;
}

/// A creature as what it walks round sees it
obstacles::Walker WalkerOf(const ecs::Registry& registry, entt::entity entity, const Creature& body,
                           const CreatureLocomotion& self, glm::vec2 position)
{
	return {.position = position,
	        .height = obstacles::k_HeightPerSize * ShownSize(body),
	        .radius = self.radius,
	        .scriptControlled = registry.AnyOf<ScriptControlled>(entity),
	        .owner = body.owner};
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
			const auto thing = ThingOf(registry, entity);
			if (!thing.has_value() || !obstacles::MustAvoid(*thing, walker))
			{
				continue;
			}
			// The Norse gate is a row of small circles across its width, with a way through the middle while it stands
			// open
			if (registry.AnyOf<AnimatedStatic>(entity) && Locator::animatedStaticSystem::has_value())
			{
				if (const auto gate = Locator::animatedStaticSystem::value().RouteCircles(entity))
				{
					for (const auto& circle : *gate)
					{
						into.Add(key, circle.centre, circle.radius);
					}
					continue;
				}
			}
			for (const auto& circle : obstacles::CirclesOf(*thing, walker))
			{
				into.Add(key, circle.centre, circle.radius);
			}
		}
	};
}

/// How far from something's middle a creature walking up to it stops short, before its own radius
float ReachOf(const ecs::Registry& registry, entt::entity entity, const obstacles::Walker& walker)
{
	if (const auto thing = ThingOf(registry, entity))
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
} // namespace

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
	self.facingOnly = false;
	if (IsMovingMotion(self.motion) && self.destination.has_value() &&
	    glm::distance(*self.destination, point) <= k_SameDestination)
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
			if (const auto valid = land.NearestValid(position, route::k_Clearance, k_StraySearch))
			{
				position = *valid;
				PlaceAt(self, position);
				self.fromPosition = self.toPosition;
			}
		}
		self.follower.Reset(position, self.radius, GathererFor(creature, WalkerOf(registry, creature, *body, self, position)));
		self.firstSearchTurn = true;
	}
	const auto distance = glm::distance(position, point);
	self.ring = locomotion::MoveRing(distance, minDistance, maxDistance);
	self.ontoDestination = ontoDestination;
	self.destination = point;
	self.fidgetMs = k_FidgetMs;
	self.fidgeted = false;
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
	return StartMove(creature, *self, point, pace == Pace::Run ? fractions.run : fractions.walk, minDistance, maxDistance,
	                 true);
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::LeadTo(entt::entity creature, glm::vec2 point, float pull,
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
	const auto fractions = FractionsOf(body->species);
	return StartMove(creature, *self, point, locomotion::LeashFraction(fractions.walk, fractions.run, pull), 0.0f, maxDistance,
	                 true);
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
	const auto walker = WalkerOf(registry, creature, *body, *self, PositionOf(*self));
	const auto ring = locomotion::ObjectRing(self->radius, ReachOf(registry, target, walker), extra);
	self->following.reset();
	const auto fractions = FractionsOf(body->species);
	return StartMove(creature, *self, glm::xz(at->position), pace == Pace::Run ? fractions.run : fractions.walk, ring.min,
	                 ring.max, false);
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

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::FleeFrom(entt::entity creature, glm::vec2 threat)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (self == nullptr || body == nullptr || !self->started)
	{
		return MoveResult::Busy;
	}
	const auto fractions = FractionsOf(body->species);
	const auto distance =
	    static_cast<float>(std::uniform_int_distribution<uint32_t>(0, k_RunAwayExtra - 1)(_random)) + fractions.runAway;
	auto point = locomotion::RunAwayPoint(PositionOf(*self), threat, distance);
	const auto& land = GetWalkableLand();
	if (!land.IsValid(point, route::k_DestinationClearance))
	{
		const auto valid = land.NearestValid(point, route::k_DestinationClearance, k_RunAwaySearch);
		if (!valid.has_value())
		{
			return MoveResult::InvalidDestination;
		}
		point = *valid;
	}
	self->following.reset();
	return StartMove(creature, *self, point, fractions.run, 0.0f, k_RunAwayArrival, false);
}

bool CreatureLocomotionSystem::TurnToFace(entt::entity creature, glm::vec2 point)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (self == nullptr || body == nullptr || !self->started)
	{
		return false;
	}
	const auto offset = point - PositionOf(*self);
	if (glm::length(offset) <= k_Tiny)
	{
		return false;
	}
	ClearWalk(*self);
	self->following.reset();
	self->facingOnly = true;
	self->targetHeading = locomotion::HeadingOf(offset);
	const auto angle = locomotion::WrapAngle(self->targetHeading - self->heading);
	const auto moves = MovesOf(body->species);
	const auto firstSpin = locomotion::SideOf(angle) == locomotion::Side::Right ? locomotion::animations::k_RightSpin
	                                                                            : locomotion::animations::k_LeftSpin;
	const auto start = locomotion::ChooseStart(
	    angle, 0.0f, {.stepStrides = std::nullopt, .hasSpins = moves.HasAll(firstSpin), .moving = false});
	if (start.kind != locomotion::Start::Kind::Turn)
	{
		// Nearly facing it already
		self->heading = self->targetHeading;
		self->facingOnly = false;
		return true;
	}
	self->motion = Motion::Turning;
	self->move =
	    CreatureLocomotion::Move {.animations = start.animations,
	                              .timeMs = 0.0f,
	                              .durationMs = start.animations.has_value() ? moves.PairDuration(*start.animations)
	                                                                         : moves.Duration(locomotion::animations::k_Stand),
	                              .displacement = glm::vec2(0.0f),
	                              .startHeading = self->heading,
	                              .startRouteHeading = self->heading};
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
}

void CreatureLocomotionSystem::Stop(entt::entity creature)
{
	if (auto* self = Locator::entitiesRegistry::value().TryGet<CreatureLocomotion>(creature))
	{
		ClearWalk(*self);
		self->following.reset();
	}
}

bool CreatureLocomotionSystem::IsMoving(entt::entity creature) const
{
	const auto* self = Locator::entitiesRegistry::value().TryGet<const CreatureLocomotion>(creature);
	return self != nullptr && IsMovingMotion(self->motion);
}

void CreatureLocomotionSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& land = GetWalkableLand();

	// Following something, the walk is pointed anew as it moves away
	std::vector<std::pair<entt::entity, entt::entity>> retarget;
	registry.Each<CreatureLocomotion>([&](entt::entity entity, CreatureLocomotion& self) {
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
			retarget.emplace_back(entity, *self.following);
		}
	});
	for (const auto& [entity, target] : retarget)
	{
		const auto& self = registry.Get<CreatureLocomotion>(entity);
		const auto& body = registry.Get<Creature>(entity);
		const auto distance = self.followDistance;
		const auto fractions = FractionsOf(body.species);
		Follow(entity, target, distance, self.fraction >= fractions.run ? Pace::Run : Pace::Walk);
	}

	std::vector<entt::entity> stopped;
	registry.Each<const Creature, CreatureLocomotion, const Transform, CreatureAnimation>(
	    [&](entt::entity entity, const Creature& creature, CreatureLocomotion& self, const Transform& transform,
	        CreatureAnimation& animation) {
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
		    self.tracks.clear();
		    self.distance = 0.0f;
		    Measure(self, creature, transform, BodyRadius(creature.species));
		    const auto moves = MovesOf(creature.species);

		    // The route is searched for a little each turn, further on a walk's first turn
		    const auto standing = [&self]() {
			    return self.follower.GetState() == route::Follower::State::Standing ||
			           self.follower.GetState() == route::Follower::State::Planning;
		    };
		    const bool wasStanding = standing();
		    if (self.follower.IsSearching())
		    {
			    const auto first = std::exchange(self.firstSearchTurn, false);
			    switch (self.follower.Update(first ? k_FirstTurnSearchSteps : k_TurnSearchSteps, first, JoinAhead(self)))
			    {
			    case route::Follower::Outcome::Found:
				    if (self.motion == Motion::Planning || self.motion == Motion::Confused)
				    {
					    self.routeReady = true;
				    }
				    break;
			    case route::Follower::Outcome::Failed:
			    case route::Follower::Outcome::GaveUp:
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
			    self.fidgetMs -= k_TurnMs;
			    if (self.fidgetMs <= 0.0f)
			    {
				    // The first fidget is always a look about
				    const auto choice = self.fidgeted ? std::uniform_int_distribution<uint32_t>(0, 2)(_random) : 0u;
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
				    else if (auto confused = creature_layers::PlayOnce(animation.body, creature_layers::animations::k_Confused,
				                                                       std::bernoulli_distribution(0.5)(_random)))
				    {
					    animation.body = *confused;
					    self.motion = Motion::Confused;
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
			    TurnStep(self, moves, creature_layers::PlaybackRate(ShownSize(creature)));
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
		        !land.IsValid(PositionOf(self), route::k_Clearance))
		    {
			    if (const auto valid = land.NearestValid(PositionOf(self), route::k_Clearance, k_StraySearch))
			    {
				    PlaceAt(self, *valid);
				    self.fromPosition = self.toPosition;
				    const auto destination = *self.destination;
				    const auto ring = self.ring;
				    ClearWalk(self);
				    StartMove(entity, self, destination, self.fraction, ring.min, ring.max, self.ontoDestination);
			    }
		    }
		    self.toHeading = self.headingSnaps ? self.fromHeading : self.heading;
		    if (!wasStanding && standing())
		    {
			    stopped.push_back(entity);
		    }
	    });

	// A creature that has just stopped is now in the way of the others still on their walks
	for (const auto entity : stopped)
	{
		const auto thing = ThingOf(registry, entity);
		if (!thing.has_value())
		{
			continue;
		}
		registry.Each<const Creature, CreatureLocomotion>(
		    [&](entt::entity other, const Creature& body, CreatureLocomotion& walking) {
			    if (other == entity || !(walking.follower.IsSearching() || walking.follower.IsFollowing()))
			    {
				    return;
			    }
			    const auto walker = WalkerOf(registry, other, body, walking, PositionOf(walking));
			    if (!obstacles::MustAvoid(*thing, walker))
			    {
				    return;
			    }
			    for (const auto& circle : obstacles::CirclesOf(*thing, walker))
			    {
				    walking.follower.GetObstacles().Add(entt::to_integral(entity), circle.centre, circle.radius);
			    }
		    });
	}
}

void CreatureLocomotionSystem::Update(float turnFraction)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto f = std::clamp(turnFraction, 0.0f, 1.0f);
	bool moved = false;
	registry.Each<CreatureLocomotion, Transform, CreatureAnimation>(
	    [&](CreatureLocomotion& self, Transform& transform, CreatureAnimation& animation) {
		    if (!self.started)
		    {
			    return;
		    }
		    const auto position = self.fromPosition + ((self.toPosition - self.fromPosition) * f);
		    const auto heading = self.fromHeading + (locomotion::WrapAngle(self.toHeading - self.fromHeading) * f);
		    const auto rotation = glm::mat3(glm::eulerAngleY(heading));
		    if (position != transform.position || rotation != transform.rotation)
		    {
			    transform.position = position;
			    transform.rotation = rotation;
			    moved = true;
		    }
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
			    animation.slots.push_back({.animation = track.animation,
			                               .timeMs = time,
			                               .weight = track.weight,
			                               .mirrored = false,
			                               .blendedWith = track.blendedWith,
			                               .blendWeight = track.blendWeight});
		    }
	    });
	if (moved)
	{
		registry.SetDirty();
	}
}
