/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StoragePitStore.h"

#include <algorithm>

#include "Common/GameRandom.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownQueries.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
void SyncStoreTotals(entt::entity store)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* abode = registry.TryGet<Abode>(store);
	if (abode == nullptr)
	{
		return;
	}
	abode->woodAmount = StoragePitStore::GetResource(store, ResourceType::Wood);
	abode->foodAmount = StoragePitStore::GetResource(store, ResourceType::Food);
}

Pot* StorePot(entt::entity pile)
{
	auto& registry = Locator::entitiesRegistry::value();
	return pile != entt::null && registry.Valid(pile) ? registry.TryGet<Pot>(pile) : nullptr;
}
} // namespace

uint32_t StoragePitStore::AddResource(entt::entity store, ResourceType type, uint32_t amount,
                                      const pot_resource::Dropper& dropper, bool poisoned)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a building site (the pit is not built yet) with WOOD or ANY -> the site's AddResource: an unbuilt storage pit
	// collects its wood on its site
	if (const auto site = abodes::GetBuildingSite(store);
	    site != entt::null && (type == ResourceType::Wood || type == ResourceType::Any))
	{
		return building_sites::AddResource(site, type, amount, nullptr, poisoned);
	}
	const auto* pit = registry.TryGet<const StoragePit>(store);
	// only FOOD and WOOD fill piles; (approximate) for any other type the original still runs the pulse test and
	// DoResourceAdding with 0
	if (pit == nullptr || (type != ResourceType::Food && type != ResourceType::Wood))
	{
		return 0;
	}
	// (*) the store's total is read after the piles but before DoResourceAdding, so it is still the total from before
	// this call
	const uint32_t before = GetResource(store, type);
	uint32_t added = 0;
	const auto fill = [&](entt::entity pile) {
		// while n != 0. A missing pile is created first: (approximate) openblack creates all six with the pit
		// (AbodeArchetype) and never deletes them, so there is none to make. Only an available pile
		if (amount == 0 || !ecs::IsAvailable(pile) || StorePot(pile) == nullptr)
		{
			return;
		}
		// AddToPotDirect: the pile sound with the n still asked for, the cap, the poison, SetSize
		const uint32_t n = pot_resource::AddToPotDirect(pile, type, amount, poisoned);
		added += n;
		amount -= n;
	};
	const auto fillPiles = [&]() {
		if (type == ResourceType::Wood)
		{
			for (const auto pile : pit->woodPiles) // Wood Pile 1..5
			{
				fill(pile);
			}
		}
		else
		{
			fill(pit->foodPile);
		}
		SyncStoreTotals(store);
		return added;
	};
	// DoResourceAdding(type, added, ...). The original fills the piles first and DoResourceAdding's desire "before"
	// still reads the old mirror; openblack's desire reads the piles' total (TownDesire GatherInputs:
	// StoragePitStore::GetResource), so the piles are filled inside its JustAddResource step, between the two desire
	// calls (the same values); its n is what the piles took (justAdd's value). Returns `added`
	object_resources::DoResourceAdding(store, type, amount, dropper, fillPiles);
	const auto town = abode_villagers::TownOf(store);
	// none of it before, something added, a town -> the pulse (before DoResourceAdding in the original; only the
	// town's update reads it, so after it here)
	if (before == 0 && added != 0 && town != entt::null)
	{
		town_queries::Pulse(town);
	}
	return added;
}

std::optional<StoragePitDeed> StoragePitStore::DoCreatureMimicAfterAddingResource(entt::entity store, ResourceType type,
                                                                                  const pot_resource::Dropper& dropper)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!dropper.hasInterface || !registry.Valid(store) || !registry.AllOf<StoragePit>(store))
	{
		return std::nullopt;
	}
	// wood on a pit that is still a building site is the building site's deed, and nothing else is asked
	if (type == ResourceType::Wood && abodes::GetBuildingSite(store) != entt::null)
	{
		return StoragePitDeed::PutWoodInBuildingSite;
	}
	// the pit's own owner is not looked at: only whether what the interface last picked up belonged to the dropper's
	// player. Nothing recorded is never the player's own
	const bool own = dropper.sourceOwner.has_value() && *dropper.sourceOwner == dropper.player;
	if (type == ResourceType::Food)
	{
		if (own)
		{
			return StoragePitDeed::PutFoodInStoragePit;
		}
		// drawn here, before anything checks for a creature
		return game_random::GameRand(2) != 0 ? StoragePitDeed::StealFoodFromStoragePit : StoragePitDeed::StealFoodFromFarm;
	}
	return own ? StoragePitDeed::PutWoodInStoragePit : StoragePitDeed::StealWoodFromStoragePit;
}

uint32_t StoragePitStore::RemoveResource(entt::entity store, ResourceType type, uint32_t amount,
                                         const pot_resource::Dropper& dropper)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pit = registry.TryGet<const StoragePit>(store);
	// only FOOD and WOOD; any other type 0, with no DoResourceRemoving
	if (pit == nullptr || (type != ResourceType::Food && type != ResourceType::Wood))
	{
		return 0;
	}
	const auto removeFromPiles = [&]() {
		uint32_t removed = 0;
		const auto take = [&](entt::entity pile) {
			// while n != 0: each existing pile's JustRemoveResource (a pit's empty pile stays; the out pointer is not
			// passed)
			if (amount == 0 || StorePot(pile) == nullptr)
			{
				return;
			}
			const uint32_t n = object_resources::RemoveFromPotDirect(pile, amount);
			removed += n;
			amount -= n;
		};
		if (type == ResourceType::Wood)
		{
			for (auto it = pit->woodPiles.rbegin(); it != pit->woodPiles.rend(); ++it) // pile 5 -> 1
			{
				take(*it);
			}
		}
		else
		{
			take(pit->foodPile);
		}
		SyncStoreTotals(store);
		return removed;
	};
	// removed != 0 -> DoResourceRemoving(type, removed, ...). Its desire "before" reads the mirror, still the old
	// total: openblack's mirror is the piles' total, so the desire is taken before the piles change. Something comes
	// off exactly when the store holds some and n != 0
	const uint32_t held = GetResource(store, type);
	if (amount == 0)
	{
		return 0;
	}
	if (held == 0)
	{
		// nothing comes off, but each existing pile is still asked while n != 0 (an empty pile: poison, reaction and
		// fire cleared again, SetSize); removed == 0 -> no DoResourceRemoving
		return removeFromPiles();
	}
	// DoResourceRemoving's n is what came off, min(n, the store's total)
	return object_resources::DoResourceRemoving(store, type, std::min(amount, held), dropper, removeFromPiles);
}

int32_t StoragePitStore::AmountOverMaximum(entt::entity store, ResourceType type)
{
	// WOOD total - 5 x the Wood Pile 1 pot's max; any other type total - the Storage Pit Food Pile's max
	const auto& pots = Locator::infoConstants::value().pot;
	const int32_t total = static_cast<int32_t>(GetResource(store, type));
	if (type == ResourceType::Wood)
	{
		return total - 5 * static_cast<int32_t>(pots.at(static_cast<size_t>(PotInfo::WoodPile_1)).maxAmountInPot);
	}
	return total - static_cast<int32_t>(pots.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).maxAmountInPot);
}

void StoragePitStore::SyncTotals(entt::entity store)
{
	SyncStoreTotals(store);
}

uint32_t StoragePitStore::GetResource(entt::entity store, ResourceType type)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pit = registry.TryGet<const StoragePit>(store);
	if (pit == nullptr)
	{
		return 0;
	}
	uint32_t total = 0;
	if (type == ResourceType::Wood)
	{
		for (const auto pile : pit->woodPiles)
		{
			if (const auto* pot = StorePot(pile); pot != nullptr)
			{
				total += pot->amount;
			}
		}
	}
	else if (const auto* pot = StorePot(pit->foodPile); pot != nullptr)
	{
		total += pot->amount;
	}
	return total;
}

entt::entity StoragePitStore::OwnerOf(entt::entity pile)
{
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity owner = entt::null;
	registry.Each<const StoragePit>([&](entt::entity store, const StoragePit& pit) {
		if (pit.foodPile == pile || std::find(pit.woodPiles.begin(), pit.woodPiles.end(), pile) != pit.woodPiles.end())
		{
			owner = store;
		}
	});
	return owner;
}
