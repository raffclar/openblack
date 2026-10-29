/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PhysicsGameHooks.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>

#include "3D/MapCoords.h"
#include "3D/ModelSurface.h"
#include "Animals/AnimalRules.h"
#include "Animals/GrazerRules.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Common/PortableMath.h"
#include "Creature/CreatureCatch.h"
#include "Creature/CreatureDeeds.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMindTables.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingPhysics.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/GamePhysicsHooksWorld.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/TempleHeartPassOn.h"
#include "ECS/TownHomes.h"
#include "Help/HelpProfile.h"
#include "Magic/MagicWorldInterface.h"
#include "Magic/SpellRules.h"
#include "Physics/Body.h"
#include "Physics/LivingRules.h"
#include "Physics/TempleHeart.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace living = openblack::physics::living;

namespace
{
using physics_hooks::World;

/// Whether a blow can hurt a villager: not one hidden in its home or a building, nor one in a hand
bool TakesBlows(World& world, entt::entity villager)
{
	if (world.Entities().AnyOf<AtHome, InHand>(villager))
	{
		return false;
	}
	const auto state = world.VillagerStateOf(villager, false);
	return !state.has_value() || *state != VillagerStates::GoAndHideInNearbyBuilding;
}

/// Where a blow's crush comes from: the thing that struck (none for a fall onto the land), and the player whose throw
/// it was, if any. A creature that struck takes the blow's alignment as its own.
magic::EffectSource BlowSource(World& world, DynamicsSystemInterface& dynamics, entt::entity hitter,
                               std::optional<PlayerNames> player)
{
	auto& registry = world.Entities();
	const bool validHitter = hitter != entt::null && registry.Valid(hitter);
	// The blow comes from the centre of the striking thing's body
	std::optional<glm::vec3> point;
	if (validHitter)
	{
		if (const auto* entry = dynamics.Find(hitter); entry != nullptr && entry->body != nullptr)
		{
			point = entry->body->Centre();
		}
	}
	return {
	    .player = player.value_or(PlayerNames::NEUTRAL),
	    .casterCreature = validHitter && registry.AllOf<Creature>(hitter) ? hitter : entt::null,
	    .appliedBy = validHitter ? hitter : entt::null,
	    .playerless = !player.has_value(),
	    .blow = true,
	    .point = point,
	};
}

/// A villager or animal knocked harder than twice its weight is crushed by the blow, through its defences, applied by
/// what struck it and put down to the thrower's player
void HurtLiving(World& world, DynamicsSystemInterface& dynamics, entt::entity living, const ImpactInfo& impact)
{
	const auto crush = living::LivingCrush(impact.g);
	auto values = world.Crush();
	if (!crush.has_value() || !values.has_value())
	{
		return;
	}
	if (world.Entities().AllOf<Villager>(living) && !TakesBlows(world, living))
	{
		return;
	}
	values->Scale(*crush);
	world.ApplyEffect(living, *values, BlowSource(world, dynamics, impact.hitBy, impact.player));
}

/// A creature struck by a thrown thing (not a toy) is hurt a hundredth of how hard it was struck for its weight, its
/// own player's blow makes it think less of the player, and every blow makes it angry and afraid
void HurtCreature(World& world, DynamicsSystemInterface& dynamics, const PhysicsEntry& entry, const ImpactInfo& impact)
{
	auto& registry = world.Entities();
	const auto creature = entry.entity;
	if (impact.hitBy == entt::null || !registry.Valid(impact.hitBy) || world.IsToy(impact.hitBy))
	{
		return;
	}
	// The turn's force on its body sways it where the striking thing is
	if (const auto* hitter = dynamics.Find(impact.hitBy); hitter != nullptr && hitter->body != nullptr)
	{
		world.KickSway(creature, entry.forceSum, hitter->body->Centre());
	}
	const auto& body = registry.Get<const Creature>(creature);
	// A script keeps the creature of a person from all harm. Every computer finds the same person, the owner of the
	// creature. In a game alone, this is only the creature of the first player.
	if (registry.AllOf<ScriptControlled>(creature) && world.PersonPlays(body.owner))
	{
		return;
	}
	const auto* morph = registry.TryGet<const CreatureMorph>(creature);
	const float mass = living::CreatureMass(ShownSize(body), morph != nullptr ? morph->drawn.thinFat : 0.0f,
	                                        morph != nullptr ? morph->drawn.weakStrong : 0.0f);
	const auto crush = living::CreatureCrush(impact.impact, mass);
	if (!crush.has_value())
	{
		return;
	}
	// The crush is the striking thing's, put down to the player whose throw it was; the creature's own credit only
	// decides whether its player struck it
	if (auto values = world.Crush())
	{
		const auto* hitter = dynamics.Find(impact.hitBy);
		const auto hitterPlayer = hitter != nullptr ? hitter->player : std::nullopt;
		values->Scale(*crush);
		world.ApplyEffect(creature, *values, BlowSource(world, dynamics, impact.hitBy, hitterPlayer));
	}
	if (!world.HasMinds())
	{
		return;
	}
	if (impact.player.has_value() && *impact.player == body.owner)
	{
		world.UpdateAttitudeFromFeedback(creature, -*crush);
	}
	world.ChangeDesireSource(creature, creature_desires::sources::k_FearFromDamage, *crush);
	world.ChangeDesireSource(creature, creature_desires::sources::k_AngerFromDamage, *crush);
}

/// Something thrown strikes a temple's heart: the heart passes the blow on to a building or a homeless villager of its
/// player's towns, and only with none to take it is the heart itself harmed, by what breaks buildings
void StrikeHeart(World& world, DynamicsSystemInterface& dynamics, PhysicsEntry& entry, const ImpactInfo& impact)
{
	auto& registry = world.Entities();
	const auto heart = entry.entity;
	const auto hitter = impact.hitBy;
	if (hitter == entt::null)
	{
		return;
	}
	const bool hitterAbout = registry.Valid(hitter);
	const bool destroys = hitterAbout && dynamics.PhysicallyDestroysAbodes(hitter);
	// While what struck it breaks buildings, the heart beams at what takes the blow, or at a point on itself with none
	const auto target = temple_heart_pass_on::PassOn(world, heart, destroys);
	auto& temple = registry.Get<Temple>(heart);
	switch (target.kind)
	{
	case physics::temple_heart::TargetKind::Building:
		world.PassOnBlowToBuilding(dynamics, target.entity, entry, impact);
		return;
	case physics::temple_heart::TargetKind::Villager:
		// A villager standing in the world is flung up into the physics; one already in it is left as it is
		if (!registry.AllOf<InPhysics>(target.entity))
		{
			dynamics.InitialisePhysics(target.entity, {.velocity = physics::temple_heart::k_VillagerLaunch,
			                                           .spin = physics::temple_heart::k_VillagerLaunchSpin,
			                                           .add = true});
		}
		return;
	case physics::temple_heart::TargetKind::Heart:
		break;
	}
	const auto* hitterEntry = hitterAbout ? dynamics.Find(hitter) : nullptr;
	if (!destroys || hitterEntry == nullptr || hitterEntry->body == nullptr || !world.HasMagic())
	{
		return;
	}
	const auto& body = *hitterEntry->body;
	temple.lastHitTurn = world.Turn();
	// A hit of that harm, applied as the hitter's player's, through the heart's defence as any effect on an object is:
	// the heart loses life and the player's alignment and the harm done against the heart's player are counted. With
	// no player behind the blow, it is the neutral player's.
	magic::EffectValues values {};
	values[magic::EffectKind::Hit] = physics::temple_heart::Harm(body.velocity, body.Mass());
	world.ApplyEffect(heart, values,
	                  magic::EffectSource {
	                      .player = hitterEntry->player.value_or(PlayerNames::NEUTRAL),
	                      .appliedBy = hitter,
	                  });
}

/// A physical shield struck by a thing that breaks buildings pays for the blow by its momentum
void StrikeShield(World& world, DynamicsSystemInterface& dynamics, entt::entity shield, const ImpactInfo& impact)
{
	if (impact.hitBy == entt::null || !dynamics.PhysicallyDestroysAbodes(impact.hitBy))
	{
		return;
	}
	const auto* hitter = dynamics.Find(impact.hitBy);
	if (hitter == nullptr || hitter->body == nullptr)
	{
		return;
	}
	const float momentum = glm::length(hitter->body->velocity) * hitter->body->Mass();
	world.ShieldImpact(shield, impact.hitBy, momentum, hitter->player);
}

/// An animal coming down stands facing opposite its body's forward axis, plays its kind's landing as it lies, and its
/// flock's home moves to it; one killed in the air dies where it lands
void LandAnimal(World& world, PhysicsEntry* entry, entt::entity entity)
{
	auto& registry = world.Entities();
	auto* animal = registry.TryGet<Animal>(entity);
	auto* transform = registry.TryGet<Transform>(entity);
	if (animal == nullptr || transform == nullptr)
	{
		return;
	}
	auto pose = living::LandingPose::None;
	if (entry != nullptr && entry->body != nullptr)
	{
		pose = living::AnimalLandingPose(entry->body->TurnStartAxes()[0].y);
		const float heading = living::AnimalLandingHeading(entry->body->Axes());
		transform->rotation = physics::QuarterTurned(living::HeadingAxes(heading));
		// Its heading across the land as the animals keep it, from the way it is drawn
		const auto side = transform->rotation[0];
		animal->heading = portable_math::Atan2(-side.x, side.z);
	}
	// It goes on from where it came down, on the land
	if (const auto land = world.LandHeight(glm::vec2(transform->position.x, transform->position.z)))
	{
		transform->position.y = *land;
	}
	const auto position = transform->position;
	animal->position = position;
	animal->previousPosition = position;
	animal->previousHeading = animal->heading;
	animal->height = 0.0f;
	animal->move.position = {map_coords::ToFixed(position.x), map_coords::ToFixed(position.z)};
	animal->move.goal = animal->move.position;
	// It keeps how it lies for its clips
	animal->landingPose = pose;
	if (animal->Dead())
	{
		// Killed in the air it starts dying where it lands; one that a kill made dying before lies dead
		const bool wasDying = animal->dying;
		world.SetDying(entity);
		animal->landingPose = pose;
		if (animals::LandsAs(false, wasDying) == animals::LandedAs::Dead)
		{
			animal->state = AnimalState::Dead;
			animal->turnsInState = 0;
			// It plays its dead clip by how it lies, not the clip of the hand or of the air
			if (const auto clip = animals::DeadClip(animal->type, pose == living::LandingPose::Front))
			{
				animal->animation = *clip;
				animal->clipPlace = 0;
			}
		}
		return;
	}
	// It plays its landing clip as it lies. Its next turn gives its flock a new home, then it gets up
	animal->state = AnimalState::Landed;
	animal->turnsInState = 0;
	if (const auto clip = living::AnimalLandedClip(animal->type, pose))
	{
		animal->animation = *clip;
		animal->clipPlace = 0;
	}
}

void StartFlying(World& world, entt::entity object)
{
	auto& registry = world.Entities();
	if (registry.AllOf<Villager>(object))
	{
		villager_physics::StartFlying(object);
		return;
	}
	if (auto* animal = registry.TryGet<Animal>(object))
	{
		// It flies, and stops what it did, also when it dies or lies dead. Its mark of a kill stays, so that it lies
		// dead when it comes down
		animal->state = AnimalState::Flying;
		animal->turnsInState = 0;
		if (const auto clip = living::ClipsOf(animal->type).thrown)
		{
			animal->animation = *clip;
			animal->clipPlace = 0;
		}
	}
}

/// Whether the creature's plan is to fight or to catch: the plan stays so from when it is made until another replaces it
bool PlansToFightOrCatch(const World& world, const Registry& registry, entt::entity creature)
{
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || !mind->planActive || !mind->planner.current.has_value())
	{
		return false;
	}
	const auto* tables = world.MindTables();
	const auto action = mind->planner.current->action;
	if (tables == nullptr || action >= tables->actions.size())
	{
		return false;
	}
	const auto& name = tables->actions[action].name;
	return name == "Fight" || name == "Catch";
}

} // namespace

PhysicsGameHooks::PhysicsGameHooks()
    : PhysicsGameHooks(std::make_unique<GamePhysicsHooksWorld>())
{
}

PhysicsGameHooks::PhysicsGameHooks(std::unique_ptr<physics_hooks::World> world)
    : _world(std::move(world))
{
}

PhysicsGameHooks::~PhysicsGameHooks() = default;

void PhysicsGameHooks::DropCarriedResource(DynamicsSystemInterface& dynamics, entt::entity villager,
                                           const DroppedLogStart& start)
{
	_world->DropCarriedLog(dynamics, villager, start);
}

PhysicsStarted PhysicsGameHooks::InitialisePhysics(DynamicsSystemInterface& dynamics, entt::entity object,
                                                   const PhysicsStart& start)
{
	auto& registry = _world->Entities();
	// A villager drops what it carries before it starts to fly
	if (registry.AllOf<Villager>(object))
	{
		DropCarriedResource(dynamics, object,
		                    {.velocity = start.velocity, .spin = start.spin, .angularMomentum = std::nullopt});
	}
	// A villager takes to the air first; a state it can't leave keeps it on the ground, and its body is not started
	if (registry.AllOf<Villager>(object) && !villager_physics::StartFlying(object))
	{
		return {};
	}
	const auto started = PhysicsClassHooks::InitialisePhysics(dynamics, object, start);
	if (started.started && !registry.AllOf<Villager>(object))
	{
		StartFlying(*_world, object);
	}
	return started;
}

entt::entity PhysicsGameHooks::EndPhysics(DynamicsSystemInterface& dynamics, PhysicsEntry* entry, entt::entity object,
                                          bool insert)
{
	auto& registry = _world->Entities();
	if (registry.AllOf<Tree>(object))
	{
		return object_physics::EndTree(dynamics, entry, object, insert);
	}
	// A big piece of a building that comes to rest goes back into it as rubble
	if (registry.AllOf<BuildingPiece>(object))
	{
		if (const auto kept = _world->PieceAtRest(dynamics, entry, object, insert))
		{
			return *kept;
		}
	}
	if (registry.AllOf<DeadTree>(object))
	{
		return object_physics::EndDeadTree(dynamics, entry, object, insert);
	}
	if (registry.AllOf<Pot>(object))
	{
		return object_physics::EndPot(dynamics, entry, object, insert);
	}
	// A rock or other static put down gently by a town becomes its artefact
	if (registry.AllOf<MobileStatic>(object))
	{
		object_physics::ConsiderArtefact(entry, object, insert);
	}
	const bool scaffold = registry.AllOf<Scaffold>(object);
	const auto kept = PhysicsClassHooks::EndPhysics(dynamics, entry, object, insert);
	if (kept == entt::null || !registry.Valid(kept))
	{
		return kept;
	}
	// A scaffold at rest can go into a place of a workshop's yard
	if (scaffold)
	{
		_world->ScaffoldAtRest(kept, entry != nullptr && entry->Has(PhysicsEntry::k_FromHand), insert);
	}
	if (registry.AllOf<Villager>(kept))
	{
		villager_physics::Land(entry, kept);
	}
	else if (registry.AllOf<Animal>(kept))
	{
		LandAnimal(*_world, entry, kept);
	}
	return kept;
}

bool PhysicsGameHooks::HasSunk(DynamicsSystemInterface& dynamics, PhysicsEntry& entry)
{
	auto& registry = _world->Entities();
	if (registry.AllOf<Villager>(entry.entity))
	{
		return villager_physics::Sink(entry);
	}
	if (registry.AllOf<Animal>(entry.entity))
	{
		// Something already going from the world hasn't sunk
		if (!registry.Valid(entry.entity))
		{
			return false;
		}
		// The player who dropped it may teach their creature to throw things in the sea; it dies and is gone
		if (const auto dropper = villager_physics::DropperOf(registry, entry.entity))
		{
			const auto& at = registry.Get<const Transform>(entry.entity).position;
			_world->PlayerDid(living::k_DeedThrowInTheSea, at, entry.entity, *dropper);
		}
		_world->Remove(entry.entity);
		return true;
	}
	return PhysicsClassHooks::HasSunk(dynamics, entry);
}

void PhysicsGameHooks::DropSound(entt::entity object)
{
	const auto ticks = machine_clock::Ticks();
	object_physics::TreeDropSound(object, static_cast<uint64_t>(ticks));
}

void PhysicsGameHooks::OfferToCatchingCreatures(entt::entity object, PhysicsEntry& entry)
{
	auto& world = *_world;
	auto& registry = world.Entities();
	auto* random = world.Random();
	if (entry.body == nullptr || !world.HasCreatureActions() || !world.HasCreatureBodies() || random == nullptr)
	{
		return;
	}
	std::vector<entt::entity> creatures;
	registry.Each<const Creature, const Transform>(
	    [&creatures](entt::entity creature, const Creature&, const Transform&) { creatures.push_back(creature); });
	const float objectWeight = world.WeightOf(object);
	for (const auto creature : creatures)
	{
		// Not one the hand is holding, one that plans to fight or is fighting, one a script controls, nor one whose plan
		// is a catch, a catch still waiting for its body included. Also not one that may not take up a reaction now,
		// unless a desire was made its only desire.
		if (world.HandHolds(creature) || PlansToFightOrCatch(world, registry, creature) ||
		    registry.AllOf<CreatureFighting>(creature) || registry.AllOf<ScriptControlled>(creature) ||
		    !world.FreeToCatch(creature) || registry.AllOf<PendingCatch>(creature))
		{
			continue;
		}
		const auto& body = registry.Get<const Creature>(creature);
		const auto& transform = registry.Get<const Transform>(creature);
		const auto* morph = registry.TryGet<const CreatureMorph>(creature);
		const float weight = living::CreatureMass(ShownSize(body), morph != nullptr ? morph->drawn.thinFat : 0.0f,
		                                          morph != nullptr ? morph->drawn.weakStrong : 0.0f);
		// The thing's own weight, not kept from nothing as its body's mass is; only the thing's own state may keep it from
		// being caught, not the rules for picking things up from the land
		if (!(objectWeight < creature_catch::k_MostWeightShare * weight) || !world.CanBeCaught(object))
		{
			continue;
		}
		// Not its own throw; another player's (none are allied in openblack) only now and then
		if (entry.thrower == creature)
		{
			continue;
		}
		if (entry.player.has_value() && *entry.player != body.owner &&
		    random->GameRand(100) > creature_catch::k_OtherPlayersChance)
		{
			continue;
		}
		const auto catchMs = world.CatchMs(creature);
		const auto stepMs = world.AnimationDuration(creature, creature_catch::k_CatchStep);
		const auto stepTravel = world.AnimationTravel(creature, creature_catch::k_CatchStep);
		if (!catchMs.has_value() || !stepMs.has_value() || !stepTravel.has_value())
		{
			continue;
		}
		const creature_catch::Approach approach {.thing = entry.body->Centre(),
		                                         .velocity = entry.body->velocity,
		                                         .creature = transform.position,
		                                         .size = ShownSize(body),
		                                         .modelScale = transform.scale.x,
		                                         .catchMs = *catchMs,
		                                         .stepMs = *stepMs,
		                                         .stepTravel = stepTravel->x};
		if (!creature_catch::Reaches(approach))
		{
			continue;
		}
		// It stops as the game stops a walk, and the game makes it catch the thing: what it was doing fails
		world.StopCreature(creature);
		if (!world.ForceCatch(creature, object))
		{
			world.Catch(creature, object);
		}
	}
}

void PhysicsGameHooks::LetGoByHand(entt::entity object, std::optional<PlayerNames> player, bool forced)
{
	// A villager dropped by force by its own player's hand stops being a disciple
	villager_disciple::LetGo(object, player, forced);
	if (_world->Entities().AllOf<Scaffold>(object))
	{
		_world->ScaffoldLetGo(object, forced);
	}
}

void PhysicsGameHooks::PutDownByHand(entt::entity object, std::optional<PlayerNames> player)
{
	villager_disciple::PutDown(object, player);
}

void PhysicsGameHooks::ConsiderMimickingLanding(entt::entity object, std::optional<PlayerNames> player)
{
	auto& registry = _world->Entities();
	if (!registry.Valid(object))
	{
		return;
	}
	// A thing that came to rest after it flew is the deed of the player who dropped it last
	if (!player.has_value())
	{
		player = villager_physics::DropperOf(registry, object);
	}
	const auto* transform = registry.TryGet<const Transform>(object);
	if (!player.has_value() || transform == nullptr)
	{
		return;
	}
	const auto here = map_coords::FromMetres(glm::vec2(transform->position.x, transform->position.z));
	const auto distanceTo = [&registry, &here](entt::entity thing) {
		const auto& at = registry.Get<const Transform>(thing).position;
		return gutils::GetDistanceInMetres(map_coords::FromMetres(glm::vec2(at.x, at.z)), here);
	};
	creature_deeds::Landing landing {.player = *player};
	registry.Each<const Temple, const Transform>([&](entt::entity temple, const Temple& owner, const Transform&) {
		if (owner.owner == *player && distanceTo(temple) < creature_deeds::k_TempleReach)
		{
			landing.nearOwnTemple = true;
		}
	});
	// The town of the thing: a villager's, a pot's or a building's
	entt::entity town = entt::null;
	if (const auto* villager = registry.TryGet<const Villager>(object))
	{
		town = villager->town;
	}
	else if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		town = pot->town;
	}
	else if (registry.AllOf<Abode>(object))
	{
		town = ecs::town_homes::TownOfAbode(registry, object);
	}
	if (registry.Valid(town) && registry.AllOf<Town>(town))
	{
		landing.thingsTown = registry.Get<const Town>(town).owner;
	}
	// The nearest town, the first of equals. The towns come in the order of each player's list in turn and the neutral
	// list last.
	float nearest = std::numeric_limits<float>::max();
	for (const auto candidate : player_towns::AllInOrder(registry))
	{
		if (!registry.AllOf<Transform>(candidate))
		{
			continue;
		}
		const float distance = distanceTo(candidate);
		if (distance < nearest)
		{
			nearest = distance;
			landing.nearestTown = registry.Get<const Town>(candidate).owner;
		}
	}
	if (const auto deed = creature_deeds::LandingDeed(landing))
	{
		_world->PlayerDid(*deed, transform->position, object, *player);
	}
}

void PhysicsGameHooks::ConsiderMimickingToyPlay(entt::entity toy, PlayerNames player)
{
	const auto* transform = _world->Entities().TryGet<const Transform>(toy);
	_world->PlayerDid(creature_deeds::k_PlayWithToy, transform != nullptr ? transform->position : glm::vec3(0.0f), toy, player);
}

void PhysicsGameHooks::NotPutDownByHand(entt::entity object)
{
	// If the hand did not put a villager on the land, the villager gets back the work that it had at the pick-up
	villager_disciple::NotPutDown(object);
	if (_world->Entities().AllOf<Scaffold>(object))
	{
		_world->ScaffoldNotPutDown(object);
	}
}

void PhysicsGameHooks::StartFlyingFromHand([[maybe_unused]] DynamicsSystemInterface& dynamics, PhysicsEntry& entry)
{
	StartFlying(*_world, entry.entity);
}

void PhysicsGameHooks::ReactToImpact(DynamicsSystemInterface& dynamics, PhysicsEntry& entry, const ImpactInfo& impact)
{
	auto& world = *_world;
	auto& registry = world.Entities();
	const auto object = entry.entity;
	const auto hitter = impact.hitBy;
	if (!registry.Valid(object))
	{
		return;
	}
	if (registry.AllOf<Creature>(object))
	{
		HurtCreature(world, dynamics, entry, impact);
		return;
	}
	if (const auto* shield = registry.TryGet<const MagicShield>(object);
	    shield != nullptr && shield->kind == MagicShield::Kind::Physical)
	{
		StrikeShield(world, dynamics, object, impact);
		return;
	}
	// A vortex is handed what struck it with the velocity it flew at and its body's place and turn
	if (registry.AllOf<Vortex>(object))
	{
		if (auto* struck = hitter != entt::null ? dynamics.Find(hitter) : nullptr; struck != nullptr && struck->body != nullptr)
		{
			const auto& body = *struck->body;
			world.StrikeVortex(object, hitter,
			                   {.velocity = body.velocity, .position = body.ObjectPose().origin, .rotation = body.Axes()});
		}
		return;
	}
	if (registry.AllOf<Temple>(object))
	{
		StrikeHeart(world, dynamics, entry, impact);
		return;
	}
	// Rocks break buildings, the village centre, storage pits and spell dispensers among them
	if (registry.AnyOf<Abode, StoragePit, SpellDispenser>(object))
	{
		world.StrikeBuilding(dynamics, entry, impact);
		return;
	}
	// A thing that is a resource and meets a store of it goes into the store: trees, dead trees and fences as wood,
	// mushrooms and animals as food, a pot or pile as what it holds, which a pile of the same also takes
	if (hitter != entt::null && registry.Valid(hitter) && world.HasStores())
	{
		const auto resource = world.ResourceOf(object);
		// A fence a script made indestructible stays out of stores
		if (resource.type != ResourceType::None && !registry.AllOf<MobileStatic, Indestructible>(object))
		{
			// An animal the store won't take is hurt by the blow as any other
			if (world.IsStore(hitter, resource.type))
			{
				// A storage pit or a worship site that takes a thing thrown by this computer's player counts for the player's
				// help
				const bool helpCounts = registry.AnyOf<StoragePit, WorshipSite>(hitter) && impact.player.has_value() &&
				                        impact.player == world.OwnPlayer();
				const bool taken = world.TakeObject(hitter, object, impact.player);
				if (taken && helpCounts)
				{
					world.CountHelpEvent(help::profile::k_ThrownIntoStore);
				}
				if (taken || !registry.AllOf<Animal>(object))
				{
					return;
				}
			}
			if (registry.AllOf<Pot>(object))
			{
				if (const auto* other = registry.TryGet<const Pot>(hitter);
				    other != nullptr && world.ResourceOf(hitter).type == resource.type)
				{
					world.AddToPile(hitter, resource.type, resource.amount, resource.poisoned);
					world.LeaveGhost(object);
					world.Remove(object);
					return;
				}
			}
		}
	}
	// People and animals are hurt by hard knocks, their own falls included
	if (registry.AnyOf<Villager, Animal>(object))
	{
		HurtLiving(world, dynamics, object, impact);
		return;
	}
	// Rocks wear away under hard knocks, and break in two once worn out
	if (world.IsRock(object))
	{
		object_physics::KnockRock(dynamics, object, impact);
	}
}

void PhysicsGameHooks::ImpactFeedback([[maybe_unused]] DynamicsSystemInterface& dynamics, PhysicsEntry& entry, bool hit)
{
	// A person, an animal or a rock the player's hand threw may teach the player's creature to do harm by throwing,
	// whenever it is knocked, and to throw things at what it struck
	auto& registry = _world->Entities();
	if (!entry.Has(PhysicsEntry::k_FromHand) || !entry.player.has_value() || !_world->HasMinds() ||
	    !registry.Valid(entry.entity))
	{
		return;
	}
	const bool alive = registry.AnyOf<Villager, Animal, Creature>(entry.entity);
	if ((!alive && !_world->IsRock(entry.entity)) || registry.AllOf<Tree>(entry.entity))
	{
		return;
	}
	const auto& at = registry.Get<const Transform>(entry.entity).position;
	_world->PlayerDid(hit ? living::k_DeedDamageByThrowingAt : living::k_DeedDamageByThrowing, at, entry.entity, *entry.player);
}
