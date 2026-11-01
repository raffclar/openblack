/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios that state their own outcome. A command line run with --scenario-check, and the inspector's
// game.scenario_checks, report whether they pass.

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddSelfCheckScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	using checks::Quantity;
	constexpr glm::vec2 k_Goal {0.0f, 150.0f};
	all.push_back({
	    .id = "checks.creature_walks_to_point",
	    .name = "Checks: a creature walks to a point",
	    .facet = Facet::Movement,
	    .description = "A grown tiger with its needs full is told to walk to a point 150 metres from it. The scenario checks "
	                   "that the tiger lives and that it gets to the point.",
	    .expected = "The tiger walks to the point and stops there. A run with --scenario-check ends with the exit code 0.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -20.0f}, {30.0f, 170.0f}}},
	    .creatures = {{.label = "walker",
	                   .species = CreatureType::Tiger,
	                   .phase = 3,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}}},
	    .commands = {{.kind = Kind::WalkTo, .delaySeconds = 1.0f, .point = k_Goal}},
	    .checks = {{.label = "the tiger lives", .quantity = Quantity::CreaturesAlive, .afterTurns = 5, .atLeast = 1.0},
	               {.label = "the tiger gets to the point",
	                .quantity = Quantity::CreatureDistance,
	                .point = k_Goal,
	                .afterTurns = 20,
	                .byTurn = 1500,
	                .atMost = 4.0}},
	});
	all.push_back({
	    .id = "checks.story_land_has_a_creature",
	    .name = "Checks: a creature on the second land",
	    .facet = Facet::Story,
	    .description = "The second land of the story loads with its own scripts, sky and people, and a grown tiger is put on "
	                   "it. The scenario checks that the land has its towns and that the tiger lives.",
	    .expected = "The land plays as the story plays it, with the tiger on it. A run with --scenario-check ends with the "
	                "exit code 0.",
	    .environment = {.land = Land::Story, .storyLand = "Land2.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -20.0f}, {20.0f, 20.0f}}},
	    .creatures = {{.label = "visitor",
	                   .species = CreatureType::Tiger,
	                   .phase = 3,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}}},
	    .checks = {{.label = "the tiger lives", .quantity = Quantity::CreaturesAlive, .afterTurns = 50, .atLeast = 1.0},
	               {.label = "the land has towns",
	                .quantity = Quantity::Entities,
	                .kind = "towns",
	                .afterTurns = 50,
	                .atLeast = 1.0}},
	});
}
