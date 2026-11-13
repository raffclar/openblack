/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scenarios of the network map for 32 gods: the whole ring of citadels from high above, the village of the first
// slot from near, and the symbols of belief of the gods after the seventh. The map loads in place of the testbed, with
// its own land and its own things.

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
	                "Each of the 32 slots has a god. Between each two slots, a Japanese town and an Indian town stand "
	                "nearer the middle, with their flocks and trees. A Tibetan town stands in the middle.",
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

	all.push_back({
	    .id = "multiplayer.gods_ring_gap",
	    .name = "A gap between two slots of the map for 32 gods",
	    .facet = Facet::Multiplayer,
	    .description = "The network map for 32 gods loads on this computer alone. The camera looks along the line "
	                   "between the first slot and the second slot, from beyond the citadels toward the middle of the "
	                   "island (deviation 256).",
	    .expected = "The villages of the two slots stand at the two sides. Between them and nearer the middle stands a "
	                "Japanese town with its fields, its flock and its trees. Farther in stands an Indian town with its "
	                "fish farms, its flock and its forest. No building of a town stands on a building of a village.",
	    .environment = {.land = Land::GodsRingMap, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {225.0f, 450.0f, -2290.0f}, .look = {150.0f, 0.0f, -1550.0f}},
	});

	// A town centre on the island, clear of the villages of the slots and of the towns between them
	constexpr glm::vec2 k_Centre {0.0f, 600.0f};
	all.push_back({
	    .id = "multiplayer.later_gods_belief_symbols",
	    .name = "The symbols of belief of the gods after the seventh",
	    .facet = Facet::Multiplayer,
	    .description = "The network map for 32 gods loads on this computer alone. A Norse town centre in the middle of "
	                   "the island believes in the first god and in the gods of the player numbers 8, 15, 16, 20, 31 and "
	                   "32, each by a different amount. The camera is still, a little above the top of the totem.",
	    .expected = "A symbol for each of the seven gods: the god believed in most sits over the totem, and the others "
	                "circle it at their levels, each in the land colour of its god. The symbols of the later gods show "
	                "their squares of the wider sheet of the players' symbols. Each later computer god shows the picture "
	                "of Nemesis, the last of the three pictures of the computer gods.",
	    .environment = {.land = Land::GodsRingMap, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Centre.x - 12.7f, 24.0f, k_Centre.y - 27.2f},
	                .look = {k_Centre.x, 21.0f, k_Centre.y}},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre}},
	    .townBeliefInPlayer = 1000.0f,
	    .townBeliefInPlayers = {{static_cast<PlayerNames>(8), 0.9f},
	                            {static_cast<PlayerNames>(15), 0.75f},
	                            {static_cast<PlayerNames>(16), 0.6f},
	                            {static_cast<PlayerNames>(20), 0.45f},
	                            {static_cast<PlayerNames>(31), 0.3f},
	                            {static_cast<PlayerNames>(32), 0.15f}},
	});
}
