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

#include "TestbedScenarioComputerPlayers.h"
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

} // namespace

void testbed_scenarios::AddComputerGodCoreScenarios(std::vector<Scenario>& all)
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
	    .id = "mind.computer_god_help_hold",
	    .name = "A computer god that the cinema bars hold",
	    .facet = Facet::Mind,
	    .description = "The land of the food miracle scenario. The cinema bars slide in after half a second and slide out "
	                   "after 30 seconds, as the help and the land scripts do.",
	    .expected = "The god of player two starts its first action before the bars slide in. While the bars are in, "
	                "its mind does nothing and its hand stops where it is. When the bars slide out, the hand "
	                "continues as in the food miracle scenario.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -110.0f), glm::vec2(80.0f, 100.0f)}},
	    .landScript = k_ComputerGodTownScript,
	    .commands = {{.kind = Command::Kind::WideScreen, .delaySeconds = 0.5f, .value = 1},
	                 {.kind = Command::Kind::WideScreen, .delaySeconds = 30.0f, .value = 0}},
	    .worshipShare = 0.5f,
	});
}
