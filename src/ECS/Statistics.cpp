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
#include <array>
#include <span>
#include <utility>
#include <vector>

#include "Common/GameRandom.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureTallies.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/TownDesire.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "LHVM.h"
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

/// The belief in the player summed over every town, in the order of the game's list of all the towns
float BeliefIn(PlayerNames name)
{
	float total = 0.0f;
	const auto index = static_cast<size_t>(name);
	const auto& registry = Entities();
	for (const auto town : ecs::player_towns::AllMadeNewestFirst(registry))
	{
		const auto* impression = registry.TryGet<const TownImpression>(town);
		if (impression != nullptr && index < impression->belief.belief.size())
		{
			total += impression->belief.belief.at(index);
		}
	}
	return total;
}

} // namespace

void player_statistics::GameStarts(ecs::Registry& registry)
{
	registry.Each<const Player>([&registry](entt::entity entity, const Player& player) {
		auto* kept = registry.TryGet<PlayerStatistics>(entity);
		auto& statistics = kept != nullptr ? *kept : registry.Assign<PlayerStatistics>(entity);
		const auto* alignment = registry.TryGet<const Alignment>(entity);
		statistics.startAlignment = alignment != nullptr ? alignment->value : 0.0f;
		const auto* creature = CreatureOf(player.name);
		statistics.startCreatureAlignment = creature != nullptr ? creature->alignment : 0.0f;
	});
	uint32_t towns = 0;
	registry.Each<const Town>([&towns](const Town&) { ++towns; });
	registry.Context().globalStatistics.townsAtStart = towns;
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

void player_statistics::CountInfluence(ecs::Registry& registry, PlayerNames player, float power)
{
	Update(registry, player, [power](PlayerStatistics& statistics) { statistics.influenceHistory.Add(power); });
}

void player_statistics::CheckBelief(uint32_t turn)
{
	if (turn % statistics::k_BeliefCheckEvery != 0)
	{
		return;
	}
	Entities().Each<const Player, PlayerStatistics>(
	    [](const Player& player, PlayerStatistics& statistics) { statistics.belief.Check(BeliefIn(player.name)); });
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

void player_statistics::ProcessGlobalTurn(uint32_t turn, int32_t frameRate)
{
	auto& global = Entities().Context().globalStatistics;
	if (turn % statistics::k_GlobalFiguresEvery != 0 || !Locator::gameRandom::has_value())
	{
		return;
	}
	statistics::StepGlobalFigures(global, frameRate, [](int32_t n) { return Locator::gameRandom::value().LocalRand(n); });
}

namespace
{
/// What the player's towns have now, for the box
void TownFigures(PlayerNames name, statistics::PlayerFigures& figures)
{
	const auto towns = TownsOf(name);
	figures.totalBuildings = statistics::TotalBuildings(towns);
	for (const auto& town : towns)
	{
		figures.men += town.men;
		figures.women += town.women;
		figures.abodes += town.abodes;
		figures.civic += town.civic;
		figures.wonders += town.wonders;
	}
	figures.towns = static_cast<uint32_t>(towns.size());
	Entities().Each<const Town, const TownStats>([name, &figures](const Town& town, const TownStats& stats) {
		if (town.owner == name)
		{
			figures.population += stats.adults + stats.children;
			// The places for adults and for children in its homes
			figures.capacity += stats.adultPlaces + stats.childPlaces;
		}
	});
}

/// What the player's creature, the first they got, has done and is now; nothing without one
void CreatureFigures(PlayerNames name, const PlayerStatistics& statistics, statistics::PlayerFigures& figures)
{
	if (!Locator::playerSystem::has_value())
	{
		return;
	}
	const auto entity = Locator::playerSystem::value().GetPrimaryCreature(name);
	auto& registry = Entities();
	if (!entity.has_value() || !registry.Valid(*entity))
	{
		return;
	}
	if (const auto* creature = registry.TryGet<const Creature>(*entity))
	{
		figures.creatureAlignment = creature->alignment;
		figures.creatureAlignmentChange = creature->alignment - statistics.startCreatureAlignment;
		if (statistics.startCreatureSize > 0.0f)
		{
			figures.creatureGrowth = (creature->size / statistics.startCreatureSize) - 1.0f;
		}
	}
	constexpr float k_Hundred = 100.0f;
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(*entity))
	{
		figures.creatureDamage = (1.0f - needs->needs.life) * k_Hundred;
		figures.creatureHunger = (1.0f - needs->needs.energy) * k_Hundred;
		figures.creatureTiredness = needs->needs.exhaustion * k_Hundred;
		figures.creatureAge = needs->needs.age;
	}
	figures.creature = player_statistics::CreatureTalliesOf(name);
}

statistics::PlayerFigures FiguresOf(PlayerNames name, entt::entity entity, const PlayerStatistics& statistics)
{
	auto& registry = Entities();
	statistics::PlayerFigures figures {.player = name};
	constexpr float k_BeliefScale = 1000.0f;
	figures.influence = Locator::influenceSystem::has_value() ? Locator::influenceSystem::value().InfluencePower(name) : 0.0f;
	figures.godsDefeated = statistics.godsDefeated;
	if (const auto* alignment = registry.TryGet<const Alignment>(entity))
	{
		figures.alignmentChange = alignment->value - statistics.startAlignment;
	}
	figures.peopleKilled = statistics.peopleKilled;
	figures.belief = BeliefIn(name) * k_BeliefScale;
	figures.maxBelief = statistics.belief.highest * k_BeliefScale;
	figures.minBelief = statistics.belief.lowest * k_BeliefScale;
	// The artefacts among any town's that the player made or last took
	registry.Each<const TownArtefact>([name, &figures](const TownArtefact& artefact) {
		if (artefact.inTown && artefact.player == name)
		{
			++figures.artefacts;
		}
	});
	figures.buildingsDestroyed = statistics.buildingsDestroyed;
	CreatureFigures(name, statistics, figures);

	figures.births = statistics.births;
	figures.deaths = statistics.deaths;
	TownFigures(name, figures);
	figures.populationExtremes = statistics.population;
	figures.menExtremes = statistics.men;
	figures.womenExtremes = statistics.women;
	figures.converted = statistics.peopleConverted;
	figures.satisfaction = statistics.satisfaction.Average();
	figures.built = statistics.built;
	figures.disciplesMade = statistics.disciplesMade;
	figures.foodUsed = statistics.foodUsed;
	figures.woodUsed = statistics.woodUsed;

	figures.castsOfType = statistics.castsOfType;
	figures.chantsUsed = statistics.chantsUsed;
	figures.sacrifices = statistics.sacrifices;

	const auto influence = statistics.influenceHistory.Finished();
	figures.influenceHistory.assign(influence.begin(), influence.end());
	const auto population = statistics.populationHistory.Finished();
	figures.populationHistory.assign(population.begin(), population.end());
	return figures;
}

/// Counts something of a creature's
template <typename Count>
void Tally(ecs::Registry& registry, entt::entity creature, Count&& count)
{
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature))
	{
		return;
	}
	auto* tallies = registry.TryGet<CreatureTallies>(creature);
	count(tallies != nullptr ? *tallies : registry.Assign<CreatureTallies>(creature));
}
} // namespace

statistics::BoxFigures player_statistics::Box()
{
	auto& registry = Entities();
	statistics::BoxFigures box;
	auto& global = registry.Context().globalStatistics;
	// When the box takes the figures, the lines of code increase one more step
	if (Locator::gameRandom::has_value())
	{
		global.linesOfCode += statistics::LinesOfCodeStep(global.townsAtStart,
		                                                  [](int32_t n) { return Locator::gameRandom::value().LocalRand(n); });
	}
	// The box counts the players that play among its places, then shows that many places from the first
	const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
	const auto playerNumbers =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().PlayerNumbersInGame() : player_slots::k_LandSlots;
	const auto places = statistics::BoxPlaces(networkGame, playerNumbers);
	std::array<bool, player_slots::k_MaxPlayers> plays {};
	for (size_t place = 0; place < places.size() && place < plays.size(); ++place)
	{
		plays.at(place) = Locator::playerSystem::has_value() && Locator::playerSystem::value().PlaysInGame(places.at(place));
	}
	box.playersInGame = statistics::BoxPlayerCount(std::span(plays).first(std::min(places.size(), plays.size())));
	for (uint32_t place = 0; place < box.playersInGame && place < places.size(); ++place)
	{
		const auto name = places.at(place);
		const auto entity = Locator::playerSystem::has_value()
		                        ? Locator::playerSystem::value().FindPlayer(name).value_or(entt::null)
		                        : entt::null;
		const auto* statistics = registry.Valid(entity) ? registry.TryGet<const PlayerStatistics>(entity) : nullptr;
		if (statistics != nullptr)
		{
			box.players.push_back(FiguresOf(name, entity, *statistics));
		}
	}
	box.landNumber = registry.Context().mapScriptGlobals.landNumber;

	auto& tech = box.tech;
	tech.linesOfCode = global.linesOfCode;
	tech.maxFrameRate = global.maxFrameRate;
	tech.minFrameRate = global.minFrameRate;
	// Every entity made from the land's start, those destroyed since too
	tech.objectsCreated = registry.GetEntitiesMade();
	tech.timePlayed = Locator::time::has_value() ? statistics::TimePlayed(Locator::time::value().GetTurn()) : 0;
	tech.scriptInstructions = Locator::vm::has_value() ? Locator::vm::value().GetExecutedInstructions() : 0;
	if (Locator::helpProfileSystem::has_value())
	{
		const auto& profile = std::as_const(Locator::helpProfileSystem::value()).Get();
		tech.help = statistics::HelpFiguresFrom(
		    [&profile](statistics::HelpEvent event) { return profile.Count(static_cast<uint32_t>(event)).Total(); });
	}
	return box;
}

void player_statistics::CountCreatureMushroom(ecs::Registry& registry, entt::entity creature)
{
	Tally(registry, creature, [](CreatureTallies& tallies) { ++tallies.mushroomsEaten; });
}

void player_statistics::CountCreaturePoo(ecs::Registry& registry, entt::entity creature)
{
	Tally(registry, creature, [](CreatureTallies& tallies) { ++tallies.poos; });
}

statistics::CreatureTallies player_statistics::CreatureTalliesOf(PlayerNames player)
{
	statistics::CreatureTallies tallies;
	if (!Locator::playerSystem::has_value())
	{
		return tallies;
	}
	const auto creature = Locator::playerSystem::value().GetPrimaryCreature(player);
	auto& registry = Entities();
	if (!creature.has_value() || !registry.Valid(*creature))
	{
		return tallies;
	}
	if (const auto* kept = registry.TryGet<const CreatureTallies>(*creature))
	{
		tallies.peopleKilled = kept->meals.peopleKilled;
		tallies.animalsKilled = kept->meals.animalsKilled;
		tallies.creaturesKilled = kept->meals.creaturesKilled;
		tallies.poos = kept->poos;
		tallies.mushroomsEaten = kept->mushroomsEaten;
	}
	if (const auto* record = registry.TryGet<const CreatureFightRecord>(*creature))
	{
		tallies.battlesFought = record->fights;
		tallies.battlesWon = record->wins;
	}
	return tallies;
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
