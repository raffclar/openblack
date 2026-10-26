/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed scenarios of the end of a skirmish: a god with no temple is out, the last god in wins, and the end box and
// the box with OK show on this computer

#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The lines of a skirmish land that switch on the computer minds of players two and three
constexpr std::string_view k_TwoComputerGods = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
)";
constexpr std::string_view k_ThreeComputerGods = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
TOGGLE_COMPUTER_PLAYER("PLAYER_THREE", 1)
)";

// The temples: the player's to the south, player two's to the north and player three's to the east
constexpr glm::vec2 k_OwnTemple {0.0f, -60.0f};
constexpr glm::vec2 k_SecondTemple {0.0f, 90.0f};
constexpr glm::vec2 k_ThirdTemple {120.0f, 20.0f};

const Framing k_Overview {.shot = Shot::Overview, .include = {{-60.0f, -100.0f}, {160.0f, 130.0f}}, .distance = 1.0f};
} // namespace

void testbed_scenarios::AddGameEndScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	all.push_back({
	    .id = "game_end.won",
	    .name = "The end box of a skirmish that the player won",
	    .facet = Facet::Multiplayer,
	    .description = "A skirmish of two gods: the player and the computer god of player two, each with a temple. One "
	                   "second in, the temple of player two starts its destruction.",
	    .expected = "When the temple of player two goes, player two is out and the game is decided. The world stops. The "
	                "end box fades in over half a second: seven tabs along its top (Game Over, Multiplayer, Statistics and "
	                "four empty tabs), \"Congratulations! You've won the game!\" in the middle of the box on a band of the "
	                "player's red at half opacity, and Leave Game at the bottom right. The Statistics tab opens the Game "
	                "Statistics box with Game Over as its first tab.",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}, .skirmish = true},
	    .framing = k_Overview,
	    .temples = {{.offset = k_OwnTemple}, {.offset = k_SecondTemple, .owner = PlayerNames::PLAYER_TWO}},
	    .landScript = k_TwoComputerGods,
	    .commands = {{.kind = Kind::DestroyTemple, .delaySeconds = 1.0f, .value = 1}},
	});
	all.push_back({
	    .id = "game_end.lost",
	    .name = "The end box of a skirmish that the player lost",
	    .facet = Facet::Multiplayer,
	    .description = "A skirmish of two gods: the player and the computer god of player two, each with a temple. One "
	                   "second in, the player's temple starts its destruction.",
	    .expected = "When the player's temple goes, the player is out and the game is decided. The world stops. The end "
	                "box says \"You have lost the game.\" with Leave Game only.",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}, .skirmish = true},
	    .framing = k_Overview,
	    .temples = {{.offset = k_OwnTemple}, {.offset = k_SecondTemple, .owner = PlayerNames::PLAYER_TWO}},
	    .landScript = k_TwoComputerGods,
	    .commands = {{.kind = Kind::DestroyTemple, .delaySeconds = 1.0f, .value = 0}},
	});
	all.push_back({
	    .id = "game_end.watch",
	    .name = "The end box of a player who is out while two gods play on",
	    .facet = Facet::Multiplayer,
	    .description = "A skirmish of three gods: the player and the computer gods of players two and three, each with a "
	                   "temple. One second in, the player's temple starts its destruction. 40 seconds in, the temple of "
	                   "player two starts its destruction.",
	    .expected = "When the player's temple goes, the end box says \"You have lost the game.\" with Watch Game at the "
	                "bottom left and Leave Game at the bottom right. The world goes on. Watch Game closes the box. When the "
	                "temple of player two goes, the game is decided and the end box opens again with \"Congratulations! "
	                "You've won the game!\", as the game does for a player who lost and watches (vanilla-bugs 469).",
	    .environment = {.dispenserGrid = false,
	                    .players = {PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE},
	                    .skirmish = true},
	    .framing = k_Overview,
	    .temples = {{.offset = k_OwnTemple},
	                {.offset = k_SecondTemple, .owner = PlayerNames::PLAYER_TWO},
	                {.offset = k_ThirdTemple, .owner = PlayerNames::PLAYER_THREE}},
	    .landScript = k_ThreeComputerGods,
	    .commands = {{.kind = Kind::DestroyTemple, .delaySeconds = 1.0f, .value = 0},
	                 {.kind = Kind::DestroyTemple, .delaySeconds = 40.0f, .value = 1}},
	});
	all.push_back({
	    .id = "game_end.god_out",
	    .name = "The box that tells the player that another god is out",
	    .facet = Facet::Multiplayer,
	    .description = "A skirmish of three gods: the player and the computer gods of players two and three, each with a "
	                   "temple. One second in, the temple of player two starts its destruction.",
	    .expected = "When the temple of player two goes, the white box with OK says \"Player[2] is out of the game.\" in "
	                "front of a dimmed box. The world goes on. OK closes the box.",
	    .environment = {.dispenserGrid = false,
	                    .players = {PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE},
	                    .skirmish = true},
	    .framing = k_Overview,
	    .temples = {{.offset = k_OwnTemple},
	                {.offset = k_SecondTemple, .owner = PlayerNames::PLAYER_TWO},
	                {.offset = k_ThirdTemple, .owner = PlayerNames::PLAYER_THREE}},
	    .landScript = k_ThreeComputerGods,
	    .commands = {{.kind = Kind::DestroyTemple, .delaySeconds = 1.0f, .value = 1}},
	});
}
