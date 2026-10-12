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
	});
}
