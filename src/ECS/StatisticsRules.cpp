/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StatisticsRules.h"

#include <algorithm>
#include <bit>

namespace openblack::statistics
{

namespace
{
/// The points the game clears after averaging the pairs: as many bytes as there are points left, from the middle
constexpr size_t k_WholePointsClearedAfterHalving = (History::k_Points / 2) / sizeof(float);
/// The bytes of the next point it clears too, the low ones
constexpr size_t k_BytesOfNextPointCleared = (History::k_Points / 2) % sizeof(float);

constexpr bool Has(AbodeType type, AbodeType flag)
{
	return (static_cast<uint32_t>(type) & static_cast<uint32_t>(flag)) != 0;
}
} // namespace

void History::Add(float value)
{
	points.at(index) += value / static_cast<float>(samplesPerPoint);
	if (++samplesInPoint != samplesPerPoint)
	{
		return;
	}
	samplesInPoint = 0;
	if (++index != k_Points)
	{
		return;
	}
	// Full: the pairs are averaged into the first half, and each later point takes twice as many samples
	index = 0;
	for (size_t i = 0; i < k_Points; i += 2)
	{
		points.at(index) = (points.at(i) + points.at(i + 1)) * 0.5f;
		++index;
	}
	const auto cleared = points.begin() + index;
	std::fill(cleared, cleared + k_WholePointsClearedAfterHalving, 0.0f);
	auto& partly = points.at(index + k_WholePointsClearedAfterHalving);
	constexpr uint32_t k_KeptBits = ~((1u << (8 * k_BytesOfNextPointCleared)) - 1u);
	partly = std::bit_cast<float>(std::bit_cast<uint32_t>(partly) & k_KeptBits);
	samplesPerPoint = static_cast<uint16_t>(samplesPerPoint * 2);
}

void CountBuilt(BuildingsBuilt& built, AbodeType type)
{
	++built.total;
	if (Has(type, AbodeType::LivingQuarters))
	{
		++built.abodes;
	}
	else if (Has(type, AbodeType::Civic))
	{
		++built.civic;
	}
	else if (Has(type, AbodeType::Wonder))
	{
		++built.wonders;
	}
}

std::optional<size_t> DiscipleMadeIndex(VillagerDisciple kind)
{
	switch (kind)
	{
	case VillagerDisciple::Farmer:
		return 0;
	case VillagerDisciple::Forester:
		return 1;
	case VillagerDisciple::Fisherman:
		return 2;
	case VillagerDisciple::Builder:
		return 3;
	case VillagerDisciple::Breeder:
		return 4;
	case VillagerDisciple::Trader:
		return 5;
	case VillagerDisciple::Missionary:
		return 6;
	case VillagerDisciple::Craftsman:
		return 7;
	default:
		return std::nullopt;
	}
}

std::optional<MiracleKind> MiracleKindOf(MagicType type)
{
	switch (type)
	{
	case MagicType::Fireball:
	case MagicType::FireballPowerUpOne:
	case MagicType::FireballPowerUpTwo:
	case MagicType::LightningBolt:
	case MagicType::LightningBoltPowerUpOne:
	case MagicType::LightningBoltPowerUpTwo:
	case MagicType::ExplosionOne:
	case MagicType::ExplosionOnePuOne:
	case MagicType::ExplosionOnePuTwo:
	case MagicType::StormWindRain:
	case MagicType::StormWindRainLightning:
	case MagicType::Tornado:
	case MagicType::FlockGround:
		return MiracleKind::Aggressive;
	case MagicType::Heal:
	case MagicType::HealPowerUpOne:
	case MagicType::Teleport:
	case MagicType::Forest:
	case MagicType::Food:
	case MagicType::FoodPowerUpOne:
	case MagicType::Shield:
	case MagicType::PhysicalShield:
	case MagicType::Wood:
	case MagicType::Water:
	case MagicType::WaterPowerUpOne:
	case MagicType::FlockFlying:
		return MiracleKind::Nice;
	case MagicType::CreatureSpellFreeze:
	case MagicType::CreatureSpellSmall:
	case MagicType::CreatureSpellBig:
	case MagicType::CreatureSpellWeak:
	case MagicType::CreatureSpellStrong:
	case MagicType::CreatureSpellFat:
	case MagicType::CreatureSpellThin:
	case MagicType::CreatureSpellInvisible:
	case MagicType::CreatureSpellCompassion:
	case MagicType::CreatureSpellAngry:
	case MagicType::CreatureSpellItchy:
		return MiracleKind::Creature;
	default:
		return std::nullopt;
	}
}

uint32_t MiraclesCast(std::span<const uint32_t> castsOfType, MiracleKind kind)
{
	uint32_t total = 0;
	for (size_t type = 0; type < castsOfType.size(); ++type)
	{
		if (MiracleKindOf(static_cast<MagicType>(type)) == kind)
		{
			total += castsOfType[type];
		}
	}
	return total;
}

float AverageDesire(const SatisfactionDesires& desires, float divisor)
{
	const float buildings = std::max(desires.abodes, desires.civic);
	const float safety = std::max(desires.protection, desires.mercy);
	return (desires.food + desires.food + desires.wood + safety + buildings) / divisor;
}

uint32_t TotalBuildings(std::span<const TownCounts> towns)
{
	uint32_t abodes = 0;
	uint32_t civic = 0;
	uint32_t total = 0;
	for (const auto& town : towns)
	{
		civic += town.civic;
		abodes += town.abodes;
		total += abodes + civic;
	}
	return total;
}

int32_t Percent(float share)
{
	return static_cast<int32_t>(share * 100.0f);
}

WorldFigures World(const WorldInputs& inputs)
{
	WorldFigures figures;
	uint32_t men = 0;
	uint32_t women = 0;
	uint32_t wonders = 0;
	std::array<uint32_t, 13> disciples {};
	for (const auto& town : inputs.towns)
	{
		men += town.men;
		women += town.women;
		wonders += town.wonders;
		for (size_t i = 0; i < disciples.size(); ++i)
		{
			disciples.at(i) += town.disciples.at(i);
		}
	}
	const uint32_t believers = men + women;
	figures.population = static_cast<int32_t>(inputs.worldPopulation);
	figures.believersPercent = inputs.worldPopulation != 0 && believers != 0
	                               ? Percent(static_cast<float>(believers) / static_cast<float>(inputs.worldPopulation))
	                               : 0;
	figures.malePercent = believers != 0 ? Percent(static_cast<float>(men) / static_cast<float>(believers)) : 0;
	figures.deaths = static_cast<int32_t>(inputs.deaths);
	figures.births = static_cast<int32_t>(inputs.births);
	figures.sacrifices = static_cast<int32_t>(inputs.sacrifices);
	figures.buildings = static_cast<int32_t>(TotalBuildings(inputs.towns));
	figures.wonders = static_cast<int32_t>(wonders);
	// Every kind of disciple in the player's towns but their missionaries, who count where they preach instead
	constexpr auto k_Missionary = static_cast<size_t>(VillagerDisciple::Missionary);
	uint32_t total = inputs.missionariesInOtherTowns;
	for (size_t i = 0; i < disciples.size(); ++i)
	{
		if (i != k_Missionary)
		{
			total += disciples.at(i);
		}
	}
	figures.disciples = static_cast<int32_t>(total);
	const auto kind = [&disciples](VillagerDisciple disciple) {
		return static_cast<int32_t>(disciples.at(static_cast<size_t>(disciple)));
	};
	figures.discipleKinds = {
	    kind(VillagerDisciple::Builder),   kind(VillagerDisciple::Breeder),  kind(VillagerDisciple::Fisherman),
	    kind(VillagerDisciple::Farmer),    kind(VillagerDisciple::Forester), static_cast<int32_t>(inputs.missionaries),
	    kind(VillagerDisciple::Craftsman), kind(VillagerDisciple::Trader),
	};
	return figures;
}

} // namespace openblack::statistics
