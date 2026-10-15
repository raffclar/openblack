/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of a creature that goes to the places of its map of the land: a hill and the walk round its
// ridge, a beach by the lake where it sits, and the coast of the lake where it dances

#include <string_view>
#include <vector>

#include "3D/FlatLand.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The testbed's lake, from the middle of the map
constexpr glm::vec2 k_Lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;

CreatureSetup Explorer(glm::vec2 offset)
{
	return {.label = "explorer",
	        .species = CreatureType::Tiger,
	        .offset = offset,
	        .facingDegrees = 0.0f,
	        .phase = 3,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}};
}

Command Do(std::string_view action, float delay)
{
	return {.kind = Kind::DoAction, .delaySeconds = delay, .object = 0, .action = action};
}
} // namespace

void testbed_scenarios::AddCreatureExplorationScenarios(std::vector<Scenario>& all)
{
	const glm::vec2 hillStart {-200.0f, -80.0f};
	all.push_back({
	    .id = "creature.explore_hill_ridge",
	    .name = "Goes to a hill and walks along its ridge",
	    .facet = Facet::Movement,
	    .description = "A tiger is told to go to a hill and walk along its ridge. The testbed is flat, so every region of "
	                   "80 metres is a hill. The tiger prefers a region that it did not go into: the region west of its "
	                   "own.",
	    .expected = "It walks west to the highest cell of the next region and stops within three times its height. Then "
	                "it walks round a circle of 15 metres about that cell, to eight points one after the other.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {hillStart + glm::vec2 {-120.0f, -40.0f}, hillStart + glm::vec2 {40.0f, 60.0f}}},
	    .creatures = {Explorer(hillStart)},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = hillStart + glm::vec2 {6.0f, 0.0f}}},
	    .commands = {Do("GoToHillAndWalkAlongRidge", 1.0f)},
	});
	const glm::vec2 beachStart {k_Lake.x + 40.0f, k_Lake.y - flat_land::k_LakeHalfExtent.y - 70.0f};
	all.push_back({
	    .id = "creature.sit_on_beach",
	    .name = "Sits down on the beach",
	    .facet = Facet::Movement,
	    .description = "A grown tiger south of the lake is told to sit down on the beach. Its map of the land gives the "
	                   "first water cell of the nearest region with water.",
	    .expected = "It smiles and walks to a clear place on the bank near that water cell, turns to face the water and "
	                "sits down for 10 to 25 seconds, then stands up.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {beachStart + glm::vec2 {-80.0f, -20.0f}, beachStart + glm::vec2 {40.0f, 140.0f}}},
	    .creatures = {Explorer(beachStart)},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = beachStart + glm::vec2 {6.0f, 0.0f}}},
	    .commands = {Do("SitDownOnBeach", 1.0f)},
	});
	const glm::vec2 danceStart {k_Lake.x - 40.0f, k_Lake.y - flat_land::k_LakeHalfExtent.y - 70.0f};
	all.push_back({
	    .id = "creature.dance_by_the_sea",
	    .name = "Dances alone by the sea",
	    .facet = Facet::Movement,
	    .description = "A grown tiger south of the lake is told to dance on its own by the sea. Its map of the land "
	                   "gives the nearest coast cell and the nearest water cell.",
	    .expected = "It smiles and walks to within one of its heights of the coast cell. It turns to face the water. "
	                "Then it dances alone with a playful face: the start of its dance, then dance moves chosen at random, "
	                "each one second long, for 20 seconds. Then it plays the end of the dance.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {danceStart + glm::vec2 {-60.0f, -20.0f}, danceStart + glm::vec2 {60.0f, 140.0f}}},
	    .creatures = {Explorer(danceStart)},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = danceStart + glm::vec2 {6.0f, 0.0f}}},
	    .commands = {Do("DanceOnYourOwnByTheSea", 1.0f)},
	});
}
