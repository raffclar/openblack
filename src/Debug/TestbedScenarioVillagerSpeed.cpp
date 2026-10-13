/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of villagers walking side by side, each going at the speed its life, load, town and state give

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
constexpr float k_Start = -30.0f;
constexpr float k_End = 30.0f;
constexpr float k_RowSpacing = 5.0f;
constexpr float k_Repeat = 60.0f;
constexpr float k_Belief = 5.0f;

ObjectSetup Walker(size_t row, VillagerInfo type)
{
	const float y = (static_cast<float>(row) - 4.5f) * k_RowSpacing;
	return {.type = type,
	        .offset = {k_Start, y},
	        .yawDegrees = 90.0f,
	        .walkTo = glm::vec2 {k_End, y},
	        .walkAfterSeconds = 2.0f,
	        .walkRepeatSeconds = k_Repeat};
}
} // namespace

void testbed_scenarios::AddVillagerSpeedScenarios(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> walkers;
	// A healthy man walking, the one every other row is measured against
	walkers.push_back(Walker(0, VillagerInfo::CelticFarmerMale));
	auto wounded = Walker(1, VillagerInfo::CelticFarmerMale);
	wounded.life = 0.25f;
	walkers.push_back(wounded);
	auto crawling = Walker(2, VillagerInfo::CelticFarmerMale);
	crawling.life = 0.1f;
	walkers.push_back(crawling);
	auto woodCarrier = Walker(3, VillagerInfo::CelticFarmerMale);
	woodCarrier.woodHeld = 250;
	walkers.push_back(woodCarrier);
	auto foodCarrier = Walker(4, VillagerInfo::CelticFarmerMale);
	foodCarrier.foodHeld = 150;
	walkers.push_back(foodCarrier);
	walkers.push_back(Walker(5, VillagerInfo::CelticHousewifeFemale));
	auto runner = Walker(6, VillagerInfo::CelticFarmerMale);
	runner.walkFinal = VillagerStates::FleeingAndLookingAtObjectReaction;
	walkers.push_back(runner);
	auto hider = Walker(7, VillagerInfo::CelticFarmerMale);
	hider.walkFinal = VillagerStates::GoAndHideInNearbyBuilding;
	walkers.push_back(hider);
	auto trader = Walker(8, VillagerInfo::CelticFarmerMale);
	trader.disciple = VillagerDisciple::Trader;
	trader.woodHeld = 250;
	walkers.push_back(trader);
	// Made last, so that only it belongs to the town the scenario makes for it
	auto believer = Walker(9, VillagerInfo::CelticFarmerMale);
	believer.joinTown = true;
	walkers.push_back(believer);
	all.push_back({
	    .id = "villagers.speed_side_by_side",
	    .name = "Villagers' speeds side by side",
	    .facet = Facet::Movement,
	    .description = "Ten villagers set off together across the testbed, 60 m, and back every minute, one row each: a "
	                   "healthy man; a man at a quarter of his life; a man at a tenth of it; a man with a full load of "
	                   "wood; a man with a full load of food; a housewife; a man running to watch something; a man going "
	                   "to hide; a trader carrying as much wood as a full load for anyone else; and last a man of the "
	                   "player's town, which believes in the player five times over. Their ages and hunger are their own. "
	                   "The scenario log gives each walk's game turns.",
	    .expected = "Each goes at the speed of the state it is walking to, scaled by its life, load and town, then by its "
	                "own way of going (when it was made, age, hunger, life, sex). A healthy man walks about 0.17 m a "
	                "turn; the wounded man walks at a random half to three quarters of that, the weak one crawls at a "
	                "random 0.4 to 0.6 of his slowest pace; a full load of wood slows to three quarters and of food to "
	                "85%; the housewife walks slower, at her kind's speed less a fifth; the runner runs at nearly three "
	                "times a walk and the hider at two and two thirds; the trader isn't slowed, as half a trader's "
	                "load is no load; the townsman walks over half as fast again, by his town's belief and needs.",
	    .framing = {.shot = Shot::Overview, .include = {{k_Start, -25.0f}, {k_End, 25.0f}}},
	    .objects = walkers,
	    .townBeliefInPlayer = k_Belief,
	});
}
