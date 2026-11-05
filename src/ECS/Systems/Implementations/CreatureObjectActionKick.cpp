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
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "Common/X87Angle.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureObstacles.h"
#include "Creature/CreatureOwner.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureStomp.h"
#include "CreatureObjectActionSystem.h"
#include "CreatureObjectActionSystemDetail.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDestruction.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/Map.h"
#include "ECS/MobileObjectRecord.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/Implementations/MagicLiving.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicWorldInterface.h"
#include "Magic/SpellRules.h"
#include "ObjectMeasures.h"
#include "Physics/LivingRules.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundId.h"
#include "Sound/SoundRequestInterface.h"
#include "VillagerPhysics.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
using openblack::creature_object_actions::Kind;
using namespace openblack::ecs::systems::object_action_detail;

namespace
{
/// The sounds of a kick play from the bank of the editor, at the foot
constexpr std::string_view k_KickSoundBank = "editor.sad";

constexpr int32_t k_BeenKicked = 150;

constexpr int32_t k_KickedBuilding = 19;

constexpr int32_t k_KickedTree = 20;

constexpr int32_t k_KickedVillager = 22;

constexpr int32_t k_KickedFence = 25;

/// The row of the effect table that a kick of another creature hits with
constexpr size_t k_KickHitEffect = 3;

/// A fence is a static of a fence model
bool IsFence(const ecs::Registry& registry, entt::entity object)
{
	const auto* still = registry.TryGet<const MobileStatic>(object);
	if (still == nullptr || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto kind = static_cast<size_t>(still->type);
	const auto& rows = Locator::infoConstants::value().mobileStatic;
	return kind < rows.size() && ecs::physics_classes::IsFenceModel(rows.at(kind).meshId);
}

/// The sound of a kick, by what the foot meets. A standing tree creaks. Other things have a sound with one key not
/// given, which nearly never matches anything (see docs/vanilla-bugs.md), so they make no sound.
void PlayKickSound(const ecs::Registry& registry, entt::entity target, glm::vec3 foot)
{
	std::optional<int32_t> object;
	if (registry.AnyOf<Abode, Field, SpellDispenser, VillageTotem, Ball>(target))
	{
		object = k_KickedBuilding;
	}
	else if (registry.AllOf<Villager>(target))
	{
		object = k_KickedVillager;
	}
	else if (IsFence(registry, target))
	{
		object = k_KickedFence;
	}
	else if (registry.AllOf<Tree>(target))
	{
		if (!registry.AllOf<InPhysics>(target))
		{
			if (Locator::soundTagSystem::has_value())
			{
				Locator::soundTagSystem::value().CreatePointSound(TreeCreak(), foot, false);
			}
			return;
		}
		object = k_KickedTree;
	}
	if (!object.has_value() || !Locator::sounds::has_value())
	{
		return;
	}
	const std::array<int32_t, 5> keys {0, 0, *object, 0, k_BeenKicked};
	Locator::sounds::value().PlayAnimEffect(std::string(k_KickSoundBank), keys, target, foot,
	                                        openblack::audio::CurrentSoundEffectConditions(),
	                                        openblack::audio::AnimEffectMeasure::AcrossFirst);
}

/// A stomp crushes a living thing under the foot of the creature. The crush comes from the creature, and counts as a
/// deed of its player.
void Crush(entt::entity creature, const Creature& body, entt::entity target)
{
	if (!Locator::magicSystem::has_value())
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} stomps on {}", entt::to_integral(creature),
	                    entt::to_integral(target));
	magic::EffectValues values;
	values[magic::EffectKind::Crush] = creature_stomp::k_StompCrush;
	Locator::magicSystem::value().ApplyEffectToObject(target, values,
	                                                  magic::EffectSource {
	                                                      .player = body.owner,
	                                                      .casterCreature = creature,
	                                                      .appliedBy = creature,
	                                                  });
}
} // namespace

bool CreatureObjectActionSystem::Destroy(entt::entity creature, entt::entity target)
{
	return Start(creature, {.kind = Kind::Destroy, .target = target});
}

bool CreatureObjectActionSystem::Kick(entt::entity creature, entt::entity target)
{
	return Start(creature, {.kind = Kind::Kick, .target = target});
}

bool CreatureObjectActionSystem::CrushUnderFoot(entt::entity creature, entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature) || !registry.Valid(thing) ||
	    !OnTheMap(registry, thing))
	{
		return false;
	}
	// A living thing takes the crush again on each turn, also when it is dead already
	if (registry.AnyOf<Villager, Animal, Creature>(thing))
	{
		Crush(creature, registry.Get<const Creature>(creature), thing);
	}
	return true;
}

bool CreatureObjectActionSystem::CanDestroy(entt::entity target) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(target) && registry.AllOf<Transform>(target) &&
	       registry.AnyOf<Tree, Abode, MobileObject, MobileStatic, Ball>(target);
}

bool CreatureObjectActionSystem::CanStompOn(entt::entity creature, entt::entity target) const
{
	using creature_stomp::StompRule;
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.Valid(target) || !registry.AllOf<Transform>(target))
	{
		return false;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return false;
	}
	const auto size = ShownSize(*body);
	const auto thing = KindFactsOf(target).thing;
	using creature_object_actions::Thing;
	// A bird has a rule of its own, by its height over the land
	if (thing == Thing::Bird)
	{
		return Locator::animalSystem::has_value() &&
		       Locator::animalSystem::value().CanBeStompedOnByCreature(target, creature_stomp::k_HeightPerSize * size);
	}
	const auto rule = [&] {
		switch (thing)
		{
		case Thing::Villager:
		case Thing::Animal:
			return StompRule::WhileAlive;
		case Thing::MobileObject:
		case Thing::Pot:
		case Thing::Pile:
		case Thing::Creed:
		case Thing::Rock:
		case Thing::Bonfire:
		case Thing::DeadTree:
		case Thing::Static:
		case Thing::BuildingPiece:
		case Thing::Reward:
		case Thing::Scaffold:
			return StompRule::Always;
		case Thing::Vortex:
		case Thing::TeleportStone:
			return StompRule::Never;
		case Thing::Ball:
			return StompRule::LowBuilding;
		default:
			break;
		}
		if (registry.AnyOf<Field, FishFarm, Temple>(target))
		{
			return StompRule::Never;
		}
		if (registry.AnyOf<Abode, SpellDispenser, VillageTotem>(target))
		{
			return StompRule::LowBuilding;
		}
		// Creatures, trees, features, lanterns, shields and what else is set into the land
		return StompRule::Short;
	}();
	const bool onMap =
	    ecs::InMapCells(registry, target) && !registry.AnyOf<InHand, HeldByCreature, InPhysics, CarriedByTornado>(target);
	return creature_stomp::CanBeStompedOn(
	    {
	        .rule = rule,
	        .dead = registry.AllOf<VillagerDeath>(target) || magic_living::IsDead(target),
	        .onMap = onMap,
	        .height = object_measures::Height(registry, target),
	    },
	    size);
}

bool CreatureObjectActionSystem::CanKick(entt::entity creature, entt::entity target) const
{
	using creature_stomp::KickRule;
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.Valid(target) || !registry.AllOf<Transform>(target))
	{
		return false;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return false;
	}
	const bool building = registry.AnyOf<Abode, Field, SpellDispenser, VillageTotem, Ball>(target);
	const auto rule = registry.AllOf<Temple>(target) ? KickRule::Never : building ? KickRule::Building : KickRule::Thing;
	const auto* progress = registry.TryGet<const BuildProgress>(target);
	const auto* leash = registry.TryGet<const CreatureLeash>(creature);
	const auto* holder = registry.TryGet<const HeldByCreature>(target);
	const auto* morph = registry.TryGet<const CreatureMorph>(creature);
	const auto weight = physics::living::CreatureMass(ShownSize(*body), morph != nullptr ? morph->drawn.thinFat : 0.0f,
	                                                  morph != nullptr ? morph->drawn.weakStrong : 0.0f);
	const bool hasDynamics = Locator::dynamicsSystem::has_value();
	return creature_stomp::CanBeKicked(
	    {
	        .rule = rule,
	        .creature = registry.AllOf<Creature>(target),
	        .leashTied = leash != nullptr && leash->worn.has_value() && leash->worn->tiedTo == target,
	        .canFly = hasDynamics && Locator::dynamicsSystem::value().CanBecomePhysicsObject(target),
	        .immovable = registry.AllOf<Immovable>(target),
	        .indestructible = registry.AllOf<Indestructible>(target),
	        .heldByKicker = holder != nullptr && holder->creature == creature,
	        .weight = hasDynamics ? Locator::dynamicsSystem::value().WeightOf(target) : 0.0f,
	        .built = progress == nullptr || progress->built >= 1.0f,
	        .onFire = Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(target),
	    },
	    weight);
}

namespace
{
/// The body action that plays now, as the spheres of the body know it
creature_stomp::BodyStart BodyStartOf(const creature_layers::BodyAction& body)
{
	using creature_layers::BodyAction;
	using StartPhase = creature_stomp::BodyStart::Phase;
	if (body.kind == BodyAction::Kind::Stand)
	{
		return {.stands = true};
	}
	const auto phase = body.kind == BodyAction::Kind::Once      ? StartPhase::Once
	                   : body.phase == BodyAction::Phase::Start ? StartPhase::Start
	                   : body.phase == BodyAction::Phase::Loop  ? StartPhase::Loop
	                                                            : StartPhase::End;
	return {.animation = creature_layers::CurrentAnimation(body), .phase = phase};
}

/// The bones of the parts of the body, in the order of creature_stomp::Part
std::array<uint32_t, creature_stomp::k_PartCount> PartBones(const CreatureRig::ActionPoints& points,
                                                            const CreatureAnimation& animation)
{
	const auto mirror = [&](uint32_t bone) { return bone < animation.mirror.size() ? animation.mirror[bone] : bone; };
	return {points.head,        points.groin,
	        points.rightHand,   mirror(points.rightHand),
	        points.rightFoot,   mirror(points.rightFoot),
	        points.rightArmpit, mirror(points.rightArmpit)};
}

/// Whether a corner of the nearest model of a thing is in a sphere. A model that moves by bones is never in one.
bool ModelTouches(const ecs::Registry& registry, entt::entity thing, glm::vec3 centre, float radius)
{
	const auto* mesh = registry.TryGet<const Mesh>(thing);
	const auto* at = registry.TryGet<const Transform>(thing);
	if (mesh == nullptr || at == nullptr || !Locator::resources::has_value())
	{
		return false;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return false;
	}
	const auto& model = *meshes.Handle(mesh->id);
	if (model.IsBoned())
	{
		return false;
	}
	// Things that can move or lean build their turn from three angles and scale it after; the others turn upright only
	const auto rows = [&] {
		if (registry.AnyOf<MobileStatic, MobileObject, Pot, Ball>(thing))
		{
			const auto angles = x87::GameAnglesYxz(at->rotation[0], at->rotation[2]);
			return creature_obstacles::LeaningPlacement(angles.y, angles.x, angles.z, at->scale.x);
		}
		return creature_obstacles::UprightPlacement(creature_obstacles::YAngleOf(at->rotation), at->scale.x);
	}();
	return std::ranges::any_of(model.GetSubMeshes(), [&](const auto& subMesh) {
		return (subMesh->GetFlags().lodMask & 1U) != 0 &&
		       std::ranges::any_of(subMesh->GetSurface().positions, [&](const glm::vec3& corner) {
			       return creature_stomp::CornerInSphere(rows, at->position, corner, centre, radius);
		       });
	});
}

/// What a sphere of the body does to a thing in its cell
creature_stomp::Victim VictimOf(const ecs::Registry& registry, entt::entity thing)
{
	const bool bird = registry.AllOf<Animal>(thing) && Locator::animalSystem::has_value() &&
	                  Locator::animalSystem::value().BirdCanBePickedUpByCreature(thing).has_value();
	return creature_stomp::VictimOf({
	    .hasModel = registry.AllOf<Mesh>(thing),
	    .indestructible = registry.AllOf<Indestructible>(thing),
	    .immovable = registry.AllOf<Immovable>(thing),
	    .passedBy = registry.AnyOf<Creature, Temple, Tree, StoragePit, VillageTotem, BuildingPiece>(thing),
	    .villager = registry.AllOf<Villager>(thing),
	    .animal = registry.AllOf<Animal>(thing) && !bird,
	    .living = registry.AnyOf<Villager, Animal, Creature>(thing),
	    .building = registry.AnyOf<Abode, SpellDispenser, Ball>(thing),
	    .canFly = Locator::dynamicsSystem::has_value() && Locator::dynamicsSystem::value().CanBecomePhysicsObject(thing),
	});
}

/// A sphere of the body of a creature moved to a point: it knocks about what it reaches in the cell of the point
void KnockAbout(ecs::Registry& registry, entt::entity creature, const Creature& body, glm::vec3 point, glm::vec3 move,
                float sphereRadius)
{
	if (!Locator::entitiesMap::has_value())
	{
		return;
	}
	const auto cell = ecs::MapInterface::GetGridCell(point);
	const auto things = Locator::entitiesMap::value().GetAllInCell(glm::ivec2(cell));
	for (const auto thing : things)
	{
		if (!registry.Valid(thing) || !registry.AllOf<Transform>(thing))
		{
			continue;
		}
		using creature_stomp::Victim;
		const auto victim = VictimOf(registry, thing);
		const auto& at = registry.Get<const Transform>(thing);
		const auto ground = glm::xz(at.position) - glm::xz(point);
		const auto distanceSquared = (ground.x * ground.x) + (ground.y * ground.y);
		if (!creature_stomp::InReach(victim, distanceSquared, sphereRadius, object_measures::TwoDRadius(registry, thing)))
		{
			continue;
		}
		if (victim == Victim::Building)
		{
			if (ModelTouches(registry, thing, point, sphereRadius) && Locator::buildingDamageSystem::has_value() &&
			    Locator::creatureObjectActionSystem::value().CanKick(creature, thing))
			{
				SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} knocks building {}", entt::to_integral(creature),
				                    entt::to_integral(thing));
				const std::array blows {creature_stomp::TrampleBlow(point, move, sphereRadius)};
				Locator::buildingDamageSystem::value().CreatureBlows(thing, creature, blows, false);
			}
			continue;
		}
		if (victim == Victim::Thing && !ModelTouches(registry, thing, point, sphereRadius))
		{
			continue;
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} knocks {} about", entt::to_integral(creature),
		                    entt::to_integral(thing));
		if (Locator::dynamicsSystem::has_value())
		{
			static_cast<void>(
			    Locator::dynamicsSystem::value().InitialisePhysics(thing, {.velocity = creature_stomp::KnockVelocity(move),
			                                                               .thrower = creature,
			                                                               .player = creature_owner::CreditedPlayer(body.owner),
			                                                               .add = true}));
		}
	}
}
} // namespace

namespace openblack::ecs::systems::object_action_detail
{
void KickAway(entt::entity creature, const Creature& body, const Transform& transform, const CreatureAnimation& animation,
              entt::entity target)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(target))
	{
		return;
	}
	// It kicks the way it faces, leaning with the land under it
	const auto tilt = ecs::creature_placement::TiltOf(registry, creature, transform);
	const glm::vec2 lean {tilt.alongX, tilt.alongZ};
	// Its heading, or the way its rotation faces when it does not walk
	const auto* walk = registry.TryGet<const CreatureLocomotion>(creature);
	const auto ahead = glm::xz(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
	const auto heading = walk != nullptr ? walk->heading : GameHeading::Of(ahead.x, ahead.y);
	const auto forward = walk != nullptr ? creature_locomotion::DirectionOf(heading) : glm::normalize(ahead);
	const auto direction = creature_object_actions::KickDirection(forward, lean, transform.scale.x);
	const auto velocity = (ShownSize(body) * creature_object_actions::k_KickSpeedPerSize) * direction;
	const auto* points = PointsOf(body);
	const auto foot = points != nullptr && points->rightFoot < animation.boneMatrices.size()
	                      ? glm::vec3(creature::PosedBone(points->rightFoot, animation.boneMatrices, PlacementOf(transform))[3])
	                      : transform.position;
	PlayKickSound(registry, target, foot);

	if (registry.AnyOf<Abode, Field, SpellDispenser, VillageTotem, Ball>(target))
	{
		if (!Locator::buildingDamageSystem::has_value() || !Locator::gameRandom::has_value() ||
		    !Locator::creatureObjectActionSystem::value().CanKick(creature, target))
		{
			return;
		}
		auto& random = Locator::gameRandom::value();
		const auto side = random.GameRand(creature_stomp::k_KickSideDraws);
		const auto first = random.GameRand(creature_stomp::k_KickHeightDraws);
		const auto second = random.GameRand(creature_stomp::k_KickHeightDraws);
		const creature_stomp::BuildingKick kick {
		    .creature = transform.position,
		    .foot = foot,
		    .direction = direction,
		    .heading = heading,
		    .creatureSize = ShownSize(body),
		    .buildingRadius = object_measures::TwoDRadius(registry, target),
		    .sideDraw = side,
		    .firstHeightDraw = first,
		    .secondHeightDraw = second,
		};
		// The land height is taken at the middle point turned to a map position
		const auto middle = creature_stomp::KickMiddle(transform.position, foot);
		const float land = Locator::terrainSystem::has_value()
		                       ? Locator::terrainSystem::value().GetAltitude(map_coords::FtoL(middle.x * 65536.0f * 0.1f),
		                                                                     map_coords::FtoL(middle.z * 65536.0f * 0.1f))
		                       : 0.0f;
		const auto blows = creature_stomp::KickBlows(kick, land);
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} kicks building {}", entt::to_integral(creature),
		                    entt::to_integral(target));
		Locator::buildingDamageSystem::value().CreatureBlows(target, creature, blows, true);
		return;
	}
	if (Locator::dynamicsSystem::has_value() && !registry.AllOf<Immovable>(target) &&
	    Locator::dynamicsSystem::value().CanBecomePhysicsObject(target))
	{
		auto& dynamics = Locator::dynamicsSystem::value();
		if (auto* entry = dynamics.Find(target); entry != nullptr && entry->body != nullptr)
		{
			entry->body->velocity = velocity;
			return;
		}
		auto* entry = dynamics
		                  .InitialisePhysics(target, {.velocity = velocity,
		                                              .thrower = creature,
		                                              .player = creature_owner::CreditedPlayer(body.owner),
		                                              .add = true})
		                  .entry;
		if (entry == nullptr)
		{
			return;
		}
		dynamics.SettleOnLand(*entry, false, true);
		dynamics.RaiseClearOfWhatIsUnder(*entry);
		return;
	}
	const auto* other = registry.TryGet<const Creature>(target);
	if (other == nullptr || !Locator::magicSystem::has_value() || !Locator::infoConstants::has_value() ||
	    k_KickHitEffect >= Locator::infoConstants::value().effect.size())
	{
		return;
	}
	auto values = magic::EffectValues::From(Locator::infoConstants::value().effect.at(k_KickHitEffect));
	values.Scale(creature_stomp::KickHitStrength(creature_stomp::k_HeightPerSize * ShownSize(body),
	                                             creature_stomp::k_HeightPerSize * ShownSize(*other)));
	Locator::magicSystem::value().ApplyEffectToObject(target, values,
	                                                  magic::EffectSource {
	                                                      .player = body.owner,
	                                                      .casterCreature = creature,
	                                                      .appliedBy = creature,
	                                                  });
}

entt::id_type TreeCreak()
{
	constexpr std::array k_Creaks {audio::SoundId::G_TreeBreak_01_2, audio::SoundId::G_TreeBreak_02_2,
	                               audio::SoundId::G_TreeBreak_03_2};
	const auto pick = Locator::gameRandom::has_value()
	                      ? Locator::gameRandom::value().LocalRand(static_cast<int32_t>(k_Creaks.size())) % k_Creaks.size()
	                      : 0;
	return static_cast<entt::id_type>(k_Creaks.at(pick));
}

void Trample(ecs::Registry& registry, entt::entity only)
{
	struct Moved
	{
		entt::entity creature;
		glm::vec3 point;
		glm::vec3 move;
		float radius;
	};
	std::vector<Moved> moves;
	std::vector<entt::entity> starting;
	registry.Each<const Creature, const Transform, const CreatureAnimation>(
	    [&](entt::entity creature, const Creature& body, const Transform& transform, const CreatureAnimation& animation) {
		    if (creature != only)
		    {
			    return;
		    }
		    const auto* points = PointsOf(body);
		    if (points == nullptr)
		    {
			    return;
		    }
		    auto* spheres = registry.TryGet<CreatureDestruction>(creature);
		    const auto start = BodyStartOf(animation.body);
		    if (spheres == nullptr)
		    {
			    // The first time that the spheres come on, the creature gets them after this pass
			    if (creature_stomp::SwitchFor(start) == creature_stomp::Switch::On)
			    {
				    starting.push_back(creature);
			    }
			    return;
		    }
		    const auto pose = creature::TurnPose(animation.newerTurnPose, animation.boneMatrices, PlacementOf(transform));
		    const auto bones = PartBones(*points, animation);
		    const auto at = [&](size_t part) {
			    const auto bone = bones.at(part);
			    return bone < animation.boneMatrices.size()
			               ? glm::vec3(creature::PosedBone(bone, pose.bones, pose.placement)[3])
			               : transform.position;
		    };
		    const auto size = ShownSize(body);
		    if (spheres->slapped || !(spheres->seen == start))
		    {
			    // A slap turns the spheres on as its reaction starts, whatever the body action is
			    const auto change = spheres->slapped ? creature_stomp::Switch::On : creature_stomp::SwitchFor(start);
			    spheres->slapped = false;
			    spheres->seen = start;
			    if (change == creature_stomp::Switch::On)
			    {
				    spheres->on = true;
				    for (size_t part = 0; part < creature_stomp::k_PartCount; ++part)
				    {
					    spheres->last.at(part) = at(part);
					    spheres->radius.at(part) = creature_stomp::SphereRadius(static_cast<creature_stomp::Part>(part), size);
				    }
				    return;
			    }
			    if (change == creature_stomp::Switch::Off)
			    {
				    spheres->on = false;
			    }
		    }
		    if (!spheres->on)
		    {
			    return;
		    }
		    for (size_t part = 0; part < creature_stomp::k_PartCount; ++part)
		    {
			    const auto now = at(part);
			    if (!creature_stomp::MovedEnough(spheres->last.at(part), now, size))
			    {
				    continue;
			    }
			    moves.push_back({creature, now, now - spheres->last.at(part), spheres->radius.at(part)});
			    spheres->last.at(part) = now;
		    }
	    });
	// A new set of spheres starts off; the next pass sees the action start and turns them on
	for (const auto creature : starting)
	{
		registry.Assign<CreatureDestruction>(creature);
	}
	for (const auto& moved : moves)
	{
		if (registry.Valid(moved.creature))
		{
			KnockAbout(registry, moved.creature, registry.Get<const Creature>(moved.creature), moved.point, moved.move,
			           moved.radius);
		}
	}
}
} // namespace openblack::ecs::systems::object_action_detail
