/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <string_view>
#include <vector>

#include <LobbiesJson.h>
#include <LobbyApiJson.h>
#include <SDL_keycode.h>
#include <gtest/gtest.h>

#include "Gui/LobbyScreenRules.h"
#include "Gui/TextDatabase.h"

using namespace openblack;
using namespace openblack::gui::lobby_screens;
using openblack::online::lobby::LobbyRoom;
using openblack::online::lobby::RoomPhase;
using openblack::online::lobby::SlotHolder;

namespace
{

constexpr std::string_view k_Build = "0.1.0+abc/codec34";
const lobby_api::AccountRef k_Host {.id = 1, .name = "marek"};
const lobby_api::AccountRef k_Me {.id = 3, .name = "gavxn"};

/// The host in slot 0, owl in slot 1, the player in slot 2, an open slot 3, a computer slot 4 and a closed slot 5. The
/// observer kim does not have the map.
constexpr std::string_view k_LobbyJson =
    R"({"id":"l1","code":"K7Q2MX9D","name":"Friday Ring","host":{"id":1,"name":"marek"},"visibility":"public",)"
    R"("locked":true,"map":{"file":"Online Maps/obm_32p_1.map","name":"Ring of gods","sha256":"ab","size":10},)"
    R"("gods":3,"god_slots":6,"open_slots":4,"free_slots":1,"observers":1,)"
    R"("slots":[{"index":0,"kind":"open","account":{"id":1,"name":"marek"}},)"
    R"({"index":1,"kind":"open","account":{"id":2,"name":"owl"}},{"index":2,"kind":"open","account":{"id":3,"name":"gavxn"}},)"
    R"({"index":3,"kind":"open","account":null},{"index":4,"kind":"computer","account":null},)"
    R"({"index":5,"kind":"closed","account":null}],)"
    R"("settings":{"time_limit_minutes":60,"conditions":[1,2,3],"observers_allowed":true},)"
    R"("build_id":"0.1.0+abc/codec34","state":"open","queue_position":null,"countdown_at":null,"started_at":null,)"
    R"("created":"2026-11-12T09:59:00Z",)"
    R"("members":[{"account":{"id":1,"name":"marek"},"role":"god","slot":0,"ready":false,"have_map":true,"away":false,)"
    R"("host":true,"joined_at":"2026-11-12T10:00:00Z"},)"
    R"({"account":{"id":2,"name":"owl"},"role":"god","slot":1,"ready":true,"have_map":true,"away":false,"host":false,)"
    R"("joined_at":"2026-11-12T10:00:01Z"},)"
    R"({"account":{"id":3,"name":"gavxn"},"role":"god","slot":2,"ready":false,"have_map":true,"away":false,"host":false,)"
    R"("joined_at":"2026-11-12T10:00:02Z"},)"
    R"({"account":{"id":9,"name":"kim"},"role":"observer","slot":null,"ready":false,"have_map":false,"away":false,)"
    R"("host":false,"joined_at":"2026-11-12T10:00:03Z"}],)"
    R"("you":null})";

lobby_api::Lobby TestLobby()
{
	auto lobby = lobby_api::ParseLobby(k_LobbyJson);
	EXPECT_TRUE(lobby.has_value());
	return lobby.value_or(lobby_api::Lobby {});
}

lobby_api::LobbySummary Summary(std::string name, std::string host, int32_t gods, lobby_api::LobbyState state)
{
	return {
	    .id = name,
	    .name = name,
	    .host = {.id = 1, .name = std::move(host)},
	    .map = {.file = "Online Maps/obm_32p_1.map", .name = "Ring", .sha256 = {}, .size = 0},
	    .gods = gods,
	    .godSlots = 8,
	    .openSlots = 8,
	    .freeSlots = 8 - gods,
	    .observers = 0,
	    .locked = false,
	    .state = state,
	    .queuePosition = std::nullopt,
	    .startedAt = std::nullopt,
	    .created = std::nullopt,
	    .buildId = std::string(k_Build),
	};
}

std::vector<std::string> Names(const std::vector<lobby_api::LobbySummary>& lobbies)
{
	std::vector<std::string> names;
	for (const auto& lobby : lobbies)
	{
		names.push_back(lobby.name);
	}
	return names;
}

std::string Utf8(std::u16string_view text)
{
	return gui::ToUtf8(text);
}

} // namespace

TEST(LobbyScreenRules, TheHeadingsSortUpThenDownThenStop)
{
	auto sort = ClickHeading({}, Column::Gods);
	EXPECT_EQ(sort.column, Column::Gods);
	EXPECT_FALSE(sort.descending);
	sort = ClickHeading(sort, Column::Gods);
	EXPECT_TRUE(sort.descending);
	EXPECT_EQ(Utf8(HeadingText(u"Players", Column::Gods, sort)), "Players v");
	sort = ClickHeading(sort, Column::Gods);
	EXPECT_FALSE(sort.column.has_value());
	// Another column sorts up first
	sort = ClickHeading(ClickHeading({}, Column::Gods), Column::Name);
	EXPECT_EQ(sort.column, Column::Name);
	EXPECT_FALSE(sort.descending);
	EXPECT_EQ(Utf8(HeadingText(u"Name", Column::Name, sort)), "Name ^");
	EXPECT_EQ(Utf8(HeadingText(u"Host", Column::Host, sort)), "Host");
}

TEST(LobbyScreenRules, TheListSortsAndFilters)
{
	const std::vector<lobby_api::LobbySummary> lobbies = {
	    Summary("beta", "Zed", 2, lobby_api::LobbyState::Open),
	    Summary("Alpha", "amy", 5, lobby_api::LobbyState::Open),
	    Summary("gamma", "bob", 3, lobby_api::LobbyState::InGame),
	};
	online::lobby::ListFilter filter;
	// The default order puts the session with more gods first
	EXPECT_EQ(Names(ShownLobbies(lobbies, filter, k_Build, {})), (std::vector<std::string> {"Alpha", "beta"}));
	filter.showInGame = true;
	EXPECT_EQ(Names(ShownLobbies(lobbies, filter, k_Build, {.column = Column::Name, .descending = false})),
	          (std::vector<std::string> {"Alpha", "beta", "gamma"}));
	EXPECT_EQ(Names(ShownLobbies(lobbies, filter, k_Build, {.column = Column::Host, .descending = false})),
	          (std::vector<std::string> {"Alpha", "gamma", "beta"}));
	EXPECT_EQ(Names(ShownLobbies(lobbies, filter, k_Build, {.column = Column::Gods, .descending = true})),
	          (std::vector<std::string> {"Alpha", "gamma", "beta"}));
	EXPECT_EQ(Names(ShownLobbies(lobbies, filter, k_Build, {.column = Column::Status, .descending = true})),
	          (std::vector<std::string> {"gamma", "Alpha", "beta"}));
}

TEST(LobbyScreenRules, TheStatusColumn)
{
	auto lobby = Summary("a", "amy", 2, lobby_api::LobbyState::Open);
	EXPECT_EQ(StatusOf(lobby, k_Build), Status::Open);
	EXPECT_EQ(Utf8(GodsText(lobby)), "2/8");
	lobby.freeSlots = 0;
	EXPECT_EQ(StatusOf(lobby, k_Build), Status::Full);
	lobby.state = lobby_api::LobbyState::Queued;
	lobby.queuePosition = 2;
	EXPECT_EQ(Utf8(StatusText(lobby, StatusOf(lobby, k_Build), 0, u"")), "Waiting for a server (2)");
	lobby.state = lobby_api::LobbyState::InGame;
	lobby.startedAt = 1'000;
	EXPECT_EQ(Utf8(StatusText(lobby, StatusOf(lobby, k_Build), 1'000 + (75 * 60'000), u"")), "In game 1:15");
	lobby.buildId = "0.2.0+def/codec34";
	EXPECT_EQ(StatusOf(lobby, k_Build), Status::WrongVersion);
	EXPECT_EQ(Utf8(StatusText(lobby, Status::WrongVersion, 0, u"(Wrong Version)")), "(Wrong Version)");
	lobby.locked = true;
	EXPECT_EQ(Utf8(NameText(lobby)), "# a");
}

TEST(LobbyScreenRules, AJoinAsksOrRefuses)
{
	auto lobby = Summary("a", "amy", 2, lobby_api::LobbyState::Open);
	EXPECT_EQ(JoinStepOf(lobby, k_Build), JoinStep::Join);
	lobby.locked = true;
	EXPECT_EQ(JoinStepOf(lobby, k_Build), JoinStep::AskPassword);
	// A full session and a launched session take no more players
	lobby.freeSlots = 0;
	EXPECT_EQ(JoinStepOf(lobby, k_Build), JoinStep::Refuse);
	EXPECT_EQ(RefusalOf(lobby, k_Build), "full");
	lobby.freeSlots = 3;
	lobby.state = lobby_api::LobbyState::Countdown;
	EXPECT_EQ(RefusalOf(lobby, k_Build), "started");
	lobby.state = lobby_api::LobbyState::Open;
	lobby.buildId = "other";
	EXPECT_EQ(RefusalOf(lobby, k_Build), "wrong_version");
}

TEST(LobbyScreenRules, EachProblemHasItsOwnBox)
{
	using ecs::systems::LobbyError;
	const auto of = [](std::string code, int32_t status = 409) {
		return ProblemOf(LobbyError {.status = status, .code = std::move(code)}, lobby_api::ClientKind::Desktop);
	};
	EXPECT_EQ(of("password_needed", 403).box, ProblemBox::AskPassword);
	EXPECT_EQ(of("wrong_password", 403).box, ProblemBox::IncorrectPassword);
	EXPECT_EQ(of("banned", 403).text, "You cannot join this game.");
	EXPECT_EQ(of("full").text, "This game is full.");
	EXPECT_EQ(of("none", 404).box, ProblemBox::AskCreate);
	EXPECT_EQ(of("", 404).text, "There is no such game.");
	// Only a problem of the network shows the vanilla box
	EXPECT_EQ(ProblemOf(LobbyError {.kind = ecs::systems::LobbyErrorKind::Timeout}, lobby_api::ClientKind::Desktop).box,
	          ProblemBox::ConnectionFailed);
	EXPECT_NE(GameRefusalText("version", lobby_api::ClientKind::Browser),
	          GameRefusalText("version", lobby_api::ClientKind::Desktop));
}

TEST(LobbyScreenRules, AMapWithTooFewPlacesNeedsKicksFirst)
{
	const LobbyRoom room(TestLobby(), k_Host);
	// The observer takes no place on the land
	EXPECT_EQ(PlayersOfRoom(room), 3);
	EXPECT_TRUE(MapFitsRoom(4, PlayersOfRoom(room)));
	EXPECT_TRUE(MapFitsRoom(3, PlayersOfRoom(room)));
	EXPECT_FALSE(MapFitsRoom(2, PlayersOfRoom(room)));
	EXPECT_EQ(TooManyPlayersText(2), "Remove players first: this land has 2 places.");
	// The service refuses the change too, and gives the slots of the map
	const auto problem =
	    ProblemOf(ecs::systems::LobbyError {.status = 409, .code = "too_many_players", .members = 3, .slots = 2},
	              lobby_api::ClientKind::Desktop);
	EXPECT_EQ(problem.box, ProblemBox::Notice);
	EXPECT_EQ(problem.text, "Remove players first: this land has 2 places.");
	EXPECT_EQ(lobby_api::ParseProblem(R"({"status":409,"code":"too_many_players","members":3,"slots":2})")->slots, 2);
}

TEST(LobbyScreenRules, AWarningMarksEachPlayerWithoutTheMap)
{
	LobbyRoom room(TestLobby(), k_Me);
	// The observer kim has no slot, but the Start button of the host warns about kim
	EXPECT_EQ(PlayersWithoutMap(room), (std::vector<std::string> {"kim"}));
	EXPECT_TRUE(StartShowsMapWarning(room));
	EXPECT_FALSE(ShowsMapWarning(room, room.Slots()[1]));
	room.Apply(
	    {.seq = 1, .type = "member.changed", .time = {}, .data = R"({"account":{"id":2,"name":"owl"},"have_map":false})"});
	EXPECT_EQ(PlayersWithoutMap(room), (std::vector<std::string> {"owl", "kim"}));
	EXPECT_TRUE(ShowsMapWarning(room, room.Slots()[1]));
	EXPECT_EQ(Utf8(SlotNote(room.Slots()[1])), "downloading map");
	EXPECT_FALSE(ShowsMapWarning(room, room.Slots()[3]));
	EXPECT_EQ(Utf8(std::u16string(k_MapWarningText)),
	          "This players needs to download the custom map first and may delay the start of play.");
	room.Apply(
	    {.seq = 2, .type = "member.changed", .time = {}, .data = R"({"account":{"id":2,"name":"owl"},"have_map":true})"});
	room.Apply(
	    {.seq = 3, .type = "member.changed", .time = {}, .data = R"({"account":{"id":9,"name":"kim"},"have_map":true})"});
	EXPECT_FALSE(ShowsMapWarning(room, room.Slots()[1]));
	EXPECT_FALSE(StartShowsMapWarning(room));
	EXPECT_TRUE(SlotNote(room.Slots()[1]).empty());

	// A land of the game needs no download, so no player shows the warning
	auto land = TestLobby();
	land.map.sha256.clear();
	LobbyRoom landRoom(land, k_Me);
	landRoom.Apply(
	    {.seq = 4, .type = "member.changed", .time = {}, .data = R"({"account":{"id":2,"name":"owl"},"have_map":false})"});
	EXPECT_FALSE(ShowsMapWarning(landRoom, landRoom.Slots()[1]));
	EXPECT_FALSE(StartShowsMapWarning(landRoom));
}

TEST(LobbyScreenRules, TheSlotLines)
{
	const LobbyRoom room(TestLobby(), k_Me);
	const SlotTexts texts {.god = u"God %d", .host = u" (HOST)", .current = u" (CURRENT)"};
	const auto slots = room.Slots();
	ASSERT_EQ(slots.size(), 6u);
	EXPECT_EQ(Utf8(SlotLine(slots[0], texts)), "God 1: marek (HOST)");
	EXPECT_EQ(Utf8(SlotLine(slots[2], texts)), "God 3: gavxn (CURRENT)");
	EXPECT_EQ(Utf8(SlotLine(slots[3], texts)), "God 4: - Open -");
	EXPECT_EQ(Utf8(SlotLine(slots[4], texts)), "God 5: - Computer -");
	EXPECT_EQ(Utf8(SlotLine(slots[5], texts)), "God 6: - Closed -");
}

TEST(LobbyScreenRules, TheHostActions)
{
	LobbyRoom host(TestLobby(), k_Host);
	EXPECT_TRUE(HostActionsOf(host, 0).empty());
	EXPECT_EQ(HostActionsOf(host, 1), (std::vector {HostAction::Kick, HostAction::Ban, HostAction::MakeHost}));
	EXPECT_EQ(HostActionsOf(host, 3), (std::vector {HostAction::Computer, HostAction::Closed}));
	EXPECT_EQ(HostActionsOf(host, 5), (std::vector {HostAction::Open, HostAction::Computer}));
	EXPECT_EQ(StartButtonOf(host), StartButton::Start);
	EXPECT_TRUE(LeaveAsks(host));
	// The other players have no host actions
	const LobbyRoom player(TestLobby(), k_Me);
	EXPECT_TRUE(HostActionsOf(player, 1).empty());
	EXPECT_EQ(StartButtonOf(player), StartButton::None);
	EXPECT_FALSE(LeaveAsks(player));
	// After the start the host can kick and ban, but cannot change the slots
	host.Apply({.seq = 1, .type = "game.assigned", .time = {}, .data = R"({"lobby":"l1","game_id":"g1","url":"wss://x/g1"})"});
	EXPECT_EQ(host.Phase(), RoomPhase::Assigned);
	EXPECT_EQ(HostActionsOf(host, 1), (std::vector {HostAction::Kick, HostAction::Ban, HostAction::MakeHost}));
	EXPECT_TRUE(HostActionsOf(host, 3).empty());
	EXPECT_EQ(StartButtonOf(host), StartButton::None);

	const auto kinds = SlotKindsAfter(TestLobby(), 3, lobby_api::SlotKind::Closed);
	ASSERT_EQ(kinds.size(), 6u);
	EXPECT_EQ(kinds[3], lobby_api::SlotKind::Closed);
	EXPECT_EQ(kinds[4], lobby_api::SlotKind::Computer);
}

TEST(LobbyScreenRules, TheCreateForm)
{
	EXPECT_EQ(Utf8(DefaultSessionName(u"gavxn")), "gavxn's game");
	EXPECT_EQ(DefaultSessionName(u"abcdefghijklmnopqrstuvwxyzabcdefgh").size(), lobby_api::k_MostLobbyNameCharacters);
	EXPECT_EQ(ClampGodSlots(40, 32), 32);
	EXPECT_EQ(ClampGodSlots(8, 4), 4);
	EXPECT_EQ(ClampGodSlots(1, 4), 2);
	EXPECT_EQ(ClampTimeLimit(5), lobby_api::k_LeastTimeLimitMinutes);

	CreateForm form {.name = u"  ", .map = std::nullopt};
	EXPECT_EQ(CreateProblem(form), "Type a name for the game.");
	form.name = u"Ring";
	EXPECT_EQ(CreateProblem(form), "Pick a map for the game.");
	form.map = 0;
	EXPECT_FALSE(CreateProblem(form).has_value());

	form.godSlots = 4;
	form.isPrivate = true;
	form.password = u"pw";
	const auto request = CreateRequestOf(form, {.file = "Online Maps/mpm_4p_1.map", .name = "Four", .sha256 = {}, .size = 0});
	EXPECT_EQ(request.name, "Ring");
	EXPECT_EQ(request.godSlots, 4);
	EXPECT_EQ(request.slotKinds.size(), 4u);
	EXPECT_EQ(request.visibility, lobby_api::LobbyVisibility::Private);
	EXPECT_EQ(request.password, "pw");
	EXPECT_EQ(MapNeedOf(request.map, true), MapNeed::Have);
	EXPECT_EQ(MapNeedOf(request.map, false), MapNeed::Missing);
	EXPECT_EQ(MapNeedOf({.file = "x", .name = {}, .sha256 = "ab", .size = 1}, false), MapNeed::Download);
}

TEST(LobbyScreenRules, TheTextFits)
{
	// Each character is 10 wide
	const MeasureText measure = [](std::u16string_view text) { return static_cast<float>(text.size() * 10); };
	EXPECT_EQ(Utf8(FitText(u"Friday", 60.0f, measure)), "Friday");
	EXPECT_EQ(Utf8(FitText(u"Friday Ring", 60.0f, measure)), "Fri...");
	EXPECT_TRUE(FitText(u"Friday", 20.0f, measure).empty());
}

TEST(LobbyScreenRules, TheKeys)
{
	EXPECT_EQ(KeyActionOf(KeyScreen::List, {.key = SDLK_F5}), KeyAction::Refresh);
	EXPECT_EQ(KeyActionOf(KeyScreen::List, {.key = SDLK_f, .control = true}), KeyAction::Search);
	EXPECT_EQ(KeyActionOf(KeyScreen::List, {.key = SDLK_j, .control = true}), KeyAction::JoinByCode);
	EXPECT_EQ(KeyActionOf(KeyScreen::List, {.key = SDLK_RETURN}), KeyAction::Join);
	EXPECT_EQ(KeyActionOf(KeyScreen::List, {.key = SDLK_RETURN, .editing = true}), KeyAction::None);
	EXPECT_EQ(KeyActionOf(KeyScreen::Room, {.key = SDLK_r}), KeyAction::ToggleReady);
	// The letter goes to the chat box
	EXPECT_EQ(KeyActionOf(KeyScreen::Room, {.key = SDLK_r, .editing = true}), KeyAction::None);
	EXPECT_EQ(KeyActionOf(KeyScreen::Room, {.key = SDLK_RETURN, .control = true, .editing = true}), KeyAction::Start);
	EXPECT_EQ(KeyActionOf(KeyScreen::Create, {.key = SDLK_ESCAPE, .editing = true}), KeyAction::Back);
}

TEST(LobbyScreenRules, TheRoomStatusAndTheServers)
{
	const LobbyRoom room(TestLobby(), k_Me);
	EXPECT_EQ(Utf8(RoomStatusText(room)), "Code: K7Q2-MX9D   Public   Password: yes");
	EXPECT_TRUE(ServersText(std::nullopt).empty());
	EXPECT_EQ(Utf8(ServersText(lobby_api::PoolState {.free = 2, .total = 5})), "Servers: 2 of 5 free");
	EXPECT_EQ(Utf8(ServersText(lobby_api::PoolState {.free = 0, .total = 5})),
	          "All servers are in use: your game will wait for a free one.");
}

TEST(LobbyScreenRules, TheLoadPauseNamesThePlayersThatLoad)
{
	constexpr int64_t k_Deadline = 1'000'000;
	EXPECT_TRUE(LoadPauseLines({}, k_Deadline, 0).empty());
	const std::vector<std::string> names {"owl", "kim"};
	// Two minutes before the end of the pause
	auto lines = LoadPauseLines(names, k_Deadline, k_Deadline - 120'000);
	ASSERT_EQ(lines.size(), 2u);
	EXPECT_EQ(Utf8(lines[0]), "The game is paused: waiting for remaining players to load (2:00)");
	EXPECT_EQ(Utf8(lines[1]), "owl, kim");
	// The time rounds up, so the line shows 0:00 only at the end
	EXPECT_EQ(Utf8(LoadPauseLines(names, k_Deadline, k_Deadline - 119'001)[0]),
	          "The game is paused: waiting for remaining players to load (2:00)");
	EXPECT_EQ(Utf8(LoadPauseLines(names, k_Deadline, k_Deadline - 119'000)[0]),
	          "The game is paused: waiting for remaining players to load (1:59)");
	EXPECT_EQ(Utf8(LoadPauseLines(names, k_Deadline, k_Deadline - 1)[0]),
	          "The game is paused: waiting for remaining players to load (0:01)");
	EXPECT_EQ(Utf8(LoadPauseLines(names, k_Deadline, k_Deadline + 5'000)[0]),
	          "The game is paused: waiting for remaining players to load (0:00)");
	EXPECT_EQ(LoadPauseSecondsLeft(k_Deadline, k_Deadline - 59'500), 60);
}
