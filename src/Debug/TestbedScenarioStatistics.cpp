/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of the player's statistics counting as their town lives

#include <array>
#include <utility>
#include <vector>

#include "ECS/Components/PlayerStatistics.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// Where the town's village centre, storage pit, built hut and planned hut stand
constexpr glm::vec2 k_Centre {0.0f, 50.0f};
constexpr glm::vec2 k_Pit {-30.0f, 30.0f};
constexpr glm::vec2 k_Hut {25.0f, 30.0f};
constexpr glm::vec2 k_PlannedHut {5.0f, 5.0f};
/// Plenty of wood and food in the pit, so that building is never held up
constexpr uint32_t k_PitStock = 5000;
/// The town's couples, too many for its one hut
constexpr size_t k_Couples = 4;
constexpr float k_VillagerSpacing = 3.0f;
/// Two of the town's men stand apart, where the player's lightning strikes them
constexpr glm::vec2 k_Struck {45.0f, 0.0f};
/// The town's belief in the player, so that it stays theirs rather than going back to the neutral player
constexpr float k_TownBelief = 1000.0f;
} // namespace

namespace
{
/// Known figures for the Game Statistics box and the temple's scrolls, over what the land itself counts
void KnownStatistics(ecs::components::PlayerStatistics& statistics)
{
	statistics.births = 7;
	statistics.deaths = 3;
	statistics.peopleKilled = 2;
	statistics.sacrifices = 4;
	statistics.peopleConverted = 12;
	statistics.built = {.total = 5, .abodes = 3, .civic = 2, .wonders = 0};
	statistics.buildingsDestroyed = 1;
	// Farmers, foresters, fishermen, builders, breeders, traders, missionaries and craftsmen
	statistics.disciplesMade = {2, 1, 0, 3, 0, 0, 1, 0};
	statistics.foodUsed = 640;
	statistics.woodUsed = 1500;
	statistics.castsOfType.at(static_cast<size_t>(MagicType::Fireball)) = 3;
	statistics.castsOfType.at(static_cast<size_t>(MagicType::Heal)) = 2;
	statistics.castsOfType.at(static_cast<size_t>(MagicType::Forest)) = 1;
	statistics.castsOfType.at(static_cast<size_t>(MagicType::CreatureSpellFreeze)) = 1;
	statistics.chantsUsed = 12.5f;
	statistics.satisfaction = {.total = 6.0f, .samples = 10};
	statistics.population = {.highest = 24, .lowest = 16};
	statistics.men = {.highest = 13, .lowest = 8};
	statistics.women = {.highest = 11, .lowest = 8};
	statistics.belief = {.highest = 2.5f, .lowest = 0.5f};
	statistics.startAlignment = -0.2f;
	// Thirty finished points of each history: the population grows from 10 to 24, the influence from 100 to 390
	constexpr int k_Points = 30;
	for (int point = 0; point < k_Points; ++point)
	{
		for (uint16_t sample = 0; sample < statistics::History::k_FirstSamplesPerPoint; ++sample)
		{
			statistics.populationHistory.Add(10.0f + (static_cast<float>(point) * 14.0f / (k_Points - 1)));
			statistics.influenceHistory.Add(100.0f + (static_cast<float>(point) * 10.0f));
		}
	}
}

/// What the player's creature has counted of its life
constexpr statistics::CreatureTallies k_KnownTallies {
    .peopleKilled = 1, .animalsKilled = 3, .creaturesKilled = 0, .poos = 5, .mushroomsEaten = 2};

/// The player's town for the two scenarios: a village centre, a storage pit, two huts and four couples
std::vector<ObjectSetup> KnownTown()
{
	std::vector<ObjectSetup> objects {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	    {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = k_PitStock, .storedWood = k_PitStock},
	    {.type = AbodeInfo::CelticHut, .offset = k_Hut, .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHut, .yawDegrees = 180.0f},
	};
	for (size_t i = 0; i < k_Couples; ++i)
	{
		const float x = (static_cast<float>(i) - (static_cast<float>(k_Couples) * 0.5f)) * k_VillagerSpacing;
		objects.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, 20.0f}, .joinTown = true});
		objects.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, 17.0f}, .joinTown = true});
	}
	return objects;
}

void AddBoxScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "statistics.box",
	    .name = "The Game Statistics box with known figures",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with two huts and eight villagers, and the player's tiger. The "
	                   "player's statistics start with known figures, and the tiger's tallies too. Push Escape for the "
	                   "menu and pick its Statistics tab.",
	    .expected = "The box opens on Player Stats: the Influence and Population graphs show 30 points each, rising, and "
	                "the right box shows the Final Area Of Influence. With one player only the Total column is written, "
	                "grey, under a Total heading: People Killed 2, Min Total Belief 500, Buildings Destroyed 1, and the "
	                "Final and Max Total Belief the town's belief times 1000. Creature Stats: Animals Killed 3, Poos "
	                "Done 5, Mushrooms Eaten 2, and the tiger's box in red. Town Stats: Births 7, Deaths 3, People "
	                "Converted 12, Buildings Built 5, Food 640 and Wood 1500 until the town eats or builds, and the "
	                "disciples made 2, 1, 0, 3, 0, 0, 1, 0. Miracle Stats: aggressive "
	                "3, nice 3, creature 1, Fireballs 3, Heals 2, Forests 1, Freeze 1, Chants 12.5, Sacrifices 4. "
	                "Tech Stats: Total Objects Created is every entity made since the land started, and grows as "
	                "things are made; Total Number Of Statistics 113; Min Frame Rate 0.0. Numeric shows the numbers; "
	                "Graphic shows full red bars.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Centre, k_Hut, k_PlannedHut}, .distance = 0.6f},
	    .creatures = {{.species = CreatureType::Tiger, .offset = {40.0f, 40.0f}}},
	    .objects = KnownTown(),
	    .townBeliefInPlayer = k_TownBelief,
	    .playerStatistics = KnownStatistics,
	    .creatureTallies = k_KnownTallies,
	});
	all.push_back({
	    .id = "statistics.scrolls",
	    .name = "The temple's world scroll with known figures",
	    .facet = Facet::Story,
	    .description = "The same town and known figures as statistics.box, and a temple of the player's west of the "
	                   "middle. Three seconds in, the player is put in the temple's main room, by its world scroll.",
	    .expected = "The world scroll gives the villagers in the world, the share of them in the player's town and the "
	                "share of its people who are men, then Deaths 3, Births 7 and Sacrifices 4, the buildings (homes "
	                "and civic buildings) and the wonders (0), and the disciples now in the town.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {{.species = CreatureType::Tiger, .offset = {40.0f, 40.0f}}},
	    .objects = KnownTown(),
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Command::Kind::EnterTemple, .delaySeconds = 3.0f, .value = 3}},
	    .townBeliefInPlayer = k_TownBelief,
	    .playerStatistics = KnownStatistics,
	    .creatureTallies = k_KnownTallies,
	});
}
} // namespace

void openblack::testbed_scenarios::AddStatisticsScenarios(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	    {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = k_PitStock, .storedWood = k_PitStock},
	    {.type = AbodeInfo::CelticHut, .offset = k_Hut, .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_PlannedHut, .yawDegrees = 180.0f, .planned = true},
	};
	for (size_t i = 0; i < k_Couples; ++i)
	{
		const float x = (static_cast<float>(i) - (static_cast<float>(k_Couples) * 0.5f)) * k_VillagerSpacing;
		objects.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, 20.0f}, .joinTown = true});
		objects.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, 17.0f}, .joinTown = true});
	}
	objects.push_back({.type = VillagerInfo::CelticFarmerMale, .offset = k_Struck, .joinTown = true});
	objects.push_back({.type = VillagerInfo::CelticFarmerMale, .offset = k_Struck + glm::vec2(1.5f, 0.0f), .joinTown = true});
	all.push_back({
	    .id = "statistics.counters",
	    .name = "The player's statistics counting",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's: a village centre, a storage pit full of wood and food, one built "
	                   "hut and one planned, eight villagers too many for the hut, and two farmers standing apart to the "
	                   "south east. Three seconds in, the player casts a lightning bolt on the two farmers. Read the "
	                   "player's PlayerStatistics with the inspector (game.entity on the player). Best at game speed 4.",
	    .expected = "The lightning counts one cast of its magic type (castsOfType 4); each farmer it kills counts a "
	                "death for the player and one killed by the player (deaths, peopleKilled). The town starts the "
	                "planned hut and each builder's stroke adds the wood used (woodUsed); when it is finished, built "
	                "total and abodes go up by one. Every turn the population's most and fewest and the population "
	                "history take the town's people; every 50 turns the satisfaction gets a sample. At dinner the food "
	                "eaten counts in foodUsed.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Centre, k_Hut, k_PlannedHut, k_Struck}, .distance = 0.6f},
	    .objects = std::move(objects),
	    .miracles = {{.type = MagicType::LightningBolt,
	                  .point = k_Struck,
	                  .handOffset = k_Struck + glm::vec2(0.0f, -5.0f),
	                  .handHeight = 15.0f,
	                  .delaySeconds = 3.0f}},
	    .townBeliefInPlayer = k_TownBelief,
	});
	AddBoxScenarios(all);
	all.push_back({
	    .id = "statistics.creature_view",
	    .name = "The player's creature on the Creature Stats tab",
	    .facet = Facet::Objects,
	    .description = "The player's tiger, and the town of statistics.box. Push Escape for the menu, pick its "
	                   "Statistics tab and then the Creature Stats tab.",
	    .expected = "Over the red box at the left of the tab, the tiger stands with its feet near the bottom of the box "
	                "and its body in the middle across it. It turns round once in about 8.2 seconds, also while the "
	                "game is paused, and it fills about the same part of the box at all sizes of the tiger.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Centre, k_Hut, k_PlannedHut}, .distance = 0.6f},
	    .creatures = {{.species = CreatureType::Tiger, .offset = {40.0f, 40.0f}}},
	    .objects = KnownTown(),
	    .townBeliefInPlayer = k_TownBelief,
	    .creatureTallies = k_KnownTallies,
	});
	all.push_back({
	    .id = "statistics.creature_hair",
	    .name = "A creature with hair on the Creature Stats tab",
	    .facet = Facet::Objects,
	    .description = "The player's ape, which has hair, and the town of statistics.box. Push Escape for the menu, pick "
	                   "its Statistics tab and then the Creature Stats tab. Then close the menu.",
	    .expected = "Over the red box the ape turns with no hair, as in vanilla: its hair stays on the ape in the world. "
	                "The ape shows in the same frame as the box: when the tab opens or the menu closes, no frame shows "
	                "the box without the ape or the ape without the box.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Centre, k_Hut, k_PlannedHut}, .distance = 0.6f},
	    .creatures = {{.species = CreatureType::GiantApe, .offset = {40.0f, 40.0f}}},
	    .objects = KnownTown(),
	    .townBeliefInPlayer = k_TownBelief,
	    .creatureTallies = k_KnownTallies,
	});
}
