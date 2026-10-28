/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerMindWorshipRules.h"

#include "Common/X87.h"

namespace openblack::computer_mind_rules
{

namespace
{

/// The desire that a disciple of each work serves, from the work of a farmer to the work from a vortex
constexpr std::array<TownDesireInfo, 13> k_DesireOfWork {
    TownDesireInfo::None,    TownDesireInfo::ForFood,     TownDesireInfo::ForWood, TownDesireInfo::ForFood,
    TownDesireInfo::ToBuild, TownDesireInfo::ForChildren, TownDesireInfo::None,    TownDesireInfo::None,
    TownDesireInfo::None,    TownDesireInfo::None,        TownDesireInfo::None,    TownDesireInfo::None,
    TownDesireInfo::None,
};

/// The disciples that a town has before it counts its disciples, so that it never divides by none
constexpr float k_DisciplesAtStart = 0.001f;
/// The share of the free adults that the god thinks fit for its disciples
constexpr float k_FreeAdultsForDisciples = 0.5f;
/// The weight of a work's share of the disciples, which the game keeps as a double
constexpr double k_ShareWeight = 0.25;
/// The distance from a building at which the hand puts a villager, as a part of the building's radius; the game keeps
/// it as a double
constexpr double k_DropRadiusTimes = 1.25;

} // namespace

float LifeDesire(float life)
{
	return 1.0f - life;
}

float DancersRestDesire(std::span<const float> lifeDesires)
{
	if (lifeDesires.empty())
	{
		return 0.0f;
	}
	float sum = 0.0f;
	for (const float desire : lifeDesires)
	{
		sum = desire + sum;
	}
	const float mean = x87::Div(sum, static_cast<double>(lifeDesires.size()));
	return mean > 1.0f ? 1.0f : mean;
}

float RestDesireCounted(float restDesire)
{
	return restDesire < k_LeastRestDesire ? 0.0f : restDesire;
}

float WorshippersRestUrge(float mostRestDesire)
{
	const float twice = mostRestDesire + mostRestDesire;
	return twice < 1.0f ? twice : 1.0f;
}

float TiredWorshipperUrge(float mostLifeDesire)
{
	return static_cast<double>(mostLifeDesire) > k_TiredWorshipper ? 1.0f : mostLifeDesire;
}

std::optional<size_t> LeastLife(std::span<const float> lives)
{
	// Only a life below full counts
	float least = 1.0f;
	std::optional<size_t> found;
	for (size_t i = 0; i < lives.size(); ++i)
	{
		if (lives[i] < least)
		{
			least = lives[i];
			found = i;
		}
	}
	return found;
}

std::optional<TownDesireInfo> DesireServedBy(VillagerDisciple work)
{
	const auto index = static_cast<size_t>(work);
	if (index >= k_DesireOfWork.size() || k_DesireOfWork.at(index) == TownDesireInfo::None)
	{
		return std::nullopt;
	}
	return k_DesireOfWork.at(index);
}

std::optional<DiscipleChoice> MostWantedDisciple(const std::array<uint8_t, 13>& disciples, uint32_t adults, float worshipShare,
                                                 std::span<const float> desires)
{
	float total = k_DisciplesAtStart;
	for (const uint8_t count : disciples)
	{
		total = total + static_cast<float>(count);
	}
	std::optional<DiscipleChoice> best;
	float most = 0.0f;
	for (size_t work = 1; work < disciples.size(); ++work)
	{
		const auto desire = DesireServedBy(static_cast<VillagerDisciple>(work));
		if (!desire.has_value())
		{
			continue;
		}
		const float free = x87::Mul(static_cast<double>(adults), 1.0f - worshipShare) * k_FreeAdultsForDisciples;
		if (free == 0.0f)
		{
			continue;
		}
		const float shareLeft =
		    x87::Mul(1.0f - (static_cast<float>(disciples.at(work)) + k_DisciplesAtStart) / total, k_ShareWeight);
		const float filled = total / free;
		const float freeFilled = filled < 1.0f ? filled : 1.0f;
		const auto index = static_cast<size_t>(*desire);
		const float wanted = index < desires.size() ? desires[index] : 0.0f;
		const float value = wanted * (1.0f - freeFilled) * shareLeft;
		if (value > most)
		{
			most = value;
			best = DiscipleChoice {.work = static_cast<VillagerDisciple>(work), .value = value};
		}
	}
	return best;
}

bool TakesWorkers(const SiteFacts& site)
{
	return site.maxBuilders > 0 && site.buildersNeeded > 0 && (!site.built || site.repairDesire > 0.0f);
}

bool TakesWood(const SiteFacts& site)
{
	return site.woodNeeded > 0.0f && (!site.built || site.repairDesire > 0.0f);
}

float SiteWorkersValue(float pull, float desireForVillagers, uint32_t builders, int32_t maxBuilders)
{
	const float share = x87::Div(static_cast<double>(builders), static_cast<double>(maxBuilders));
	const float filled = share < 1.0f ? share : 1.0f;
	return pull * ((1.0f - filled) * desireForVillagers);
}

float SiteWoodValue(float pull, uint32_t wood, float woodNeeded)
{
	const float share = x87::RoundToFloat(static_cast<double>(wood)) / woodNeeded;
	const float filled = share < 1.0f ? share : 1.0f;
	const float cubed = filled * filled * filled;
	return pull * (1.0f - cubed);
}

float FreeBuilderValue(float pull, float life, float nearness)
{
	return pull * (life * nearness);
}

float SiteWorkerDropRadius(float buildingRadius)
{
	return x87::Mul(buildingRadius, k_DropRadiusTimes);
}

float HomeDropDistance(float homeRadius, float villagerRadius)
{
	return x87::Add(villagerRadius, x87::Mul(homeRadius, k_DropRadiusTimes));
}

} // namespace openblack::computer_mind_rules
