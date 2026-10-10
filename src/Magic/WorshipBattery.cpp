/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipBattery.h"

#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// An empty battery speeds the dance up by this much, falling to nothing as the battery fills
constexpr float k_EmptyBatteryBoost = 0.5f;
/// Any boost at all is at least this much
constexpr float k_MinimumBatteryBoost = 0.2f;
} // namespace

WorshipBatteryRules magic::WorshipBatteryRulesFor(const GWorshipSiteInfo& info, float tribalPower)
{
	return {
	    .chantsPerVillager = info.chantsPerVillager,
	    .chantsToFillBattery = info.chantsToFillBattery,
	    .eachVillagerAddToFillBattery = info.eachVillagerAddToFillBattery,
	    .chantsToReserveForMaintaining = info.chantsToReserveForMaintaining,
	    .tribalPower = tribalPower,
	};
}

float magic::WorshipCapacity(const WorshipBatteryRules& rules, uint32_t dancers)
{
	return static_cast<float>(dancers) * rules.chantsPerVillager * rules.tribalPower;
}

float magic::WorshipMaxBattery(const WorshipBatteryRules& rules, uint32_t dancers)
{
	return static_cast<float>(dancers) * rules.eachVillagerAddToFillBattery + rules.chantsToFillBattery;
}

float magic::WorshipAvailable(const WorshipBattery& site)
{
	return site.infinite ? k_InfiniteChants : site.available - site.used;
}

float magic::WorshipAvailableForIcons(const WorshipBattery& site, const WorshipBatteryRules& rules, bool seedsOut)
{
	const float available = WorshipAvailable(site);
	if (!seedsOut)
	{
		return available;
	}
	const float left = available - rules.chantsToReserveForMaintaining;
	return 0.0f <= left ? left : 0.0f;
}

float magic::UseWorshipChants(WorshipBattery& site, float amount)
{
	if (amount < 0.0f)
	{
		return 0.0f;
	}
	site.requested += amount;
	const float available = WorshipAvailable(site);
	if (amount <= available)
	{
		site.used += amount;
		return amount;
	}
	site.used = site.available;
	return available;
}

float magic::UseWorshipChantsIfNotInfinite(WorshipBattery& site, float amount)
{
	return site.infinite ? amount : UseWorshipChants(site, amount);
}

float magic::MaintainSpellFromWorship(WorshipBattery& site, float amount)
{
	return site.freeMaintenance ? amount : UseWorshipChants(site, amount);
}

void magic::UpdateWorshipStrain(WorshipBattery& site, const WorshipBatteryRules& rules, uint32_t dancers)
{
	const float capacity = WorshipCapacity(rules, dancers);
	if (capacity == 0.0f)
	{
		site.strain = site.requested != 0.0f ? 1.0f : 0.0f;
	}
	else
	{
		site.strain = (site.requested - capacity) / capacity;
	}
}

float magic::WorshipIconShare(const WorshipBattery& site, const WorshipBatteryRules& rules, uint32_t chargingIcons,
                              float neededByIcons, bool seedsOut)
{
	if (site.strain > 0.0f || chargingIcons == 0)
	{
		return 0.0f;
	}
	const float available = WorshipAvailableForIcons(site, rules, seedsOut);
	const float given = neededByIcons <= available ? neededByIcons : available;
	return given / static_cast<float>(chargingIcons);
}

void magic::EndWorshipTurn(WorshipBattery& site, const WorshipBatteryRules& rules, uint32_t dancers)
{
	const float capacity = WorshipCapacity(rules, dancers);
	// How much of what the dancers can chant was drawn: all of it when they cannot chant at all
	const float drawn = capacity != 0.0f ? site.used / capacity : 1.0f;
	float boost = k_EmptyBatteryBoost - site.battery / WorshipMaxBattery(rules, dancers) * k_EmptyBatteryBoost;
	if (boost <= 0.0f)
	{
		boost = 0.0f;
	}
	else if (boost < k_MinimumBatteryBoost)
	{
		boost = k_MinimumBatteryBoost;
	}
	const float intensity = drawn + boost;
	site.danceIntensity = intensity < 1.0f ? intensity : 1.0f;

	// Artifacts placed at the site are also powered up every thousand turns; openblack has no artifacts yet
	const float chanted = capacity * site.danceIntensity;
	site.chantsPerDancer = dancers != 0 ? chanted / static_cast<float>(dancers) : 0.0f;
	const float battery = site.battery - (site.used - chanted);
	site.battery = battery <= 0.0f ? 0.0f : battery;
	site.used = 0.0f;
	site.requested = 0.0f;
	site.available = site.battery + capacity;
}

float magic::AddToIconStore(WorshipIconCharge& icon, float share)
{
	if (icon.store + share > icon.required)
	{
		// The game charges the site only what overflows the icon, not what went into it
		const float charged = share - (icon.required - icon.store);
		icon.store = icon.required;
		return charged;
	}
	icon.store += share;
	return share;
}

float magic::ProcessWorshipTurn(WorshipBattery& site, const WorshipBatteryRules& rules, uint32_t dancers,
                                std::span<WorshipIconCharge> icons)
{
	float drawn = 0.0f;
	UpdateWorshipStrain(site, rules, dancers);
	if (site.strain <= 0.0f)
	{
		uint32_t charging = 0;
		float needed = 0.0f;
		bool seedsOut = false;
		for (const auto& icon : icons)
		{
			const float need = icon.required - icon.store;
			if (icon.charging && need > 0.0f)
			{
				++charging;
				needed += need;
			}
			seedsOut = seedsOut || icon.seedOut;
		}
		const float share = WorshipIconShare(site, rules, charging, needed, seedsOut);
		if (share != 0.0f)
		{
			for (auto& icon : icons)
			{
				if (icon.charging && icon.required - icon.store > 0.0f)
				{
					const float charged = AddToIconStore(icon, share);
					if (!site.infinite)
					{
						drawn += UseWorshipChants(site, charged);
					}
				}
			}
		}
	}
	EndWorshipTurn(site, rules, dancers);
	return drawn;
}

float magic::WorshipAvailableForVirtualInfluence(const WorshipBattery& site, const WorshipBatteryRules& rules, uint32_t dancers,
                                                 uint32_t interfaces)
{
	const float available = WorshipAvailable(site);
	if (available == 0.0f || interfaces == 0)
	{
		return 0.0f;
	}
	float taken = available - rules.chantsToReserveForMaintaining;
	// Only what the dancers chant this turn, never the battery
	const float chanting = WorshipCapacity(rules, dancers) - site.used;
	if (chanting <= taken)
	{
		taken = chanting;
	}
	if (taken < 0.0f)
	{
		taken = 0.0f;
	}
	return taken / static_cast<float>(interfaces);
}
