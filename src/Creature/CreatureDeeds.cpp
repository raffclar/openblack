/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDeeds.h"

using namespace openblack;
using namespace openblack::creature_deeds;

std::optional<uint32_t> creature_deeds::GiftDeed(const Gift& gift, const std::function<uint32_t(uint32_t)>& random)
{
	if (gift.goingUp && gift.resource == ResourceType::Wood)
	{
		return k_PutWoodInBuildingSite;
	}
	switch (gift.receiver)
	{
	case Receiver::StoragePit:
		if (gift.resource == ResourceType::Food)
		{
			if (gift.ownResource)
			{
				return k_PutFoodInStoragePit;
			}
			return random(2) != 0 ? k_StealFoodFromStoragePit : k_StealFoodFromFarm;
		}
		return gift.ownResource ? k_PutWoodInStoragePit : k_StealWoodFromStoragePit;
	case Receiver::Workshop:
		return gift.resource == ResourceType::Wood ? std::optional(k_PutWoodInWorkshop) : std::nullopt;
	case Receiver::WorshipSite:
		return gift.resource == ResourceType::Food ? std::optional(k_PutFoodInWorshipSite) : std::nullopt;
	case Receiver::Other:
		break;
	}
	return std::nullopt;
}

std::optional<uint32_t> creature_deeds::LandingDeed(const Landing& landing)
{
	const bool othersThing = landing.thingsTown.has_value() && *landing.thingsTown != landing.player;
	if (landing.nearOwnTemple)
	{
		return othersThing ? k_StealObjectAndPutByTemple : k_TakeObjectHome;
	}
	if (othersThing && landing.nearestTown == landing.player)
	{
		return k_StealObjectAndPutInTown;
	}
	return std::nullopt;
}

std::optional<uint32_t> creature_deeds::DiscipleDeed(VillagerDisciple type)
{
	const auto deed = k_ThrowInTheSea + static_cast<uint32_t>(type);
	return deed < k_NoDeed ? std::optional(deed) : std::nullopt;
}
