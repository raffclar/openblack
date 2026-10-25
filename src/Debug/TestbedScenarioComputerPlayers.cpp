/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{

/// The lines of a skirmish land that give player two a computer mind and a creature from the computer's mind file. The
/// size of that creature comes from the local player's creature.
constexpr std::string_view k_ComputerPlayerScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CREATURE_FROM_FILE("PLAYER_TWO", 3, "ComputerControlledCreature", "25.0,0.0")
SET_COMPUTER_PLAYER_CREATURE_LIKE("PLAYER_TWO", "PLAYER_ONE")
)";

/// The line of the Island Wars land that makes an anti-influence of 20 m for player one, here 40 m north of the middle
constexpr std::string_view k_AntiInfluenceRingScript = R"(CREATE_INFLUENCE_RING("0.0,40.0", 0, 20.000000, 202594004)
)";

} // namespace

void testbed_scenarios::AddComputerPlayerScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "mind.computer_player_start",
	    .name = "A computer player's creature at the start of its mind",
	    .facet = Facet::Mind,
	    .description = "The player's tiger stands to the west at size 1. A temple of player two stands to the north. Lines "
	                   "of a skirmish land switch on the computer mind of player two. They make a leopard for it from the "
	                   "computer's mind file to the east, and name player one for the size of the leopard.",
	    .expected = "At the first turn the leopard grows at once to 0.9 to 1.2 times the size of the tiger. The leopard "
	                "knows heal, food, wood and water, and each miracle that player two can cast. The players.list query "
	                "shows the computer mind of player two as switched on, marked and started.",
	    .creatures = {{.label = "the player's", .species = CreatureType::Tiger, .offset = {-25.0f, 0.0f}, .size = 1.0f}},
	    .temples = {{.offset = {0.0f, 90.0f}, .owner = PlayerNames::PLAYER_TWO}},
	    .landScript = k_ComputerPlayerScript,
	});

	all.push_back({
	    .id = "multiplayer.land_anti_influence_ring",
	    .name = "An anti-influence ring that a land's script makes",
	    .facet = Facet::Multiplayer,
	    .description = "The line of the Island Wars land that makes an influence ring. Its last value is not 0, so it is "
	                   "an anti-influence of 20 m for player one, here 40 m north of the middle, inside the player's "
	                   "testbed influence.",
	    .expected = "Inside 20 m of the spot the player has no influence: the hand cannot pick up, drop or cast there, and "
	                "the influence.at query gives 0 for player one. Just outside it the hand works as before.",
	    .environment = {.dispenserGrid = false},
	    .landScript = k_AntiInfluenceRingScript,
	});
}
