/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Statistics.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ECS/Components/Alignment.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Registry.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/TownDesire.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// Counts something for a player, when the player is in the game
template <typename Count>
void Update(ecs::Registry& registry, PlayerNames name, Count&& count)
{
	registry.Each<const Player, PlayerStatistics>([name, &count](const Player& player, PlayerStatistics& statistics) {
		if (player.name == name)
		{
			count(statistics);
		}
	});
}

/// The player's creature, the first they got, none when they have none
const Creature* CreatureOf(PlayerNames name)
{
	if (!Locator::playerSystem::has_value())
	{
		return nullptr;
	}
	const auto creature = Locator::playerSystem::value().GetPrimaryCreature(name);
	const auto& registry = Entities();
	return creature.has_value() && registry.Valid(*creature) ? registry.TryGet<const Creature>(*creature) : nullptr;
}

/// What each of a player's towns has now, in the order the player gained them
std::vector<statistics::TownCounts> TownsOf(PlayerNames name)
{
	std::vector<std::pair<uint32_t, statistics::TownCounts>> towns;
	Entities().Each<const Town, const TownStats>([name, &towns](const Town& town, const TownStats& stats) {
		if (town.owner != name)
		{
			return;
		}
		statistics::TownCounts counts {
		    .men = stats.men,
		    .women = stats.women,
		    .abodes = stats.abodesWithPlaces,
		    .civic = stats.civicBuildings,
		    .wonders = stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::Wonder)),
		};
		std::ranges::copy(stats.disciples, counts.disciples.begin());
		towns.emplace_back(town.gained, counts);
	});
	std::ranges::sort(towns, {}, &std::pair<uint32_t, statistics::TownCounts>::first);
	std::vector<statistics::TownCounts> counts;
	counts.reserve(towns.size());
	std::ranges::transform(towns, std::back_inserter(counts), &std::pair<uint32_t, statistics::TownCounts>::second);
	return counts;
}

/// Each town with a player weighs how satisfied its people are
void SampleSatisfaction()
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const float divisor = Locator::infoConstants::value().town.divisorForAverageDesires;
	Entities().Each<const Town, const TownDesire>([divisor](const Town& town, const TownDesire& desire) {
		const auto raw = [&desire](TownDesireInfo d) { return ecs::town_desire::GetRawDesire(desire, static_cast<size_t>(d)); };
		const statistics::SatisfactionDesires desires {
		    .food = raw(TownDesireInfo::ForFood),
		    .wood = raw(TownDesireInfo::ForWood),
		    .protection = raw(TownDesireInfo::ForProtection),
		    .mercy = raw(TownDesireInfo::ForMercy),
		    .abodes = raw(TownDesireInfo::ForAbodes),
		    .civic = raw(TownDesireInfo::ForCivicBuilding),
		};
		const float average = statistics::AverageDesire(desires, divisor);
		Update(Entities(), town.owner, [average](PlayerStatistics& statistics) { statistics.satisfaction.Sample(average); });
	});
}

/// The belief in the player summed over every town
float BeliefIn(PlayerNames name)
{
	float total = 0.0f;
	const auto index = static_cast<size_t>(name);
	Entities().Each<const Town, const TownImpression>([index, &total](const Town&, const TownImpression& impression) {
		if (index < impression.belief.belief.size())
		{
			total += impression.belief.belief.at(index);
		}
	});
	return total;
}

} // namespace

void player_statistics::Made(entt::entity player)
{
	auto& registry = Entities();
	const auto* record = registry.TryGet<const Player>(player);
	if (record == nullptr)
	{
		return;
	}
	auto* kept = registry.TryGet<PlayerStatistics>(player);
	auto& statistics = kept != nullptr ? *kept : registry.Assign<PlayerStatistics>(player);
	if (const auto* alignment = registry.TryGet<const Alignment>(player))
	{
		statistics.startAlignment = alignment->value;
	}
	if (const auto* creature = CreatureOf(record->name))
	{
		statistics.startCreatureAlignment = creature->alignment;
	}
}

void player_statistics::CountBirth(ecs::Registry& registry, PlayerNames player)
{
	Update(registry, player, [](PlayerStatistics& statistics) { ++statistics.births; });
}

void player_statistics::CountDeath(ecs::Registry& registry, PlayerNames owner, PlayerNames killer)
{
	Update(registry, owner, [](PlayerStatistics& statistics) { ++statistics.deaths; });
	Update(registry, killer, [](PlayerStatistics& statistics) { ++statistics.peopleKilled; });
}

void player_statistics::CountSacrifice(ecs::Registry& registry, PlayerNames player)
{
	Update(registry, player, [](PlayerStatistics& statistics) { ++statistics.sacrifices; });
}

void player_statistics::CountBuilt(ecs::Registry& registry, PlayerNames player, AbodeType type)
{
	Update(registry, player, [type](PlayerStatistics& statistics) { statistics::CountBuilt(statistics.built, type); });
}

void player_statistics::CountDestroyed(ecs::Registry& registry, PlayerNames player)
{
	Update(registry, player, [](PlayerStatistics& statistics) { ++statistics.buildingsDestroyed; });
}

void player_statistics::CountDiscipleMade(ecs::Registry& registry, PlayerNames player, VillagerDisciple kind)
{
	const auto index = statistics::DiscipleMadeIndex(kind);
	if (!index.has_value())
	{
		return;
	}
	Update(registry, player, [index](PlayerStatistics& statistics) { ++statistics.disciplesMade.at(*index); });
}

void player_statistics::CountFoodUsed(ecs::Registry& registry, PlayerNames player, uint32_t amount)
{
	Update(registry, player, [amount](PlayerStatistics& statistics) { statistics.foodUsed += amount; });
}

void player_statistics::CountWoodUsed(ecs::Registry& registry, PlayerNames player, uint32_t amount)
{
	Update(registry, player, [amount](PlayerStatistics& statistics) { statistics.woodUsed += amount; });
}

void player_statistics::CountConverted(ecs::Registry& registry, PlayerNames player, uint32_t people)
{
	Update(registry, player, [people](PlayerStatistics& statistics) { statistics.peopleConverted += people; });
}

void player_statistics::CountCast(ecs::Registry& registry, PlayerNames player, MagicType type)
{
	const auto index = static_cast<size_t>(type);
	Update(registry, player, [index](PlayerStatistics& statistics) {
		if (index < statistics.castsOfType.size())
		{
			++statistics.castsOfType.at(index);
		}
	});
}

void player_statistics::CountChantsUsed(ecs::Registry& registry, PlayerNames player, float amount)
{
	Update(registry, player, [amount](PlayerStatistics& statistics) { statistics.chantsUsed += amount; });
}

void player_statistics::ProcessTurn(uint32_t turn)
{
	auto& registry = Entities();
	registry.Each<const Player, PlayerStatistics>([&](const Player& player, PlayerStatistics& statistics) {
		// The people of the player's towns, men and women of any age
		uint32_t men = 0;
		uint32_t women = 0;
		registry.Each<const Town, const TownStats>([&](const Town& town, const TownStats& stats) {
			if (town.owner == player.name)
			{
				men += stats.men;
				women += stats.women;
			}
		});
		statistics.population.Check(men + women);
		statistics.men.Check(men);
		statistics.women.Check(women);
		statistics.populationHistory.Add(static_cast<float>(men + women));
		if (Locator::influenceSystem::has_value())
		{
			statistics.influenceHistory.Add(Locator::influenceSystem::value().InfluencePower(player.name));
		}
		if (turn % statistics::k_BeliefCheckEvery == 0)
		{
			statistics.belief.Check(BeliefIn(player.name));
		}
		// The player's creature's size is noted the first time it is seen on the land
		if (statistics.startCreatureSize == 0.0f)
		{
			if (const auto* creature = CreatureOf(player.name))
			{
				statistics.startCreatureSize = creature->size;
			}
		}
	});
	if (turn % statistics::k_SatisfactionEvery == 0)
	{
		SampleSatisfaction();
	}
}

statistics::WorldFigures player_statistics::World(PlayerNames player)
{
	auto& registry = Entities();
	const auto towns = TownsOf(player);
	statistics::WorldInputs inputs {.towns = towns};
	// Every villager in the world, of any town or none, until it starts to die
	registry.Each<const Villager>([&registry, &inputs](entt::entity villager, const Villager&) {
		if (!registry.AllOf<VillagerDeath>(villager))
		{
			++inputs.worldPopulation;
		}
	});
	registry.Each<const MissionaryControl>([&registry, &inputs, player](const MissionaryControl& missionary) {
		// A missionary counts in the town it preaches in
		const auto* town = registry.Valid(missionary.town) ? registry.TryGet<const Town>(missionary.town) : nullptr;
		if (missionary.player != player || town == nullptr)
		{
			return;
		}
		++inputs.missionaries;
		if (town->owner != player)
		{
			++inputs.missionariesInOtherTowns;
		}
	});
	Update(registry, player, [&inputs](const PlayerStatistics& statistics) {
		inputs.deaths = statistics.deaths;
		inputs.births = statistics.births;
		inputs.sacrifices = statistics.sacrifices;
	});
	return statistics::World(inputs);
}
