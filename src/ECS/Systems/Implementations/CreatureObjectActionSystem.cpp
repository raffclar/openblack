/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureObjectActionSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <limits>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/Sound.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureCatch.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMeals.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreaturePointing.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureStomp.h"
#include "Creature/CreatureThrow.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDestruction.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreatureTallies.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Graveyards.h"
#include "ECS/Map.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/Implementations/MagicLiving.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicWorldInterface.h"
#include "Magic/SpellRules.h"
#include "ObjectMeasures.h"
#include "Physics/LivingRules.h"
#include "Resources/ResourcesInterface.h"
#include "VillagerFire.h"
#include "VillagerPhysics.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
using openblack::creature_object_actions::Kind;
using openblack::creature_object_actions::Status;
using Phase = openblack::ecs::components::CreatureObjectAction::Phase;

namespace
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
/// The meshes look back along their z axis: ahead is -z
constexpr glm::vec3 k_MeshBack {0.0f, 0.0f, 1.0f};
/// Turning to face what it throws at or points at, it tries this many times
constexpr uint32_t k_TurnAttempts = 2;
/// Walking up to something it walks to within this share of the distance to the middle of its reach, and stops no
/// nearer than this share of it
constexpr float k_ApproachFar = 1.0f;
constexpr float k_ApproachNear = 0.6f;
/// Near enough to reach once turned: within this share of the distance to the middle of its reach, either way
constexpr float k_TurnInReachShare = 0.5f;
/// Nothing smaller than this counts as a distance
constexpr float k_Tiny = 1e-4f;

const CreatureRig* RigOf(const Creature& creature)
{
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(creature.species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}

const CreatureRig::ActionPoints* PointsOf(const Creature& creature)
{
	const auto* rig = RigOf(creature);
	return rig != nullptr && rig->actionPoints.has_value() ? &*rig->actionPoints : nullptr;
}

glm::mat4 PlacementOf(const Transform& transform)
{
	return creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
}

/// A point of the world in the creature's own space, kept at the world's scale
glm::vec3 ToLocal(const Transform& transform, const glm::vec3& world)
{
	return glm::transpose(transform.rotation) * (world - transform.position);
}

/// How far round from ahead a point is, along the ground
float BearingTo(const Transform& transform, const glm::vec3& target)
{
	const auto local = ToLocal(transform, target);
	const glm::vec2 ahead {0.0f, -1.0f};
	const glm::vec2 towards {local.x, local.z};
	if (glm::length(towards) < k_Tiny)
	{
		return 0.0f;
	}
	return std::acos(std::clamp(glm::dot(ahead, glm::normalize(towards)), -1.0f, 1.0f));
}

/// Where the creature's head bone is as posed now, or its position when its species has no head bone
glm::vec3 HeadOf(entt::entity creature, const Transform& transform, const Creature& body)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* points = PointsOf(body);
	if (animation == nullptr || points == nullptr || points->head >= animation->boneMatrices.size())
	{
		return transform.position;
	}
	return glm::vec3(creature::PosedBone(points->head, animation->boneMatrices, PlacementOf(transform))[3]);
}

/// The hand a creature acts with: the right, or its mirror
uint32_t HandBone(const CreatureRig::ActionPoints& points, const CreatureAnimation& animation, bool mirrored)
{
	return mirrored && points.rightHand < animation.mirror.size() ? animation.mirror[points.rightHand] : points.rightHand;
}

/// Where the middle of the finger ends of a hand is, as the body is posed this frame. A held thing sits there, and the
/// hand lets go of it there.
glm::vec3 FingerEndsOf(uint32_t hand, const CreatureAnimation& animation, const glm::mat4& placement)
{
	const auto bones = creature_throw::TossPointBones(animation.skeleton.parents, hand);
	if (bones.empty())
	{
		return glm::vec3(creature::PosedBone(hand, animation.boneMatrices, placement)[3]);
	}
	glm::vec3 sum(0.0f);
	for (const auto bone : bones)
	{
		sum += glm::vec3(creature::PosedBone(bone, animation.boneMatrices, placement)[3]);
	}
	return sum / static_cast<float>(bones.size());
}

/// Where a held thing is and how it turns, from the hand that holds it as the body is posed this frame
creature_throw::HeldPose HeldPoseOf(const CreatureHeldObject& held, const Transform& transform,
                                    const CreatureAnimation& animation)
{
	const auto placement = PlacementOf(transform);
	const auto hand = creature::PosedBone(held.bone, animation.boneMatrices, placement);
	const auto hang = Locator::handGrabSystem::has_value() ? Locator::handGrabSystem::value().HangOf(held.object) : 0.0f;
	return creature_throw::HeldPlacement(glm::mat3(hand), FingerEndsOf(held.bone, animation, placement), held.mirrored, hang);
}

/// Where a bone is in an animation at a time, in the creature's own space at the world's scale
std::optional<glm::vec3> BoneAt(entt::entity creature, const Transform& transform, uint32_t bone, size_t animation,
                                float timeMs)
{
	const auto at = Locator::creatureAnimationSystem::value().BoneInAnimation(creature, animation, timeMs, bone, false);
	return at.has_value() ? std::optional(*at * transform.scale) : std::nullopt;
}

/// Where the middle of the finger ends of a hand is in an animation at a time, in the creature's own space at its own
/// size
std::optional<glm::vec3> FingerEndsInAnimation(entt::entity creature, uint32_t hand, size_t animation, float timeMs)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const CreatureAnimation>(creature);
	if (body == nullptr)
	{
		return std::nullopt;
	}
	const auto bones = creature_throw::TossPointBones(body->skeleton.parents, hand);
	return Locator::creatureAnimationSystem::value().MiddleOfBonesInAnimation(creature, animation, timeMs, bones);
}

/// Where the middle of the finger ends of the right hand is in an animation at a time, in the creature's own space at
/// the world's scale
std::optional<glm::vec3> HandAt(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points,
                                size_t animation, float timeMs)
{
	const auto at = FingerEndsInAnimation(creature, points.rightHand, animation, timeMs);
	return at.has_value() ? std::optional(*at * transform.scale) : std::nullopt;
}

/// How fast the hand moves as it tosses something away, in the model's units a second at its own size: the middle of
/// the ends of the hand's bones over the span before the toss lets go. It is measured at the creature's first toss and
/// kept, whatever its shape becomes.
std::optional<glm::vec3> TossHand(entt::entity creature, const CreatureRig::ActionPoints& points)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* kept = registry.TryGet<const TossHandVelocity>(creature))
	{
		return kept->velocity;
	}
	const auto* body = registry.TryGet<const CreatureAnimation>(creature);
	if (body == nullptr)
	{
		return std::nullopt;
	}
	const auto bones = creature_throw::TossPointBones(body->skeleton.parents, points.rightHand);
	auto& animations = Locator::creatureAnimationSystem::value();
	const auto now = animations.MiddleOfBonesInAnimation(creature, creature_throw::k_Discard, points.discardMs, bones);
	const auto before = animations.MiddleOfBonesInAnimation(
	    creature, creature_throw::k_Discard, std::max(points.discardMs - creature_throw::k_HandSpeedSpanMs, 0.0f), bones);
	if (!now.has_value() || !before.has_value())
	{
		return std::nullopt;
	}
	const auto velocity = creature_throw::HandVelocity(*now, *before, creature_throw::k_HandSpeedSpanMs);
	registry.AssignOrReplace<TossHandVelocity>(creature, TossHandVelocity {.velocity = velocity});
	return velocity;
}

/// What a creature reaches with in its reaching animations
enum class ReachWith : uint8_t
{
	/// The middle of the finger ends of the right hand, to pick something up
	FingerEnds,
	/// The right foot bone, to strike a blow
	Foot,
};
/// Where the part it reaches with gets to in each of four reaching animations
std::optional<creature_reach::Points> MeasureReach(entt::entity creature, const Transform& transform,
                                                   const CreatureRig::ActionPoints& points, ReachWith with,
                                                   const std::array<size_t, creature_reach::k_CornerCount>& animations,
                                                   float timeMs)
{
	creature_reach::Points reach {};
	for (size_t i = 0; i < animations.size(); ++i)
	{
		const auto hand = with == ReachWith::FingerEnds
		                      ? HandAt(creature, transform, points, animations.at(i), timeMs)
		                      : BoneAt(creature, transform, points.rightFoot, animations.at(i), timeMs);
		if (!hand.has_value())
		{
			return std::nullopt;
		}
		reach.at(i) = *hand;
	}
	return reach;
}

float DurationOf(entt::entity creature, size_t animation)
{
	return Locator::creatureAnimationSystem::value().AnimationDuration(creature, animation).value_or(0.0f);
}

/// How heavy something is, from the game's tables
std::optional<float> WeightOf(const ecs::Registry& registry, entt::entity entity)
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		const auto kind = static_cast<size_t>(GVillagerInfo::Find(villager->tribe, villager->number));
		return kind < info.villager.size() ? std::optional(info.villager.at(kind).weight) : std::nullopt;
	}
	if (const auto* object = registry.TryGet<const MobileObject>(entity))
	{
		const auto kind = static_cast<size_t>(object->type);
		return kind < info.mobileObject.size() ? std::optional(info.mobileObject.at(kind).weight) : std::nullopt;
	}
	if (registry.AllOf<Ball>(entity))
	{
		return info.ball.weight;
	}
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		const auto row = creature::InfoRow(creature->species);
		return row < info.creature.size() ? std::optional(info.creature.at(row).weight) : std::nullopt;
	}
	return std::nullopt;
}

/// Something eaten is gone: out of its home and its town first
void Consume(ecs::Registry& registry, entt::entity creature, entt::entity food)
{
	// A living villager eaten dies of no cause, put down to the creature's player, as any death is heard of and counted.
	// One already dead was counted when it died.
	if (registry.AllOf<Villager>(food) && !registry.AllOf<VillagerDeath>(food))
	{
		const auto& body = registry.Get<const Creature>(creature);
		ecs::villager_fire::DieByEffect(food, ecs::villager_fire::EatenDeath(body.owner, ecs::world_objects::LifeOf(food)));
		// A villager that can't die as others do still counts among its town's burials, though no body is left
		if (!registry.AllOf<VillagerDeath>(food))
		{
			graveyards::TownPersonDied(registry.Get<const Villager>(food).town);
		}
	}
	if (const auto* villager = registry.TryGet<const Villager>(food))
	{
		if (auto* abode = registry.TryGet<Abode>(villager->abode))
		{
			std::erase(abode->inhabitants, food);
		}
		if (auto* town = registry.TryGet<Town>(villager->town))
		{
			std::erase(town->homelessVillagers, food);
		}
	}
	// An animal eaten leaves its flock, which goes once it has no one left
	if (registry.AllOf<Animal>(food) && Locator::animalSystem::has_value())
	{
		Locator::animalSystem::value().LeaveFlock(food);
	}
	registry.Destroy(food);
}

/// Whether a creature about to swallow a thing brings it back up instead: one made indestructible (as Sable is for her
/// lessons), or one the scripts hold while they own the widescreen bars
bool BringsBackUp(const ecs::Registry& registry, entt::entity food)
{
	bool heldDuringCutscene = false;
	if (registry.AllOf<InScript>(food) && Locator::cinematicDirectorSystem::has_value())
	{
		const auto& director = Locator::cinematicDirectorSystem::value();
		heldDuringCutscene = director.IsWideScreenOn() && director.GetWideScreenOwner() != 0;
	}
	return creature_object_actions::BringsBackUp(registry.AllOf<Indestructible>(food), heldDuringCutscene);
}

/// The info of a thing that a creature may eat: an animal's own, or that of any other thing
const GObjectInfo* FoodInfoOf(const ecs::Registry& registry, entt::entity object)
{
	if (const auto* animal = registry.TryGet<const Animal>(object))
	{
		const auto kind = static_cast<size_t>(animal->type);
		if (!Locator::infoConstants::has_value() || kind >= Locator::infoConstants::value().animal.size())
		{
			return nullptr;
		}
		return &Locator::infoConstants::value().animal.at(kind);
	}
	return ecs::world_objects::InfoOf(object);
}

/// A magic mushroom growing on the land, or one of the mushrooms lying about
bool IsMagicMushroom(const ecs::Registry& registry, entt::entity object)
{
	const auto* feature = registry.TryGet<const Feature>(object);
	const auto* mobile = registry.TryGet<const MobileObject>(object);
	return (feature != nullptr && feature->type == FeatureInfo::MagicMushroom) ||
	       (mobile != nullptr &&
	        (mobile->type == MobileObjectInfo::MagicMushroom || mobile->type == MobileObjectInfo::Toadstool));
}

/// What the creature notes about a thing that it eats, before the thing goes
struct Meal
{
	std::optional<uint32_t> sort;
	bool magicMushroom {false};
	bool foodPile {false};
	bool poisoned {false};
};

Meal MealOf(const ecs::Registry& registry, entt::entity food)
{
	Meal meal;
	if (const auto* info = FoodInfoOf(registry, food))
	{
		meal.sort = static_cast<uint32_t>(info->type);
	}
	meal.magicMushroom = IsMagicMushroom(registry, food);
	if (const auto* pot = registry.TryGet<const Pot>(food))
	{
		const auto kind = static_cast<size_t>(pot->type);
		meal.foodPile = Locator::infoConstants::has_value() && kind < Locator::infoConstants::value().pot.size() &&
		                Locator::infoConstants::value().pot.at(kind).potType == PotType::PileFood;
		meal.poisoned = pot->poisoned;
	}
	else
	{
		meal.poisoned = registry.AllOf<Poisoned>(food);
	}
	return meal;
}

/// Decrease the hunger of the creature by the energy of a meal that it did not bring up
void KeepMealDown(entt::entity creature, float energy)
{
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ChangeDesires(creature, [energy](creature_desires::Desires& desires) {
			auto& hunger = desires[creature_desires::Desire::Hunger];
			hunger.value = creature_meals::HungerAfterMeal(hunger.value, energy);
		});
	}
}

/// A magic mushroom or a toadstool that the creature eats counts among its mushrooms
void CountMushroom(ecs::Registry& registry, entt::entity creature, const Meal& meal)
{
	if (meal.magicMushroom)
	{
		player_statistics::CountCreatureMushroom(registry, creature);
	}
}

/// The creature brings up a meal with too little food in it, or a poisoned meal. It is sick and is not hungry for a
/// time. Unless the meal was poisoned, it does not eat that sort of thing again.
void RefuseMeal(entt::entity creature, const Meal& meal)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = registry.TryGet<CreatureMindState>(creature);
	    mind != nullptr && meal.sort.has_value() && creature_object_actions::RefusesSortAfter(meal.poisoned))
	{
		mind->refusedFoodSorts.insert(*meal.sort);
	}
	if (Locator::creatureMindSystem::has_value())
	{
		auto& minds = Locator::creatureMindSystem::value();
		minds.ChangeDesires(creature, [](creature_desires::Desires& desires) {
			creature_desires::Suppress(desires, creature_desires::Desire::Hunger,
			                           creature_object_actions::k_NotHungryAfterBringingUpSeconds, 1.0f / k_TurnSeconds);
		});
		minds.Puke(creature);
	}
}

/// The creature brings up what it was about to swallow: it falls from where it is, uneaten, the creature isn't hungry
/// for a while and is sick
void BringBackUp(entt::entity creature, entt::entity food)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureHeldObject>(creature);
	registry.Remove<HeldByCreature>(food);
	if (Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().InitialisePhysics(food, {.thrower = creature});
	}
	if (Locator::creatureMindSystem::has_value())
	{
		auto& minds = Locator::creatureMindSystem::value();
		minds.ChangeDesires(creature, [](creature_desires::Desires& desires) {
			creature_desires::Suppress(desires, creature_desires::Desire::Hunger,
			                           creature_object_actions::k_NotHungryAfterBringingUpSeconds, 1.0f / k_TurnSeconds);
		});
		minds.Puke(creature);
	}
}

/// One of the creaks of a tree that a creature pulls from the land, picked at random
entt::id_type TreeCreak();

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
	if (!object.has_value() || !Locator::audio::has_value())
	{
		return;
	}
	const std::array<int32_t, 5> keys {0, 0, *object, 0, k_BeenKicked};
	Locator::audio::value().PlayAnimEffect(std::string(k_KickSoundBank), keys, target, foot);
}

/// The foot meets what the creature kicks. The order of the rules is:
/// 1. A building breaks with two blows when it is fully built and not on fire.
/// 2. A thing that can fly flies off ahead of the creature. A thing in the air takes the new speed. A thing on the land
///    goes into the air from where it lies, from the player of the creature.
/// 3. Another creature takes a tenth of a hit, and more as the kicker is tall for it.
void KickAway(entt::entity creature, const Creature& body, const Transform& transform, const CreatureAnimation& animation,
              entt::entity target)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(target))
	{
		return;
	}
	// It kicks the way it faces, leaning with the land under it
	const glm::vec2 at {transform.position.x, transform.position.z};
	const auto radius = object_measures::TwoDRadius(registry, creature);
	const auto height = [](glm::vec2 point) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
	};
	const auto lean =
	    creature_object_actions::LeanOf(height(at + glm::vec2(radius, 0.0f)), height(at - glm::vec2(radius, 0.0f)),
	                                    height(at + glm::vec2(0.0f, radius)), height(at - glm::vec2(0.0f, radius)), radius);
	const auto ahead = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
	const auto velocity = creature_object_actions::KickVelocity(glm::normalize(glm::xz(ahead)), lean, ShownSize(body));
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
		    .direction = glm::length(velocity) > 0.0f ? glm::normalize(velocity) : glm::vec3(0.0f),
		    // The angle whose sine is the way ahead along x and whose minus cosine is the way ahead along z
		    .yaw = std::atan2(ahead.x, -ahead.z),
		    .creatureSize = ShownSize(body),
		    .buildingRadius = object_measures::TwoDRadius(registry, target),
		    .sideDraw = side,
		    .firstHeightDraw = first,
		    .secondHeightDraw = second,
		};
		const auto middle = transform.position + ((foot - transform.position) * 0.5f);
		const auto blows = creature_stomp::KickBlows(kick, height({middle.x, middle.z}));
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} kicks building {}", entt::to_integral(creature),
		                    entt::to_integral(target));
		Locator::buildingDamageSystem::value().CreatureBlows(target, creature, blows);
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
		auto* entry =
		    dynamics.InitialisePhysics(target, {.velocity = velocity, .thrower = creature, .player = body.owner, .add = true})
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

/// A villager picked up stops wherever it was walking
void StopWalking(ecs::Registry& registry, entt::entity entity)
{
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(entity);
}

/// A person taken into the hand is drawn in the hand from this frame on, not where its drawing put it on the land
void DrawInHand(ecs::Registry& registry, entt::entity entity)
{
	if (auto* pose = registry.TryGet<VillagerPose>(entity))
	{
		pose->drawnAt.reset();
		pose->drawnHeading.reset();
	}
}

/// The blend of the four reaching animations for a target now, mirrored when it lies to the creature's left
struct Reach
{
	bool mirrored;
	creature_reach::Blend blend;
};
Reach ReachFor(const creature_reach::Points& points, const glm::vec3& local)
{
	const bool mirrored = creature_reach::ReachesMirrored(points, local);
	return {.mirrored = mirrored, .blend = creature_reach::Solve(mirrored ? creature_reach::Mirrored(points) : points, local)};
}

void SetSlots(CreatureObjectAction& action, std::span<const size_t> animations, std::span<const float> weights)
{
	action.animationCount = static_cast<uint8_t>(std::min(animations.size(), action.animations.size()));
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		action.animations.at(i) = animations[i];
		action.weights.at(i) = weights[i];
	}
}

/// The body stops whatever else it plays and walking, and the animations start from their beginning
void BeginPlaying(entt::entity creature, CreatureObjectAction& action, CreatureAnimation& animation)
{
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	animation.body = {};
	action.phase = Phase::Playing;
	action.timeMs = 0.0f;
	action.durationMs = 0.0f;
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		action.durationMs = std::max(action.durationMs, DurationOf(creature, action.animations.at(i)));
	}
	// Whatever it does at a moment happens by the end at the latest
	action.eventMs = std::min(action.eventMs, std::max(action.durationMs - 1.0f, 0.0f));
}

/// Whether the creature's body is acting something out, a catch included
bool BodyBusy(const CreatureObjectAction* doing)
{
	return doing != nullptr && doing->phase == Phase::Playing &&
	       (doing->status == Status::Running || doing->status == Status::Contact);
}

void Fail(CreatureObjectAction& action, std::string reason,
          creature_object_actions::Failure kind = creature_object_actions::Failure::Other)
{
	action.status = Status::Failed;
	action.failure = std::move(reason);
	action.failureKind = kind;
}

/// The thing a creature catches is taken into its hand out of the air: it leaves the physics without going back on the
/// map, and a person or animal is held
void TakeInHand(ecs::Registry& registry, entt::entity creature, entt::entity object, const CreatureObjectAction& action,
                const CreatureRig::ActionPoints& points, const CreatureAnimation& animation)
{
	registry.AssignOrReplace<CreatureHeldObject>(
	    creature, CreatureHeldObject {
	                  .object = object, .mirrored = action.mirrored, .bone = HandBone(points, animation, action.mirrored)});
	registry.AssignOrReplace<HeldByCreature>(object, HeldByCreature {.creature = creature});
	// It leaves the physics with its end, not going back on the map
	if (Locator::dynamicsSystem::has_value() && Locator::dynamicsSystem::value().Find(object) != nullptr)
	{
		Locator::dynamicsSystem::value().RemoveObject(object, false, true);
	}
	if (registry.AllOf<Villager>(object))
	{
		ecs::villager_physics::IntoHand(object);
		StopWalking(registry, object);
		DrawInHand(registry, object);
	}
	else if (registry.AllOf<Animal>(object) && Locator::animalSystem::has_value())
	{
		Locator::animalSystem::value().IntoHand(object);
	}
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

/// The flat and high throws blended by how high the target is against where the hand lets go in each
std::array<float, 2> ThrowWeights(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points,
                                  const glm::vec3& target)
{
	const auto slope = [](const glm::vec3& local) { return local.y / std::max(-local.z, k_Tiny); };
	const auto flat = HandAt(creature, transform, points, creature_throw::k_HurlFlat, points.throwMs);
	const auto high = HandAt(creature, transform, points, creature_throw::k_HurlHigh, points.throwMs);
	if (!flat.has_value() || !high.has_value())
	{
		return {1.0f, 0.0f};
	}
	const auto weight = creature_throw::HighThrowWeight(slope(ToLocal(transform, target)), slope(*flat), slope(*high));
	return {1.0f - weight, weight};
}

/// Pointing: the four pointing animations blended by where the point is across and up from the top of the creature.
/// The arm is chosen as it starts to point, and stays the same until the point ends.
void SetPointing(CreatureObjectAction& action, const Transform& transform, const Creature& body, const CreatureHeldObject* held,
                 bool choosesArm)
{
	const auto origin = transform.position + glm::vec3(0.0f, creature_pointing::k_OriginHeightPerSize * ShownSize(body), 0.0f);
	const auto facing = glm::xz(transform.rotation * (-k_MeshBack));
	const auto aim = creature_pointing::AimAt(origin, facing, action.point);
	if (choosesArm && action.pointTurned)
	{
		action.mirrored =
		    creature_pointing::PointsWithLeftAfterTurn(aim, held != nullptr ? std::optional(held->mirrored) : std::nullopt);
	}
	const auto weights = creature_pointing::Weights(aim, action.mirrored);
	SetSlots(action, creature_pointing::k_Animations, weights);
}

/// A vector along the ground turned by an angle
glm::vec2 Turned(glm::vec2 v, float angle)
{
	const auto c = std::cos(angle);
	const auto s = std::sin(angle);
	return {(v.x * c) - (v.y * s), (v.x * s) + (v.y * c)};
}

/// The point to turn to face so that a target lies in the direction of the middle of the reach, on whichever side
/// needs the smaller turn
glm::vec2 FacingToReach(const Transform& transform, const creature_reach::Points& reach, const glm::vec3& target)
{
	const auto from = glm::xz(transform.position);
	const auto towards = glm::xz(target) - from;
	const auto centre = creature_reach::Centre(reach);
	const glm::vec2 ahead {0.0f, -1.0f};
	const auto aheadInWorld = glm::xz(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
	std::optional<glm::vec2> best;
	float bestTurn = 0.0f;
	for (const auto side : {1.0f, -1.0f})
	{
		const glm::vec2 middle {centre.x * side, centre.z};
		if (glm::length(middle) < k_Tiny)
		{
			continue;
		}
		// How far round from ahead the middle of the reach is, and so which way to face for it to lie on the target
		const auto offset = std::atan2((ahead.x * middle.y) - (ahead.y * middle.x), glm::dot(ahead, middle));
		const auto facing = Turned(towards, -offset);
		const auto turn = std::acos(std::clamp(glm::dot(glm::normalize(facing), glm::normalize(aheadInWorld)), -1.0f, 1.0f));
		if (!best.has_value() || turn < bestTurn)
		{
			best = facing;
			bestTurn = turn;
		}
	}
	return from + best.value_or(towards);
}

/// Whether a thing lies on the map, where a creature can stomp on it: not held, flying or carried off
bool OnTheMap(const ecs::Registry& registry, entt::entity thing)
{
	return registry.AllOf<MapCellResident>(thing) &&
	       !registry.AnyOf<InHand, HeldByCreature, InPhysics, CarriedByTornado>(thing);
}

/// One game turn of walking or turning up to what the creature acts on
void Approach(entt::entity creature, CreatureObjectAction& action, const Creature& body, const Transform& transform,
              CreatureAnimation& animation, const CreatureHeldObject* held)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	// On each turn of the walk, a thing that it can no longer pick up stops the pick-up and the walk
	if (action.kind == Kind::PickUp && action.target.has_value() && registry.Valid(*action.target) &&
	    Locator::creatureObjectActionSystem::has_value() &&
	    !Locator::creatureObjectActionSystem::value().CanPickUp(creature, *action.target))
	{
		locomotion.Stop(creature);
		Fail(action, "it can't be picked up", creature_object_actions::Failure::NoWay);
		return;
	}
	// On each turn of the walk, a thing that is no longer on the map stops the stomp and the walk
	if (action.kind == Kind::Destroy && action.target.has_value() && registry.Valid(*action.target) &&
	    !OnTheMap(registry, *action.target))
	{
		locomotion.Stop(creature);
		Fail(action, "it is not on the map", creature_object_actions::Failure::NoWay);
		return;
	}
	if (locomotion.IsMoving(creature))
	{
		return;
	}
	const auto* points = PointsOf(body);
	if (points == nullptr)
	{
		Fail(action, "its species has nothing to act with");
		return;
	}
	// The walk to the thing stopped because the search found no route: the action stops at once
	if (const auto* walk = registry.TryGet<const CreatureLocomotion>(creature);
	    walk != nullptr && walk->failed && action.walked)
	{
		Fail(action, "it found no route", creature_object_actions::Failure::NoRoute);
		return;
	}
	// The walk stopped because the creature is shut in. In the game, the creature clears the way at the moment that the
	// walk stops, and the step starts a new walk only on its next turn. Here the mind finds the stop on its next turn,
	// so the hands wait for that turn. Then the cleared way or the new walk starts on the same turn as in the game. When
	// nothing takes the stop by then, the walk starts again.
	if (auto* walk = registry.TryGet<CreatureLocomotion>(creature); walk != nullptr && walk->gaveUp.has_value())
	{
		if (!std::exchange(action.waitedForWay, true))
		{
			return;
		}
		walk->gaveUp.reset();
	}
	action.waitedForWay = false;

	if (action.kind == Kind::Throw || action.kind == Kind::Point)
	{
		const auto limit = action.kind == Kind::Throw ? creature_object_actions::k_ThrowTurnRadians
		                                              : creature_object_actions::k_PointTurnRadians;
		if (action.kind == Kind::Point)
		{
			// The arm points without a turn when the point is near enough ahead, as seen from the head
			const auto facing = glm::xz(transform.rotation * (-k_MeshBack));
			const auto turn = creature_pointing::TurnAngle(HeadOf(creature, transform, body), facing, action.point);
			if (std::abs(turn) >= creature_pointing::k_TurnLimit && action.attempts < k_TurnAttempts)
			{
				++action.attempts;
				action.pointTurned = true;
				locomotion.TurnToFace(creature, glm::xz(action.point));
				return;
			}
			if (!action.pointTurned)
			{
				action.mirrored =
				    creature_pointing::PointsWithLeft(turn, held != nullptr ? std::optional(held->mirrored) : std::nullopt);
			}
		}
		else if (BearingTo(transform, action.point) > limit && action.attempts < k_TurnAttempts)
		{
			++action.attempts;
			locomotion.TurnToFace(creature, glm::xz(action.point));
			return;
		}
		if (action.kind == Kind::Throw)
		{
			action.flightSeconds =
			    action.givenFlightSeconds.value_or(creature_throw::FlightTime(glm::distance(transform.position, action.point)));
			const auto weights = ThrowWeights(creature, transform, *points, action.point);
			const std::array<size_t, 2> animations {creature_throw::k_HurlFlat, creature_throw::k_HurlHigh};
			SetSlots(action, animations, weights);
			action.mirrored = held != nullptr && held->mirrored;
			action.eventMs = points->throwMs;
		}
		else
		{
			SetPointing(action, transform, body, held, true);
			action.holdMs = action.pointsUntilTold
			                    ? std::numeric_limits<float>::infinity()
			                    : action.pointSeconds.value_or(creature_object_actions::k_PointSeconds) * 1000.0f;
			action.pointMs = points->pointMs;
		}
		BeginPlaying(creature, action, animation);
		if (action.kind == Kind::Point)
		{
			// The arm plays in the time of the first pointing animation
			action.durationMs = DurationOf(creature, creature_pointing::k_Animations.front());
		}
		return;
	}

	// Walking up to something and reaching for it
	const auto* at = action.target.has_value() && registry.Valid(*action.target)
	                     ? registry.TryGet<const Transform>(*action.target)
	                     : nullptr;
	if (at == nullptr)
	{
		Fail(action, "it has gone");
		return;
	}
	if (!action.reach.has_value())
	{
		const bool pickingUp = action.kind == Kind::PickUp;
		action.eventMs = pickingUp ? points->pickUpMs : points->destroyMs;
		action.reach =
		    MeasureReach(creature, transform, *points, pickingUp ? ReachWith::FingerEnds : ReachWith::Foot,
		                 pickingUp ? creature_reach::k_PickUpAnimations : creature_reach::k_DestroyAnimations, action.eventMs);
		if (!action.reach.has_value())
		{
			// Not posed yet: it tries again next turn
			return;
		}
		action.maxReach = creature_reach::MaxReach(*action.reach);
	}
	const auto local = ToLocal(transform, at->position);
	const auto reach = ReachFor(*action.reach, local);
	const auto centre = creature_reach::Centre(*action.reach);
	const auto ideal = std::max(glm::length(glm::vec2(centre.x, centre.z)), k_Tiny);
	const auto distance = glm::length(glm::vec2(local.x, local.z));
	// It continues until the thing is in reach or the action fails, with no limit to its walks
	switch (creature_object_actions::NextApproachStep(
	    reach.blend.inRange, std::abs(distance - ideal) <= ideal * k_TurnInReachShare, action.turnedSinceWalk))
	{
	case creature_object_actions::ApproachStep::Begin:
	{
		const auto& animations =
		    action.kind == Kind::PickUp ? creature_reach::k_PickUpAnimations : creature_reach::k_DestroyAnimations;
		SetSlots(action, animations, reach.blend.weights);
		action.mirrored = reach.mirrored;
		BeginPlaying(creature, action, animation);
		return;
	}
	case creature_object_actions::ApproachStep::Turn:
		++action.attempts;
		action.turnedSinceWalk = true;
		locomotion.TurnToFace(creature, FacingToReach(transform, *action.reach, at->position));
		return;
	case creature_object_actions::ApproachStep::Walk:
		break;
	}
	++action.attempts;
	using MoveResult = CreatureLocomotionSystemInterface::MoveResult;
	const auto result = locomotion.MoveTo(creature, glm::xz(at->position), CreatureLocomotionSystemInterface::Pace::Walk,
	                                      ideal * k_ApproachNear, ideal * k_ApproachFar);
	switch (creature_cast_moves::WalkStartOf(result == MoveResult::Started, result != MoveResult::InvalidDestination))
	{
	case creature_cast_moves::WalkStart::Walking:
		action.turnedSinceWalk = false;
		break;
	case creature_cast_moves::WalkStart::TryAgain:
		// The creature cannot walk now: it asks again on the next turn
		return;
	case creature_cast_moves::WalkStart::NoWay:
		Fail(action, "navigation failed", creature_object_actions::Failure::NoWay);
		return;
	}
	action.walked = true;
}

/// One of the three creaks of a tree a creature pulls from the ground, drawn from the local random numbers
entt::id_type TreeCreak()
{
	constexpr std::array k_Creaks {audio::SoundId::G_TreeBreak_01_2, audio::SoundId::G_TreeBreak_02_2,
	                               audio::SoundId::G_TreeBreak_03_2};
	const auto pick = Locator::gameRandom::has_value()
	                      ? Locator::gameRandom::value().LocalRand(static_cast<int32_t>(k_Creaks.size())) % k_Creaks.size()
	                      : 0;
	return static_cast<entt::id_type>(k_Creaks.at(pick));
}
} // namespace

namespace
{
/// The catch starts with the four catching animations, the thing flying across the land fast enough: the creature turns
/// to face it first when it comes from too far round, and where its hand closes in each animation is measured
bool StartCatch(entt::entity creature, CreatureObjectAction& action, const CreatureRig::ActionPoints& points)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto flight = action.target.has_value() ? FlightOf(*action.target) : std::nullopt;
	const bool hasAnimations = std::ranges::all_of(creature_catch::k_CatchAnimations,
	                                               [creature](size_t clip) { return DurationOf(creature, clip) > 0.0f; });
	// Its speed is weighed across and up or down, not along the land's other axis, as the game does
	if (!hasAnimations || !flight.has_value() ||
	    flight->second.y * flight->second.y + flight->second.x * flight->second.x < creature_catch::k_LeastSpeedSquared)
	{
		Fail(action, "it can't catch that");
		return false;
	}
	if (!Locator::creatureAnimationSystem::has_value())
	{
		Fail(action, "it can't catch that");
		return false;
	}
	// Where the finger ends of the hand close is measured at the creature's first catch and kept
	if (!registry.AllOf<CatchHands>(creature))
	{
		CatchHands measured;
		for (size_t i = 0; i < creature_catch::k_CatchAnimations.size(); ++i)
		{
			measured.hands.at(i) =
			    FingerEndsInAnimation(creature, points.rightHand, creature_catch::k_CatchAnimations.at(i), points.catchMs)
			        .value_or(glm::vec3(0.0f));
		}
		registry.Assign<CatchHands>(creature, measured);
	}
	action.catchHands = registry.Get<const CatchHands>(creature).hands;
	const std::array<float, 4> weights {0.25f, 0.25f, 0.25f, 0.25f};
	SetSlots(action, creature_catch::k_CatchAnimations, weights);
	action.mirrored = false;
	action.eventMs = points.catchMs;
	action.catching = CreatureObjectAction::Catching::Ready;
	return true;
}

/// A catch's creature faces back along the thing's flight, turning when it comes from too far round. It is the last thing
/// the catch's start does, once the body has stopped what else it played and its walking.
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
	if (bearing > creature_catch::k_TurnAngle)
	{
		Locator::creatureLocomotionSystem::value().TurnToFace(creature, glm::xz(towards));
	}
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
	const auto end = transform.position + (transform.rotation * (step * transform.scale));
	// The circles of the land it may not stand on and of the things it walks round in the block's cells. They give way to
	// where the creature's route last left it, which is where it stands.
	const auto circles = creature_route::StepBlockCircles(
	    locomotion.GetWalkableLand(), creature_route::StepBlock(end), glm::xz(transform.position),
	    [&locomotion, creature](glm::ivec2 cell) { return locomotion.ThingCirclesIn(creature, cell); });
	return !creature_route::InsideAny(glm::xz(end), circles);
}

/// Where the creature stands and faces now: the heading its walking last reached, which the drawn body only catches up
/// with over the turn, so that a catch weighs the thing against the way the creature has finished turning
Transform AsFacing(const ecs::Registry& registry, entt::entity creature, const Transform& transform)
{
	auto facing = transform;
	if (const auto* walking = registry.TryGet<const CreatureLocomotion>(creature); walking != nullptr && walking->started)
	{
		facing.rotation = glm::mat3(glm::eulerAngleY(walking->heading));
	}
	return facing;
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
			if (const auto valid = land.NearestValid(glm::xz(position), creature_catch::k_StepSearchClearance,
			                                         creature_catch::k_StepSearchDistance))
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

/// Ready to catch: once it has turned, it waits for the thing, steps across to where it will pass when that is out of
/// reach, and starts the catch when the thing will arrive as the hand closes; behind it or too late, it gives up. The
/// step plays to its end whatever the thing does, and the creature is ready again in the frame it ends.
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

/// Each frame of a catch: the four animations blended by where the thing is against where the hand closes in each, the
/// catch played on while reaching or once caught and played back after a miss, the hand closing on it at its moment
/// unless the blend had to lean too far, and the catch over at either end
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
} // namespace

bool CreatureObjectActionSystem::Start(entt::entity creature, CreatureObjectAction action)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation, Transform>(creature))
	{
		return false;
	}
	const auto& body = registry.Get<const Creature>(creature);
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	action.status = Status::Running;
	action.phase = Phase::Approach;
	switch (creature_object_actions::HandsFor(action.kind))
	{
	case creature_object_actions::Hands::Holding:
		if (held == nullptr)
		{
			Fail(action, "it has nothing in its hand");
		}
		break;
	case creature_object_actions::Hands::Empty:
		if (held != nullptr)
		{
			Fail(action, "its hands are full");
		}
		break;
	case creature_object_actions::Hands::Either:
		break;
	}
	const auto* points = PointsOf(body);
	if (action.status == Status::Running && points == nullptr)
	{
		Fail(action, "its species has nothing to act with");
	}
	if (action.status == Status::Running && action.target.has_value())
	{
		if (!registry.Valid(*action.target) || !registry.AllOf<Transform>(*action.target))
		{
			Fail(action, "there is nothing there");
		}
		else if (action.kind == Kind::PickUp &&
		         (!CanPickUp(creature, *action.target) || registry.AllOf<HeldByCreature>(*action.target)))
		{
			Fail(action, "it can't be picked up", creature_object_actions::Failure::NoWay);
		}
		else if (action.kind == Kind::Destroy && !OnTheMap(registry, *action.target))
		{
			// A stomp needs the thing on the map
			Fail(action, "it is not on the map", creature_object_actions::Failure::NoWay);
		}
		else if (action.kind == Kind::Kick &&
		         ((held != nullptr && held->object == *action.target) || registry.AllOf<InHand>(*action.target)))
		{
			Fail(action, "it is in a hand");
		}
		else if (action.kind == Kind::Kick && DurationOf(creature, creature_object_actions::k_KickAnimation) <= 0.0f)
		{
			Fail(action, "its species can't kick");
		}
	}
	if (action.status == Status::Running && action.kind == Kind::Throw)
	{
		const auto& transform = registry.Get<const Transform>(creature);
		const auto ground = glm::distance(glm::xz(transform.position), glm::xz(action.point));
		if (!creature_throw::FarEnoughToThrow(ground, ShownSize(body)))
		{
			Fail(action, "too close to throw at");
		}
	}

	if (action.status == Status::Running && points != nullptr)
	{
		// Acting on what it holds starts at once, with the hand it holds it in
		const auto single = [&](size_t animation, float eventMs) {
			const std::array<size_t, 1> animations {animation};
			const std::array<float, 1> weights {1.0f};
			SetSlots(action, animations, weights);
			action.mirrored = held != nullptr && held->mirrored;
			action.eventMs = eventMs;
		};
		// The creature measures the speed of its hand for a toss when it first starts to let go of a thing, and keeps
		// that speed
		if (action.kind == Kind::PutDown || action.kind == Kind::Discard)
		{
			TossHand(creature, *points);
		}
		bool now = true;
		switch (action.kind)
		{
		case Kind::PutDown:
			single(creature_throw::k_PutDown, points->putDownMs);
			break;
		case Kind::Discard:
		{
			// The animation that puts the thing down lets go at the moment of the put-down. Any other animation lets go at
			// the moment of the toss, if it lets go.
			const auto animation = action.animations.front() != 0 ? action.animations.front() : creature_throw::k_Discard;
			single(animation, creature_throw::LetGoOf(animation) == creature_throw::LetGo::PutDownStill ? points->putDownMs
			                                                                                            : points->discardMs);
			break;
		}
		case Kind::Eat:
			single(creature_throw::k_Eat, points->eatMs);
			break;
		case Kind::Keep:
			// The animation was chosen as the action was asked for
			single(action.animations.front(), 0.0f);
			action.eventDone = true;
			break;
		case Kind::PickUp:
		case Kind::Destroy:
		case Kind::Throw:
		case Kind::Point:
			now = false;
			break;
		case Kind::Catch:
			now = StartCatch(creature, action, *points);
			break;
		case Kind::Kick:
			single(creature_object_actions::k_KickAnimation, points->kickMs);
			action.mirrored = false;
			break;
		case Kind::TakeFromHand:
		{
			// It reaches with the hand it was asked to, whatever it holds
			const bool mirrored = action.mirrored;
			single(creature_giving::k_TakeAnimation, points->takeFromHandMs);
			action.mirrored = mirrored;
			break;
		}
		}
		if (now)
		{
			BeginPlaying(creature, action, registry.Get<CreatureAnimation>(creature));
			if (action.kind == Kind::Catch)
			{
				TurnToCatch(creature, action);
			}
		}
	}
	const bool started = action.status == Status::Running;
	registry.AssignOrReplace<CreatureObjectAction>(creature, std::move(action));
	return started;
}

bool CreatureObjectActionSystem::Catch(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	// While its body acts out something else the catch waits, tried again each frame
	if (BodyBusy(registry.TryGet<const CreatureObjectAction>(creature)))
	{
		registry.AssignOrReplace<PendingCatch>(creature, PendingCatch {.object = object});
		return true;
	}
	registry.Remove<PendingCatch>(creature);
	return Start(creature, {.kind = Kind::Catch, .target = object});
}

void CreatureObjectActionSystem::StartPendingCatches()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<entt::entity, entt::entity>> due;
	registry.Each<const PendingCatch>([&](entt::entity creature, const PendingCatch& pending) {
		if (!BodyBusy(registry.TryGet<const CreatureObjectAction>(creature)))
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
	const auto heading = glm::length(towards) > 0.0f ? std::atan2(-towards.x, -towards.y) : 0.0f;
	return transform.position + glm::vec3(glm::eulerAngleY(heading) * glm::vec4(*hand, 0.0f));
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

bool CreatureObjectActionSystem::EatHeld(entt::entity creature)
{
	const auto held = GetHeld(creature);
	// It eats whatever it holds. A meal with no food in it comes back up after.
	if (!held.has_value())
	{
		auto action = CreatureObjectAction {.kind = Kind::Eat};
		Fail(action, "it holds nothing to eat");
		Locator::entitiesRegistry::value().AssignOrReplace<CreatureObjectAction>(creature, std::move(action));
		return false;
	}
	return Start(creature, {.kind = Kind::Eat, .target = held});
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

bool CreatureObjectActionSystem::Destroy(entt::entity creature, entt::entity target)
{
	return Start(creature, {.kind = Kind::Destroy, .target = target});
}

bool CreatureObjectActionSystem::Kick(entt::entity creature, entt::entity target)
{
	return Start(creature, {.kind = Kind::Kick, .target = target});
}

bool CreatureObjectActionSystem::PointAt(entt::entity creature, const glm::vec3& point)
{
	return Start(creature, {.kind = Kind::Point, .point = point});
}

bool CreatureObjectActionSystem::PointAtFor(entt::entity creature, const glm::vec3& point, float seconds,
                                            std::optional<entt::entity> thing)
{
	return Start(creature, {.kind = Kind::Point, .target = thing, .point = point, .pointSeconds = seconds});
}

bool CreatureObjectActionSystem::PointUntilTold(entt::entity creature, const glm::vec3& point)
{
	return Start(creature, {.kind = Kind::Point, .point = point, .pointsUntilTold = true});
}

void CreatureObjectActionSystem::MovePoint(entt::entity creature, const glm::vec3& point)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.Valid(creature) ? registry.TryGet<CreatureObjectAction>(creature) : nullptr;
	if (action != nullptr && action->kind == Kind::Point)
	{
		action->point = point;
	}
}

void CreatureObjectActionSystem::EndPoint(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.Valid(creature) ? registry.TryGet<CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->kind != Kind::Point || action->status != Status::Running)
	{
		return;
	}
	if (action->phase != Phase::Playing)
	{
		// It has not started to point yet: it stops at once
		action->status = Status::Done;
		return;
	}
	action->holdMs = 0.0f;
	action->pointEnding = true;
}

std::optional<glm::vec3> CreatureObjectActionSystem::GetPointingHand(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return std::nullopt;
	}
	const auto* action = registry.TryGet<const CreatureObjectAction>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (action == nullptr || animation == nullptr || transform == nullptr || action->kind != Kind::Point ||
	    action->phase != Phase::Playing || action->status != Status::Running || action->pointEnding)
	{
		return std::nullopt;
	}
	const auto* points = PointsOf(registry.Get<const Creature>(creature));
	if (points == nullptr || animation->boneMatrices.empty())
	{
		return std::nullopt;
	}
	const auto hand = HandBone(*points, *animation, action->mirrored);
	if (hand >= animation->boneMatrices.size())
	{
		return std::nullopt;
	}
	return glm::vec3(creature::PosedBone(hand, animation->boneMatrices, PlacementOf(*transform))[3]);
}

void CreatureObjectActionSystem::Cancel(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(creature))
	{
		registry.Remove<CreatureObjectAction>(creature);
		if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
		{
			animation->slots.clear();
		}
	}
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
	// The thing keeps the turn that it has in the hand as the body is posed now. Let go, it starts where it hangs, as
	// it is drawn. Thrown, it starts with its origin at the finger ends.
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	auto pose =
	    transform != nullptr && animation != nullptr ? std::optional(HeldPoseOf(*held, *transform, *animation)) : std::nullopt;
	if (pose.has_value())
	{
		pose = creature_throw::ReleaseStart(*pose, FingerEndsOf(held->bone, *animation, PlacementOf(*transform)),
		                                    from == ReleaseFrom::FingerEnds);
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
	// the up axis (the game's turning is the other way round from the physics' own), credited to the creature's player
	constexpr glm::vec3 k_ReleaseTurn {0.0f, -1.0f, 0.0f};
	auto& registry = Locator::entitiesRegistry::value();
	const auto* owner = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	Locator::dynamicsSystem::value().LetGoFromHand(object,
	                                               {.velocity = velocity,
	                                                .angularMomentum = k_ReleaseTurn,
	                                                .player = owner != nullptr ? std::optional(owner->owner) : std::nullopt,
	                                                .creature = registry.Valid(creature) ? creature : entt::null});
	// The creature remembers it as the last thing it dropped
	if (registry.Valid(creature))
	{
		registry.AssignOrReplace<CreatureDroppedObject>(creature, CreatureDroppedObject {.object = object});
	}
}

CreatureObjectActionSystem::State CreatureObjectActionSystem::GetState(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr)
	{
		return State::Idle;
	}
	switch (action->status)
	{
	case Status::Running:
	case Status::Contact:
		return State::Busy;
	case Status::Done:
		return State::Done;
	case Status::Failed:
		break;
	}
	return State::Failed;
}

creature_object_actions::Failure CreatureObjectActionSystem::GetFailure(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	return action != nullptr && action->status == Status::Failed ? action->failureKind
	                                                             : creature_object_actions::Failure::Other;
}

bool CreatureObjectActionSystem::IsCatching(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	return action != nullptr && action->kind == Kind::Catch && action->phase == Phase::Playing &&
	       (action->status == Status::Running || action->status == Status::Contact);
}

std::optional<float> CreatureObjectActionSystem::GetProgress(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->phase != Phase::Playing ||
	    (action->status != Status::Running && action->status != Status::Contact))
	{
		return std::nullopt;
	}
	if (action->holdMs > 0.0f || action->durationMs <= 0.0f)
	{
		return 0.0f;
	}
	return std::clamp(action->timeMs / action->durationMs, 0.0f, 1.0f);
}

std::optional<entt::entity> CreatureObjectActionSystem::GetHeld(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* held = registry.Valid(creature) ? registry.TryGet<const CreatureHeldObject>(creature) : nullptr;
	return held != nullptr && registry.Valid(held->object) ? std::optional(held->object) : std::nullopt;
}

std::optional<float> CreatureObjectActionSystem::FoodValueOf(entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::infoConstants::has_value() || !registry.Valid(object))
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	float value = 0.0f;
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		// A pot or pile of food is worth as much as the food in it. A storage pit's pile is eaten from where it lies,
		// which isn't done here.
		const auto kind = static_cast<size_t>(pot->type);
		const bool food = kind < info.pot.size() && info.pot.at(kind).resourceType == ResourceType::Food &&
		                  pot->type != PotInfo::StoragePitFoodPile;
		value = food ? static_cast<float>(pot->amount) : 0.0f;
	}
	else if (const auto* objectInfo = FoodInfoOf(registry, object))
	{
		// Anything else has the food value of its kind, when its kind is meat or vegetable
		const auto eaten = static_cast<uint32_t>(FoodType::Meat) | static_cast<uint32_t>(FoodType::Vegetable);
		value = (static_cast<uint32_t>(objectInfo->foodType) & eaten) != 0 ? objectInfo->foodValue : 0.0f;
	}
	return value > 0.0f ? std::optional(value) : std::nullopt;
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
	facts.holdable = creature_object_actions::Holdable(thing);
	return facts;
}

creature_object_actions::PickUpFacts CreatureObjectActionSystem::PickUpFactsOf(entt::entity creature, entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	const auto owner = body != nullptr ? std::optional(body->owner) : std::nullopt;
	const auto* action = registry.AllOf<Villager>(object) ? registry.TryGet<const LivingAction>(object) : nullptr;
	const bool worshipping = action != nullptr && owner.has_value() &&
	                         action->states[static_cast<size_t>(LivingAction::Index::Final)] ==
	                             static_cast<uint8_t>(VillagerStates::WorshippingAtWorshipSite) &&
	                         ecs::world_objects::PlayerOf(object) == owner;
	const bool computerCreature =
	    owner.has_value() && Locator::playerSystem::has_value() && Locator::playerSystem::value().IsComputerPlayer(*owner);
	// In the map: in its cells, and out of any hand, flight or tornado
	const auto* holder = registry.TryGet<const HeldByCreature>(object);
	const bool inMap =
	    registry.AllOf<MapCellResident>(object) && !registry.AnyOf<InHand, HeldByCreature, InPhysics, CarriedByTornado>(object);
	const bool heldByThisCreature = holder != nullptr && holder->creature == creature;
	const auto* leash = registry.Valid(creature) ? registry.TryGet<const CreatureLeash>(creature) : nullptr;
	const bool leashTied = leash != nullptr && leash->worn.has_value() && leash->worn->tiedTo == object;

	std::optional<creature_object_actions::LiftFacts> lift;
	if (body != nullptr)
	{
		if (const auto reach = PickUpStraightReach(creature))
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
	    .cannotBePickedUp = registry.AllOf<CannotBePickedUp>(object),
	    .villagerAtHome = registry.AllOf<Villager, AtHome>(object),
	    .worshippingForCreaturesPlayer = worshipping,
	    .inScript = registry.AllOf<InScript>(object),
	    .computerCreatureNotScripted =
	        computerCreature && registry.Valid(creature) && !registry.AllOf<ScriptControlled>(creature),
	    .townArtefact = registry.AllOf<TownArtefact>(object),
	    .inMapOrHeldByCreature = inMap || heldByThisCreature,
	    .leashTiedToIt = leashTied,
	    .lift = lift,
	};
}

bool CreatureObjectActionSystem::CanPickUp(entt::entity creature, entt::entity object) const
{
	const auto kind = KindFactsOf(object);
	return kind.rule != creature_object_actions::KindRule::Never &&
	       creature_object_actions::MayPickUpKind(kind, PickUpFactsOf(creature, object));
}

bool CreatureObjectActionSystem::CanEat(entt::entity creature, entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.Valid(object))
	{
		return false;
	}
	const auto pickUp = PickUpFactsOf(creature, object);
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* info = FoodInfoOf(registry, object);
	const bool refused =
	    mind != nullptr && info != nullptr && mind->refusedFoodSorts.contains(static_cast<uint32_t>(info->type));
	const auto* body = registry.TryGet<const Creature>(creature);
	const bool ownVillager = registry.AllOf<Villager>(object) && body != nullptr &&
	                         ecs::world_objects::PlayerOf(object) == std::optional(body->owner);
	return creature_object_actions::MayEat(KindFactsOf(object),
	                                       {.mayPickUp = CanPickUp(creature, object),
	                                        .refusedSort = refused,
	                                        .computersOwnVillager = pickUp.computerCreatureNotScripted && ownVillager});
}

bool CreatureObjectActionSystem::CanEatHeld(entt::entity creature) const
{
	const auto held = GetHeld(creature);
	return held.has_value() && creature_object_actions::MayEatHeld(KindFactsOf(*held), CanPickUp(creature, *held));
}

bool CreatureObjectActionSystem::CanTakeFromHand(entt::entity creature, entt::entity object) const
{
	const auto kind = KindFactsOf(object);
	return kind.holdable && creature_object_actions::MayTakeKindFromHand(kind, PickUpFactsOf(creature, object));
}

bool CreatureObjectActionSystem::CanBeCaught(entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return creature_object_actions::MayCatchKind(KindFactsOf(object),
	                                             {.cannotBePickedUp = registry.AllOf<CannotBePickedUp>(object),
	                                              .villagerAtHome = registry.AllOf<Villager, AtHome>(object)});
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
	    registry.AllOf<MapCellResident>(target) && !registry.AnyOf<InHand, HeldByCreature, InPhysics, CarriedByTornado>(target);
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

std::optional<float> CreatureObjectActionSystem::PickUpStraightReach(entt::entity creature) const
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	// Measured once in the model's own size, the first time it is wanted, and scaled as the creature is drawn
	if (const auto* kept = registry.TryGet<const PickUpReach>(creature))
	{
		return kept->straightReach * transform->scale.x;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* points = body != nullptr ? PointsOf(*body) : nullptr;
	if (points == nullptr || !Locator::creatureAnimationSystem::has_value())
	{
		return std::nullopt;
	}
	auto unscaled = *transform;
	unscaled.scale = glm::vec3(1.0f);
	const auto reach =
	    MeasureReach(creature, unscaled, *points, ReachWith::FingerEnds, creature_reach::k_PickUpAnimations, points->pickUpMs);
	if (!reach.has_value())
	{
		return std::nullopt;
	}
	const float straight = creature_reach::StraightReach(*reach);
	registry.AssignOrReplace<PickUpReach>(creature, PickUpReach {.straightReach = straight});
	return straight * transform->scale.x;
}

void CreatureObjectActionSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::creatureLocomotionSystem::has_value() || !Locator::creatureAnimationSystem::has_value())
	{
		return;
	}

	// What is held by a creature that is gone, or no longer holds it, falls
	std::vector<entt::entity> loose;
	registry.Each<const HeldByCreature>([&](entt::entity entity, const HeldByCreature& by) {
		const auto* held = registry.Valid(by.creature) ? registry.TryGet<const CreatureHeldObject>(by.creature) : nullptr;
		if (held == nullptr || held->object != entity)
		{
			loose.push_back(entity);
		}
	});
	for (const auto entity : loose)
	{
		registry.Remove<HeldByCreature>(entity);
		LetGo(entt::null, entity, glm::vec3(0.0f));
	}
	std::vector<entt::entity> emptyHanded;
	registry.Each<const CreatureHeldObject>([&](entt::entity entity, const CreatureHeldObject& held) {
		if (!registry.Valid(held.object))
		{
			emptyHanded.push_back(entity);
		}
	});
	for (const auto entity : emptyHanded)
	{
		registry.Remove<CreatureHeldObject>(entity);
	}

	registry.Each<const Creature, const Transform, CreatureAnimation, CreatureObjectAction>(
	    [&](entt::entity entity, const Creature& body, const Transform& transform, CreatureAnimation& animation,
	        CreatureObjectAction& action) {
		    if (action.status == Status::Running && action.phase == Phase::Approach)
		    {
			    Approach(entity, action, body, transform, animation, registry.TryGet<const CreatureHeldObject>(entity));
		    }
	    });

	// Carrying something heavy makes it stronger as it walks about
	registry.Each<const Creature, CreatureNeeds>([&](entt::entity entity, const Creature&, CreatureNeeds& needs) {
		needs.carriedWeight.reset();
		const auto held = GetHeld(entity);
		if (!held.has_value())
		{
			return;
		}
		const auto weight = WeightOf(registry, *held);
		const auto own = WeightOf(registry, entity);
		if (weight.has_value() && own.has_value() && *own > 0.0f)
		{
			needs.carriedWeight = std::clamp(*weight / *own, 0.0f, 1.0f);
		}
	});
}

void CreatureObjectActionSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Creature, const Transform, CreatureAnimation, CreatureObjectAction>(
	    [&](entt::entity entity, const Creature& body, const Transform& transform, CreatureAnimation& animation,
	        CreatureObjectAction& action) {
		    if (action.phase != Phase::Playing || (action.status != Status::Running && action.status != Status::Contact))
		    {
			    return;
		    }
		    const float step = gameTime.count() * creature_layers::PlaybackRate(ShownSize(body));
		    if (action.kind == Kind::Catch)
		    {
			    using Catching = CreatureObjectAction::Catching;
			    if (action.catching == Catching::Ready || action.catching == Catching::Stepping)
			    {
				    UpdateReady(registry, entity, body, transform, animation, action, step);
			    }
			    if (action.catching != Catching::Ready && action.catching != Catching::Stepping &&
			        action.status != Status::Done)
			    {
				    UpdateCatch(registry, entity, body, transform, animation, action, step);
			    }
			    return;
		    }
		    action.timeMs = action.kind == Kind::Point
		                        ? creature_pointing::AdvanceTime(action.timeMs, step, action.pointMs, action.pointEnding)
		                        : action.timeMs + step;

		    // Until it has hold, the reach follows what it reaches for, and fails when it gets out of reach
		    if ((action.kind == Kind::PickUp || action.kind == Kind::Destroy) && !action.eventDone && action.reach.has_value())
		    {
			    const auto* at = action.target.has_value() && registry.Valid(*action.target)
			                         ? registry.TryGet<const Transform>(*action.target)
			                         : nullptr;
			    if (at == nullptr)
			    {
				    Fail(action, "it has gone");
				    animation.slots.clear();
				    return;
			    }
			    const auto local = ToLocal(transform, at->position);
			    const auto reach =
			        creature_reach::Solve(action.mirrored ? creature_reach::Mirrored(*action.reach) : *action.reach, local);
			    if (!reach.inRange)
			    {
				    Fail(action, "it got out of reach");
				    animation.slots.clear();
				    return;
			    }
			    action.weights = reach.weights;
		    }

		    if (action.kind == Kind::Point)
		    {
			    // Pointing at a thing, it points wherever the thing has got to. The blend follows the point each frame.
			    if (action.target.has_value() && registry.Valid(*action.target))
			    {
				    if (const auto* at = registry.TryGet<const Transform>(*action.target); at != nullptr)
				    {
					    action.point = at->position;
				    }
			    }
			    SetPointing(action, transform, body, registry.TryGet<const CreatureHeldObject>(entity), false);
			    if (!action.pointEnding)
			    {
				    action.holdMs -= gameTime.count();
				    if (action.holdMs <= 0.0f)
				    {
					    // The point ends, and the arm plays to the end of its animations
					    action.holdMs = 0.0f;
					    action.pointEnding = true;
				    }
			    }
			    else if (action.timeMs >= action.durationMs)
			    {
				    action.status = Status::Done;
			    }
		    }
		    else if (action.timeMs >= action.durationMs && (action.eventDone || action.eventMs > action.durationMs))
		    {
			    // Thrown into a store, it waits for the thing to come down first
			    const bool flying = action.thrown.has_value() && registry.Valid(*action.thrown) &&
			                        Locator::dynamicsSystem::has_value() &&
			                        Locator::dynamicsSystem::value().IsFlying(*action.thrown);
			    if (!creature_object_actions::ThrowOver(true, action.waitsForLanding, flying))
			    {
				    animation.slots.clear();
				    return;
			    }
			    action.status = Status::Done;
		    }
		    if (action.status == Status::Done)
		    {
			    animation.slots.clear();
			    return;
		    }

		    animation.slots.clear();
		    for (size_t i = 0; i < action.animationCount; ++i)
		    {
			    const auto duration = DurationOf(entity, action.animations.at(i));
			    // Each animation holds its last frame
			    const auto time = std::clamp(action.timeMs, 0.0f, std::max(duration - 1.0f, 0.0f));
			    animation.slots.push_back({.animation = action.animations.at(i),
			                               .timeMs = time,
			                               .weight = action.weights.at(i),
			                               .mirrored = action.mirrored});
		    }
	    });
	StartPendingCatches();
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
	const auto limit = radius * radius;
	for (const auto& subMesh : model.GetSubMeshes())
	{
		if ((subMesh->GetFlags().lodMask & 1U) == 0)
		{
			continue;
		}
		for (const auto& corner : subMesh->GetSurface().positions)
		{
			const auto world = at->position + (at->rotation * (at->scale * corner));
			const auto away = world - centre;
			if (glm::dot(away, away) < limit)
			{
				return true;
			}
		}
	}
	return false;
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
		const auto distance = glm::distance(glm::xz(at.position), glm::xz(point));
		if (!creature_stomp::InReach(victim, distance, sphereRadius, object_measures::TwoDRadius(registry, thing)))
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
				Locator::buildingDamageSystem::value().CreatureBlows(thing, creature, blows);
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
			static_cast<void>(Locator::dynamicsSystem::value().InitialisePhysics(
			    thing,
			    {.velocity = creature_stomp::KnockVelocity(move), .thrower = creature, .player = body.owner, .add = true}));
		}
	}
}

/// The spheres round the body of each creature come on and go off as its body actions start. While they are on, each
/// part that moved far enough since its last check knocks about what it reaches.
void Trample(ecs::Registry& registry)
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
		    const auto placement = PlacementOf(transform);
		    const auto bones = PartBones(*points, animation);
		    const auto at = [&](size_t part) {
			    const auto bone = bones.at(part);
			    return bone < animation.boneMatrices.size()
			               ? glm::vec3(creature::PosedBone(bone, animation.boneMatrices, placement)[3])
			               : transform.position;
		    };
		    const auto size = ShownSize(body);
		    if (!(spheres->seen == start))
		    {
			    const auto change = creature_stomp::SwitchFor(start);
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
} // namespace

void CreatureObjectActionSystem::LateUpdate(std::chrono::duration<float, std::milli> /*gameTime*/)
{
	auto& registry = Locator::entitiesRegistry::value();
	Trample(registry);

	// The moments things are taken hold of and let go of, with the hand where the body was posed this frame
	struct Moment
	{
		entt::entity creature;
	};
	std::vector<Moment> moments;
	registry.Each<CreatureObjectAction>([&](entt::entity entity, const CreatureObjectAction& action) {
		if (action.phase == Phase::Playing && action.status == Status::Running && !action.eventDone &&
		    action.timeMs >= action.eventMs)
		{
			moments.push_back({entity});
		}
	});
	for (const auto& moment : moments)
	{
		const auto creature = moment.creature;
		auto& action = registry.Get<CreatureObjectAction>(creature);
		const auto& body = registry.Get<const Creature>(creature);
		const auto& transform = registry.Get<const Transform>(creature);
		const auto& animation = registry.Get<const CreatureAnimation>(creature);
		const auto* points = PointsOf(body);
		action.eventDone = true;
		if (points == nullptr)
		{
			continue;
		}
		const auto handPosition =
		    FingerEndsOf(HandBone(*points, animation, action.mirrored), animation, PlacementOf(transform));
		switch (action.kind)
		{
		case Kind::PickUp:
		{
			const auto object = *action.target;
			if (!registry.Valid(object) || registry.AllOf<HeldByCreature>(object))
			{
				Fail(action, "it has gone");
				break;
			}
			registry.AssignOrReplace<CreatureHeldObject>(
			    creature, CreatureHeldObject {.object = object,
			                                  .mirrored = action.mirrored,
			                                  .bone = HandBone(*points, animation, action.mirrored)});
			registry.AssignOrReplace<HeldByCreature>(object, HeldByCreature {.creature = creature});
			const bool flying = Locator::dynamicsSystem::has_value() && Locator::dynamicsSystem::value().IsFlying(object);
			// Whatever it is, it leaves the map's cells while the creature holds it
			registry.Remove<MapCellResident, MapCellMover>(object);
			// A standing tree pulled from the ground creaks, at the creature
			if (registry.AllOf<Tree>(object) && !flying && Locator::soundTagSystem::has_value())
			{
				Locator::soundTagSystem::value().CreatePointSound(TreeCreak(), transform.position, false);
			}
			// Taken out of the air, it leaves the physics without going back on the map
			if (flying)
			{
				Locator::dynamicsSystem::value().RemoveObject(object, false, true);
				// What its landing set, the creature's hand overrides: a person or animal is held
				if (registry.AllOf<Villager>(object))
				{
					villager_physics::IntoHand(object);
				}
				else if (registry.AllOf<Animal>(object) && Locator::animalSystem::has_value())
				{
					Locator::animalSystem::value().IntoHand(object);
				}
			}
			if (registry.AllOf<Villager>(object))
			{
				StopWalking(registry, object);
				DrawInHand(registry, object);
			}
			action.status = Status::Contact;
			break;
		}
		case Kind::Destroy:
		{
			const auto target = *action.target;
			action.status = Status::Contact;
			if (!registry.Valid(target))
			{
				break;
			}
			// A living thing under the foot takes the crush of the stomp, from the creature and its player
			if (registry.AnyOf<Villager, Animal, Creature>(target))
			{
				Crush(creature, body, target);
			}
			// A building is broken about where it stands, and a rock is smashed in two; the blow does nothing to anything
			// else
			else if (registry.AnyOf<Abode, SpellDispenser>(target))
			{
				if (Locator::buildingDamageSystem::has_value())
				{
					Locator::buildingDamageSystem::value().Smash(target, creature, ShownSize(body));
				}
			}
			else if (object_physics::IsRock(target) && Locator::dynamicsSystem::has_value())
			{
				object_physics::SmashRock(Locator::dynamicsSystem::value(), target, creature);
			}
			break;
		}
		case Kind::PutDown:
			Release(creature, glm::vec3(0.0f));
			break;
		case Kind::Discard:
		{
			const auto letGo = creature_throw::LetGoOf(action.animations.front());
			// The animation that puts the thing down lets go of it at rest. An animation that does not toss or put down
			// keeps the thing in the hand.
			if (letGo == creature_throw::LetGo::PutDownStill)
			{
				Release(creature, glm::vec3(0.0f));
				break;
			}
			if (letGo == creature_throw::LetGo::Keep)
			{
				break;
			}
			// The toss away takes the speed of the hand that the creature kept since its first toss
			const auto hand = TossHand(creature, *points);
			const auto velocity = hand.has_value()
			                          ? creature_throw::TossVelocity(*hand * transform.scale, action.mirrored,
			                                                         transform.rotation, creature_throw::k_DiscardSpeedShare)
			                          : glm::vec3(0.0f);
			Release(creature, velocity);
			break;
		}
		case Kind::Throw:
			action.thrown = GetHeld(creature);
			Release(creature,
			        creature_throw::ReleaseVelocity(action.point, handPosition, std::max(action.flightSeconds, k_Tiny)),
			        ReleaseFrom::FingerEnds);
			break;
		case Kind::Eat:
		{
			const auto held = GetHeld(creature);
			if (!held.has_value())
			{
				break;
			}
			if (BringsBackUp(registry, *held))
			{
				BringBackUp(creature, *held);
				break;
			}
			const auto value = FoodValueOf(*held);
			// A magic mushroom makes it high once it is done with what it is doing
			if (IsMagicMushroom(registry, *held) && Locator::creatureMindSystem::has_value())
			{
				Locator::creatureMindSystem::value().AteMagicMushroom(creature);
			}
			const auto meal = MealOf(registry, *held);
			CountMushroom(registry, creature, meal);
			registry.Remove<CreatureHeldObject>(creature);
			Consume(registry, creature, *held);
			const auto energy = value.has_value() && Locator::creaturePhysiologySystem::has_value()
			                        ? Locator::creaturePhysiologySystem::value().Eat(creature, *value)
			                        : 0.0f;
			const bool bringsUp = creature_object_actions::BringsMealBackUp(value.value_or(0.0f), meal.magicMushroom,
			                                                                meal.foodPile, meal.poisoned);
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} eats a thing of sort {}: food {}, brought up {}",
			                    entt::to_integral(creature), meal.sort.value_or(0), value.value_or(0.0f), bringsUp);
			auto* tallies = registry.TryGet<CreatureTallies>(creature);
			creature_meals::CountMeal(tallies != nullptr ? tallies->meals : registry.Assign<CreatureTallies>(creature).meals,
			                          meal.sort, !bringsUp);
			if (bringsUp)
			{
				RefuseMeal(creature, meal);
			}
			else
			{
				KeepMealDown(creature, energy);
			}
			break;
		}
		case Kind::TakeFromHand:
		{
			// It takes the thing only if the player's hand still gives it; otherwise its hand closes on nothing
			const auto object = *action.target;
			if (!registry.Valid(object) || !Locator::handGrabSystem::has_value() ||
			    !Locator::handGrabSystem::value().HandOver(object))
			{
				Fail(action, "the hand no longer gives it");
				break;
			}
			TakeInHand(registry, creature, object, action, *points, animation);
			action.status = Status::Contact;
			break;
		}
		case Kind::Kick:
			KickAway(creature, body, transform, animation, *action.target);
			action.status = Status::Contact;
			break;
		case Kind::Keep:
		case Kind::Point:
		case Kind::Catch:
			break;
		}
	}

	// What is held rides in the hand, turned with it
	bool moved = false;
	registry.Each<const Creature, const Transform, const CreatureAnimation, const CreatureHeldObject>(
	    [&](const Creature&, const Transform& transform, const CreatureAnimation& animation, const CreatureHeldObject& held) {
		    auto* at = registry.Valid(held.object) ? registry.TryGet<Transform>(held.object) : nullptr;
		    if (at == nullptr)
		    {
			    return;
		    }
		    // It turns with the hand bone and hangs below the finger ends by its own distance
		    const auto pose = HeldPoseOf(held, transform, animation);
		    at->rotation = pose.rotation;
		    at->position = pose.position;
		    moved = true;
	    });

	if (moved)
	{
		registry.SetDirty();
	}
}
