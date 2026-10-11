/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipperRules.h"

#include <algorithm>
#include <numeric>

#include "Common/GUtilsDistance.h"

namespace openblack::ecs::worshipper_rules
{

int32_t Target(uint32_t population, float share)
{
	if (share <= 0.0f)
	{
		return 0;
	}
	// Rounded to the nearest person, and never fewer than one
	const auto wanted = static_cast<int32_t>(static_cast<float>(population) * share + 0.5f);
	return std::max(wanted, 1);
}

Needed WorshippersNeeded(const Headcount& town, bool countOnTheWay, bool makeUpForThoseGoingHome)
{
	const auto have = static_cast<int32_t>(town.worshipping + (countOnTheWay ? town.onTheWay : 0u));
	const auto goingHome = makeUpForThoseGoingHome ? static_cast<int32_t>(town.askingToGoHome) : 0;
	const auto target = Target(town.population, town.share);
	const int32_t count = target - have + goingHome;
	return {.count = count, .alreadyEnough = count > 0 && have >= target};
}

std::optional<float> ShareAfterDisciple(int32_t needed, uint32_t worshipping, uint32_t onTheWay, uint32_t population)
{
	if (needed > 0 || population == 0)
	{
		return std::nullopt;
	}
	const float share = (static_cast<float>(needed) + k_DiscipleShareSlack + static_cast<float>(onTheWay) +
	                     static_cast<float>(static_cast<int32_t>(worshipping))) /
	                    static_cast<float>(population);
	return std::min(share, 1.0f);
}

float CandidateKey(float villagerToSite, float townToSite, float life)
{
	const float nearness = gutils::GetDistanceModifier(villagerToSite, townToSite + k_TownDistanceSlack);
	return life * life * life * nearness;
}

std::vector<size_t> CandidateOrder(std::span<const float> keys, bool fittestFirst)
{
	std::vector<size_t> order(keys.size());
	std::iota(order.begin(), order.end(), size_t {0});
	std::ranges::stable_sort(
	    order, [keys, fittestFirst](size_t a, size_t b) { return fittestFirst ? keys[a] > keys[b] : keys[a] < keys[b]; });
	return order;
}

bool LifeAllowsWorship(bool ignoreLowLife, float life, float damageThresholdToGoHome)
{
	return ignoreLowLife || life > damageThresholdToGoHome;
}

bool CanReachSite(float distance, float maxDistance, bool hasPlayer, bool stoneFound)
{
	if (distance <= maxDistance || !hasPlayer)
	{
		return true;
	}
	return stoneFound;
}

Arrival OnReachingGate(float distanceToGate, uint32_t dancers, uint32_t maxDancersVisible)
{
	if (!(distanceToGate < k_ArriveDistance))
	{
		return Arrival::WalkOn;
	}
	return dancers < maxDancersVisible ? Arrival::Dance : Arrival::Hide;
}

float ChantDamage(float chantsPerDancer, float chantLifeRate)
{
	return chantsPerDancer * chantLifeRate;
}

bool EatsAtSite(float belly, float hungryForFood, uint32_t siteFood, uint32_t meal)
{
	return belly < hungryForFood && siteFood >= meal;
}

GoingBack CheckGoingBack(bool townCanWorship, int32_t neededNotCountingOnTheWay, bool mayGoHome)
{
	if (!townCanWorship)
	{
		return GoingBack::Decide;
	}
	return neededNotCountingOnTheWay < 0 && mayGoHome ? GoingBack::GoHome : GoingBack::Stay;
}

float DancersFoodNeed(std::span<const float> bellies, std::span<const int32_t> dinnerSizes)
{
	float need = 0.0f;
	const auto count = std::min(bellies.size(), dinnerSizes.size());
	for (size_t i = 0; i < count; ++i)
	{
		need += (1.0f - bellies[i]) * static_cast<float>(dinnerSizes[i]);
	}
	return need;
}

int32_t FoodNeededFromTown(float dancersNeed, uint32_t inPot, int32_t onTheWay)
{
	const int32_t wanted = static_cast<int32_t>(dancersNeed) - (static_cast<int32_t>(inPot) + onTheWay);
	return std::max(wanted, 0);
}

int32_t FoodSpareForWorship(std::optional<uint32_t> pitFood, float foodForDinner)
{
	if (!pitFood.has_value())
	{
		return 0;
	}
	const auto spare = static_cast<int32_t>(static_cast<double>(*pitFood) - static_cast<double>(foodForDinner));
	return std::max(spare, 0);
}

int32_t FoodForSiteIfEnough(int32_t wanted, int32_t spare)
{
	if (wanted != 0 && (spare < wanted || wanted < 0))
	{
		return 0;
	}
	return wanted;
}

} // namespace openblack::ecs::worshipper_rules
