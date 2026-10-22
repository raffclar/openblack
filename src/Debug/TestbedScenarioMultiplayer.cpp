/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed scenarios of the multiplayer front end: each box over the temple's future room, with the demonstration
// lobby, which has no network

#include <array>
#include <string_view>
#include <vector>

#include "TestbedMultiplayer.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{

struct BoxScenario
{
	std::string_view id;
	std::string_view name;
	testbed_multiplayer::Stage stage;
	std::string_view description;
	std::string_view expected;
};

} // namespace

void testbed_scenarios::AddMultiplayerFrontEndScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	using Stage = testbed_multiplayer::Stage;
	const std::array<BoxScenario, 12> boxes = {{
	    {"multiplayer.choose_connection", "The multiplayer front end: choose a connection", Stage::ChooseConnection,
	     "The multiplayer front end opens with the demonstration lobby. The player goes straight into the temple's "
	     "future room.",
	     "The box \"Choose a connection from the list below.\" shows over the future room, which turns slowly behind "
	     "it. The room's words do not show. The list has \"1.Local Area Network\" and \"2. demo.invalid (default)\", the "
	     "second picked. Back is at the bottom left, Next at the bottom right."},
	    {"multiplayer.find_games", "The multiplayer front end: find games", Stage::FindGames,
	     "The front end goes past the connections' box.",
	     "The box \"Find Black & White games.\" has two round check boxes: \"On your Local Network\" above and \"On "
	     "the Internet\" below, the second ticked. A click on one takes the tick from the other."},
	    {"multiplayer.login", "The multiplayer front end: login", Stage::Login,
	     "The front end goes to the login box of the internet.",
	     "The box \"Login to Black & White online.\" has \"User Name:\" and \"Password:\" at the left of their boxes. "
	     "The name's box has the keyboard. The password shows a # for each character. Each box takes 32 characters."},
	    {"multiplayer.game_list", "The multiplayer front end: the game list", Stage::GameList,
	     "The player logs in as Alice, and the first game of the list is picked.",
	     "The box \"Find/Create a game on the Internet.\" lists \"Bob's game (2/4)\", \"Fun (1/2) - PLAYING\", \"Test "
	     "(1/4) (Wrong Version)\", \"Clan war (3/4)(Needs Password)-Locked\" and \"Thirty-two gods (20/32)\". The "
	     "players of Bob's game show at the right. \"Join game\" is at the bottom right. Click no game: the help "
	     "shows, and the second box reads \"Game Name Filter:\"."},
	    {"multiplayer.room_chat", "The multiplayer front end: the game room's chat", Stage::RoomChat,
	     "The player joins Bob's game. The lobby gives a ticket, and the game socket opens. Bob's game has Bob and Alice as "
	     "gods 1 and 2, and Eve as an observer, on Bombardment.",
	     "The game room has four tabs: Chat, Landscape, Clans and Player Details. The caption reads \"Alice, you are "
	     "in Bob's game, landscape: Bombardment - 2 players (2 players).\". The gods show in their colours, and the "
	     "observer in grey. The chat shows Bob's line in red, Eve's in grey and Alice's own line as \"Alice> Hi, "
	     "all!\" in light grey. Type a line and push Return: it comes back as Alice's line."},
	    {"multiplayer.room_landscape", "The multiplayer front end: the game room's land", Stage::RoomLandscape,
	     "The player joins Bob's game and opens the Landscape tab. Bob's game has Bob and Alice as gods 1 and 2, and Eve as an "
	     "observer, on Bombardment.",
	     "The lands of the local maps show, with \"(HOST)\" after Bombardment, which the server plays. Land Info shows "
	     "\"Bombardment action for 2 players...\". The gods' list does not show on this tab."},
	    {"multiplayer.room_clans", "The multiplayer front end: the game room's clans", Stage::RoomClans,
	     "The player joins Bob's game, opens the Clans tab and picks The Clan. Bob's game has Bob and Alice as gods 1 and 2, "
	     "and Eve as an observer, on Bombardment.",
	     "\"Alice's clans:\" lists The Clan and Other Clan. \"Clan The Clan info:\" shows \"Clan Description: Our "
	     "motto\", \"Clan Leader: Alice\", \"Clan URL: www.example.com\" and \"Credits 12.50\". \"Use This Clan\" "
	     "shows; a click on it changes it to \"Don't use any Clan.\"."},
	    {"multiplayer.room_player_details", "The multiplayer front end: the game room's player details",
	     Stage::RoomPlayerDetails,
	     "The player joins Bob's game, opens the Player Details tab and picks Bob. Bob's game has Bob and Alice as gods 1 and "
	     "2, and Eve as an observer, on Bombardment.",
	     "\"Player Bob information:\" shows, with \"God 1\" under it."},
	    {"multiplayer.conditions", "The multiplayer front end: the winning conditions", Stage::Conditions,
	     "The player joins Bob's game and clicks Next. Bob's game has Bob and Alice as gods 1 and 2, and Eve as an observer, "
	     "on Bombardment.",
	     "The page lists the 15 winning conditions of Bombardment. \"Condition Value:\" and \"Time Limit "
	     "(Hours:Minutes):\" show, and \"View Chat\" at the right. The Ready box is at the bottom left."},
	    {"multiplayer.waiting_for_start", "The multiplayer front end: joined and waiting for the start", Stage::WaitingForStart,
	     "The player joins Bob's game, goes to the conditions and ticks Ready. Bob's game has Bob and Alice as gods 1 and 2, "
	     "and Eve as an observer, on Bombardment.",
	     "A white box says \"You are in the game. Wait for the start of the game.\" with Cancel, over the dimmed "
	     "room. Cancel takes the tick from Ready."},
	    {"multiplayer.lan_list", "The multiplayer front end: the local network's games", Stage::LanGameList,
	     "The front end looks for games on the demonstration's local network.",
	     "The box \"Find a game on your Local Network.\" lists \"Office game (1/4)\" and \"Upstairs (0/32)\". It has no "
	     "Refresh List and no password box."},
	    {"multiplayer.not_registered", "The multiplayer front end: a name with no account", Stage::NotRegistered,
	     "The player logs in as Zed, a name that has no account.",
	     "A white box says \"You are not registered. Please visit the Black & White registration pages.\" with Yes "
	     "and No. Yes opens the registration page of the lobby's site in the browser."},
	}};
	for (const auto& box : boxes)
	{
		// The texts are literals, which the scenario keeps as views
		Scenario scenario {
		    .id = box.id,
		    .name = box.name,
		    .facet = Facet::Interface,
		    .description = box.description,
		    .expected = box.expected,
		    .framing = {.shot = Shot::Overview},
		    .commands = {{.kind = Kind::OpenMultiplayerBox, .delaySeconds = 1.0f, .value = static_cast<size_t>(box.stage)}},
		};
		all.push_back(std::move(scenario));
	}
}
