/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's side of the physics system: what thrown trees, pots and wood do when they hit a store or come to rest
// (their physics impact and end of physics), and the hand's hooks of physics::from_hand

#define LOCATOR_IMPLEMENTATIONS

#include <array>

#include <spdlog/spdlog.h>

#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/HeldApply.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/ToBeDeleted.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

std::vector<entt::entity> HandSystem::GetThrownObjects() const noexcept
{
	// everything the hand throws is a physics object: there is no other flight
	return {};
}

void HandSystem::RegisterPhysicsHandlers() noexcept
{
	using physics::PhysicsClass;
	using physics::PhysicsObjects;
	// a tree or dead tree hitting a wood store turns into wood (deleted, its resource taken)
	const auto treeImpact = [this](entt::entity entity, physics::PhysicsObject& po, const physics::ImpactInfo&) {
		auto& registry = Locator::entitiesRegistry::value();
		if (po.hitBy != nullptr && registry.Valid(po.hitBy->entity) && registry.AllOf<StoragePit>(po.hitBy->entity))
		{
			// the store takes the tree's resource with the PhysicsObject still alive (the store's Supply help trigger
			// reads it), the thrower's interface as `is`
			pot_resource::Dropper dropper;
			if (po.byPlayer)
			{
				dropper = InterfaceStatus();
			}
			DepositInStore(entity, po.hitBy->entity, dropper);
			PhysicsObjects::RemoveObject(entity);
			return false;
		}
		return true;
	};
	PhysicsObjects::ClassHandlers tree;
	tree.reactToImpact = treeImpact;
	tree.endPhysics = [this](entt::entity entity, physics::PhysicsObject& po) {
		auto& registry = Locator::entitiesRegistry::value();
		const auto position = registry.Get<const Transform>(entity).position;
		{
			// LANDED (only a gentle release sets it) on land and with no fire (hot or burning, ECS/Fire) -> planted again
			// (altitude 0, smoke, the forest, SPOT_VISUAL 0x2C, alignment: Replant); otherwise it becomes a DeadTree (the
			// same entity keeps its fire). A magic tree (the forest miracle's) only stops where it fell, neither replanted
			// nor dead: not this tree.
			const bool landedOnLand = (po.flags & physics::PhysicsObject::k_Landed) != 0 && IsLand(position);
			if (landedOnLand && fire::Find(entity) == nullptr)
			{
				Replant(entity);
			}
			else
			{
				UpdateRoots(entity, true);
				MakeDeadTree(entity, po.body.velocity, false);
			}
			return entity;
		}
	};
	tree.moved = [this](entt::entity entity) { UpdateRoots(entity); };
	// the drop sound (LANDED on land, still a tree after its end of physics): bank InGame, owned by the tree, 3D, not
	// tracked, sample 83 G_PlantTree_01 + tick count % 3, at the tree's point
	tree.dropSfx = [](entt::entity entity) {
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), 83 + static_cast<int>(audio::TickCount() % 3)};
		options.owner = audio::Owner::Thing(entity);
		options.is3D = true;
		options.track = false;
		options.position = Locator::entitiesRegistry::value().Get<const Transform>(entity).position;
		audio::PlaySoundEffect(options);
	};
	PhysicsObjects::SetClassHandlers(PhysicsClass::Tree, std::move(tree));
	PhysicsObjects::ClassHandlers deadTree;
	deadTree.reactToImpact = treeImpact;
	PhysicsObjects::SetClassHandlers(PhysicsClass::DeadTree, std::move(deadTree));
	PhysicsObjects::ClassHandlers pot;
	// a thrown pot or pile that hits a store of its type, or a pot of its type, goes into it (the delivery, the
	// thrower's interface as `is`); else nothing happens to it
	pot.reactToImpact = [this](entt::entity entity, physics::PhysicsObject& po, const physics::ImpactInfo& impact) {
		const auto hit = impact.hitBy;
		const auto facts = ecs::held_apply::FactsFor(entity, hit);
		if (!ecs::held_apply::PotImpactTakes(ecs::IsAvailable(hit), ecs::IsAvailable(entity), facts))
		{
			return true;
		}
		pot_resource::Dropper dropper;
		if (po.byPlayer)
		{
			dropper = InterfaceStatus();
		}
		if (!ecs::resource_stores::DeleteObjectAndTakeResource(hit, entity, dropper))
		{
			return true;
		}
		PhysicsObjects::RemoveObject(entity);
		return false;
	};
	pot.endPhysics = [this](entt::entity entity, physics::PhysicsObject&) {
		auto& registry = Locator::entitiesRegistry::value();
		const auto position = registry.Get<const Transform>(entity).position;
		if (const auto type = PotInfoOf(entity); (type == PotInfo::HandWood || type == PotInfo::HandFood) && IsLand(position))
		{
			// a hand pot out of the water becomes a pile (AddResourceToPos)
			PutDownHandPot(entity);
			return registry.Valid(entity) ? entity : entt::entity {entt::null};
		}
		return entity;
	};
	PhysicsObjects::SetClassHandlers(PhysicsClass::Pot, std::move(pot));

	physics::from_hand::HandHooks hooks;
	hooks.putDownHandPot = [this](entt::entity entity) { PutDownHandPot(entity); };
	hooks.isHandPot = [](entt::entity entity) {
		const auto type = PotInfoOf(entity);
		return type == PotInfo::HandWood || type == PotInfo::HandFood;
	};
	physics::from_hand::SetHandHooks(std::move(hooks));
	PhysicsObjects::LoadConstants();
}
