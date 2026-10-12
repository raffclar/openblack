/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/StatisticsRules.h"

using namespace openblack;
using namespace openblack::statistics;

namespace
{

/// A town of the player's with so many men and women, homes, civic buildings and wonders
TownCounts Town(uint32_t men, uint32_t women, uint32_t abodes = 0, uint32_t civic = 0, uint32_t wonders = 0)
{
	return {.men = men, .women = women, .abodes = abodes, .civic = civic, .wonders = wonders};
}

} // namespace

TEST(StatisticsExtremes, ANewHighestIsNotTestedAsALowest)
{
	Extremes<uint32_t> population;
	population.Check(5);
	population.Check(7);
	// Only ever growing, the lowest stays at its start
	EXPECT_EQ(population.highest, 7u);
	EXPECT_EQ(population.lowest, std::numeric_limits<uint32_t>::max());
	population.Check(3);
	EXPECT_EQ(population.lowest, 3u);
	EXPECT_EQ(population.highest, 7u);
}

TEST(StatisticsExtremes, BeliefStartsWithTheLargestLowest)
{
	Extremes<float> belief;
	EXPECT_EQ(belief.highest, 0.0f);
	EXPECT_EQ(belief.lowest, std::numeric_limits<float>::max());
	// Never above nothing, the first check is a lowest
	belief.Check(0.0f);
	EXPECT_EQ(belief.lowest, 0.0f);
}

TEST(StatisticsHistory, EachPointAveragesFiftySamples)
{
	History history;
	for (int i = 0; i < 50; ++i)
	{
		history.Add(10.0f);
	}
	history.Add(4.0f);
	ASSERT_EQ(history.Finished().size(), 1u);
	EXPECT_NEAR(history.Finished()[0], 10.0f, 1e-4f);
	// The point still being filled isn't finished
	EXPECT_NEAR(history.points[1], 4.0f / 50.0f, 1e-6f);
}

TEST(StatisticsHistory, WhenFullThePairsAreAveragedAndPointsTakeTwiceAsLong)
{
	History history;
	for (size_t point = 0; point < History::k_Points; ++point)
	{
		for (int i = 0; i < 50; ++i)
		{
			history.Add(static_cast<float>(point % 2 == 0 ? 2 : 4));
		}
	}
	EXPECT_EQ(history.index, 250u);
	EXPECT_EQ(history.samplesPerPoint, 100);
	EXPECT_NEAR(history.points[0], 3.0f, 1e-4f);
	EXPECT_NEAR(history.points[249], 3.0f, 1e-4f);
	// As the game clears them, the points just after the middle are emptied but the last ones keep their old values
	EXPECT_EQ(history.points[250], 0.0f);
	EXPECT_EQ(history.points[311], 0.0f);
	EXPECT_NEAR(history.points[313], 4.0f, 1e-4f);
	EXPECT_NEAR(history.points[499], 4.0f, 1e-4f);
	// The new samples go on top of what was left
	for (int i = 0; i < 100; ++i)
	{
		history.Add(1.0f);
	}
	EXPECT_NEAR(history.points[250], 1.0f, 1e-4f);
}

TEST(StatisticsBuildings, EachBuildingCountsForTheFirstOfItsKinds)
{
	BuildingsBuilt built;
	CountBuilt(built, AbodeType::Windmill);
	CountBuilt(built, AbodeType::StoragePit);
	CountBuilt(built, AbodeType::Wonder);
	CountBuilt(built, AbodeType::General);
	EXPECT_EQ(built.total, 4u);
	EXPECT_EQ(built.abodes, 1u);
	EXPECT_EQ(built.civic, 1u);
	EXPECT_EQ(built.wonders, 1u);
}

TEST(StatisticsBuildings, EarlierTownsCountAgainInTheTotal)
{
	const std::array towns {Town(0, 0, 3, 1), Town(0, 0, 2, 2)};
	// First town: 3 + 1; second: (3 + 2) + (1 + 2)
	EXPECT_EQ(TotalBuildings(towns), 4u + 8u);
	EXPECT_EQ(TotalBuildings(std::span(towns).first(1)), 4u);
}

TEST(StatisticsBuildings, ABuildingAgesOnItsTownsPassesUpToSettled)
{
	EXPECT_TRUE(TownPassesItsBuildings(0, 10));
	EXPECT_FALSE(TownPassesItsBuildings(15, 10));
	EXPECT_TRUE(TownPassesItsBuildings(30, 10));
	uint32_t age = 0;
	for (int pass = 0; pass < 250; ++pass)
	{
		age = AgedOnePass(age);
	}
	EXPECT_EQ(age, k_SettledAge);
}

TEST(StatisticsBuildings, OnlyHarmTakingItOutOfActionStopsItWorking)
{
	// From above the share to at or under it
	EXPECT_TRUE(StopsWorking(0.6f, 0.5f, 0.5f));
	EXPECT_TRUE(StopsWorking(1.0f, 0.0f, 0.5f));
	// Already out of action, or still above the share
	EXPECT_FALSE(StopsWorking(0.5f, 0.2f, 0.5f));
	EXPECT_FALSE(StopsWorking(1.0f, 0.7f, 0.5f));
	// Left with all its life
	EXPECT_FALSE(StopsWorking(1.0f, 1.0f, 1.0f));
}

TEST(StatisticsBuildings, OnlyASettledBuildingAPlayerPutsOutCountsAsDestroyed)
{
	EXPECT_TRUE(CountsAsDestroyed(true, k_SettledAge));
	EXPECT_FALSE(CountsAsDestroyed(true, k_SettledAge - 1));
	EXPECT_FALSE(CountsAsDestroyed(false, k_SettledAge));
}

TEST(StatisticsBuildings, CivicBuildingsLeaveOutTemplesAndFields)
{
	EXPECT_TRUE(IsCivicBuilding(AbodeType::StoragePit));
	EXPECT_TRUE(IsCivicBuilding(AbodeType::TownCentre));
	EXPECT_TRUE(IsCivicBuilding(AbodeType::Wonder));
	EXPECT_TRUE(IsCivicBuilding(AbodeType::SpellDispenser));
	EXPECT_FALSE(IsCivicBuilding(AbodeType::Citadel));
	EXPECT_FALSE(IsCivicBuilding(AbodeType::Field));
	EXPECT_FALSE(IsCivicBuilding(AbodeType::LivingQuarters));
	EXPECT_FALSE(IsCivicBuilding(AbodeType::Windmill));
}

TEST(StatisticsDisciples, OnlyTheEightWorkingKindsAreCounted)
{
	EXPECT_EQ(DiscipleMadeIndex(VillagerDisciple::Farmer), 0u);
	EXPECT_EQ(DiscipleMadeIndex(VillagerDisciple::Craftsman), 7u);
	EXPECT_EQ(DiscipleMadeIndex(VillagerDisciple::Missionary), 6u);
	EXPECT_FALSE(DiscipleMadeIndex(VillagerDisciple::Protection).has_value());
	EXPECT_FALSE(DiscipleMadeIndex(VillagerDisciple::Worship).has_value());
	EXPECT_FALSE(DiscipleMadeIndex(VillagerDisciple::None).has_value());
}

TEST(StatisticsMiracles, EachTypeCountsTowardsItsTotal)
{
	EXPECT_EQ(MiracleKindOf(MagicType::FlockGround), MiracleKind::Aggressive);
	EXPECT_EQ(MiracleKindOf(MagicType::FlockFlying), MiracleKind::Nice);
	EXPECT_EQ(MiracleKindOf(MagicType::CreatureSpellItchy), MiracleKind::Creature);
	EXPECT_FALSE(MiracleKindOf(MagicType::CreatureSpellHungry).has_value());
	EXPECT_FALSE(MiracleKindOf(MagicType::None).has_value());
	std::array<uint32_t, 42> casts {};
	casts.at(static_cast<size_t>(MagicType::Fireball)) = 2;
	casts.at(static_cast<size_t>(MagicType::Tornado)) = 1;
	casts.at(static_cast<size_t>(MagicType::Water)) = 3;
	casts.at(static_cast<size_t>(MagicType::CreatureSpellThirsty)) = 5;
	EXPECT_EQ(MiraclesCast(casts, MiracleKind::Aggressive), 3u);
	EXPECT_EQ(MiraclesCast(casts, MiracleKind::Nice), 3u);
	EXPECT_EQ(MiraclesCast(casts, MiracleKind::Creature), 0u);
}

TEST(StatisticsSatisfaction, FoodWeighsTwiceAndTheGreaterOfEachPair)
{
	const SatisfactionDesires desires {
	    .food = 0.5f, .wood = 0.2f, .protection = 0.1f, .mercy = 0.3f, .abodes = 0.4f, .civic = 0.0f};
	EXPECT_FLOAT_EQ(AverageDesire(desires, 4.0f), (1.0f + 0.2f + 0.3f + 0.4f) / 4.0f);
	Satisfaction satisfaction;
	EXPECT_EQ(satisfaction.Average(), 0.0f);
	satisfaction.Sample(0.25f);
	satisfaction.Sample(0.75f);
	EXPECT_FLOAT_EQ(satisfaction.Average(), 0.5f);
}

TEST(StatisticsWorld, SharesAreWholePercentagesRoundedDown)
{
	EXPECT_EQ(Percent(0.999f), 99);
	EXPECT_EQ(Percent(1.0f / 3.0f), 33);
	EXPECT_EQ(Percent(0.0f), 0);
}

TEST(StatisticsWorld, TheScrollIsWorkedOutFromThePlayersTowns)
{
	auto first = Town(3, 5, 4, 2, 1);
	first.disciples.at(static_cast<size_t>(VillagerDisciple::Builder)) = 2;
	first.disciples.at(static_cast<size_t>(VillagerDisciple::Worship)) = 3;
	first.disciples.at(static_cast<size_t>(VillagerDisciple::Protection)) = 1;
	// A missionary living in the player's own town counts where it preaches
	first.disciples.at(static_cast<size_t>(VillagerDisciple::Missionary)) = 1;
	auto second = Town(2, 0, 1, 0, 0);
	second.disciples.at(static_cast<size_t>(VillagerDisciple::Trader)) = 1;
	const std::array towns {first, second};
	const auto figures = World({
	    .worldPopulation = 30,
	    .towns = towns,
	    .missionariesInOtherTowns = 1,
	    .missionaries = 2,
	    .deaths = 4,
	    .births = 6,
	    .sacrifices = 1,
	});
	EXPECT_EQ(figures.population, 30);
	// 10 of 30 believe, 5 of 10 are men
	EXPECT_EQ(figures.believersPercent, 33);
	EXPECT_EQ(figures.malePercent, 50);
	EXPECT_EQ(figures.deaths, 4);
	EXPECT_EQ(figures.births, 6);
	EXPECT_EQ(figures.sacrifices, 1);
	EXPECT_EQ(figures.buildings, (4 + 2) + (5 + 2));
	EXPECT_EQ(figures.wonders, 1);
	// Builders 2, worshippers 3, protectors 1 and a trader, and the missionary in another town
	EXPECT_EQ(figures.disciples, 8);
	EXPECT_EQ(figures.discipleKinds, (std::array<int32_t, 8> {2, 0, 0, 0, 0, 2, 0, 1}));
}

TEST(StatisticsWorld, NobodyAnywhereGivesNoShares)
{
	const auto figures = World({});
	EXPECT_EQ(figures.believersPercent, 0);
	EXPECT_EQ(figures.malePercent, 0);
	EXPECT_EQ(figures.buildings, 0);
}

TEST(PlayerStatistics, CountsGoToTheNamedPlayer)
{
	ecs::Registry registry;
	const auto one = registry.Create();
	registry.Assign<ecs::components::Player>(one, ecs::components::Player {.name = PlayerNames::PLAYER_ONE});
	registry.Assign<ecs::components::PlayerStatistics>(one);
	const auto neutral = registry.Create();
	registry.Assign<ecs::components::Player>(neutral, ecs::components::Player {.name = PlayerNames::NEUTRAL});
	registry.Assign<ecs::components::PlayerStatistics>(neutral);

	player_statistics::CountBirth(registry, PlayerNames::PLAYER_ONE);
	player_statistics::CountDeath(registry, PlayerNames::PLAYER_ONE, PlayerNames::NEUTRAL);
	player_statistics::CountSacrifice(registry, PlayerNames::PLAYER_ONE);
	player_statistics::CountBuilt(registry, PlayerNames::PLAYER_ONE, AbodeType::Graveyard);
	player_statistics::CountDiscipleMade(registry, PlayerNames::PLAYER_ONE, VillagerDisciple::Forester);
	player_statistics::CountDiscipleMade(registry, PlayerNames::PLAYER_ONE, VillagerDisciple::Worship);
	player_statistics::CountFoodUsed(registry, PlayerNames::PLAYER_ONE, 7);
	player_statistics::CountWoodUsed(registry, PlayerNames::PLAYER_ONE, 9);
	player_statistics::CountConverted(registry, PlayerNames::PLAYER_ONE, 12);
	player_statistics::CountCast(registry, PlayerNames::PLAYER_ONE, MagicType::Heal);
	player_statistics::CountChantsUsed(registry, PlayerNames::PLAYER_ONE, 2.5f);
	// A player not in the game counts nothing
	player_statistics::CountBirth(registry, PlayerNames::PLAYER_TWO);

	const auto& stats = registry.Get<const ecs::components::PlayerStatistics>(one);
	EXPECT_EQ(stats.births, 1u);
	EXPECT_EQ(stats.deaths, 1u);
	EXPECT_EQ(stats.peopleKilled, 0u);
	EXPECT_EQ(stats.sacrifices, 1u);
	EXPECT_EQ(stats.built.total, 1u);
	EXPECT_EQ(stats.built.civic, 1u);
	EXPECT_EQ(stats.disciplesMade.at(1), 1u);
	EXPECT_EQ(std::accumulate(stats.disciplesMade.begin(), stats.disciplesMade.end(), 0u), 1u);
	EXPECT_EQ(stats.foodUsed, 7u);
	EXPECT_EQ(stats.woodUsed, 9u);
	EXPECT_EQ(stats.peopleConverted, 12u);
	EXPECT_EQ(stats.castsOfType.at(static_cast<size_t>(MagicType::Heal)), 1u);
	EXPECT_FLOAT_EQ(stats.chantsUsed, 2.5f);
	// The death was put down to nobody: the neutral player
	EXPECT_EQ(registry.Get<const ecs::components::PlayerStatistics>(neutral).peopleKilled, 1u);
}
