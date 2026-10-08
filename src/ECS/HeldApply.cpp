/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HeldApply.h"

#include "ECS/Components/Animal.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

held_apply::ApplyAction held_apply::HeldClassApply(HeldKind held, const TargetFacts& target)
{
	switch (held)
	{
	case HeldKind::Tree:
	case HeldKind::DeadTree:
		// (pending) a tree over a worship totem is a sacrifice: not ported, so a totem does not take it
		return target.storesHeldType ? ApplyAction::GiveToStore : ApplyAction::None;
	case HeldKind::Pot:
		if (target.storesHeldType)
		{
			return ApplyAction::GiveToStore;
		}
		return target.sameTypePot ? ApplyAction::MergeIntoPot : ApplyAction::None;
	case HeldKind::Animal:
		// (pending) an animal over a worship totem is a sacrifice: not ported
		return HeldClassValid(held, target) && !target.heldIndestructible && target.storesHeldType ? ApplyAction::GiveToStore
		                                                                                           : ApplyAction::None;
	case HeldKind::Fence:
		return target.storesHeldType && !target.heldIndestructible ? ApplyAction::GiveToStore : ApplyAction::None;
	case HeldKind::Other:
		return ApplyAction::None;
	}
	return ApplyAction::None;
}

bool held_apply::HeldClassValid(HeldKind held, const TargetFacts& target)
{
	if (held == HeldKind::Animal)
	{
		// only a storage pit, whatever it holds
		return target.storagePit;
	}
	if (held == HeldKind::Fence)
	{
		return target.storesHeldType;
	}
	return HeldClassApply(held, target) != ApplyAction::None;
}

bool held_apply::AnimalImpactTakes(bool hitAvailable, bool selfAvailable, bool hitStoresFood)
{
	return hitAvailable && selfAvailable && hitStoresFood;
}

bool held_apply::FenceImpactTakes(bool isFence, bool indestructible, bool hitStoresWood)
{
	return isFence && !indestructible && hitStoresWood;
}

pot_resource::Dropper held_apply::ThrowerInterface(bool byPlayer)
{
	if (!byPlayer)
	{
		return {};
	}
	return {.hasInterface = true,
	        .player = PlayerNames::PLAYER_ONE,
	        .isMyInterface = true,
	        .sourceOwner = Locator::handSystem::has_value() ? Locator::handSystem::value().GetSourceOwner() : std::nullopt};
}

bool held_apply::PotImpactTakes(bool hitAvailable, bool potAvailable, const TargetFacts& hit)
{
	return hitAvailable && potAvailable && (hit.storesHeldType || hit.sameTypePot);
}

int held_apply::ApplyResult(ApplyAction action, bool done)
{
	return action != ApplyAction::None && done ? k_ApplyConsumed : 0;
}

held_apply::HeldKind held_apply::KindOfHeld(entt::entity held)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (held == entt::null || !registry.Valid(held))
	{
		return HeldKind::Other;
	}
	if (registry.AllOf<Tree>(held))
	{
		return HeldKind::Tree;
	}
	if (registry.AllOf<DeadTree>(held))
	{
		return HeldKind::DeadTree;
	}
	if (registry.AllOf<Pot>(held))
	{
		return HeldKind::Pot;
	}
	if (registry.AllOf<Animal>(held))
	{
		return HeldKind::Animal;
	}
	if (registry.AllOf<MobileStatic>(held) && physics::from_hand::IsFence(held))
	{
		return HeldKind::Fence;
	}
	return HeldKind::Other;
}

held_apply::TargetFacts held_apply::FactsFor(entt::entity held, entt::entity target)
{
	TargetFacts facts;
	switch (KindOfHeld(held))
	{
	case HeldKind::Tree:
	case HeldKind::DeadTree:
		facts.storesHeldType = resource_stores::IsResourceStore(target, ResourceType::Wood);
		break;
	case HeldKind::Pot:
	{
		const auto type = object_resources::GetResourceType(held);
		facts.storesHeldType = resource_stores::IsResourceStore(target, type);
		auto& registry = Locator::entitiesRegistry::value();
		facts.sameTypePot = target != entt::null && registry.Valid(target) && registry.AllOf<Pot>(target) &&
		                    object_resources::GetResourceType(target) == type;
		break;
	}
	case HeldKind::Animal:
	{
		auto& registry = Locator::entitiesRegistry::value();
		facts.storesHeldType = resource_stores::IsResourceStore(target, ResourceType::Food);
		facts.storagePit = target != entt::null && registry.Valid(target) && registry.AllOf<StoragePit>(target);
		facts.heldIndestructible = registry.AllOf<Indestructible>(held);
		break;
	}
	case HeldKind::Fence:
		facts.storesHeldType = resource_stores::IsResourceStore(target, ResourceType::Wood);
		facts.heldIndestructible = Locator::entitiesRegistry::value().AllOf<Indestructible>(held);
		break;
	case HeldKind::Other:
		break;
	}
	return facts;
}

bool held_apply::ValidToApplyThisToObject(entt::entity held, entt::entity target)
{
	return HeldClassValid(KindOfHeld(held), FactsFor(held, target));
}

int held_apply::ApplyThisToObject(entt::entity held, entt::entity target, const pot_resource::Dropper& dropper,
                                  const std::function<void()>& leaveHand)
{
	const auto action = HeldClassApply(KindOfHeld(held), FactsFor(held, target));
	bool done = false;
	if (action == ApplyAction::GiveToStore)
	{
		// the store takes the object: out of the hand first, then its resource is taken and it goes
		if (leaveHand)
		{
			leaveHand();
		}
		done = resource_stores::DeleteObjectAndTakeResource(target, held, dropper);
	}
	else if (action == ApplyAction::MergeIntoPot)
	{
		// the held pot's resource is put down at its own position (it merges into the pots and stores around, else
		// makes a pile), then the pot goes
		auto& registry = Locator::entitiesRegistry::value();
		const auto type = object_resources::GetResourceType(held);
		const uint32_t amount = object_resources::GetResource(held, type);
		const bool poisoned = object_resources::IsPoisoned(held);
		const auto position = registry.Get<const Transform>(held).position;
		if (leaveHand)
		{
			leaveHand();
		}
		pot_resource::AddResourceToPos(position, dropper, type, amount, poisoned, false);
		object_ghosts::Add(held);
		ToBeDeleted(held);
		done = true;
	}
	Locator::entitiesRegistry::value().SetDirty();
	return ApplyResult(action, done);
}
