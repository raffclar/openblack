/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownMagic.h"

#include <algorithm>

#include "Citadel.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownQueries.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/MagicTables.h"
#include "TownCentreSpellIcon.h"
#include "WorshipSite.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
TownMagic* MagicOf(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (town == entt::null || !registry.Valid(town))
	{
		return nullptr;
	}
	return registry.TryGet<TownMagic>(town);
}

size_t Index(MagicType type)
{
	return static_cast<size_t>(type);
}
} // namespace

bool town::AddMagicTypesHeld(entt::entity townEntity, MagicType type)
{
	auto* magic = MagicOf(townEntity);
	if (magic == nullptr || Index(type) >= TownMagic::k_MagicTypes || magic->held[Index(type)])
	{
		return false;
	}
	magic->held[Index(type)] = true;
	magic::players::SetMagicTypeEnabled(OwnerOf(townEntity), type, true);
	const auto& tables = Locator::infoConstants::value();
	const auto seed = magic::GetFirstSpellSeedForMagicType(tables, type);
	if (static_cast<int>(seed) < 0)
	{
		return true;
	}
	const auto& seedInfo = magic::GetSpellSeedInfo(tables, seed);
	const auto base = magic::MagicTypeForPowerUpLevel(seedInfo, -1);
	const int powerUp = magic::GetPowerUpFromMagicType(seedInfo, type);
	const auto centre = TownCentreOf(townEntity);
	if (centre != entt::null)
	{
		if (base == type)
		{
			// the first seed of the magic
			town_centre::AddSpell(
			    centre,
			    static_cast<SpellSeedType>(magic::GetSpellSeedForMagicType(tables, type).value_or(magic::k_SpellSeedNotFound)));
		}
		else
		{
			town_centre::AddPowerUp(centre, seed, powerUp);
		}
	}
	return true;
}

void town::RemoveMagicTypesHeld(entt::entity townEntity, MagicType type)
{
	auto* magic = MagicOf(townEntity);
	if (magic == nullptr || Index(type) >= TownMagic::k_MagicTypes || !magic->held[Index(type)])
	{
		return;
	}
	magic->held[Index(type)] = false;
	magic::players::SetMagicTypeEnabled(OwnerOf(townEntity), type, false);
	const auto& tables = Locator::infoConstants::value();
	const auto seed = magic::GetFirstSpellSeedForMagicType(tables, type);
	if (static_cast<int>(seed) < 0)
	{
		return;
	}
	const auto& seedInfo = magic::GetSpellSeedInfo(tables, seed);
	const auto base = magic::MagicTypeForPowerUpLevel(seedInfo, -1);
	const int powerUp = magic::GetPowerUpFromMagicType(seedInfo, type);
	const auto centre = TownCentreOf(townEntity);
	if (centre != entt::null)
	{
		if (base == type)
		{
			town_centre::RemoveSpell(
			    centre,
			    static_cast<SpellSeedType>(magic::GetSpellSeedForMagicType(tables, type).value_or(magic::k_SpellSeedNotFound)));
		}
		else
		{
			town_centre::ClearPowerUp(centre, seed, powerUp);
		}
	}
}

bool town::IsMagicTypeHeld(entt::entity townEntity, MagicType type)
{
	const auto* magic = MagicOf(townEntity);
	return magic != nullptr && Index(type) < TownMagic::k_MagicTypes && magic->held[Index(type)];
}

bool town::TakeSeedMagic(entt::entity townEntity, SpellSeedType seed, std::array<bool, 4>& taken)
{
	const auto& seedInfo = magic::GetSpellSeedInfo(Locator::infoConstants::value(), seed);
	const auto base = seedInfo.magicTypes[0];
	if (!IsMagicTypeHeld(townEntity, base))
	{
		return false;
	}
	taken[0] = true;
	RemoveMagicTypesHeld(townEntity, base);
	for (int pu = 0; pu < 3; ++pu)
	{
		const auto type = magic::MagicTypeForPowerUpLevel(seedInfo, pu);
		const bool held = type != MagicType::None && IsMagicTypeHeld(townEntity, type);
		if (held)
		{
			RemoveMagicTypesHeld(townEntity, type);
		}
		taken[static_cast<size_t>(pu + 1)] = held;
	}
	return true;
}

void town::GiveSeedMagic(entt::entity townEntity, SpellSeedType seed, const std::array<bool, 4>& taken)
{
	const auto& seedInfo = magic::GetSpellSeedInfo(Locator::infoConstants::value(), seed);
	for (int pu = -1; pu < 3; ++pu)
	{
		if (!taken[static_cast<size_t>(pu + 1)])
		{
			continue;
		}
		if (const auto type = magic::MagicTypeForPowerUpLevel(seedInfo, pu); type != MagicType::None)
		{
			AddMagicTypesHeld(townEntity, type);
		}
	}
}

bool town::IsAllowedToCreateWorshipSite(entt::entity townEntity)
{
	const auto* magic = MagicOf(townEntity);
	if (magic == nullptr || influence::LandNumber() == 1 || magic->forbidWorshipSite)
	{
		return false;
	}
	return Population(townEntity) != 0;
}

void town::CheckAddWorshipSite(entt::entity townEntity)
{
	if (!IsAllowedToCreateWorshipSite(townEntity))
	{
		return;
	}
	const auto owner = OwnerOf(townEntity);
	// a player of type 3 has no worship site (UNVERIFIED which player type that is; no openblack player has it)
	const auto citadel = citadel::Of(owner);
	if (citadel == entt::null)
	{
		return;
	}
	const auto site = citadel::FindOrCreateWorshipSite(citadel, townEntity);
	if (site == entt::null)
	{
		return;
	}
	const auto& towns = Locator::entitiesRegistry::value().Get<const WorshipSite>(site).towns;
	if (std::ranges::find(towns, townEntity) == towns.end())
	{
		site::AddTown(site, townEntity);
	}
}

// (inferred): the original keeps a pointer to the centre; here, the first Abode of the town whose info is
// TOWN_CENTRE
entt::entity town::TownCentreOf(entt::entity townEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (townEntity == entt::null || !registry.Valid(townEntity) || !registry.AllOf<Town>(townEntity))
	{
		return entt::null;
	}
	const auto id = registry.Get<const Town>(townEntity).id;
	const auto& abodes = Locator::infoConstants::value().abode;
	entt::entity found = entt::null;
	registry.Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		if (found != entt::null || abode.townId != id)
		{
			return;
		}
		for (const auto& info : abodes)
		{
			if (info.abodeNumber == abode.type && info.abodeType == AbodeType::TownCentre)
			{
				found = entity;
				return;
			}
		}
	});
	return found;
}

int town::Population(entt::entity townEntity)
{
	int count = 0;
	Locator::entitiesRegistry::value().Each<const Villager>([&](const Villager& villager) {
		if (villager.town == townEntity)
		{
			++count;
		}
	});
	return count;
}

PlayerNames town::OwnerOf(entt::entity townEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (townEntity == entt::null || !registry.Valid(townEntity))
	{
		return PlayerNames::NEUTRAL;
	}
	const auto* town = registry.TryGet<const Town>(townEntity);
	return town != nullptr ? town->owner : PlayerNames::NEUTRAL;
}

entt::entity town::FromId(uint32_t id)
{
	return ecs::town_queries::FindTownWithID(id);
}
