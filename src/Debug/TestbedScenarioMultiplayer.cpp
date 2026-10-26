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
	const std::array<BoxScenario, 18> boxes = {{
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
	     "(1/4) (Wrong Version)\", \"Clan war (3/4)(Needs Password)-Locked\", \"Thirty-two gods (32/32)\" and \"Full "
	     "game (2/2)\". The "
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
	     "The Ready box shows a tick. No message box shows, and the conditions page stays as it is. A click on the tick "
	     "takes the Ready away."},
	    {"multiplayer.lan_list", "The multiplayer front end: the local network's games", Stage::LanGameList,
	     "The front end looks for games on the demonstration's local network.",
	     "The box \"Find a game on your Local Network.\" lists \"Office game (1/4)\" and \"Upstairs (0/32)\". It has no "
	     "Refresh List and no password box."},
	    {"multiplayer.not_registered", "The multiplayer front end: a name with no account", Stage::NotRegistered,
	     "The player logs in as Zed, a name that has no account.",
	     "A white box says \"You are not registered. Please visit the Black & White registration pages.\" with Yes "
	     "and No. Yes opens the registration page of the lobby's site in the browser."},
	    {"multiplayer.room_chat_32_gods", "The multiplayer front end: the chat colours of 32 gods", Stage::RoomChat32,
	     "The player joins the game of 32 gods as god 32. Each of the other 31 gods says a line in the chat.",
	     "The chat shows 31 lines, \"God1: I am god 1.\" to \"God31: I am god 31.\". Gods 1 to 4 are red, green, blue "
	     "and yellow. Each of gods 5 to 31 has a different colour, and each line is easy to read on the chat box. Scroll "
	     "the chat to see all the lines. The list of gods at the side uses the same colours."},
	    {"multiplayer.full_room_observer", "The multiplayer front end: a full room joined as an observer",
	     Stage::FullRoomObserver,
	     "The player picks \"Full game (2/2)\" and joins it. The lobby gives no god's ticket for the full room.",
	     "No message shows. The game room of \"Full game\" opens, with Ivy and Jon as gods 1 and 2. The player is an "
	     "observer, with no god."},
	    {"multiplayer.start_transfer", "The multiplayer front end: the creature files of the start", Stage::StartTransfer,
	     "The player joins Bob's game and ticks Ready. The server asks for the land, and then for the creature files.",
	     "A white box says \"Sending/Receiving start data...\" with no button, over the dimmed room."},
	    {"multiplayer.start_cancelled", "The multiplayer front end: a start that another player stops", Stage::StartCancelled,
	     "The player joins Bob's game and ticks Ready. The server asks for the land, and another player does not have it.",
	     "A white box says \"The game could not be started because other people are still downloading the map.\" with "
	     "OK. OK closes the box, and the player stays ready."},
	    {"multiplayer.kicked_from_game", "The multiplayer front end: a land file that is not the server's",
	     Stage::KickedFromGame,
	     "The player joins Bob's game and ticks Ready. The player's land file is not the server's, so the server removes the "
	     "player from the room.",
	     "A white box says \"Bob's game has kicked you from the game.\" with OK. OK goes back to the game list."},
	    {"multiplayer.room_ping", "The multiplayer front end: the player's own ping in the game room", Stage::RoomPing,
	     "The player joins Bob's game with the ping display on, as the --show-ping switch does. The game socket of the "
	     "demonstration answers each ping after 48 ms.",
	     "The game room's player list shows \"God 2: Alice (CURRENT) ping 48\" in Alice's god colour, in the same letters "
	     "as the other lines. The lines of Bob and Eve show no ping. The number shows after the first second in the room."},
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

void testbed_scenarios::AddJoinOnlineScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	// The page's Join Online Game button, pressed while a script's cut scene has the cinema bars in
	all.push_back({
	    .id = "multiplayer.page_join_online",
	    .name = "The page's Join Online Game button in a cut scene",
	    .facet = Facet::Interface,
	    .description = "A script's cinema bars slide in after half a second. One second later the player presses the Join "
	                   "Online Game button of the browser page, which the scenario does for the page. Escape does not "
	                   "open the menu while the bars are in.",
	    .expected = "The multiplayer front end opens in the temple's future room with the box \"Choose a connection from "
	                "the list below.\". The land behind stops. Click Back: the front end closes, the land shows again "
	                "with the cinema bars in, and the land continues.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::WideScreen, .delaySeconds = 0.5f, .value = 1},
	                 {.kind = Kind::JoinOnline, .delaySeconds = 1.0f}},
	});
}

void testbed_scenarios::AddOtherGodScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	// Another god's hand holds a barrel high over the land, and that god's camera stands above it
	all.push_back({
	    .id = "multiplayer.other_god_hand",
	    .name = "Another god's sparkles and held thing",
	    .facet = Facet::Multiplayer,
	    .description = "The testbed acts as a network game in which the second player is another god. After a second, the "
	                   "second player's hand takes the barrel to the east, and the second player reports the hand 5 m above "
	                   "the land to the west and the camera 30 m above that hand. This computer does not draw the hand of "
	                   "the other god.",
	    .expected = "The barrel moves from where it stood to the west and stays in the air 5 m above the land, with no hand "
	                "about it. High above the barrel, sparkles in the second player's colour come out one after another "
	                "and fall down slowly. They do not stop while the scenario runs.",
	    .environment = {.clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-10.0f, 22.0f, -40.0f}, .look = {-20.0f, 20.0f, 0.0f}},
	    .objects = {{.type = MobileObjectInfo::EgyptBarrel, .offset = {20.0f, 0.0f}}},
	    .commands = {{.kind = Kind::OtherGodHand,
	                  .player = PlayerNames::PLAYER_TWO,
	                  .delaySeconds = 1.0f,
	                  .point = {-20.0f, 0.0f},
	                  .object = 0,
	                  .radius = 5.0f}},
	});
	// The player's own ping in the top left corner, as --show-ping shows it in a network game (deviation 143)
	all.push_back({
	    .id = "multiplayer.ping_line",
	    .name = "The player's own ping on the game screen",
	    .facet = Facet::Multiplayer,
	    .description = "After a second, the game screen shows a made-up ping, as --show-ping does in a network game. The "
	                   "round trip starts at 40 ms and grows by 8 ms each second to 64 ms, then starts again.",
	    .expected = "A small white line \"ping 40\" with a black shadow shows in the top left corner of the screen. Each "
	                "second the number changes: 48, 56, 64, then 40 again. The menu (Escape) hides the line.",
	    .framing = {.shot = Shot::Overview},
	    .commands = {{.kind = Kind::ShowDemoPing, .delaySeconds = 1.0f, .value = 1}},
	});
}
