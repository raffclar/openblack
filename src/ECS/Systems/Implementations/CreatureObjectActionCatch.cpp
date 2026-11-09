/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>
#include <span>
#include <utility>
#include <vector>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureCatch.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureRoute.h"
#include "CreatureBodyBusy.h"
#include "CreatureObjectActionSystem.h"
#include "CreatureObjectActionSystemDetail.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/Map.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "Sound/SoundRequestInterface.h"
#include "VillagerPhysics.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
using openblack::creature_object_actions::Kind;
using openblack::creature_object_actions::Status;
using Phase = openblack::ecs::components::CreatureObjectAction::Phase;
using namespace openblack::ecs::systems::object_action_detail;

namespace
{
/// Whether a catch must wait before it starts. It waits while the body plays any action or does an action on a thing,
/// and while the creature walks, turns or waits for its route. A stop lets a walking creature slow to a stand first.
bool CatchWaits(entt::entity creature, const CreatureObjectActionSystemInterface& hands)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* locomotion =
	    Locator::creatureLocomotionSystem::has_value() ? &Locator::creatureLocomotionSystem::value() : nullptr;
	const auto body = creature_body_busy::ActivityOf(registry, creature, locomotion, &hands);
	return creature_catch::WaitsToStart(creature_body::ActsBesidesMoving(body), body.moving);
}

/// The thing's centre and velocity while it is in the physics, flying or resting there
std::optional<std::pair<glm::vec3, glm::vec3>> FlightOf(entt::entity object)
{
	if (!Locator::dynamicsSystem::has_value())
	{
		return std::nullopt;
	}
	const auto* entry = Locator::dynamicsSystem::value().Find(object);
	if (entry == nullptr || entry->body == nullptr)
	{
		return std::nullopt;
	}
	return std::pair {entry->body->Centre(), entry->body->velocity};
}

/// Whether a side step towards a catch may be taken: not while the creature moves, and not when the step would end inside
/// a cell it may not stand on or a thing it walks round, gathered over the block the game checks
bool StepAccepted(entt::entity creature, const Transform& transform, bool mirrored)
{
	if (!Locator::creatureLocomotionSystem::has_value() || !Locator::creatureAnimationSystem::has_value())
	{
		return false;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	if (locomotion.IsMoving(creature))
	{
		return false;
	}
	const auto travel = Locator::creatureAnimationSystem::value().AnimationTravel(creature, creature_catch::k_CatchStep);
	if (!travel.has_value())
	{
		return false;
	}
	// Where the step ends: the step's travel, the other way across for the left hand, turned and sized as the creature is
	auto step = *travel;
	if (mirrored)
	{
		step.x = -step.x;
	}
	// The step leans with the slope of the land, as the body does
	const auto placement = ecs::creature_placement::PlacementOf(Locator::entitiesRegistry::value(), creature, transform);
	const auto end = glm::vec3(placement * glm::vec4(step, 1.0f));
	// The circles of the land it may not stand on and of the things it walks round in the block's cells. They give way to
	// where the creature's route last left it, which is where it stands.
	const auto circles = creature_route::StepBlockCircles(
	    locomotion.GetWalkableLand(), creature_route::StepBlock(end), glm::xz(transform.position),
	    [&locomotion, creature](glm::ivec2 cell) { return locomotion.ThingCirclesIn(creature, cell); });
	return !creature_route::InsideAny(glm::xz(end), circles);
}

/// A frame of the side step: the creature is carried by its share of the step's travel, put back on ground it may stand on
/// when the step took it too near ground it may not, and the step plays on
void StepFrame(ecs::Registry& registry, entt::entity creature, CreatureAnimation& animation, CreatureObjectAction& action,
               float step)
{
	const float duration = DurationOf(creature, creature_catch::k_CatchStep);
	const auto transform = AsFacing(registry, creature, registry.Get<const Transform>(creature));
	const auto travel = Locator::creatureAnimationSystem::has_value()
	                        ? Locator::creatureAnimationSystem::value().AnimationTravel(creature, creature_catch::k_CatchStep)
	                        : std::nullopt;
	if (travel.has_value() && duration > 0.0f && Locator::creatureLocomotionSystem::has_value())
	{
		auto& locomotion = Locator::creatureLocomotionSystem::value();
		auto position = transform.position + creature_catch::StepMove(*travel, transform.rotation, transform.scale.x, step,
		                                                              duration, action.mirrored);
		const auto& land = locomotion.GetWalkableLand();
		if (!land.IsValid(glm::xz(position), creature_route::k_Clearance))
		{
			// TODO(physics): the game also puts the creature at height 0 when it moves it here; what its placing does
			// with that height wasn't traced, so the height is kept
			if (const auto valid = land.NearestStandable(glm::xz(position)))
			{
				position.x = valid->x;
				position.z = valid->y;
			}
		}
		locomotion.Place(creature, position);
	}
	action.timeMs += step;
	animation.slots.clear();
	animation.slots.push_back({.animation = creature_catch::k_CatchStep,
	                           .timeMs = std::clamp(action.timeMs, 0.0f, std::max(duration - 1.0f, 0.0f)),
	                           .weight = 1.0f,
	                           .mirrored = action.mirrored});
}
} // namespace

namespace openblack::ecs::systems::object_action_detail
{
bool StartCatch(entt::entity creature, CreatureObjectAction& action, const CreatureRig::ActionPoints& points)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto flight = action.target.has_value() ? FlightOf(*action.target) : std::nullopt;
	const bool hasAnimations = std::ranges::all_of(creature_catch::k_CatchAnimations,
	                                               [creature](size_t clip) { return DurationOf(creature, clip) > 0.0f; });
	if (!hasAnimations || !flight.has_value())
	{
		Fail(action, "it can't catch that");
		return false;
	}
	// Its speed is weighed across and up or down, not along the land's other axis, as the game does
	const glm::vec3 velocity = flight->second;
	if (velocity.y * velocity.y + velocity.x * velocity.x < creature_catch::k_LeastSpeedSquared)
	{
		Fail(action, "it can't catch that");
		return false;
	}
	if (!Locator::creatureAnimationSystem::has_value())
	{
		Fail(action, "it can't catch that");
		return false;
	}
	// Where the finger ends of the hand close is measured at the creature's first catch and kept until the animations
	// follow a new body shape
	if (KeptMeasure<CatchHands>(registry, creature) == nullptr)
	{
		CatchHands measured {.bodyRevision = BodyRevision(registry, creature)};
		for (size_t i = 0; i < creature_catch::k_CatchAnimations.size(); ++i)
		{
			measured.hands.at(i) =
			    FingerEndsInAnimation(creature, points.rightHand, creature_catch::k_CatchAnimations.at(i), points.catchMs)
			        .value_or(glm::vec3(0.0f));
		}
		registry.AssignOrReplace<CatchHands>(creature, measured);
	}
	action.catchHands = registry.Get<const CatchHands>(creature).hands;
	const std::array<float, 4> weights {0.25f, 0.25f, 0.25f, 0.25f};
	SetSlots(action, creature_catch::k_CatchAnimations, weights);
	action.mirrored = false;
	action.eventMs = points.catchMs;
	action.catching = CreatureObjectAction::Catching::Ready;
	return true;
}

void TurnToCatch(entt::entity creature, const CreatureObjectAction& action)
{
	const auto flight = action.target.has_value() ? FlightOf(*action.target) : std::nullopt;
	if (!flight.has_value() || !Locator::creatureLocomotionSystem::has_value())
	{
		return;
	}
	const auto& transform = Locator::entitiesRegistry::value().Get<const Transform>(creature);
	const auto towards = transform.position - glm::vec3(flight->second.x, 0.0f, flight->second.z);
	const float bearing = BearingTo(transform, towards);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} starts its catch at ({:.1f}, {:.1f}), {:.2f} rad off the thing",
	                    entt::to_integral(creature), transform.position.x, transform.position.z, bearing);
	// The catch turns the creature itself, while its body plays the catch
	if (bearing > creature_catch::k_TurnAngle)
	{
		Locator::creatureLocomotionSystem::value().TurnToFaceAtOnce(creature, glm::xz(towards));
	}
}

void UpdateReady(ecs::Registry& registry, entt::entity creature, const Creature& body, const Transform& drawn,
                 CreatureAnimation& animation, CreatureObjectAction& action, float step)
{
	using Catching = CreatureObjectAction::Catching;
	if (action.catching == Catching::Stepping)
	{
		if (action.timeMs + step < DurationOf(creature, creature_catch::k_CatchStep))
		{
			StepFrame(registry, creature, animation, action, step);
			return;
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} ends its step at ({:.2f}, {:.2f})", entt::to_integral(creature),
		                    drawn.position.x, drawn.position.z);
		action.catching = Catching::Ready;
		action.timeMs = 0.0f;
	}
	// It turns to face the thing once, before anything else
	if (!action.catchTurned)
	{
		if (Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(creature))
		{
			return;
		}
		action.catchTurned = true;
	}
	const auto flight = action.target.has_value() && registry.Valid(*action.target) ? FlightOf(*action.target) : std::nullopt;
	if (!flight.has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} gives up its catch: the thing has left the air",
		                    entt::to_integral(creature));
		action.status = Status::Done;
		animation.slots.clear();
		return;
	}
	const auto transform = AsFacing(registry, creature, drawn);
	const auto* points = PointsOf(body);
	if (points == nullptr)
	{
		return;
	}
	const auto lead = points->catchMs / (1000.0f * creature_layers::PlaybackRate(ShownSize(body)));
	const auto velocity = glm::transpose(transform.rotation) * flight->second;
	const auto ready =
	    creature_catch::ReadyToCatch(ToLocal(transform, flight->first), velocity, action.catchHands, transform.scale.x, lead);
	switch (ready.readiness)
	{
	case creature_catch::Readiness::Wait:
		// It stands breathing as it waits for the thing to come nearer
		animation.slots.clear();
		animation.slots.push_back(
		    {.animation = creature_layers::animations::k_Stand,
		     .timeMs = static_cast<float>(creature_animation::BreathTime(
		         animation.breathPhase, static_cast<uint32_t>(DurationOf(creature, creature_layers::animations::k_Stand)))),
		     .weight = 1.0f,
		     .mirrored = false});
		break;
	case creature_catch::Readiness::Step:
	{
		const bool accepted = StepAccepted(creature, transform, ready.mirrored);
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} steps towards its catch from ({:.1f}, {:.1f}): {}",
		                    entt::to_integral(creature), transform.position.x, transform.position.z,
		                    accepted ? "taken" : "refused");
		if (!accepted)
		{
			action.status = Status::Done;
			animation.slots.clear();
			break;
		}
		action.catching = Catching::Stepping;
		action.catchStepped = true;
		action.mirrored = ready.mirrored;
		action.timeMs = 0.0f;
		// The step's first frame is played at once
		StepFrame(registry, creature, animation, action, step);
		break;
	}
	case creature_catch::Readiness::Catch:
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} reaches for its catch", entt::to_integral(creature));
		action.catching = Catching::Reaching;
		action.mirrored = ready.mirrored;
		action.timeMs = 0.0f;
		break;
	case creature_catch::Readiness::GiveUp:
	{
		[[maybe_unused]] const auto local = ToLocal(transform, flight->first);
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"),
		                    "Creature {} gives up its catch: the thing is {:.1f} ahead, {:.1f} across, closing at {:.1f}, "
		                    "the catch takes {:.2f} s",
		                    entt::to_integral(creature), -local.z, -local.x, velocity.z, lead);
	}
		action.status = Status::Done;
		animation.slots.clear();
		break;
	}
}

void UpdateCatch(ecs::Registry& registry, entt::entity creature, const Creature& body, const Transform& transform,
                 CreatureAnimation& animation, CreatureObjectAction& action, float step)
{
	using Catching = CreatureObjectAction::Catching;
	const auto* points = PointsOf(body);
	const auto flight = action.target.has_value() && registry.Valid(*action.target) ? FlightOf(*action.target) : std::nullopt;
	creature_catch::Blend blend;
	if (flight.has_value())
	{
		const auto facing = AsFacing(registry, creature, transform);
		blend = creature_catch::Weigh(ToLocal(facing, flight->first), action.catchHands, facing.scale.x, action.mirrored);
		action.catchHeight = blend.height;
	}
	else
	{
		// Once the thing has left the physics, missed or caught, the catch is drawn back to its start: a caught thing
		// leaves on the frame after it is taken, so the creature draws its catch back with the thing in its hand
		blend = creature_catch::WeighWithout(action.catchHeight);
		action.catching = Catching::Missed;
	}
	action.weights = blend.weights;
	const float duration = DurationOf(creature, creature_catch::k_CatchAnimations.front());
	if (action.catching == Catching::Missed)
	{
		if (action.timeMs < step)
		{
			action.status = Status::Done;
		}
		action.timeMs = std::max(action.timeMs - step, 0.0f);
	}
	else
	{
		if (action.timeMs + step >= duration)
		{
			action.status = Status::Done;
		}
		action.timeMs += step;
		if (action.catching == Catching::Reaching && action.timeMs >= action.eventMs && points != nullptr)
		{
			action.eventDone = true;
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} {} its catch", entt::to_integral(creature),
			                    blend.clamped || !flight.has_value() ? "misses" : "takes");
			if (blend.clamped || !flight.has_value())
			{
				action.catching = Catching::Missed;
			}
			else
			{
				action.catching = Catching::Caught;
				TakeInHand(registry, creature, *action.target, action, *points, animation);
				action.status = Status::Contact;
			}
		}
	}
	animation.slots.clear();
	if (action.status == Status::Done)
	{
		return;
	}
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		animation.slots.push_back({.animation = action.animations.at(i),
		                           .timeMs = std::clamp(action.timeMs, 0.0f, std::max(duration - 1.0f, 0.0f)),
		                           .weight = action.weights.at(i),
		                           .mirrored = action.mirrored});
	}
}
} // namespace openblack::ecs::systems::object_action_detail

bool CreatureObjectActionSystem::Catch(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	// While its body acts out something else, or while it walks on to its stop or turns, the catch waits. The catch
	// tries again each frame.
	if (CatchWaits(creature, *this))
	{
		registry.AssignOrReplace<PendingCatch>(creature, PendingCatch {.object = object});
		return true;
	}
	registry.Remove<PendingCatch>(creature);
	return Start(creature, {.kind = Kind::Catch, .target = object});
}

void CreatureObjectActionSystem::StartPendingCatches(std::optional<entt::entity> only)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<entt::entity, entt::entity>> due;
	registry.Each<const PendingCatch>([&](entt::entity creature, const PendingCatch& pending) {
		if ((!only.has_value() || *only == creature) && !CatchWaits(creature, *this))
		{
			due.emplace_back(creature, pending.object);
		}
	});
	for (const auto& [creature, object] : due)
	{
		registry.Remove<PendingCatch>(creature);
		if (registry.Valid(object))
		{
			Start(creature, {.kind = Kind::Catch, .target = object});
		}
	}
}

bool CreatureObjectActionSystem::IsCatching(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	return action != nullptr && action->kind == Kind::Catch && action->phase == Phase::Playing &&
	       (action->status == Status::Running || action->status == Status::Contact);
}

bool CreatureObjectActionSystem::CanBeCaught(entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return creature_object_actions::MayCatchKind(
	    KindFactsOf(object),
	    {.cannotBePickedUp = RefusesCreature(registry, object), .villagerAtHome = registry.AllOf<Villager, AtHome>(object)});
}
