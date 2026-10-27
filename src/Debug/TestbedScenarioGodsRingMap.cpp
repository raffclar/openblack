/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scenarios of the network map for 32 gods: the whole ring of citadels from high above, and the village of the
// first slot from near. The map loads in place of the testbed, with its own land and its own things.

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddGodsRingMapScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "multiplayer.gods_ring_map",
	    .name = "The map for 32 gods",
	    .facet = Facet::Multiplayer,
	    .description = "The network map for 32 gods loads on this computer alone. The camera looks down on the whole "
	                   "island from high above its south side. Each of the 32 slots has a citadel and the starting "
	                   "village of a network land of the game. The map needs the game's Online Maps folder.",
	    .expected = "A round island of checkerboard grass in the sea. 32 citadels stand in a ring, 2000 from the middle, "
	                "at equal angles. Each citadel has the same village on its side toward the middle, turned with it. "
	                "The first seven slots have gods; the other slots are neutral.",
	    .environment = {.land = Land::GodsRingMap, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 5500.0f, -2600.0f}, .look = {0.0f, 0.0f, -200.0f}},
	});

	all.push_back({
	    .id = "multiplayer.gods_ring_slot",
	    .name = "A slot of the map for 32 gods",
	    .facet = Facet::Multiplayer,
	    .description = "The network map for 32 gods loads on this computer alone. The camera looks at the first slot "
	                   "from beyond its citadel, toward the middle of the island.",
	    .expected = "The first slot's citadel stands in front of its village: the town centre, the houses, the "
	                "storage pit, the graveyard, the creche, the workshop and the fields, with the villagers about them. "
	                "The citadels of the next slots show at the two sides.",
	    .environment = {.land = Land::GodsRingMap, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 300.0f, -2350.0f}, .look = {0.0f, 0.0f, -1850.0f}},
	});
}
