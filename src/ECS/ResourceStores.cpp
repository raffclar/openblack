/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ResourceStores.h"

#include <algorithm>

#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ObjectDelivery.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/TakeResource.h"
#include "ECS/Town/Workshops.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/WorshipSite.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// The structure a pile is part of (a storage pit or a workshop), or null
entt::entity StructureOfPile(entt::entity pile)
{
	if (const auto pit = StoragePitStore::OwnerOf(pile); pit != entt::null)
	{
		return pit;
	}
	return workshops::WorkshopOfPile(pile);
}

/// The resource type of a pot's info, or none
ResourceType PotResourceType(const Pot& pot)
{
	if (pot.type == PotInfo::_COUNT || !Locator::infoConstants::has_value())
	{
		return ResourceType::None;
	}
	return Locator::infoConstants::value().pot.at(static_cast<size_t>(pot.type)).resourceType;
}
} // namespace

bool resource_stores::IsStoreForType(StoreKind kind, ResourceType type, bool hasBuildingSite)
{
	const bool siteWood = type == ResourceType::Wood && hasBuildingSite;
	switch (kind)
	{
	case StoreKind::StoragePit:
		return true;
	case StoreKind::Abode:
		return siteWood;
	case StoreKind::Workshop:
		return siteWood || type == ResourceType::Wood || type == ResourceType::Any;
	case StoreKind::WorshipSite:
		return siteWood || type == ResourceType::Food || type == ResourceType::Any;
	case StoreKind::StructurePile: // answered with its structure (PileIsStoreForType)
	case StoreKind::LoosePot:
	case StoreKind::None:
		return false;
	}
	return false;
}

bool resource_stores::PileIsStoreForType(bool structureTakesType, ResourceType type, ResourceType pileType)
{
	return structureTakesType && (type == pileType || type == ResourceType::Any);
}

uint32_t resource_stores::AnimalFood(uint32_t foodTypeBits, float foodValue)
{
	// meat or vegetable; a grazer only is worth nothing
	if ((foodTypeBits & 3u) == 0 || !(foodValue > 0.0f))
	{
		return 0;
	}
	return static_cast<uint32_t>(foodValue);
}

uint32_t resource_stores::FenceWood(uint32_t woodValue, float life, float scale)
{
	const float wood = static_cast<float>(woodValue) * life * scale * scale * scale;
	return wood > 0.0f ? static_cast<uint32_t>(wood) : 0u;
}

resource_stores::StoreKind resource_stores::KindOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		return StoreKind::None;
	}
	// the storage pit first: it is a building too
	if (registry.AllOf<StoragePit>(object))
	{
		return StoreKind::StoragePit;
	}
	if (registry.AllOf<Pot>(object))
	{
		return StructureOfPile(object) != entt::null ? StoreKind::StructurePile : StoreKind::LoosePot;
	}
	if (workshops::IsWorkshop(object))
	{
		return StoreKind::Workshop;
	}
	if (registry.AllOf<WorshipSite>(object))
	{
		return StoreKind::WorshipSite;
	}
	if (registry.AllOf<Abode>(object))
	{
		return StoreKind::Abode;
	}
	return StoreKind::None;
}

bool resource_stores::IsResourceStore(entt::entity object, ResourceType type)
{
	const auto kind = KindOf(object);
	if (kind == StoreKind::StructurePile)
	{
		const auto& pot = Locator::entitiesRegistry::value().Get<const Pot>(object);
		return PileIsStoreForType(IsResourceStore(StructureOfPile(object), type), type, PotResourceType(pot));
	}
	return IsStoreForType(kind, type, abodes::GetBuildingSite(object) != entt::null);
}

bool resource_stores::DeleteObjectAndTakeResource(entt::entity store, entt::entity object, const pot_resource::Dropper& dropper)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	switch (KindOf(store))
	{
	case StoreKind::StoragePit:
		return take_resource::StoragePit(store, object, dropper);
	case StoreKind::WorshipSite:
		return worship::site::DeleteObjectAndTakeResource(store, object, dropper);
	case StoreKind::StructurePile:
	case StoreKind::LoosePot:
	case StoreKind::Workshop:
		object_delivery::DoDeleteObjectAndTakeResource(store, object, dropper);
		return true;
	case StoreKind::Abode:
		// a building takes a given object only on its building site
		if (abodes::GetBuildingSite(store) == entt::null)
		{
			return false;
		}
		object_delivery::DoDeleteObjectAndTakeResource(store, object, dropper);
		return true;
	case StoreKind::None:
		return false;
	}
	return false;
}
