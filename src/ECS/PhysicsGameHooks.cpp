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
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>

#include "3D/MapCoords.h"
#include "3D/ModelSurface.h"
#include "Animals/GrazerRules.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Creature/CreatureCatch.h"
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
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/GamePhysicsHooksWorld.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
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
/// A temple heart's five beam sounds, played in turn
constexpr std::array<entt::hashed_string, 5> k_HeartBeamSounds {
    entt::hashed_string("InGame.sad/206"), entt::hashed_string("InGame.sad/207"), entt::hashed_string("InGame.sad/208"),
    entt::hashed_string("InGame.sad/209"), entt::hashed_string("InGame.sad/210")};
static_assert(k_HeartBeamSounds.size() == physics::temple_heart::k_BeamSounds);

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
	// The creature of the player at this computer is not hurt at all while a script controls it
	if (registry.AllOf<ScriptControlled>(creature) && world.LocalPlayer() == body.owner)
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

/// The temple heart's player's towns as the heart passes a blow on over them: in the order the player gained them, each
/// with its buildings and homeless people, the newest first
// TODO(physics): a town's spell dispenser is one of its buildings the heart may pass a blow on to; openblack's dispensers
// belong to no town yet
std::vector<physics::temple_heart::Town> HeartTowns(World& world, PlayerNames owner)
{
	auto& registry = world.Entities();
	std::vector<const Town*> towns;
	registry.Each<const Town>([&towns, owner](entt::entity, const Town& town) {
		if (town.owner == owner)
		{
			towns.push_back(&town);
		}
	});
	std::ranges::sort(towns, {}, &Town::gained);
	const auto dying = [&world](entt::entity villager) {
		return world.VillagerStateOf(villager, true) == VillagerStates::Dying;
	};
	std::vector<physics::temple_heart::Town> lists;
	lists.reserve(towns.size());
	for (const auto* town : towns)
	{
		auto& list = lists.emplace_back();
		for (const auto abode : town->abodes)
		{
			const bool available = registry.Valid(abode);
			const auto* progress = available ? registry.TryGet<const BuildProgress>(abode) : nullptr;
			const auto* info = available ? world.AbodeInfoOf(abode) : nullptr;
			list.buildings.push_back({.entity = abode,
			                          .available = available,
			                          .life = available ? world.LifeOf(abode) : 0.0f,
			                          .built = progress != nullptr ? progress->built : 1.0f,
			                          .field = available && registry.AllOf<Field>(abode),
			                          .footballPitch = info != nullptr && info->abodeType == AbodeType::FootballPitch});
		}
		for (const auto villager : town->homelessVillagers)
		{
			list.homeless.push_back({.entity = villager, .available = registry.Valid(villager) && !dying(villager)});
		}
	}
	return lists;
}

/// The spot visual a temple's heart fires its beams from, made the first time it beams; tried again on a later beam when
/// it couldn't be made
std::optional<uint32_t> HeartBeamSource(World& world, entt::entity heart, Temple& temple)
{
	if (!temple.beamSource.has_value())
	{
		temple.beamSource = world.StartHeartBeamSource(world.Entities().Get<const Transform>(heart).position, temple.owner);
	}
	return temple.beamSource;
}

/// A point's place with an object's height added
glm::vec3 TopOf(World& world, entt::entity object)
{
	auto top = world.Entities().Get<const Transform>(object).position;
	top.y += world.HeightOf(object);
	return top;
}

/// A temple's heart passing a blow that breaks buildings on to a target beams at it every two seconds, with one of its
/// five beam sounds in turn
void BeamAtTarget(World& world, entt::entity heart, Temple& temple, entt::entity target)
{
	namespace temple_heart = physics::temple_heart;
	const auto interval = temple_heart::BeamInterval(static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count()));
	temple_heart::Beam beam {.target = temple.beamTarget, .turn = temple.beamTurn};
	const bool due = temple_heart::BeamAtTargetDue(beam, target, world.Turn(), interval);
	temple.beamTarget = beam.target;
	temple.beamTurn = beam.turn;
	if (!due)
	{
		return;
	}
	const auto source = HeartBeamSource(world, heart, temple);
	if (!source.has_value())
	{
		return;
	}
	const auto heartPosition = world.Entities().Get<const Transform>(heart).position;
	// The beam sounds take their turns whether or not they can be heard
	auto& next = world.Entities().Context().nextHeartBeamSound;
	const auto sound = k_HeartBeamSounds.at(next);
	next = (next + 1) % temple_heart::k_BeamSounds;
	world.PlaySound(sound.value(), heartPosition, heart);
	world.AddPlasma(*source, {
	                             .start = TopOf(world, heart),
	                             .end = TopOf(world, target),
	                             .startTangent = particles::k_HeartPlasmaStartTangent,
	                             .endTangent = particles::k_HeartPlasmaEndTangent,
	                             .life = particles::k_HeartPlasmaLife,
	                             .speed = particles::k_HeartPlasmaSpeed,
	                             .alpha = particles::k_HeartPlasmaAlpha,
	                         });
}

/// A temple's heart taking a blow that breaks buildings itself forgets what it beamed at, and on the same wait beams
/// at a point on itself
void BeamAtItself(World& world, entt::entity heart, Temple& temple)
{
	namespace temple_heart = physics::temple_heart;
	const auto interval = temple_heart::BeamInterval(static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count()));
	temple_heart::Beam beam {.target = temple.beamTarget, .turn = temple.beamTurn};
	const bool due = temple_heart::BeamAtItselfDue(beam, world.Turn(), interval);
	temple.beamTarget = beam.target;
	temple.beamTurn = beam.turn;
	if (!due)
	{
		return;
	}
	static_cast<void>(HeartBeamSource(world, heart, temple));
	// The beam ends at a random point of the heart's model facing no more than a little downwards, arriving against the
	// surface; the point is drawn even when there is nothing to fire the beam from
	auto* random = world.Random();
	if (random == nullptr)
	{
		return;
	}
	const auto triangles = world.DrawnTrianglesOf(heart);
	const auto point = model_surface::RandomUpwardPoint(triangles, world.PlacementOf(heart), *random);
	if (!temple.beamSource.has_value() || !point.has_value())
	{
		return;
	}
	world.AddPlasma(*temple.beamSource, {
	                                        .start = TopOf(world, heart),
	                                        .end = point->position,
	                                        .startTangent = particles::k_HeartPlasmaStartTangent,
	                                        .endTangent = -point->normal,
	                                        .life = particles::k_HeartPlasmaLife,
	                                        .speed = particles::k_HeartPlasmaSpeed,
	                                        .alpha = particles::k_HeartPlasmaAlpha,
	                                    });
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
	auto& temple = registry.Get<Temple>(heart);
	const auto towns = HeartTowns(world, temple.owner);
	const auto target = physics::temple_heart::Choose(towns);
	// While what struck it breaks buildings, the heart beams at what takes the blow, or at a point on itself with none
	if (destroys)
	{
		if (target.kind == physics::temple_heart::TargetKind::Heart)
		{
			BeamAtItself(world, heart, temple);
		}
		else
		{
			BeamAtTarget(world, heart, temple, target.entity);
		}
	}
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
		animal->heading = std::atan2(-side.x, side.z);
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
	if (animal->Dead())
	{
		// Killed in the air it starts dying where it lands; one already dying lies dead
		const bool wasDying = animal->state == AnimalState::Dying || animal->state == AnimalState::Dead;
		world.SetDying(entity);
		if (wasDying)
		{
			animal->state = AnimalState::Dead;
			animal->turnsInState = 0;
		}
		return;
	}
	// Its flock's home moves by its kind's rule
	if (registry.Valid(animal->flock) && world.HasAnimals())
	{
		const bool leader = world.LeaderOf(animal->flock) == entity;
		switch (living::LairOnLanding(animal->type, leader))
		{
		case living::LandedLair::WhereItLanded:
			world.SetFlockCentre(animal->flock, glm::vec2(position.x, position.z));
			break;
		case living::LandedLair::ForestOfItsKind:
		{
			// Tigers and wolves choose a forest; a wolf only when it leads its flock, keeping to where it is while a
			// script holds it. (A flag of the wolf's flock that stops the choice altogether isn't known in openblack.)
			if (animal->type == AnimalInfo::Wolf && !leader)
			{
				break;
			}
			std::optional<glm::vec3> lair;
			if (animal->type == AnimalInfo::Wolf && registry.AllOf<InScript>(entity))
			{
				lair = position;
			}
			else
			{
				lair = world.ForestLair(animal->type, position);
			}
			if (lair.has_value())
			{
				world.SetFlockCentre(animal->flock, glm::vec2(lair->x, lair->z));
			}
			break;
		}
		case living::LandedLair::Unchanged:
			break;
		}
	}
	// It plays its landing clip through, kept still, then decides what to do; a kind with none waits on its current clip
	if (const auto clip = living::AnimalLandedClip(animal->type, pose))
	{
		animal->animation = *clip;
		animal->clipPlace = 0;
	}
	// A grazer, once up, joins a herd nearby if it can and wanders off
	animal->afterClip =
	    animals::grazers::IsGrazer(animal->type) ? AnimalState::InteractDecideWhatToDo : AnimalState::DecideWhatToDo;
	animal->state = AnimalState::WaitForClip;
	animal->turnsInState = 0;
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

PhysicsStarted PhysicsGameHooks::InitialisePhysics(DynamicsSystemInterface& dynamics, entt::entity object,
                                                   const PhysicsStart& start)
{
	auto& registry = _world->Entities();
	// A villager drops what it carries before it starts to fly
	if (registry.AllOf<Villager>(object))
	{
		DropCarriedResource(dynamics, object, start.velocity);
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
	const auto kept = PhysicsClassHooks::EndPhysics(dynamics, entry, object, insert);
	if (kept == entt::null || !registry.Valid(kept))
	{
		return kept;
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
		if (const auto dropper = villager_physics::DropperOf(registry, entry.entity, _world->LocalPlayer()))
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
	const auto handHeld = world.HandHeldCreature();
	const float objectWeight = world.WeightOf(object);
	for (const auto creature : creatures)
	{
		// Not one the hand is holding, one that plans to fight or is fighting, one a script controls, nor one whose plan
		// is a catch, a catch still waiting for its body included. (A creature under a script's only desire must also be
		// free to react; openblack's scripts set no only desire.)
		if (handHeld == creature || PlansToFightOrCatch(world, registry, creature) ||
		    registry.AllOf<CreatureFighting>(creature) || registry.AllOf<ScriptControlled>(creature) ||
		    registry.AllOf<PendingCatch>(creature))
		{
			continue;
		}
		const auto& body = registry.Get<const Creature>(creature);
		const auto& transform = registry.Get<const Transform>(creature);
		const auto* morph = registry.TryGet<const CreatureMorph>(creature);
		const float weight = living::CreatureMass(ShownSize(body), morph != nullptr ? morph->drawn.thinFat : 0.0f,
		                                          morph != nullptr ? morph->drawn.weakStrong : 0.0f);
		// The thing's own weight, not kept from nothing as its body's mass is
		if (!(objectWeight < creature_catch::k_MostWeightShare * weight) || !world.CanPickUp(object))
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
		// It stops where it is, and is made to play by catching the thing: what it was doing fails
		world.StopCreature(creature);
		if (!world.ForceCatch(creature, object))
		{
			world.Catch(creature, object);
		}
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
			if (world.IsStore(hitter, resource.type) &&
			    (world.TakeObject(hitter, object, impact.player) || !registry.AllOf<Animal>(object)))
			{
				return;
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
