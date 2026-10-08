/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectResources.h"

#include <algorithm>

#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/StoragePitStore.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownStores.h"
#include "ECS/Town/Workshops.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/WorshipSite.h"

namespace openblack::ecs::object_resources
{
using namespace components;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Abode* AbodeComponent(entt::entity abode)
{
	auto& registry = Entities();
	return abode != entt::null && registry.Valid(abode) ? registry.TryGet<Abode>(abode) : nullptr;
}

Pot* PotComponent(entt::entity pot)
{
	auto& registry = Entities();
	return pot != entt::null && registry.Valid(pot) ? registry.TryGet<Pot>(pot) : nullptr;
}

/// the town's desire for the resource (wood for anything but FOOD): d = 1 (Wood) or 0 (Food)
float CallResourceDesire(entt::entity town, ResourceType type)
{
	return town_desire::CallDesireFunctionNow(town,
	                                          type != ResourceType::Food ? TownDesireInfo::ForWood : TownDesireInfo::ForFood);
}

/// The town's owner
PlayerNames TownOwner(entt::entity town)
{
	const auto* t = Entities().TryGet<const Town>(town);
	return t != nullptr ? t->owner : PlayerNames::NEUTRAL;
}
} // namespace

uint32_t GetResource(entt::entity object, ResourceType type)
{
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return 0;
	}
	if (registry.AllOf<StoragePit>(object))
	{
		return StoragePitStore::GetResource(object, type);
	}
	if (const auto* a = registry.TryGet<const Abode>(object); a != nullptr)
	{
		// its own amount of that type
		return type == ResourceType::Food ? a->foodAmount : type == ResourceType::Wood ? a->woodAmount : 0;
	}
	if (const auto* pot = registry.TryGet<const Pot>(object); pot != nullptr)
	{
		// a pile linked to its building site: the pile's own amount; part of another structure: that structure's amount
		// (a pit's mirror, its whole total)
		if (building_sites::SiteOfPile(object) != entt::null)
		{
			return PotAmount(object, type);
		}
		if (const auto owner = StoragePitStore::OwnerOf(object); owner != entt::null)
		{
			return StoragePitStore::GetResource(owner, type);
		}
		// a workshop's pile (part of the workshop, not linked to its site): the workshop's amount (its mirror)
		if (const auto workshop = workshops::WorkshopOfPile(object); workshop != entt::null)
		{
			return GetResource(workshop, type);
		}
		return PotAmount(object, type);
	}
	if (scaffolds::IsScaffold(object))
	{
		// the scaffold's own type -> its default resource (value x WoodValue); else 0
		return type == GetResourceType(object) ? scaffolds::GetDefaultResource(object) : 0;
	}
	return 0;
}

ResourceType GetResourceType(entt::entity object)
{
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return ResourceType::None;
	}
	if (const auto* p = registry.TryGet<const Pot>(object); p != nullptr)
	{
		// its info's type
		return Locator::infoConstants::value().pot.at(static_cast<size_t>(p->type)).resourceType;
	}
	// a scaffold holds WOOD
	return scaffolds::IsScaffold(object) ? ResourceType::Wood : ResourceType::None;
}

uint32_t PotAmount(entt::entity pot, ResourceType type)
{
	const auto* p = PotComponent(pot);
	if (p == nullptr)
	{
		return 0;
	}
	// 0 unless the type is the pot's own, else its amount
	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(p->type));
	return info.resourceType == type ? p->amount : 0;
}

uint32_t RemoveFromPotDirect(entt::entity object, uint32_t amount)
{
	auto* pot = PotComponent(object);
	if (pot == nullptr)
	{
		return 0;
	}
	// n >= the amount -> all of it, the reaction removed, the poison off and the fire deleted; else amount -= n
	uint32_t removed = amount;
	if (amount >= pot->amount)
	{
		removed = pot->amount;
		pot->amount = 0;
		animal_ai::RemovePotReaction(object);
		pot->poisoned = false;
		if (auto* fire = fire::Find(object); fire != nullptr)
		{
			fire::ToBeDeleted(*fire);
		}
	}
	else
	{
		pot->amount = static_cast<uint16_t>(pot->amount - amount);
	}
	// its size follows the amount
	archetypes::PotArchetype::SetSize(object, true);
	// a structure's pot: still something -> its size again (the same result here). Empty: a pile of a structure stays;
	// any other pile, MagicFood or MagicWood is deleted. A plain pot has no such step: it stays. (the deletion:
	// ecs::ToBeDeleted, out of the physics, the map cells, then the entity)
	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
	// A building site's pile is part of its building: it stays too, until the building site is deleted and releases it
	// A workshop's pile is part of the workshop: it stays too
	if (pot->amount == 0 && info.potType != PotType::Pot && StoragePitStore::OwnerOf(object) == entt::null &&
	    building_sites::SiteOfPile(object) == entt::null && workshops::WorkshopOfPile(object) == entt::null)
	{
		// a food pile closes its speed-up visual first, then the common deletion (on the dead list when the deferral is on).
		// A town's temporary pot that goes stays in Town::temporaryPots until the town's turn
		// (town_stores::ProcessTemporaryPots) finds it unavailable; the temporary store and storage pit lookups ask
		// IsAvailable meanwhile
		pot_resource::SetSpeedUp(object, false);
		ecs::ToBeDeleted(object); // the common deletion: physics, map cells, the entity
	}
	return removed;
}

uint32_t DoResourceRemoving(entt::entity abode, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper,
                            const std::function<uint32_t()>& justRemove)
{
	// amount >= what it has -> the poison flag would be cleared: no effect (an abode keeps no poison flag; a storage
	// pit ORs with each pile's own: nothing is ever cleared)
	const auto town = abode_villagers::TownOf(abode);
	// the town's desire before the change
	const float before = town != entt::null ? CallResourceDesire(town, type) : 0.0f;
	// the change itself
	const uint32_t removed = justRemove();
	// with an interface and a town
	if (dropper.hasInterface && town != entt::null)
	{
		town_stores::SetGameTurnResourceLastRemoved(town, dropper.player, type);
		const float after = CallResourceDesire(town, type);
		// before - after; the alignment of the town's owner, with -n the amount asked for (literal negation, not verified in
		// game: a huge unsigned n wraps to a positive int, as in the original)
		effects::alignment::UpdateForResource(TownOwner(town), abode, -static_cast<int32_t>(amount), before - after);
	}
	return removed;
}

uint32_t DoResourceAdding(entt::entity abode, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper,
                          const std::function<uint32_t()>& justAdd)
{
	const auto town = abode_villagers::TownOf(abode);
	// no interface or no town -> the change only
	if (!dropper.hasInterface || town == entt::null)
	{
		return justAdd();
	}
	// the desire before and after the change
	const float before = CallResourceDesire(town, type);
	const uint32_t added = justAdd();
	float delta = before - CallResourceDesire(town, type);
	// x the modifier for the interface's player's last removal of that type
	delta *= town_stores::GetGameTurnResourceLastRemovedModifier(town, dropper.player, type);
	// the interface's player's alignment, with n: an abode's is the amount asked for, which the change returns whole; a
	// pit's is what its piles took, justAdd's value here. The type only feeds the alignment history, not kept
	// (UpdateForResource has no type)
	static_cast<void>(amount);
	effects::alignment::UpdateForResource(dropper.player, abode, static_cast<int32_t>(added), delta);
	// the town's belief: delta x (not the town's owner ? the non-owner multiplier : 1) x the town multiplier
	const auto& info = Locator::infoConstants::value().town;
	const float nonOwner = dropper.player != TownOwner(town) ? info.multiplierForNonOwnerAddingResource : 1.0f;
	town_stores::AddToBelief(town, dropper.player, delta * nonOwner * info.multiplierForAddingResourceToTown, abode, true, 1);
	// then the creature is told what was done, whatever was added (a storage pit's deed, its draw included; (pending)
	// the other abodes' deeds and the creature's mimicry)
	[[maybe_unused]] const auto deed = StoragePitStore::DoCreatureMimicAfterAddingResource(abode, type, dropper);
	return added;
}

uint32_t RemoveResource(entt::entity object, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper)
{
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return 0;
	}
	// the storage pit: the store's piles
	if (registry.AllOf<StoragePit>(object))
	{
		return StoragePitStore::RemoveResource(object, type, amount, dropper);
	}
	// a workshop (no building-site redirect): its pile, then its mirror
	if (workshops::IsWorkshop(object))
	{
		return workshops::RemoveResource(object, type, amount, dropper);
	}
	if (auto* a = registry.TryGet<Abode>(object); a != nullptr)
	{
		// an abode with a building site and WOOD or ANY -> the site's RemoveResource
		if (a->buildingSite != entt::null && (type == ResourceType::Wood || type == ResourceType::Any))
		{
			return building_sites::RemoveResource(a->buildingSite, type, amount);
		}
		if (type != ResourceType::Food && type != ResourceType::Wood)
		{
			return 0;
		}
		// then DoResourceRemoving
		auto& held = type == ResourceType::Food ? a->foodAmount : a->woodAmount;
		return DoResourceRemoving(object, type, amount, dropper, [&held, amount]() {
			// min(amount, what it has) off
			const uint32_t removed = std::min(amount, held);
			held -= removed;
			return removed;
		});
	}
	auto* pot = PotComponent(object);
	if (pot == nullptr)
	{
		return 0;
	}
	// a pile linked to its structure's building site -> the site's RemoveResource (every argument passed on)
	if (const auto site = building_sites::SiteOfPile(object); site != entt::null)
	{
		return building_sites::RemoveResource(site, type, amount);
	}
	// a workshop's pile (nothing over the maximum for a workshop): the pile, the workshop's DoResourceRemoving, then the
	// workshop's RemoveResource for the rest
	if (workshops::WorkshopOfPile(object) != entt::null)
	{
		return workshops::RemoveResourceFromPile(object, type, amount, dropper);
	}
	const auto owner = StoragePitStore::OwnerOf(object);
	if (owner == entt::null)
	{
		// a pot or pile without a structure: RemoveFromPotDirect, whatever the type (no test)
		return RemoveFromPotDirect(object, amount);
	}
	// a pile of a storage pit: over = the pit's amount above its maximum; the touched pile gives n - min(over, n) when
	// over > 0, else n
	const int32_t over = StoragePitStore::AmountOverMaximum(owner, type);
	const uint32_t mine = over > 0 ? amount - std::min(static_cast<uint32_t>(over), amount) : amount;
	uint32_t removed = 0;
	// r = what the pile gave, then r != 0 -> the pit's DoResourceRemoving(r). The desire before reads the pit's mirror
	// before the pile changes: openblack's mirror is the piles' total, so it runs around the pile's change (the same
	// values)
	if (mine != 0)
	{
		if (pot->amount != 0)
		{
			// DoResourceRemoving's n is r, what the pile gave
			const uint32_t r = std::min<uint32_t>(mine, pot->amount);
			removed = DoResourceRemoving(owner, type, r, dropper, [object, mine, owner]() {
				const uint32_t n = RemoveFromPotDirect(object, mine);
				StoragePitStore::SyncTotals(owner);
				return n;
			});
		}
		else
		{
			// an empty pile is still asked (poison, reaction and fire cleared, its size); r == 0 -> no DoResourceRemoving
			RemoveFromPotDirect(object, mine);
		}
	}
	// r < n -> r += the pit's RemoveResource(n - r) (pile 5 -> 1, the touched pile again)
	if (removed < amount)
	{
		removed += StoragePitStore::RemoveResource(owner, type, amount - removed, dropper);
	}
	return removed;
}

uint32_t AddResource(entt::entity object, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper,
                     bool poisoned)
{
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return 0;
	}
	// the storage pit (which is also an abode): the store's piles
	if (registry.AllOf<StoragePit>(object))
	{
		return StoragePitStore::AddResource(object, type, amount, dropper, poisoned);
	}
	// a workshop (no building-site redirect): its pile and its mirror
	if (workshops::IsWorkshop(object))
	{
		return workshops::AddResource(object, type, amount, poisoned);
	}
	// a worship site: its building site's wood, else its food pot
	if (registry.AllOf<WorshipSite>(object))
	{
		return ::openblack::worship::site::AddResource(object, type, amount, poisoned);
	}
	if (auto* a = AbodeComponent(object); a != nullptr)
	{
		// an abode with a building site and WOOD or ANY -> the site's AddResource (no position: a citadel building site would
		// add nothing)
		if (a->buildingSite != entt::null && (type == ResourceType::Wood || type == ResourceType::Any))
		{
			return building_sites::AddResource(a->buildingSite, type, amount, nullptr, poisoned);
		}
		if (type != ResourceType::Food && type != ResourceType::Wood)
		{
			return 0;
		}
		// else DoResourceAdding: amount added, no cap (poisoned is not kept by an abode)
		auto& held = type == ResourceType::Food ? a->foodAmount : a->woodAmount;
		return DoResourceAdding(object, type, amount, dropper, [&held, amount]() {
			held += amount;
			return amount;
		});
	}
	if (registry.AllOf<Pot>(object))
	{
		// a pile linked to its structure's building site -> the site's AddResource (every argument passed on;
		// object_resources has no position. TODO: the citadel site's nearest pile needs it)
		if (const auto site = building_sites::SiteOfPile(object); site != entt::null)
		{
			return building_sites::AddResource(site, type, amount, nullptr, poisoned);
		}
		// part of a structure not linked to its site -> the structure's AddResource (every argument passed on): a
		// workshop's pile -> the workshop's AddResource
		if (const auto workshop = workshops::WorkshopOfPile(object); workshop != entt::null)
		{
			return workshops::AddResource(workshop, type, amount, poisoned);
		}
		// any other pot or pile (pot_resource): a pile of a storage pit passes the interface and the poison on to the pit
		return pot_resource::PotStructureAddResource(object, type, amount, poisoned, dropper);
	}
	// TODO: a villager's AddResource (villager::AddResourceToVillager) once it exists
	return 0;
}

bool IsPileResource(entt::entity object)
{
	if (object == entt::null || !Entities().Valid(object))
	{
		return false;
	}
	const auto* info = object::PotInfoOf(object);
	return info != nullptr && info->potType != PotType::Pot;
}

PlayerNames PlayerOfPile(entt::entity pile)
{
	const auto* pot = PotComponent(pile);
	if (pot == nullptr)
	{
		return PlayerNames::NEUTRAL;
	}
	// a pile of a storage pit answers with the pit's player: the pit's town's owner, the neutral player without a town
	if (const auto pit = StoragePitStore::OwnerOf(pile); pit != entt::null)
	{
		const auto town = abode_villagers::TownOf(pit);
		return town != entt::null ? TownOwner(town) : PlayerNames::NEUTRAL;
	}
	return pot->owner;
}

bool IsPoisoned(entt::entity object)
{
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return false;
	}
	if (const auto* pit = registry.TryGet<const StoragePit>(object))
	{
		// an available pile of the food one or the five wood ones whose poison flag is set
		const auto poisoned = [&registry](entt::entity pile) {
			const auto* pot = ecs::IsAvailable(pile) ? registry.TryGet<const Pot>(pile) : nullptr;
			return pot != nullptr && pot->poisoned;
		};
		return poisoned(pit->foodPile) || std::any_of(pit->woodPiles.begin(), pit->woodPiles.end(), poisoned);
	}
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		return pot->poisoned;
	}
	if (registry.AnyOf<Villager>(object))
	{
		return life::IsPoisoned(object);
	}
	return false; // a plain object is never poisoned
}
} // namespace openblack::ecs::object_resources
