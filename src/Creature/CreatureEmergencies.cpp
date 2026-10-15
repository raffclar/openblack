/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureEmergencies.h"

using namespace openblack;

bool creature_emergencies::LooksAtEmergencies(const Gates& gates)
{
	return !gates.emergencyRunning && !gates.scripted && !gates.planForced && !gates.fighting;
}

bool creature_emergencies::IsHurt(float life)
{
	return life < k_HurtLife;
}

creature_emergencies::OwnFire creature_emergencies::ChooseOwnFire(float seen, float needed, bool canPay)
{
	// A share of more than all also passes the test of the part
	return seen / needed > k_WaterMiracleShare && canPay ? OwnFire::CastWater : OwnFire::DrinkFromTheSea;
}

bool creature_emergencies::PutsOutTownFire(const TownFireFacts& facts)
{
	// More sightings than it needs count as all that it needs. A share that is not a number stays and fails the test.
	const float share = facts.seen / facts.needed > 1.0f ? 1.0f : facts.seen / facts.needed;
	return facts.knowsWater && share > k_WaterMiracleShare && facts.canPay && facts.compassionActivated &&
	       !facts.compassionSuppressed && facts.anger < facts.compassion;
}

std::optional<size_t> creature_emergencies::PickBurningBuilding(std::span<const Building> buildings)
{
	std::optional<size_t> last;
	for (size_t i = 0; i < buildings.size(); ++i)
	{
		if (!buildings[i].onFire)
		{
			continue;
		}
		if (buildings[i].comesFirst)
		{
			return i;
		}
		last = i;
	}
	return last;
}

bool creature_emergencies::KeepsRunSpeed(bool hasPlayer, bool computerPlayer)
{
	return !hasPlayer || computerPlayer;
}
