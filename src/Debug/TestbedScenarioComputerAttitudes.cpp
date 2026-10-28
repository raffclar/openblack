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

#include <glm/vec2.hpp>

#include "TestbedScenarioComputerPlayers.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{

/// The lines of a land that switch on the computer minds of players two and three, and give player two a Celtic town
constexpr std::string_view k_RivalGodsScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
TOGGLE_COMPUTER_PLAYER("PLAYER_THREE", 1)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF(900, "PLAYER_TWO", 10.0)
CREATE_ABODE(900, "10.0,50.0", "CELTIC_ABODE_STORAGE_PIT", 0, 1000, 100, 100)
)";

} // namespace

void testbed_scenarios::AddComputerGodAttitudesScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "mind.computer_god_rivals",
	    .name = "Two computer gods that think about which player to defeat",
	    .facet = Facet::Mind,
	    .description = "Lines of a land switch on the computer minds of players two and three. Each has a temple, 600 "
	                   "metres apart, and player two has a Celtic town with a storage pit. Player one stands as a human.",
	    .expected = "At each turn the players note their influence power and its average. When the mind of player two "
	                "looks at its need to defeat a player, the need has player one or player three as its object, "
	                "with a value from 0 to 1. The needs for the player with more miracles and the player with the "
	                "better creature have the first rival as their object. The need for the player with more "
	                "worshippers has a value of 0. Nothing that the gods do changes yet, because their attacks are "
	                "not made. The players.list query shows the needs, and the entity query shows the records of each "
	                "player.",
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-80.0f, -150.0f), glm::vec2(680.0f, 100.0f)}},
	    .temples = {{.offset = {0.0f, -90.0f}, .owner = PlayerNames::PLAYER_TWO},
	                {.offset = {600.0f, -90.0f}, .owner = PlayerNames::PLAYER_THREE}},
	    .landScript = k_RivalGodsScript,
	});
}
