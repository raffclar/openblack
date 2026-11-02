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
constexpr std::string_view k_FourGods = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
TOGGLE_COMPUTER_PLAYER("PLAYER_THREE", 1)
TOGGLE_COMPUTER_PLAYER("PLAYER_FOUR", 1)
)";

// The temples: the player's to the south, player two's to the north and player three's to the east
constexpr glm::vec2 k_OwnTemple {0.0f, -60.0f};
constexpr glm::vec2 k_SecondTemple {0.0f, 90.0f};
constexpr glm::vec2 k_ThirdTemple {120.0f, 20.0f};
constexpr glm::vec2 k_FourthTemple {-120.0f, 20.0f};

// The notches of the mouse wheel that scroll the window of gods part of the way along the gods
constexpr size_t k_ScrollNotches = 7;

const Framing k_Overview {.shot = Shot::Overview, .include = {{-60.0f, -100.0f}, {160.0f, 130.0f}}, .distance = 1.0f};

/// The temples of the gods after the fourth go first, a quarter second apart. 20 seconds later, the temples of the
/// gods two to four go, so that only the god of the player is left.
std::vector<Command> TemplesOfTheGodsAfterTheFirst()
{
	constexpr size_t k_LastSlot = 31;
	constexpr size_t k_FirstSlotAfterTheFourth = 4;
	std::vector<Command> commands;
	for (size_t slot = k_FirstSlotAfterTheFourth; slot <= k_LastSlot; ++slot)
	{
		commands.push_back({.kind = Command::Kind::DestroyTemple,
		                    .delaySeconds = slot == k_FirstSlotAfterTheFourth ? 2.0f : 0.25f,
		                    .value = slot});
	}
	for (size_t slot = 1; slot < k_FirstSlotAfterTheFourth; ++slot)
	{
		commands.push_back({.kind = Command::Kind::DestroyTemple, .delaySeconds = slot == 1 ? 20.0f : 0.25f, .value = slot});
	}
	return commands;
}
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
	all.push_back({
	    .id = "game_end.leave_question",
	    .name = "The question to leave a network game",
	    .facet = Facet::Multiplayer,
	    .description = "A skirmish of two gods: the player and the computer god of player two, each with a temple. One "
	                   "second in, the end box asks the question to leave, as Exit Online Game of the menu asks it in a "
	                   "network game.",
	    .expected = "The end box fades in over half a second with \"Are you sure you want to quit this game?\" in the "
	                "middle of the box on a band of the player's red at half opacity, Back to Game at the bottom left and "
	                "Leave Game at the bottom right. The world goes on behind the box. Back to Game closes the box.",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}, .skirmish = true},
	    .framing = k_Overview,
	    .temples = {{.offset = k_OwnTemple}, {.offset = k_SecondTemple, .owner = PlayerNames::PLAYER_TWO}},
	    .landScript = k_TwoComputerGods,
	    .commands = {{.kind = Kind::EndBox, .delaySeconds = 1.0f, .value = 0}},
	});
	all.push_back({
	    .id = "game_end.conditions",
	    .name = "The Multiplayer tab of the end box",
	    .facet = Facet::Multiplayer,
	    .description = "A skirmish of three gods: the player and the computer gods of players two and three, each with a "
	                   "temple. 31 seconds in, the player's temple starts its destruction. 25 seconds after that, the "
	                   "Multiplayer tab of the end box opens.",
	    .expected = "The box of the winning conditions replaces the end box: five tabs (Game Over, Multiplayer and three "
	                "empty tabs) with Multiplayer open, \"Multiplayer Winning Condition Status\" on a dark band, a line "
	                "graph from 0 to 1 with one line for each god in its colour (the share of the influence each ten turns, "
	                "as a game with no winning conditions keeps it), the check boxes Player 1, Player 2 and Player 3 under "
	                "it, and an empty list, because a skirmish has no winning conditions. A click on a check box takes the "
	                "line of the god out of the graph or puts it back. A click on the graph shows the lines as shares. Game "
	                "Over and Back go back to the end box.",
	    .environment = {.dispenserGrid = false,
	                    .players = {PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE},
	                    .skirmish = true},
	    .framing = k_Overview,
	    .temples = {{.offset = k_OwnTemple},
	                {.offset = k_SecondTemple, .owner = PlayerNames::PLAYER_TWO},
	                {.offset = k_ThirdTemple, .owner = PlayerNames::PLAYER_THREE}},
	    .landScript = k_ThreeComputerGods,
	    .commands = {{.kind = Kind::DestroyTemple, .delaySeconds = 31.0f, .value = 0},
	                 {.kind = Kind::EndBox, .delaySeconds = 25.0f, .value = 1}},
	});
	all.push_back({
	    .id = "game_end.network_menu",
	    .name = "The main menu of a network game",
	    .facet = Facet::Multiplayer,
	    .description = "A skirmish of two gods. One second in, the main menu opens as it shows in a network game.",
	    .expected = "The main menu has four buttons: Continue Game, Exit Online Game., Options and Quit Black & White, 80 "
	                "high and 100 apart from the top at 150, with no Start Skirmish Game. Its third tab reads Multiplayer "
	                "and opens the box of the winning conditions with Main Menu as its first tab. Exit Online Game. closes "
	                "the menu and the end box asks \"Are you sure you want to quit this game?\".",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}, .skirmish = true},
	    .framing = k_Overview,
	    .temples = {{.offset = k_OwnTemple}, {.offset = k_SecondTemple, .owner = PlayerNames::PLAYER_TWO}},
	    .landScript = k_TwoComputerGods,
	    .commands = {{.kind = Kind::EndBox, .delaySeconds = 1.0f, .value = 3}},
	});
	all.push_back({
	    .id = "game_end.skirmish_ping",
	    .name = "The message to the lobby when a skirmish is decided",
	    .facet = Facet::Multiplayer,
	    .description = "A skirmish of two gods: the player and the computer god of player two, each with a temple. One "
	                   "second in, the temple of player two starts its destruction. Start the game with the address of a "
	                   "lobby (--lobby) and a session that the store of the profile keeps.",
	    .expected = "When the temple of player two goes, the game is decided and the end box shows as in game_end.won. "
	                "The game sends one message to the lobby, which keeps one row for the account of the player. The log "
	                "says \"The lobby got the message of the decided skirmish\". With no session, no lobby or no "
	                "network, the game sends nothing and shows nothing more.",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}, .skirmish = true},
	    .framing = k_Overview,
	    .temples = {{.offset = k_OwnTemple}, {.offset = k_SecondTemple, .owner = PlayerNames::PLAYER_TWO}},
	    .landScript = k_TwoComputerGods,
	    .commands = {{.kind = Kind::DestroyTemple, .delaySeconds = 1.0f, .value = 1}},
	});
	all.push_back({
	    .id = "game_end.statistics_4_gods",
	    .name = "The Statistics tab of the end box with 4 gods",
	    .facet = Facet::Multiplayer,
	    .description = "A skirmish of four gods: the player and the computer gods of players two, three and four, each "
	                   "with a temple. Two seconds in, the end box asks the question to leave. One second after that, "
	                   "the Statistics tab of the end box opens.",
	    .expected = "The Game Statistics box shows as the game shows it for four players: Game Over as its first tab, "
	                "Player Stats open, the two line graphs and the bars of Final Area Of Influence with one line and one "
	                "bar for each of the four gods in its colour, and each row with one value or one part of the bar for "
	                "each god. No arrows, no \"Players\" text and no scroll bar show.",
	    .environment = {.dispenserGrid = false,
	                    .players = {PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE, PlayerNames::PLAYER_FOUR},
	                    .skirmish = true},
	    .framing = k_Overview,
	    .temples = {{.offset = k_OwnTemple},
	                {.offset = k_SecondTemple, .owner = PlayerNames::PLAYER_TWO},
	                {.offset = k_ThirdTemple, .owner = PlayerNames::PLAYER_THREE},
	                {.offset = k_FourthTemple, .owner = PlayerNames::PLAYER_FOUR}},
	    .landScript = k_FourGods,
	    .commands = {{.kind = Kind::EndBox, .delaySeconds = 2.0f, .value = 0},
	                 {.kind = Kind::EndBox, .delaySeconds = 1.0f, .value = 2}},
	});
	all.push_back({
	    .id = "game_end.statistics_32_gods",
	    .name = "The Statistics tab of the end box with 32 gods",
	    .facet = Facet::Multiplayer,
	    .description = "The network map for 32 gods loads on this computer alone and plays as a network game. Two seconds "
	                   "in, the end box asks the question to leave. One second after that, the Statistics tab of the end "
	                   "box opens. One second after that, the god columns scroll on by seven notches of the mouse wheel. "
	                   "The map needs the game's Online Maps folder.",
	    .expected = "The Game Statistics box opens with the first 4 gods, in the places of the box with 4 gods. A scroll "
	                "bar shows under the god columns of the rows, and the list of rows stops above it. The scroll moves "
	                "the values of the gods to the left by 140, two and a third columns: the view cuts the columns at its "
	                "edges. The text reads \"Players 3-6 of 32\" between a left and a right arrow, and the totals, the "
	                "bars and the line graphs show those 4 gods. The thumb of the scroll bar is an eighth of the bar. A drag "
	                "of the thumb, the wheel over the columns and the arrows scroll the view (deviation 195).",
	    .environment = {.land = Land::GodsRingMap, .clockRuns = false, .dispenserGrid = false, .networkGame = true},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 5500.0f, -2600.0f}, .look = {0.0f, 0.0f, -200.0f}},
	    .commands = {{.kind = Kind::EndBox, .delaySeconds = 2.0f, .value = 0},
	                 {.kind = Kind::EndBox, .delaySeconds = 1.0f, .value = 2},
	                 {.kind = Kind::EndBox, .delaySeconds = 1.0f, .value = 3 + k_ScrollNotches}},
	});
	all.push_back({
	    .id = "game_end.conditions_32_gods",
	    .name = "The Multiplayer tab of the end box with 32 gods",
	    .facet = Facet::Multiplayer,
	    .description = "The network map for 32 gods loads on this computer alone and plays as a network game. Two seconds "
	                   "in, the end box asks the question to leave. One second after that, the Multiplayer tab of the end "
	                   "box opens. One second after that, the check boxes scroll on by seven notches of the mouse wheel. The "
	                   "map needs the game's Online Maps folder.",
	    .expected = "The box of the winning conditions opens with the check boxes Player 1 to Player 4 under the graph. A "
	                "scroll bar shows under the check boxes, and the list starts below it. The scroll moves the check boxes "
	                "to the left by 140: Player 2 shows cut at the left edge of the view and Player 6 at the right edge. "
	                "\"Players 2-5 of 32\" shows between the arrows, and the graph shows those 4 gods, each ticked as the "
	                "player left it. A click on a check box takes the line of its god out of the graph or puts it back "
	                "(deviation 195).",
	    .environment = {.land = Land::GodsRingMap, .clockRuns = false, .dispenserGrid = false, .networkGame = true},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 5500.0f, -2600.0f}, .look = {0.0f, 0.0f, -200.0f}},
	    .commands = {{.kind = Kind::EndBox, .delaySeconds = 2.0f, .value = 0},
	                 {.kind = Kind::EndBox, .delaySeconds = 1.0f, .value = 1},
	                 {.kind = Kind::EndBox, .delaySeconds = 1.0f, .value = 3 + k_ScrollNotches}},
	});
	all.push_back({
	    .id = "game_end.gods_after_the_fourth",
	    .name = "The gods after the fourth lose a network game of 32 gods",
	    .facet = Facet::Multiplayer,
	    .description = "The network map for 32 gods loads on this computer alone and plays as a network game. Two seconds "
	                   "in, the temples of the gods 5 to 32 start their destruction, a quarter second apart. 20 seconds "
	                   "later, the temples of the gods 2 to 4 start their destruction. The map needs the game's Online "
	                   "Maps folder.",
	    .expected = "When the temple of each god after the fourth goes, that god is out: its creature goes from the land, "
	                "and the white box says \"Player[n] is out of the game.\". The game goes on with 4 gods. When the "
	                "temples of the gods 2 to 4 go, only the god of the player is left, and the end box opens with "
	                "\"Congratulations! You've won the game!\" (deviation 193).",
	    .environment = {.land = Land::GodsRingMap, .dispenserGrid = false, .networkGame = true},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 5500.0f, -2600.0f}, .look = {0.0f, 0.0f, -200.0f}},
	    .commands = TemplesOfTheGodsAfterTheFirst(),
	});
}
