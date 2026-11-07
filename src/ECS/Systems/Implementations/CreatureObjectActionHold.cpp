/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <span>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "Common/PortableMath.h"
#include "Common/PortableRotations.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureOwner.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureThrow.h"
#include "CreatureObjectActionSystem.h"
#include "CreatureObjectActionSystemDetail.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/Map.h"
#include "ECS/MobileObjectRecord.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "Physics/LivingRules.h"
#include "Sound/SoundRequestInterface.h"
#include "VillagerFire.h"
#include "VillagerPhysics.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature_object_actions::Kind;
using openblack::creature_object_actions::Status;
using Phase = openblack::ecs::components::CreatureObjectAction::Phase;
using namespace openblack::ecs::systems::object_action_detail;

namespace
{
/// The pose drawn this frame, with the placement that the frame draws the creature at
creature::RulePose DrawnPoseOf(const CreatureAnimation& animation, const Transform& transform)
{
	return {.bones = animation.boneMatrices, .placement = PlacementOf(DrawnTransform(transform, &animation))};
}

/// Where a held thing is and how it turns, from the hand that holds it in a pose of the body
creature_throw::HeldPose HeldPoseOf(const CreatureHeldObject& held, const CreatureAnimation& animation,
                                    const creature::RulePose& pose)
{
	const auto hand = creature::PosedBone(held.bone, pose.bones, pose.placement);
	const auto hang = Locator::handGrabSystem::has_value() ? Locator::handGrabSystem::value().HangOf(held.object) : 0.0f;
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* thing = registry.Valid(held.object) ? registry.TryGet<const Transform>(held.object) : nullptr;
	const float thingScale = thing != nullptr ? thing->scale.x : 1.0f;
	return creature_throw::HeldPlacement(glm::mat3(hand), FingerEndsOf(held.bone, animation, pose), held.mirrored, hang,
	                                     thingScale);
}
} // namespace

bool CreatureObjectActionSystem::TakeFromHand(entt::entity creature, entt::entity object, bool mirrored)
{
	return Start(creature, {.kind = Kind::TakeFromHand, .target = object, .mirrored = mirrored});
}

std::optional<CreatureObjectActionSystemInterface::TakingFromHand>
CreatureObjectActionSystem::GetTakingFromHand(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->kind != Kind::TakeFromHand || action->phase != Phase::Playing ||
	    (action->status != Status::Running && action->status != Status::Contact) || !action->target.has_value())
	{
		return std::nullopt;
	}
	return TakingFromHand {.object = *action->target, .msFromTake = action->timeMs - action->eventMs};
}

std::optional<glm::vec3> CreatureObjectActionSystem::TakePoint(entt::entity creature, glm::vec3 facing) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature))
	{
		return std::nullopt;
	}
	const auto* points = PointsOf(registry.Get<const Creature>(creature));
	const auto& transform = registry.Get<const Transform>(creature);
	if (points == nullptr)
	{
		return std::nullopt;
	}
	// The finger ends of the right hand at the moment of taking, with the creature turned to face the point
	const auto hand = HandAt(creature, transform, *points, creature_giving::k_TakeAnimation, points->takeFromHandMs);
	if (!hand.has_value())
	{
		return std::nullopt;
	}
	const glm::vec2 towards {facing.x - transform.position.x, facing.z - transform.position.z};
	const auto heading = glm::length(towards) > 0.0f ? portable_math::Atan2(-towards.x, -towards.y) : 0.0f;
	return transform.position + glm::vec3(portable_math::EulerAngleY(heading) * glm::vec4(*hand, 0.0f));
}

bool CreatureObjectActionSystem::PickUp(entt::entity creature, entt::entity object)
{
	return Start(creature, {.kind = Kind::PickUp, .target = object});
}

bool CreatureObjectActionSystem::PutDown(entt::entity creature)
{
	return Start(creature, {.kind = Kind::PutDown});
}

bool CreatureObjectActionSystem::Discard(entt::entity creature)
{
	return Start(creature, {.kind = Kind::Discard});
}

bool CreatureObjectActionSystem::DiscardWith(entt::entity creature, size_t animation)
{
	CreatureObjectAction action {.kind = Kind::Discard};
	action.animations.front() = animation;
	return Start(creature, std::move(action));
}

bool CreatureObjectActionSystem::Keep(entt::entity creature, size_t animation)
{
	CreatureObjectAction action {.kind = Kind::Keep};
	action.animations.front() = animation;
	return Start(creature, std::move(action));
}

bool CreatureObjectActionSystem::Throw(entt::entity creature, const glm::vec3& target)
{
	return Start(creature, {.kind = Kind::Throw, .point = target});
}

bool CreatureObjectActionSystem::ThrowTaking(entt::entity creature, const glm::vec3& target, float flightSeconds)
{
	CreatureObjectAction action {.kind = Kind::Throw, .point = target};
	action.givenFlightSeconds = flightSeconds;
	action.waitsForLanding = true;
	return Start(creature, std::move(action));
}

bool CreatureObjectActionSystem::ThrowAimed(entt::entity creature, const glm::vec3& target, float flightSeconds)
{
	CreatureObjectAction action {.kind = Kind::Throw, .point = target};
	action.givenFlightSeconds = flightSeconds;
	return Start(creature, std::move(action));
}

void CreatureObjectActionSystem::Drop(entt::entity creature)
{
	Release(creature, glm::vec3(0.0f));
}

void CreatureObjectActionSystem::Release(entt::entity creature, const glm::vec3& velocity, ReleaseFrom from)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	if (held == nullptr)
	{
		return;
	}
	const auto object = held->object;
	// The thing keeps the turn that it has in the hand as the body is posed at the end of its newest game turn. Let go,
	// it starts where it hangs in that pose. Thrown, it starts with its origin at the finger ends.
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	auto pose = transform != nullptr && animation != nullptr
	                ? std::optional(HeldPoseOf(*held, *animation, TurnPoseOf(*animation, *transform)))
	                : std::nullopt;
	if (pose.has_value())
	{
		pose = creature_throw::ReleaseStart(*pose, FingerEndsOf(held->bone, *animation, TurnPoseOf(*animation, *transform)),
		                                    from == ReleaseFrom::Thrown);
	}
	registry.Remove<CreatureHeldObject>(creature);
	if (!registry.Valid(object))
	{
		return;
	}
	registry.Remove<HeldByCreature>(object);
	if (auto* at = registry.TryGet<Transform>(object); at != nullptr && pose.has_value())
	{
		at->position = pose->position;
		at->rotation = pose->rotation;
		// A thing that the creature lets go of or throws takes its angles from its turn in the hand. A held creature keeps
		// the turn of the hand. A thrown villager or animal stands upright with the heading of the hand. It keeps its own
		// heading and no angles, and its landing gives it a new heading.
		if (from != ReleaseFrom::Dropped && !registry.AllOf<Creature>(object))
		{
			const auto taken = ecs::placed_turn::TurnFromHeld(
			    pose->rotation, at->scale.x, ecs::placed_turn::CanLean(registry, object), from == ReleaseFrom::Thrown);
			at->rotation = taken.turn;
			if (!registry.AnyOf<Villager, Animal>(object))
			{
				ecs::placed_turn::KeepAngles(registry, object, taken.angles);
			}
		}
	}
	LetGo(creature, object, velocity);
}

void CreatureObjectActionSystem::LetGo(entt::entity creature, entt::entity object, const glm::vec3& velocity)
{
	if (!Locator::dynamicsSystem::has_value())
	{
		return;
	}
	// It leaves the hand into the physics as a god's hand lets things go, put down or thrown, turning a little about
	// the up axis (the game's turning is the other way round from the physics' own), credited to the creature's player. A
	// creature of no player gives the credit to no player.
	constexpr glm::vec3 k_ReleaseTurn {0.0f, -1.0f, 0.0f};
	auto& registry = Locator::entitiesRegistry::value();
	const auto* owner = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	Locator::dynamicsSystem::value().LetGoFromHand(
	    object, {.velocity = velocity,
	             .angularMomentum = k_ReleaseTurn,
	             .player = owner != nullptr ? creature_owner::CreditedPlayer(owner->owner) : std::nullopt,
	             .creature = registry.Valid(creature) ? creature : entt::null});
	// The creature remembers it as the last thing it dropped
	if (registry.Valid(creature))
	{
		registry.AssignOrReplace<CreatureDroppedObject>(creature, CreatureDroppedObject {.object = object});
	}
}

std::optional<entt::entity> CreatureObjectActionSystem::GetHeld(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* held = registry.Valid(creature) ? registry.TryGet<const CreatureHeldObject>(creature) : nullptr;
	return held != nullptr && registry.Valid(held->object) ? std::optional(held->object) : std::nullopt;
}

std::optional<entt::entity> CreatureObjectActionSystem::GetCarried(entt::entity creature) const
{
	return ecs::creature_carried::CarriedBy(Locator::entitiesRegistry::value(), creature);
}

bool CreatureObjectActionSystem::HandsTouched(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	return action != nullptr && action->phase == Phase::Playing && action->status == Status::Contact;
}

creature_object_actions::KindFacts CreatureObjectActionSystem::KindFactsOf(entt::entity object) const
{
	using creature_object_actions::Thing;
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || !registry.AllOf<Transform>(object))
	{
		return {};
	}
	creature_object_actions::KindFacts facts;
	const auto thing = [&] {
		if (registry.AllOf<Creature>(object))
		{
			return Thing::Creature;
		}
		if (registry.AllOf<Villager>(object))
		{
			return Thing::Villager;
		}
		if (registry.AllOf<Animal>(object))
		{
			// A bird goes by its height over the land
			const auto bird = Locator::animalSystem::has_value()
			                      ? Locator::animalSystem::value().BirdCanBePickedUpByCreature(object)
			                      : std::nullopt;
			facts.low = bird.value_or(false);
			return bird.has_value() ? Thing::Bird : Thing::Animal;
		}
		if (registry.AllOf<Ball>(object))
		{
			return Thing::Ball;
		}
		if (registry.AllOf<BuildingPiece>(object))
		{
			return Thing::BuildingPiece;
		}
		if (registry.AllOf<Reward>(object))
		{
			return Thing::Reward;
		}
		if (registry.AllOf<Scaffold>(object))
		{
			// A creature picks up a scaffold only when a hand could take it
			facts.handCanTake = Locator::workshopSystem::has_value() && Locator::workshopSystem::value().CanTake(object);
			return Thing::Scaffold;
		}
		if (const auto* pot = registry.TryGet<const Pot>(object))
		{
			// A pile of food or wood goes by whether its kind may fly about
			const auto kind = static_cast<size_t>(pot->type);
			const auto* info = Locator::infoConstants::has_value() && kind < Locator::infoConstants::value().pot.size()
			                       ? &Locator::infoConstants::value().pot.at(kind)
			                       : nullptr;
			if (info != nullptr && info->potType != PotType::Pot)
			{
				facts.mayFly = info->canBecomeAPhysicsObject != 0;
				return Thing::Pile;
			}
			return Thing::Pot;
		}
		if (const auto* mobile = registry.TryGet<const MobileObject>(object))
		{
			return mobile->type == MobileObjectInfo::Creed ? Thing::Creed : Thing::MobileObject;
		}
		// A one-shot miracle's globe lies about as any other thing does
		if (registry.AllOf<OneOffSpellSeed>(object))
		{
			return Thing::MobileObject;
		}
		if (registry.AllOf<Rock>(object))
		{
			return Thing::Rock;
		}
		if (registry.AllOf<DeadTree>(object))
		{
			return Thing::DeadTree;
		}
		if (registry.AllOf<Tree>(object))
		{
			return Thing::Tree;
		}
		if (const auto* feature = registry.TryGet<const Feature>(object);
		    feature != nullptr && feature->type == FeatureInfo::MagicMushroom)
		{
			return Thing::MagicMushroom;
		}
		if (const auto* still = registry.TryGet<const MobileStatic>(object))
		{
			switch (still->type)
			{
			case MobileStaticInfo::Bonfire:
				return Thing::Bonfire;
			case MobileStaticInfo::StreetLantern:
			case MobileStaticInfo::CountryLantern:
				return Thing::Lantern;
			case MobileStaticInfo::PhysicalShield:
				return Thing::Shield;
			case MobileStaticInfo::Vortex:
				return Thing::Vortex;
			case MobileStaticInfo::Teleport:
				return Thing::TeleportStone;
			default:
				break;
			}
			// The toys are known by their models: a ball, a bowling ball, a cuddly toy, a die and a skittle
			const auto kind = static_cast<size_t>(still->type);
			if (Locator::infoConstants::has_value() && kind < Locator::infoConstants::value().mobileStatic.size())
			{
				facts.toy =
				    creature_object_actions::ToyOf(Locator::infoConstants::value().mobileStatic.at(kind).meshId).has_value();
			}
			return Thing::Static;
		}
		// What is built or set into the land: buildings, the temple, totems, worship sites, features, animated statics and
		// flowers
		if (registry.AnyOf<Abode, Temple, WorshipSite, VillageTotem, Feature, AnimatedStatic, Flowers>(object))
		{
			return Thing::Fixture;
		}
		return Thing::Other;
	}();
	facts.thing = thing;
	facts.rule = creature_object_actions::RuleOf(thing);
	facts.canBePickedUp = creature_object_actions::CanBePickedUp(thing);
	return facts;
}

void CreatureObjectActionSystem::KeepPickUpReach(entt::entity creature)
{
	// The full pick-up facts measure the reach of a creature, and the synced world keeps that reach. Thus a test with
	// fewer facts keeps the reach too, at the same turn.
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(creature) && registry.AllOf<Creature>(creature))
	{
		static_cast<void>(KeptReachPoints<PickUpReach>(creature, ReachWith::FingerEnds, creature_reach::k_PickUpAnimations));
	}
}

bool CreatureObjectActionSystem::ComputerCreatureNotScripted(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	return body != nullptr && Locator::playerSystem::has_value() &&
	       Locator::playerSystem::value().IsComputerPlayer(body->owner) && !registry.AllOf<ScriptControlled>(creature);
}

creature_object_actions::PickUpFacts CreatureObjectActionSystem::PickUpFactsOf(entt::entity creature, entt::entity object,
                                                                               ReachKeep keep) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	const auto owner = body != nullptr ? std::optional(body->owner) : std::nullopt;
	const auto* action = registry.AllOf<Villager>(object) ? registry.TryGet<const LivingAction>(object) : nullptr;
	const bool worshipping = action != nullptr && owner.has_value() &&
	                         action->states[static_cast<size_t>(LivingAction::Index::Final)] ==
	                             static_cast<uint8_t>(VillagerStates::WorshippingAtWorshipSite) &&
	                         ecs::world_objects::PlayerOf(object) == owner;
	// In the map: in its cells, and out of any hand, flight or tornado
	const auto* holder = registry.TryGet<const HeldByCreature>(object);
	const bool inMap =
	    ecs::InMapCells(registry, object) && !registry.AnyOf<InHand, HeldByCreature, InPhysics, CarriedByTornado>(object);
	const bool heldByThisCreature = holder != nullptr && ecs::creature_carried::Carries(registry, creature, object);
	const auto* leash = registry.Valid(creature) ? registry.TryGet<const CreatureLeash>(creature) : nullptr;
	const bool leashTied = leash != nullptr && leash->worn.has_value() && leash->worn->tiedTo == object;

	std::optional<creature_object_actions::LiftFacts> lift;
	if (body != nullptr)
	{
		if (const auto reach =
		        KeptStraightReach<PickUpReach>(creature, ReachWith::FingerEnds, creature_reach::k_PickUpAnimations, keep))
		{
			const auto* morph = registry.TryGet<const CreatureMorph>(creature);
			lift = creature_object_actions::LiftFacts {
			    .creatureRadius = object_measures::TwoDRadius(registry, creature),
			    .thingRadius = object_measures::TwoDRadius(registry, object),
			    .reach = *reach,
			    .creatureWeight =
			        physics::living::CreatureMass(ShownSize(*body), morph != nullptr ? morph->drawn.thinFat : 0.0f,
			                                      morph != nullptr ? morph->drawn.weakStrong : 0.0f),
			    .thingWeight = Locator::dynamicsSystem::has_value() ? Locator::dynamicsSystem::value().WeightOf(object) : 0.0f,
			};
		}
	}
	return {
	    .cannotBePickedUp = RefusesCreature(registry, object),
	    .villagerAtHome = registry.AllOf<Villager, AtHome>(object),
	    .worshippingForCreaturesPlayer = worshipping,
	    .inScript = registry.AllOf<InScript>(object),
	    .computerCreatureNotScripted = ComputerCreatureNotScripted(creature),
	    .townArtefact = registry.AllOf<TownArtefact>(object),
	    .inMapOrHeldByCreature = inMap || heldByThisCreature,
	    .leashTiedToIt = leashTied,
	    .lift = lift,
	};
}

bool CreatureObjectActionSystem::CanPickUp(entt::entity creature, entt::entity object) const
{
	return CanPickUpKind(creature, object, KindFactsOf(object));
}

bool CreatureObjectActionSystem::CanPickUpKind(entt::entity creature, entt::entity object,
                                               const creature_object_actions::KindFacts& kind) const
{
	// Find only the pick-up facts that the rule of the kind reads
	using creature_object_actions::PickUpFactsUsed;
	const auto used = creature_object_actions::PickUpFactsUsedBy(kind);
	if (used != PickUpFactsUsed::All)
	{
		KeepPickUpReach(creature);
	}
	switch (used)
	{
	case PickUpFactsUsed::None:
		return creature_object_actions::MayPickUpKind(kind, {});
	case PickUpFactsUsed::Refusal:
		return creature_object_actions::MayPickUpKind(
		    kind, {.cannotBePickedUp = RefusesCreature(Locator::entitiesRegistry::value(), object)});
	case PickUpFactsUsed::All:
		break;
	}
	return creature_object_actions::MayPickUpKind(kind, PickUpFactsOf(creature, object, ReachKeep::Keep));
}

bool CreatureObjectActionSystem::CanTakeFromHand(entt::entity creature, entt::entity object) const
{
	// The frames of the hand ask this as it offers a thing, so the test only reads the synced world
	const auto kind = KindFactsOf(object);
	return creature_object_actions::MayTakeKindFromHand(kind, PickUpFactsOf(creature, object, ReachKeep::ReadOnly));
}

bool CreatureObjectActionSystem::CanThrowToImpress(entt::entity creature, entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr || !registry.Valid(object))
	{
		return false;
	}
	const auto* morph = registry.TryGet<const CreatureMorph>(creature);
	const auto weight = physics::living::CreatureMass(ShownSize(*body), morph != nullptr ? morph->drawn.thinFat : 0.0f,
	                                                  morph != nullptr ? morph->drawn.weakStrong : 0.0f);
	return creature_object_actions::CanBeThrownToImpress(KindFactsOf(object).thing, object_measures::Height(registry, object),
	                                                     weight);
}

std::optional<float> CreatureObjectActionSystem::PickUpStraightReach(entt::entity creature) const
{
	return KeptStraightReach<PickUpReach>(creature, ReachWith::FingerEnds, creature_reach::k_PickUpAnimations);
}

void CreatureObjectActionSystem::LetGoHeldThings(std::optional<entt::entity> creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	// What is held by a creature that is gone, or no longer holds it, falls. What left its hand to be eaten stays out of
	// sight until the creature swallows it. If the eat stopped some other way, the creature swallows it now, and if the
	// creature is gone, it comes back into sight and falls.
	std::vector<entt::entity> loose;
	std::vector<entt::entity> stoppedEats;
	registry.Each<const HeldByCreature>([&](entt::entity entity, const HeldByCreature& by) {
		const bool creatureExists = registry.Valid(by.creature);
		// The creature's own turn sees to the things that it held. The start of the turn sees to the things of creatures
		// that are gone.
		if (creature.has_value() ? by.creature != *creature : creatureExists)
		{
			return;
		}
		const auto* held = creatureExists ? registry.TryGet<const CreatureHeldObject>(by.creature) : nullptr;
		if (held != nullptr && held->object == entity)
		{
			return;
		}
		if (!registry.AllOf<HiddenByState>(entity))
		{
			loose.push_back(entity);
			return;
		}
		const auto* eat = creatureExists ? registry.TryGet<const CreatureObjectAction>(by.creature) : nullptr;
		const bool eatGoesOn = eat != nullptr && eat->kind == Kind::Eat && eat->eaten == entity;
		switch (creature_object_actions::EatenThingEndFor(eatGoesOn, creatureExists))
		{
		case creature_object_actions::EatenThingEnd::Wait:
			break;
		case creature_object_actions::EatenThingEnd::Swallow:
			stoppedEats.push_back(entity);
			break;
		case creature_object_actions::EatenThingEnd::Fall:
			loose.push_back(entity);
			break;
		}
	});
	for (const auto entity : stoppedEats)
	{
		const auto eater = registry.Get<const HeldByCreature>(entity).creature;
		CreatureObjectAction ended {.kind = Kind::Eat, .eaten = entity};
		Swallow(eater, ended, FoodValueOf(entity));
	}
	for (const auto entity : loose)
	{
		registry.Remove<HiddenByState>(entity);
		registry.Remove<HeldByCreature>(entity);
		LetGo(entt::null, entity, glm::vec3(0.0f));
	}
}

void CreatureObjectActionSystem::PlaceHeld(bool drawn)
{
	auto& registry = Locator::entitiesRegistry::value();
	// What is held is drawn in the hand, turned with it. Its place in the world does not follow the hand.
	bool moved = false;
	registry.Each<const Creature, const Transform, const CreatureAnimation, const CreatureHeldObject>(
	    [&](entt::entity creature, const Creature&, const Transform& transform, const CreatureAnimation& animation,
	        const CreatureHeldObject& held) {
		    auto* holder = registry.Valid(held.object) ? registry.TryGet<HeldByCreature>(held.object) : nullptr;
		    if (holder == nullptr || holder->creature != creature || !registry.AllOf<Transform>(held.object))
		    {
			    return;
		    }
		    // It turns with the hand bone and hangs below the finger ends by its own distance. A frame draws it where the
		    // drawn hand is. After a turn, it is drawn where the hand is in the pose of that turn, until the next frame
		    // places it.
		    const auto pose =
		        HeldPoseOf(held, animation, drawn ? DrawnPoseOf(animation, transform) : TurnPoseOf(animation, transform));
		    holder->drawn = HeldByCreature::Drawn {.position = pose.position, .rotation = pose.rotation};
		    moved = true;
	    });

	if (moved)
	{
		registry.SetDirty();
	}
}

void CreatureObjectActionSystem::UpdateHeld()
{
	// Each frame draws the thing in the hand as the frame draws the body
	PlaceHeld(true);
}

void CreatureObjectActionSystem::ShowHeldAtTurnEnd()
{
	// The frames draw the thing in the hand as the last turn posed the body, at any frame rate
	PlaceHeld(false);
}
