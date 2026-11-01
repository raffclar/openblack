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

/// The lines of a land script that switch the computer mind of player two on and then off: the player stays a computer
/// player with no mind that plays
constexpr std::string_view k_StoppedComputerGod = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 0)
)";

/// The lines of a land that give player two a temple, and then a computer mind that has nothing to do
constexpr std::string_view k_ComputerGodAtCitadel = R"(CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
)";

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
	const std::array<BoxScenario, 21> boxes = {{
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
	     "\"Bombardment action for 2 players...\". The thumbnail of Bombardment shows between the two lists, with the "
	     "marks of its citadels and towns. The gods' list does not show on this tab."},
	    {"multiplayer.room_clans", "The multiplayer front end: the game room's clans", Stage::RoomClans,
	     "The player joins Bob's game, opens the Clans tab and picks The Clan. Bob's game has Bob and Alice as gods 1 and 2, "
	     "and Eve as an observer, on Bombardment.",
	     "\"Alice's clans:\" lists The Clan and Other Clan. \"Clan The Clan info:\" shows \"Clan Description: Our "
	     "motto\", \"Clan Leader: Alice\", \"Clan URL: www.example.com\" and \"Credits 12.50\". \"Use This Clan\" "
	     "shows; a click on it changes it to \"Don't use any Clan.\". The clan's logo shows faintly behind the clan's "
	     "information, in its middle."},
	    {"multiplayer.room_player_details", "The multiplayer front end: the game room's player details",
	     Stage::RoomPlayerDetails,
	     "The player joins Bob's game, opens the Player Details tab and picks Bob. Bob's game has Bob and Alice as gods 1 and "
	     "2, and Eve as an observer, on Bombardment.",
	     "\"Player Bob information:\" shows, with the two lines of the demonstration lobby under it: \"Points:1000\" and "
	     "\"Games Played:12\". The lobby gives each label and each value. There is no space after the colon."},
	    {"multiplayer.conditions", "The multiplayer front end: the winning conditions", Stage::Conditions,
	     "The player joins Bob's game and clicks Next. Bob's game has Bob and Alice as gods 1 and 2, and Eve as an observer, "
	     "on Bombardment.",
	     "The list of the winning conditions is empty, because the game has no conditions. \"Time Limit "
	     "(Hours:Minutes):\" shows with 0:30, and \"View Chat\" is at the right. \"Condition Value:\" does not show. The "
	     "Ready box is at the bottom left."},
	    {"multiplayer.conditions_picked", "The multiplayer front end: the winning conditions that the server picked",
	     Stage::ConditionsPicked,
	     "The player joins Bob's game and clicks Next. The server of Bob's game picked three winning conditions and a "
	     "time limit of an hour and a half.",
	     "The list shows three lines, from the top: \"Number of trees grown (100)\", \"Amount of food in storage pit "
	     "(9000)\" and \"Number of houses built by a player in a town (20)\". The game adds each condition at the top of "
	     "the list, so the last condition shows first. \"Time Limit (Hours:Minutes):\" shows with 1:30. No check box "
	     "and no arrows show."},
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
	    {"multiplayer.map_download_failed", "The multiplayer front end: a land that the lobby does not give",
	     Stage::MapDownloadFailed,
	     "The player joins Bob's game. The room plays a land that the player does not have, and the lobby of the "
	     "demonstration does not give its file (deviation 169).",
	     "A white box says \"ERROR: Could not download map from Internet.\" and \"Please check your Internet "
	     "connection.\" with OK, over the dimmed room. OK closes the box, and the player stays in the room."},
	    {"multiplayer.lan_room_chat", "The multiplayer front end: the chat of a room on the local network", Stage::LanRoomChat,
	     "The player joins \"Office game\" on the demonstration's local network by the profile's name. The room's game "
	     "server sends a line of Ian, god 1.",
	     "The game room of \"Office game\" opens on the Chat tab. The chat shows \"Ian: Hello from the office!\" in red. "
	     "Type a line and push Return: it shows at once as \"name> text\" in light grey, with the profile's name, and "
	     "goes to the room's game server. A line of only spaces does not go."},
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
	    .environment = {.clockRuns = false, .dispenserGrid = false, .networkGame = true},
	    .framing = {.shot = Shot::Placed, .eye = {-10.0f, 22.0f, -40.0f}, .look = {-20.0f, 20.0f, 0.0f}},
	    .objects = {{.type = MobileObjectInfo::EgyptBarrel, .offset = {20.0f, 0.0f}}},
	    .commands = {{.kind = Kind::OtherGodHand,
	                  .player = PlayerNames::PLAYER_TWO,
	                  .delaySeconds = 1.0f,
	                  .point = {-20.0f, 0.0f},
	                  .object = 0,
	                  .radius = 5.0f}},
	});
	// Another god's hand holds a miracle seed whose model shows in the hand, and that god's camera stands above it
	all.push_back({
	    .id = "multiplayer.other_god_seed",
	    .name = "Another god's held miracle",
	    .facet = Facet::Multiplayer,
	    .description = "The testbed acts as a network game in which the second player is another god. After a second, the "
	                   "second player's hand holds a miracle seed, and the second player reports the hand 5 m above the "
	                   "land to the west and the camera 30 m above that hand. This computer does not draw the hand of the "
	                   "other god, or the effect of the seed in that hand.",
	    .expected = "After a short time, the model of the miracle shows 5 m above the land to the west, with no hand about it "
	                "and no effect round it. High above it, sparkles in the second player's colour come out one after "
	                "another and fall down slowly.",
	    .environment = {.clockRuns = false, .dispenserGrid = false, .networkGame = true},
	    .framing = {.shot = Shot::Placed, .eye = {-10.0f, 22.0f, -40.0f}, .look = {-20.0f, 20.0f, 0.0f}},
	    .commands = {{.kind = Kind::OtherGodHand,
	                  .player = PlayerNames::PLAYER_TWO,
	                  .delaySeconds = 1.0f,
	                  .point = {-20.0f, 0.0f},
	                  .heldSeed = SpellSeedType::Food,
	                  .radius = 5.0f}},
	});
	// A computer god whose mind stopped keeps its hands and its last camera, but it shows no sparkles
	all.push_back({
	    .id = "multiplayer.stopped_god_sparkles",
	    .name = "No sparkles of a computer god whose mind stopped",
	    .facet = Facet::Multiplayer,
	    .description = "The testbed acts as a network game. The land script switches on the computer mind of the second "
	                   "player and then switches it off again. After a second, the second player's hand takes the barrel "
	                   "to the west, and the second player reports the hand 5 m above the land and the camera 30 m above "
	                   "that hand, as in the scenario of another god's sparkles.",
	    .expected = "The barrel moves from where it stood to the west and stays in the air 5 m above the land, with no hand "
	                "about it. No sparkles come out above the barrel while the scenario runs: a computer god shows its "
	                "sparkles only while its mind plays.",
	    .environment = {.clockRuns = false, .dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}, .networkGame = true},
	    .framing = {.shot = Shot::Placed, .eye = {-10.0f, 22.0f, -40.0f}, .look = {-20.0f, 20.0f, 0.0f}},
	    .objects = {{.type = MobileObjectInfo::EgyptBarrel, .offset = {20.0f, 0.0f}}},
	    .landScript = k_StoppedComputerGod,
	    .commands = {{.kind = Kind::OtherGodHand,
	                  .player = PlayerNames::PLAYER_TWO,
	                  .delaySeconds = 1.0f,
	                  .point = {-20.0f, 0.0f},
	                  .object = 0,
	                  .radius = 5.0f}},
	});
	// The camera of a computer god starts above its temple, so its sparkles come out there before its hand moves
	all.push_back({
	    .id = "multiplayer.citadel_start_sparkles",
	    .name = "The sparkles of a computer god above its new temple",
	    .facet = Facet::Multiplayer,
	    .description = "The testbed acts as a network game. The land script makes the temple of the second player 90 m from "
	                   "the middle of the testbed, and then switches on the computer mind of that player. The mind has no "
	                   "town and does not move its hand. The camera of a god starts 20 m higher than the base of its temple, "
	                   "20 m nearer the middle of the testbed.",
	    .expected = "Sparkles in the second player's colour come out one after another at that point in front of the temple "
	                "and fall down slowly. They do not stop while the scenario runs.",
	    .environment = {.clockRuns = false, .dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}, .networkGame = true},
	    .framing = {.shot = Shot::Placed, .eye = {40.0f, 35.0f, -20.0f}, .look = {0.0f, 20.0f, -70.0f}},
	    .landScript = k_ComputerGodAtCitadel,
	});
	// The second god brings its player's creature at the start of a network game
	all.push_back({
	    .id = "multiplayer.brought_creature",
	    .name = "A god brings its player's creature into a network game",
	    .facet = Facet::Multiplayer,
	    .description = "A temple of player two stands to the north. After a second, the god of player two brings a creature "
	                   "as at the start of a network game. The mind file LethysCreature of the game takes the place of the "
	                   "creature file that the player of that god sends before the start.",
	    .expected = "A grown ape named Matey comes at the creature home of the temple of player two, at once and with no "
	                "sparkle. It belongs to player two, it knows every leash, and its mind and body come from the file.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-30.0f, 18.0f, 60.0f}, .look = {-14.0f, 6.0f, 88.0f}},
	    .temples = {{.offset = {0.0f, 90.0f}, .owner = PlayerNames::PLAYER_TWO}},
	    .commands = {{.kind = Kind::BringCreature,
	                  .player = PlayerNames::PLAYER_TWO,
	                  .delaySeconds = 1.0f,
	                  .mindFile = "LethysCreature"}},
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
