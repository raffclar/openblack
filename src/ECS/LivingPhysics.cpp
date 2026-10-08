/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingPhysics.h"

#include <cmath>
#include <cstdlib>

#include <functional>
#include <optional>
#include <utility>

#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsAngle.h"
#include "Debug/DebugEnv.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/HeldApply.h"
#include "ECS/Life.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/SeaCells.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerDrowning.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;
using openblack::ecs::life::LifeOf;
using openblack::ecs::life::ReduceLife;

namespace
{

/// The impact's damage: the crush effect (crush 1.0) x the object's defenceMultiplierCrush, then ReduceLife; at 0 life
/// with a damage the class's DestroyedByEffect.
void HurtByImpact(entt::entity entity, float damage)
{
	const bool villager = Locator::entitiesRegistry::value().AllOf<Villager>(entity);
	const float life = ReduceLife(entity, damage);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: {} hurt {:.3f}, life {:.2f}", villager ? "villager" : "animal", damage,
	                   life);
	if (life <= 0.0f)
	{
		if (Locator::entitiesRegistry::value().AllOf<Animal>(entity))
		{
			// the animal's DestroyedByEffect: SetDying (nothing while it flies, EndPhysics does it)
			ecs::animal_ai::DestroyedByEffect(entity);
			return;
		}
		// the villager's DestroyedByEffect -> VillagerDead(2 SPELL): nothing while it flies, and a corpse is dead
		// already; the landing's EndPhysics kills a flying one (reason 5 / 6). (approximate) the impact's EffectValues
		// player is not kept here: none
		ecs::villager::DestroyedByEffect(entity, std::nullopt, damage);
	}
}

/// A Living's reaction to a physics impact: hurt above 2 G. Returns false when the entity went away.
bool LivingReactToPhysicsImpact(entt::entity entity, float g)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (g > 2.0f)
	{
		float multiplier = 1.0f;
		if (const auto* animal = registry.TryGet<const Animal>(entity);
		    animal != nullptr && Locator::infoConstants::has_value())
		{
			multiplier = Locator::infoConstants::value().animal.at(static_cast<size_t>(animal->type)).defenceMultiplierCrush;
		}
		HurtByImpact(entity, (g - 2.0f) * 0.03f * multiplier);
		return ecs::IsAvailable(entity);
	}
	return true;
}

/// The animal's reaction to a physics impact, then the Living's.
bool AnimalReactToPhysicsImpact(entt::entity entity, [[maybe_unused]] PhysicsObject& po, const ImpactInfo& impact)
{
	auto& registry = Locator::entitiesRegistry::value();
	// hitting an available store of food, the animal available too: the store takes it with the thrower's interface
	// (its food, maybe none), and the animal goes; then nothing more
	const auto hit = impact.hitBy;
	if (registry.AllOf<Animal>(entity) &&
	    ecs::held_apply::AnimalImpactTakes(ecs::IsAvailable(hit), ecs::IsAvailable(entity),
	                                       ecs::resource_stores::IsResourceStore(hit, ResourceType::Food)) &&
	    ecs::resource_stores::DeleteObjectAndTakeResource(hit, entity, ecs::held_apply::ThrowerInterface(impact.byPlayer)))
	{
		return false;
	}
	return LivingReactToPhysicsImpact(entity, impact.g);
}

/// The animal's landing from where the bodiless path joins it (landType 3, no SetYAngle): back on the map, the
/// landType into its status, altitude 0, then the life / state work (LANDED, or dying / dead). False when the animal
/// is gone. Both paths share it
bool AnimalEndPhysicsFrom5F0DF9(entt::entity entity, uint16_t landType)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(entity))
	{
		return false;
	}
	// the landing's SetYAngle (or the bodiless path's redraw) has used the held y angle
	registry.Remove<living::HeldYAngle>(entity);
	auto& transform = registry.Get<Transform>(entity);
	if (Locator::terrainSystem::has_value())
	{
		transform.position.y =
		    Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
	}
	// back on the map after SetYAngle, before the landType / life / state work
	if (!PhysicsObjects::BackInMap(entity))
	{
		return false; // (approximate) the original goes on with an object marked for deletion; openblack destroys it at once
	}
	// the landType, altitude and the life / state work
	ecs::animal_ai::EndPhysics(entity, landType);
	registry.SetDirty();
	return registry.Valid(entity);
}

/// The animal's landing, the class's part.
entt::entity AnimalEndPhysics(entt::entity entity, PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the landType from the turn-start matrix, the heading from the current one, back on the land (altitude 0) and out
	// of the physics; LANDED, or dying / dead. There is no drowning for animals (only a sunk corpse goes, HasSunk). The
	// rows are the original's (living::BodyRows: as they are, from the hand and from the world path's
	// WorldMatrixRows(ObjectYAngle, S))
	const auto current = living::BodyRows(po.body.Rotation(), (po.flags & PhysicsObject::k_FromHand) != 0);
	const auto turnStart = living::BodyRows(po.turnStartRotation, (po.flags & PhysicsObject::k_FromHand) != 0);
	// the DecomposeYXZ yaw of the current matrix: the physics sets the angles from it again after EndPhysics
	const float drawnYaw = living::YawFromRotation(current);
	// the landType from the turn-start right.y
	const uint16_t landType = living::AnimalLandType(turnStart[0].y);
	// SetYAngle of Wrap(GetYAngle(current fwd) + pi): the y angle and the game angle
	const float yaw = living::AnimalLandingYaw(current[2]);
	ecs::animal_ai::SetYAngle(entity, yaw);
	// back on the map, the landType, the life / state work (the bodiless path's too)
	if (!AnimalEndPhysicsFrom5F0DF9(entity, landType))
	{
		return entt::null;
	}
	// the caller's angles from DecomposeYXZ(M): drawn at the current matrix's yaw (the game angle is kept)
	registry.Get<Transform>(entity).rotation = living::DrawnRotation(drawnYaw);
	if (debug_env::AnimalTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "landing: animal {} landType {} (original right.y {:.2f}, openblack column 0 y {:.2f}) yaw {:.4f} "
		                   "drawn {:.4f} landed {}",
		                   static_cast<uint32_t>(entity), landType, turnStart[0].y, po.turnStartRotation[0].y, yaw, drawnYaw,
		                   (po.flags & PhysicsObject::k_Landed) != 0);
	}
	registry.SetDirty();
	return entt::null;
}

entt::entity VillagerEndPhysicsFrom5F0B79(entt::entity entity, uint16_t landType, bool byPlayer);

/// The landType into the villager's status bits 4-5
void SetLandType(entt::entity entity, uint16_t landType)
{
	if (auto* v = Locator::entitiesRegistry::value().TryGet<Villager>(entity))
	{
		const auto bits = static_cast<uint16_t>(landType << Villager::k_LandTypeShift);
		v->status = static_cast<uint16_t>((v->status & ~Villager::k_StatusLandTypeMask) | bits);
	}
}

/// The villager's landing, the class's part.
entt::entity VillagerEndPhysics(entt::entity entity, PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the object's landing clears its in-physics flag before the dead branches below (VillagerDead's first test)
	const ecs::villager::EndingPhysicsScope ending(entity);
	// The rows are the original's (living::BodyRows: as they are, from the hand and from the world path's
	// WorldMatrixRows(ObjectYAngle, S))
	const auto current = living::BodyRows(po.body.Rotation(), (po.flags & PhysicsObject::k_FromHand) != 0);
	// the DecomposeYXZ yaw of the current matrix (only y is read): the y angle before EndPhysics, and again after it (below)
	const float drawnYaw = living::YawFromRotation(current);
	// the pose from the turn-start matrix (SyncTurnStart keeps it the ground-aligned one for a put-down)
	const bool fromHand = (po.flags & PhysicsObject::k_FromHand) != 0;
	const auto pose = living::VillagerLandingPoseOf(living::BodyRows(po.turnStartRotation, fromHand));
	// the player who threw it makes the creature empathise (desire, amount, position). TODO(creature): po.byPlayer stands
	// for "thrown by a player"
	// SetYAngle: the y angle and the game angle
	ecs::villager::SetYAngle(entity, pose.yaw);
	// not put down gently (not LANDED) -> no longer marked as held: it lands as a plain villager
	if (auto* v = registry.TryGet<Villager>(entity); v != nullptr && (po.flags & PhysicsObject::k_Landed) == 0)
	{
		v->flags = static_cast<uint16_t>(v->flags & ~Villager::k_FlagInHand);
	}
	// the caller's angles from DecomposeYXZ(M) after EndPhysics (nothing in between reads the y angle): drawn at the current
	// matrix's yaw, walking off on the game angle
	auto& transform = registry.Get<Transform>(entity);
	transform.rotation = living::DrawnRotation(drawnYaw);
	if (ecs::villager::TraceOn(entity))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "landing: villager {} landType {} (original right.y {:.2f}, openblack column 0 y {:.2f}) yaw {:.4f} "
		                   "game {} "
		                   "drawn {:.4f} landed {}",
		                   static_cast<uint32_t>(entity), pose.landType, living::BodyRows(po.turnStartRotation, fromHand)[0].y,
		                   po.turnStartRotation[0].y, pose.yaw, ecs::villager::GetGameAngle(entity), drawnYaw,
		                   (po.flags & PhysicsObject::k_Landed) != 0);
	}
	// back on the map, the landType, the water / death branches, LANDED (the bodiless path's too)
	return VillagerEndPhysicsFrom5F0B79(entity, pose.landType, po.byPlayer);
}

/// The villager's landing from where the bodiless path joins it (landType 3, no pose, no empathy, no SetYAngle, still
/// marked as held). byPlayer = thrown by a player; on the bodiless path the player is never asked: false. Both paths
/// share it
entt::entity VillagerEndPhysicsFrom5F0B79(entt::entity entity, uint16_t landType, bool byPlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the landing's SetYAngle (or the bodiless path's redraw) has used the held y angle
	registry.Remove<living::HeldYAngle>(entity);
	auto& transform = registry.Get<Transform>(entity);
	if (Locator::terrainSystem::has_value())
	{
		transform.position.y =
		    Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
	}
	// back on the map after SetYAngle, before the landType, the water test and the state / death work
	if (!PhysicsObjects::BackInMap(entity))
	{
		return entt::null; // (approximate) the original goes on with an object marked for deletion; openblack destroys it at
		                   // once
	}
	// the landType into the status bits 4-5, after the object's landing; altitude 0: the position is on the ground
	// (above)
	SetLandType(entity, landType);
	// the water test of the position (the cell's water bit, the shallow shore too; not the body's inWater)
	if (ecs::sea_cells::IsWater(transform.position))
	{
		// alive: the last player to interact is the thrower, none without a body; dead: VillagerDead(6, the thrower or,
		// without a body, the neutral player, so NEUTRAL in VillagerDeadDrowned), 0.01, 1)
		ecs::RememberLastPlayerToInteract(entity, byPlayer);
		ecs::VillagerEndPhysicsInWater(entity);
		return entt::null;
	}
	// on land with life <= 0: player = the thrower (the hand's, PLAYER_ONE when the hand threw it, (inferred) as
	// VillagerDrowning's last player); dead already -> SetTopState(DEAD) with its counter kept, else
	// VillagerDead(5 PLAYER_INTERACTION, player, 0.0, 1); then the landType again (SetDying's change undone)
	if (LifeOf(entity) <= 0.0f)
	{
		std::optional<PlayerNames> player;
		if (byPlayer) // without a body: none
		{
			player = PlayerNames::PLAYER_ONE;
		}
		const auto* v = registry.TryGet<const Villager>(entity);
		if (v != nullptr && (v->status & Villager::k_StatusDead) != 0)
		{
			ecs::villager::SetTopState(entity, VillagerStates::Dead);
		}
		else
		{
			ecs::villager::VillagerDead(entity, DeathReason::PlayerInteraction, player, 0.0f, 1);
		}
		SetLandType(entity, landType);
		registry.SetDirty();
		return entt::null;
	}
	// SetTopState(LANDED): its landing clip (VillagerLandedClip by the landType), then the villager's Landed.
	// TODO(villager-jobs): the special test of a marked villager and the return to the previous state (for a flagged
	// villager or a state that asks for it) are not ported
	ecs::SetVillagerState(entity, VillagerStates::Landed);
	registry.SetDirty();
	return entity;
}

/// The class's InitialisePhysics for a knocked resting proxy (ClassHandlers::initialisePhysicsKnocked), on top of its
/// without-body part: the same Villager / Living InitialisePhysics runs, only the body is the proxy's own
std::function<bool(entt::entity, PhysicsObject&)> KnockedInitialisePhysics(std::function<bool(entt::entity)> classPart)
{
	return [classPart = std::move(classPart)](entt::entity entity, PhysicsObject&) {
		// the physics asks CanBecomeAPhysicsObject and the carried flag before the call
		if (!PhysicsObjects::CanWakeKnockedProxy(entity))
		{
			return false;
		}
		// the villager's InitialisePhysics (CreateDroppedResource with v = w = 0) / the Living's: refused when
		// SetTopState(FLYING) does not take
		if (!classPart || !classPart(entity))
		{
			return false;
		}
		// the object's InitialisePhysics, last: started
		return PhysicsObjects::CanWakeKnockedProxy(entity);
	};
}

} // namespace

namespace openblack::ecs::living
{

void RegisterPhysicsHandlers()
{
	PhysicsObjects::ClassHandlers villager;
	// The villager's InitialisePhysics (anything but the hand: CreateDroppedResource(v, av, none), then the Living's);
	// from the hand the physics calls CreateDroppedResource itself (ECS/Physics/FromHand). (approximate) here after the
	// body was added, not before, and without the angular velocity (the handler is not given it; the log that would use
	// it is not made yet, VillagerResources).
	// The Living's InitialisePhysics: the villager flies (THROWN clips, ECS/VillagerAnimations).
	// From the hand (fromHand) the physics calls it only when the villager did not land (a body for this object, not
	// LANDED -> SetTopState(FLYING); PhysicsObjects::InitialisePhysicsOfClass): a villager put down gently goes IN_HAND
	// -> LANDED through EndPhysics and stays marked as held. TODO: from the hand the villager's disciple is reset
	// when it flies or has no body (skipped when put down gently), and the Living's StorePreviousState
	villager.initialisePhysics = [](entt::entity entity, PhysicsObject& po, bool fromHand) {
		if (!fromHand)
		{
			villager::CreateDroppedResource(entity, po.body.velocity, std::nullopt, std::nullopt);
		}
		ecs::SetVillagerState(entity, VillagerStates::Flying);
	};
	// the Living's reaction to an impact (the villager shares it)
	villager.reactToImpact = [](entt::entity entity, PhysicsObject&, const ImpactInfo& impact) {
		return LivingReactToPhysicsImpact(entity, impact.g);
	};
	villager.endPhysics = VillagerEndPhysics;
	// The villager's InitialisePhysics without a body (a particle system takes it): CreateDroppedResource, then the
	// Living's: FLYING already -> taken; else StorePreviousState unless IN_HAND, and SetTopState(FLYING) must return 1,
	// else refused
	villager.initialisePhysicsWithoutBody = [](entt::entity entity) {
		// (approximate) CreateDroppedResource(v, w, none): the take's v = (0, 0, 0), w = (0, 1, 0) are not passed yet: the
		// handler is bool(entity)
		villager::CreateDroppedResource(entity, std::nullopt, std::nullopt, std::nullopt);
		const auto* action = Locator::entitiesRegistry::value().TryGet<const components::LivingAction>(entity);
		if (action == nullptr)
		{
			return false;
		}
		const auto top = static_cast<VillagerStates>(action->states[static_cast<size_t>(components::LivingAction::Index::Top)]);
		if (top == VillagerStates::Flying)
		{
			return true;
		}
		if (top != VillagerStates::InHand)
		{
			villager::StorePreviousState(entity);
		}
		return villager::SetTopState(entity, VillagerStates::Flying) == 1;
	};
	// the villager's InitialisePhysics for a knocked proxy: the without-body part, then the object's
	villager.initialisePhysicsKnocked = KnockedInitialisePhysics(villager.initialisePhysicsWithoutBody);
	PhysicsObjects::SetClassHandlers(PhysicsClass::Villager, std::move(villager));

	PhysicsObjects::ClassHandlers animal;
	// from the hand: FLYING, the species' THROWN clip
	animal.initialisePhysics = [](entt::entity entity, PhysicsObject&, bool) { ecs::animal_ai::InitialisePhysics(entity); };
	animal.reactToImpact = AnimalReactToPhysicsImpact;
	animal.endPhysics = AnimalEndPhysics;
	animal.initialisePhysicsWithoutBody = ecs::animal_ai::InitialisePhysicsWithoutBody; // the Living's
	// the Living's InitialisePhysics for a knocked proxy (the animal has no override)
	animal.initialisePhysicsKnocked = KnockedInitialisePhysics(animal.initialisePhysicsWithoutBody);
	PhysicsObjects::SetClassHandlers(PhysicsClass::Animal, std::move(animal));
}

void InterfaceSetInMagicHand(entt::entity entity)
{
	// marked as held: read when the villager lands and cleared by EndPhysics (a thrown landing) or by Landed.
	// TODO: a FINAL villager with a partner: the partner is freed and decides what to do; the player and a
	// sexually active villager -> a reaction
	if (auto* v = Locator::entitiesRegistry::value().TryGet<Villager>(entity); v != nullptr)
	{
		v->flags = static_cast<uint16_t>(v->flags | Villager::k_FlagInHand);
	}
	// the y angle while in the hand: nothing of the hand writes it (HandStateHolding builds only the drawn matrix, no
	// hold rotation), so it keeps the value from before the pick-up; openblack keeps it in HeldYAngle until the landing's
	// SetYAngle (EndPhysics / EndPhysicsWithoutBody)
	if (auto& registry = Locator::entitiesRegistry::value(); registry.AnyOf<Villager, Animal>(entity))
	{
		registry.AssignOrReplace<HeldYAngle>(entity, ObjectYAngle(entity));
	}
	// the Living: IN_HAND (its clip SCARED_STIFF, ECS/VillagerAnimations)
	ecs::SetVillagerState(entity, VillagerStates::InHand);
	// the animal: off its flock, IN_HAND
	if (Locator::entitiesRegistry::value().AllOf<Animal>(entity))
	{
		ecs::animal_ai::PlaceInHand(entity);
	}
}

void InterfaceSetOutMagicHand(entt::entity entity)
{
	// what every object does: its fire effect, if any, is told it left the hand (fire::SetOutMagicHand: nothing without a
	// fire)
	fire::SetOutMagicHand(entity);
}

void EndPhysicsWithoutBody(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(entity) || !registry.AllOf<Transform>(entity))
	{
		return;
	}
	const bool villager = registry.AllOf<Villager>(entity);
	if (!villager && !registry.AllOf<Animal>(entity))
	{
		return;
	}
	// from the hand with no body (or flying) the villager's previous disciple comes back. TODO: disciples are not
	// ported. The object's landing with no body too: the object's flying-object reaction (9) goes (as the physics'
	// EndPhysicsOfClass does for a body) and ConsiderCreatureMimickingWhenObjectLands (TODO(creature))
	ecs::animal_ai::EndReactionsOf(entity);
	// (openblack) the drawn rotation without the hand's tilt: the original draws a Living from its y angle alone and
	// the bodiless landing does not touch it (no SetYAngle, no caller's angles)
	registry.Get<Transform>(entity).rotation = DrawnRotation(ObjectYAngle(entity));
	if (villager)
	{
		// the object's landing clears its in-physics flag with no body too (VillagerDead's first test)
		const ecs::villager::EndingPhysicsScope ending(entity);
		// landType 3, straight to the shared part
		VillagerEndPhysicsFrom5F0B79(entity, k_LandTypeWithoutBody, false);
		return;
	}
	// landType 3, straight to the shared part
	AnimalEndPhysicsFrom5F0DF9(entity, k_LandTypeWithoutBody);
}

float ObjectYAngle(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return 0.0f;
	}
	const auto& drawn = transform->rotation;
	// held (or flying after the hand): the y angle kept from before the pick-up (InterfaceSetInMagicHand)
	if (const auto* held = registry.TryGet<const HeldYAngle>(entity); held != nullptr)
	{
		return held->value;
	}
	// a walking object's game angle sets its y angle through ConvertGameAngleTo3D, so the y angle is that float.
	// openblack keeps it in WallHug::yAngle (villager::SetGameAngle) and the animal's game angle in
	// AnimalBrain::angle (FaceAngle draws ConvertGameAngleTo3D(angle) + pi / 2, bit for bit DrawnRotation's): exact when
	// the Transform is still the one drawn from it
	if (const auto* wallHug = registry.TryGet<const WallHug>(entity);
	    wallHug != nullptr && DrawnRotation(wallHug->yAngle) == drawn)
	{
		return wallHug->yAngle;
	}
	if (const auto* brain = registry.TryGet<const AnimalBrain>(entity); brain != nullptr)
	{
		const float angle = gutils::ConvertGameAngleTo3D(brain->angle);
		if (DrawnRotation(angle) == drawn)
		{
			return angle;
		}
	}
	// any other writer (EndPhysics' SetYAngle then the caller's angles from DecomposeYXZ(M)): openblack keeps only the drawn
	// matrix, so its DecomposeYXZ yaw through the original's rows. (approximate) within the rounding of the matrix's
	// floats (cos / sin of the y angle + pi / 2), in DecomposeYXZ's range (-pi, pi]
	return YawFromRotation(OriginalRows(drawn));
}

glm::mat3 OriginalRows(const glm::mat3& body)
{
	// drawn = M x K (the drawn pi / 2, see the header): M's right = -drawn[2], up = drawn[1], fwd = drawn[0] (exact). Only
	// for a drawn matrix now (the bodies have the original's rows)
	return glm::mat3(-body[2], body[1], body[0]);
}

glm::mat3 BodyRows(const glm::mat3& body, [[maybe_unused]] bool fromHand)
{
	// both paths build the body with the original's rows: as they are
	return body;
}

VillagerLandingPose VillagerLandingPoseOf(const glm::mat3& turnStartRows)
{
	const float a = turnStartRows[0].y; // the turn-start right.y
	// float(pi) added to the double, then one float rounding of the sum
	const auto plusPi = [](double angle) { return static_cast<float>(angle + static_cast<double>(glm::pi<float>())); };
	if (!(a >= -0.5f)) // less or NaN
	{
		// GetYAngle(up) as it is: no pi, no wrap
		return {1, static_cast<float>(affine::GetYAngle(turnStartRows[1])), k_DesireAnger, 0.5f};
	}
	if (a > 0.5f) // strictly greater
	{
		return {2, affine::WrapAngle(plusPi(affine::GetYAngle(turnStartRows[1]))), k_DesireAnger, 0.5f};
	}
	return {0, affine::WrapAngle(plusPi(affine::GetYAngle(turnStartRows[2]))), k_DesireCompassion, 0.1f};
}

uint16_t AnimalLandType(float rightY)
{
	if (rightY > 0.5f) // strictly greater
	{
		return 1;
	}
	if (!(rightY >= -0.5f)) // less or NaN
	{
		return 2;
	}
	return 0;
}

float AnimalLandingYaw(const glm::vec3& currentFwdRow)
{
	// one float rounding of GetYAngle + pi, then the wrap
	return affine::WrapAngle(static_cast<float>(affine::GetYAngle(currentFwdRow) + static_cast<double>(glm::pi<float>())));
}

float YawFromRotation(const glm::mat3& rows)
{
	// m6 = fwd.x, m8 = fwd.z: atan2(-fwd.x, fwd.z), stored as a float
	return static_cast<float>(affine::ArcTanOctant(rows[2].z, -rows[2].x));
}

glm::mat3 DrawnRotation(float yAngle)
{
	return affine::AngleY(yAngle + glm::half_pi<float>());
}

} // namespace openblack::ecs::living
