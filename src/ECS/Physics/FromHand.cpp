/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a released object does when the hand lets it go: the throw and the physics it starts with

#include "FromHand.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/FishShoals.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectFlags.h"
#include "ECS/Physics/PhysicsObjectsState.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/PhysicsObjectsSystemInterface.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerDrowning.h"
#include "ECS/WaterRings.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "PhysicsObjects.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace openblack::ecs::physics::from_hand
{
namespace
{
/// The physics objects' state (Locator::physicsObjectsSystem)
openblack::ecs::physics::State& PhysicsState()
{
	return openblack::Locator::physicsObjectsSystem::value().GetState();
}

bool IsHandPot(entt::entity entity)
{
	return PhysicsState().handHooks.isHandPot && PhysicsState().handHooks.isHandPot(entity);
}

} // namespace

void SetHandHooks(HandHooks hooks)
{
	PhysicsState().handHooks = std::move(hooks);
}

bool IsFence(entt::entity entity)
{
	const auto* statics = Locator::entitiesRegistry::value().TryGet<const MobileStatic>(entity);
	if (statics == nullptr || !Locator::infoConstants::has_value() || statics->type == MobileStaticInfo::None)
	{
		return false;
	}
	const auto mesh = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(statics->type)).meshId;
	return mesh == MeshId::BuildingAmericanFence || mesh == MeshId::BuildingCelticFenceShort ||
	       mesh == MeshId::BuildingCelticFenceTall;
}

void PlaceWithoutBody(entt::entity entity)
{
	// (approximate) openblack only: an object AddObject cannot build a body for (no mesh) is put where it
	// is, on the ground (in the original a living thing's physics ends at once when AddObject
	// fails; any other object stays in physics where the hand left it)
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(entity);
	const float ground =
	    Locator::terrainSystem::has_value()
	        ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z))
	        : 0.0f;
	transform.position.y = ground;
	if (auto* fixed = registry.TryGet<Fixed>(entity); fixed != nullptr)
	{
		fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
	}
	// its EndPhysics at once: it goes back in
	// the map cells
	map_cells::InsertMapObject(entity);
	if (registry.AllOf<Villager>(entity) && sea_cells::IsWater(transform.position))
	{
		VillagerEndPhysicsInWater(entity);
	}
	else
	{
		SetVillagerState(entity, VillagerStates::Landed);
	}
	animal_ai::PutDown(entity);
	registry.SetDirty();
}

bool IsThrown(glm::vec3 velocity, bool byCreature)
{
	return velocity.x * velocity.x + velocity.z * velocity.z > (byCreature ? 1.0f : 4.0f);
}

std::optional<bool> InitialisePhysicsFromHand(entt::entity entity, glm::vec3 velocity, bool dontReplant,
                                              std::optional<glm::vec3> angularMomentum, bool byCreature)
{
	// From the hand (no thrower). Living things and villagers add FLYING and the disciple around it: FLYING (the
	// class's initialisePhysics) at the end, only when the object does not land.
	auto& registry = Locator::entitiesRegistry::value();
	const auto& handlers = PhysicsObjects::Handlers(entity);
	// the class's part before the shared one (a scaffold destroys the things in its way)
	if (handlers.beforeInitialisePhysicsFromHand)
	{
		handlers.beforeInitialisePhysicsFromHand(entity);
	}
	const bool tree = registry.AnyOf<Tree, DeadTree>(entity);
	// AddObject's own fromHand path would spread the flying-object reaction at once, but here it is only spread when
	// the object does not land (below); the from-hand flag and the player are set by
	// AddObjectFromHand
	// the prediction of this object ends (before AddObject)
	PhysicsObjects::EndPredictionOf(entity);
	auto* po = PhysicsObjects::AddObjectFromHand(entity, velocity, glm::vec3(0.0f));
	if (po == nullptr)
	{
		return std::nullopt;
	}
	// the angular momentum L = the throw's, after AddObject(zero) and the paper / disappear block, before
	// AdjustToGroundLevel
	if (angularMomentum)
	{
		po->body.angularMomentum = *angularMomentum;
	}
	// thrown = |v.xz|^2 > 4 from the hand (> 1 from a creature); either way AdjustToGroundLevel(thrown, !tree) (thrown:
	// only raised out of the ground), ZeroForces, RaiseUntilNotIntersecting and the from-hand flag
	const bool thrown = IsThrown(velocity, byCreature);
	po->body.AdjustToGroundLevel(thrown, !tree);
	// AdjustToGroundLevel ends copying the matrix into the turn-start one: a put-down villager's landType
	// comes from the ground-aligned rows, not the hand's (RaiseUntilNotIntersecting does the same when it raises it)
	PhysicsObjects::SyncTurnStart(*po);
	const float oldAltitude = po->body.Centre().y;
	po->body.ZeroForces();
	PhysicsObjects::RaiseUntilNotIntersecting(*po);
	const auto at = po->body.Centre();
	// not thrown and not raised over something (the exception of a computer player's villager has no hand
	// here): landed on dry land, or on a cell (rounded) of altitude > 1
	bool landed = false;
	if (!thrown && oldAltitude == at.y && Locator::terrainSystem::has_value())
	{
		const auto& island = Locator::terrainSystem::value();
		landed = sea_cells::IsDryLand(island, sea_cells::CellOf(at)) ||
		         sea_cells::AltitudeAt(island, sea_cells::RoundedCellOf(at)) > 1;
	}
	const bool living = registry.AnyOf<Villager, Animal>(entity);
	const bool fence = IsFence(entity);
	if (landed && (living || fence) && LandscapeNormal(at).y < 0.7f)
	{
		landed = false; // too steep to stand on
	}
	// the yaw, pitch and roll of the object's matrix, as floats
	float tiltY = 0.0f;
	glm::vec2 tilt(0.0f);
	affine::DecomposeYXZ(registry.Get<const Transform>(entity).rotation, tiltY, tilt.x, tilt.y);
	const char* outcome = "in physics";
	if (landed)
	{
		// TODO(villager-jobs): the disciple sound effect when the villager's disciple changed.
		// TODO(creature): the creature may mimic the player when the object lands.
		po->flags |= PhysicsObject::k_Landed;
		// Living, Fence, or a Tree with no FireEffect (ECS/Fire) on land: out of physics at once, where it is,
		// through its EndPhysics; a tree held tilted (DecomposeYXZ: |x| or |z| > 0.2) or dontReplant stays in physics, not
		// LANDED. A hot or burning tree stays in physics, LANDED, and ends a DeadTree.
		const bool treeOnLand = registry.AllOf<Tree>(entity) && fire::Find(entity) == nullptr && sea_cells::IsLand(at);
		if (living || fence || treeOnLand)
		{
			if (treeOnLand && (dontReplant || std::abs(tilt.x) > 0.2f || std::abs(tilt.y) > 0.2f))
			{
				po->flags &= ~PhysicsObject::k_Landed;
				landed = false;
				outcome = "tilted tree: in physics";
			}
			else
			{
				PhysicsObjects::RemoveObjectWithEndPhysics(entity);
				outcome = "landed: out of physics";
			}
		}
		else
		{
			outcome = "landed: in physics";
		}
	}
	else
	{
		if (registry.AllOf<Villager>(entity))
		{
			// the villager drops what it carries (ECS/Villager), with the throw's velocity; the hand's angular
			// velocity is 0
			ecs::villager::CreateDroppedResource(entity, velocity, std::nullopt, glm::vec3(0.0f));
		}
		// the flying-object reaction: spread once to the living things near it; the thrower is the hand's player
		// (PLAYER_ONE)
		animal_ai::SpreadFlyingObjectReaction(entity, PlayerNames::PLAYER_ONE);
		PhysicsObjects::CheckAllCreaturesForCatching(entity, *po); // (the creature's part: pending)
	}
	// TODO(creature): a toy makes the creature consider mimicking the player playing with it.
	// A living thing, once the shared part has returned: a body, not LANDED and still this object -> FLYING.
	// Find(entity) is the "still this one" (the ones taken out of the physics are gone), so a put-down
	// villager or animal goes to LANDED through its EndPhysics without passing through FLYING.
	if (auto* now = PhysicsObjects::Find(entity); now != nullptr && (now->flags & PhysicsObject::k_Landed) == 0)
	{
		PhysicsObjects::InitialisePhysicsOfClass(*now, true);
	}
	// the class's part after the shared one returned the body (a scaffold reads its LANDED flag and
	// dontReplant); a body already taken out of the physics (Living, Fence, replanted Tree) has no class part here
	if (const auto* now = PhysicsObjects::Find(entity); now != nullptr && handlers.initialisedPhysicsFromHand)
	{
		handlers.initialisedPhysicsFromHand(entity, *now, dontReplant);
	}
	SPDLOG_LOGGER_INFO(
	    spdlog::get("game"),
	    "Hand: released {} at ({:.1f}, {:.1f}, {:.1f}) v ({:.1f}, {:.1f}, {:.1f}) thrown {} tilt ({:.2f}, {:.2f}): {}",
	    static_cast<uint32_t>(entity), at.x, at.y, at.z, velocity.x, velocity.y, velocity.z, thrown, tilt.x, tilt.y, outcome);
	registry.SetDirty();
	return landed;
}

bool Throw(entt::entity entity, glm::vec3 velocity, bool dontReplant, std::optional<glm::vec3> angularMomentum)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return false;
	}
	// a hand pot with |v|^2 <= 5 (all three axes) puts the resource down at once (the put-down effect, the
	// resource onto the ground, the pot deleted); faster it flies as an object
	if (IsHandPot(entity) && glm::dot(velocity, velocity) <= 5.0f)
	{
		if (PhysicsState().handHooks.putDownHandPot)
		{
			PhysicsState().handHooks.putDownHandPot(entity);
		}
		return true;
	}
	// the class's throw before the shared one (a villager's disciple)
	PhysicsObjects::ThrowFromHandOfClass(entity, dontReplant);
	if (const auto landed = InitialisePhysicsFromHand(entity, velocity, dontReplant, angularMomentum))
	{
		return *landed;
	}
	PlaceWithoutBody(entity); // no body could be built (no mesh)
	return true;
}

CappedReleaseVelocity CapReleaseVelocity(glm::vec3 handVelocity)
{
	// l2 = (z z + y y) + x x; l2 > 124 x 124 (NaN fails) and not all three 0: k = 124 / sqrt(l2),
	// v = (k x, k y, k z); the square root and the division round to float. fast = the UNCAPPED l2 > 1
	CappedReleaseVelocity out;
	glm::vec3 v = handVelocity;
	const float l2 = (v.z * v.z + v.y * v.y) + v.x * v.x;
	if (l2 > PhysicsBody::k_MaxSpeed * PhysicsBody::k_MaxSpeed && !(v.x == 0.0f && v.y == 0.0f && v.z == 0.0f))
	{
		const float k = PhysicsBody::k_MaxSpeed / std::sqrt(l2);
		v = glm::vec3(k * v.x, k * v.y, k * v.z);
	}
	out.velocity = v;
	out.fast = l2 > 1.0f;
	return out;
}

std::optional<ReleasePrediction> PredictRelease(entt::entity entity, glm::vec3 handVelocity, glm::vec3 handAngularVelocity,
                                                uint32_t turns)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the old prediction ends and the new one is active with the held object
	// before the body is built
	PhysicsObjects::BeginPrediction(entity);
	// the body from the held object's mesh and scale, set up by it: dynamic when CanBecomeAPhysicsObject and not
	// IMMOVABLE, placed at the held pose
	PhysicsBody body;
	const bool dynamic =
	    registry.Valid(entity) && PhysicsObjects::CanBecomeAPhysicsObject(entity) && !object_flags::IsImmovable(entity);
	if (!registry.Valid(entity) || !PhysicsObjects::SetUpBody(entity, body, dynamic))
	{
		// (openblack guard) the setup cannot fail in the original; here an invalid object or one with no mesh ends it
		PhysicsObjects::EndPredictionOf(entity);
		return std::nullopt;
	}
	// the angular momentum from the hand's angular velocity
	body.SetAngularMomentumFromHand(handAngularVelocity);
	// Velocity = the hand's, capped at 124
	const auto capped = CapReleaseVelocity(handVelocity);
	body.velocity = capped.velocity;
	// AdjustToGroundLevel(1, (tree || fast) ? 0 : 1)
	const bool tree = registry.AnyOf<Tree, DeadTree>(entity);
	body.AdjustToGroundLevel(true, !(tree || capped.fast));
	// n = min(n, 15); history[i] = the pose, then 20 x ZeroForces, GroundAndWater,
	// ContactForces, Integrate (its result ignored); history[n] = the final pose
	const uint32_t n = std::min(turns, 15u);
	ReleasePrediction prediction;
	prediction.history.reserve(n + 1);
	for (uint32_t i = 0; i < n; ++i)
	{
		prediction.history.emplace_back(body.Rotation(), body.Centre());
		for (int s = 0; s < PhysicsBody::k_SubstepsPerTurn; ++s)
		{
			body.ZeroForces();
			body.GroundAndWater();
			body.ContactForces();
			static_cast<void>(body.Integrate());
		}
	}
	prediction.history.emplace_back(body.Rotation(), body.Centre());
	// the object's translation, Velocity, the angular momentum and the rotation rows for DecomposeYXZ
	prediction.velocity = body.velocity;
	prediction.angularMomentum = body.angularMomentum;
	prediction.origin = body.ObjectOrigin();
	prediction.rotation = body.Rotation();
	// the prediction's body and history are kept for the drawing until the throw
	PhysicsObjects::SetPrediction(entity, std::move(body), prediction.history);
	return prediction;
}

void ApplyReleaseSpin(entt::entity entity, glm::vec3 handDelta)
{
	auto* po = PhysicsObjects::Find(entity);
	if (po == nullptr)
	{
		return;
	}
	const auto& v = po->body.velocity;
	const float speed = std::sqrt((v.z * v.z + v.y * v.y) + v.x * v.x);
	const float m = po->body.Mass();
	const float tx = -(((m * handDelta.z) * 1.6f) * speed);
	const float tz = ((m * handDelta.x) * 1.6f) * speed;
	// += into the external torque, with this port's torque sign
	po->body.externalTorque += -glm::vec3(tx, 0.0f, tz);
}

bool ForceDrop(entt::entity entity)
{
	return Throw(entity, glm::vec3(0.0f), true);
}

} // namespace openblack::ecs::physics::from_hand
