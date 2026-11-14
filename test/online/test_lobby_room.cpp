/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LobbiesJson.h>
#include <LobbyApiJson.h>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "Online/LobbyNotices.h"
#include "Online/LobbyRoom.h"

// Enable this define because the test makes the system itself
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/LobbyRoomSystem.h"

using namespace openblack;
using namespace openblack::online::lobby;
using openblack::ecs::systems::LobbyCallback;
using openblack::ecs::systems::LobbyDone;
using openblack::ecs::systems::LobbyError;
using openblack::ecs::systems::LobbyErrorKind;
using openblack::ecs::systems::LobbyResult;
using openblack::online::HttpMethod;
using openblack::online::HttpResponse;

namespace
{

constexpr std::string_view k_Build = "0.1.0+abc/codec34";
const lobby_api::AccountRef k_Host {.id = 1, .name = "marek"};
const lobby_api::AccountRef k_Owl {.id = 2, .name = "owl"};
const lobby_api::AccountRef k_Me {.id = 3, .name = "gavxn"};

/// A lobby with the host in slot 0, owl in slot 1 and the player in slot 2. Slot 4 is for a computer god, and slot 5 is
/// closed. The text has the shape of the detail of the lobby that bwgame-service gives (LobbySerializer).
constexpr std::string_view k_LobbyJson =
    R"({"id":"l1","code":"K7Q2-MX9D","name":"Friday Ring","host":{"id":1,"name":"marek"},"visibility":"public",)"
    R"("locked":true,"map":{"file":"Online Maps/obm_32p_1.map","name":"Ring of gods","sha256":"ab","size":10},)"
    R"("gods":3,"god_slots":6,"open_slots":4,"free_slots":1,"observers":1,)"
    R"("slots":[{"index":0,"kind":"open","account":{"id":1,"name":"marek"}},)"
    R"({"index":1,"kind":"open","account":{"id":2,"name":"owl"}},{"index":2,"kind":"open","account":{"id":3,"name":"gavxn"}},)"
    R"({"index":3,"kind":"open","account":null},{"index":4,"kind":"computer","account":null},)"
    R"({"index":5,"kind":"closed","account":null}],)"
    R"("settings":{"time_limit_minutes":60,"conditions":[1,2,3],"observers_allowed":true},)"
    R"("build_id":"0.1.0+abc/codec34","state":"open","queue_position":null,"countdown_at":null,"started_at":null,)"
    R"("created":"2026-11-12T09:59:00.123456Z",)"
    R"("members":[{"account":{"id":1,"name":"marek"},"role":"god","slot":0,"ready":false,"have_map":true,"away":false,)"
    R"("host":true,"joined_at":"2026-11-12T10:00:00Z"},)"
    R"({"account":{"id":2,"name":"owl"},"role":"god","slot":1,"ready":true,"have_map":true,"away":false,"host":false,)"
    R"("joined_at":"2026-11-12T10:00:01Z"},)"
    R"({"account":{"id":3,"name":"gavxn"},"role":"god","slot":2,"ready":false,"have_map":true,"away":false,"host":false,)"
    R"("joined_at":"2026-11-12T10:00:02Z"},)"
    R"({"account":{"id":9,"name":"kim"},"role":"observer","slot":null,"ready":false,"have_map":false,"away":false,)"
    R"("host":false,"joined_at":"2026-11-12T10:00:03Z"}],)"
    R"("you":{"account":{"id":3,"name":"gavxn"},"role":"god","slot":2,"ready":false,"have_map":true,"away":false,)"
    R"("host":false,"joined_at":"2026-11-12T10:00:02Z"}})";

lobby_api::Lobby TestLobby()
{
	auto lobby = lobby_api::ParseLobby(k_LobbyJson);
	EXPECT_TRUE(lobby.has_value());
	return lobby.value_or(lobby_api::Lobby {});
}

lobby_api::PushEvent Event(std::string_view type, std::string data)
{
	static int64_t seq = 0;
	return {.seq = ++seq, .type = std::string(type), .time = "2026-11-12T10:00:00Z", .data = std::move(data)};
}

std::string Account(const lobby_api::AccountRef& account)
{
	return fmt::format(R"({{"id":{},"name":"{}"}})", account.id, account.name);
}

std::vector<std::string> LineTexts(const LobbyRoom& room)
{
	std::vector<std::string> texts;
	for (const auto& line : room.Lines())
	{
		texts.push_back(RoomLineText(line));
	}
	return texts;
}

/// 2026-11-12T10:00:00Z in milliseconds
constexpr int64_t k_TenOClock = 1'794'477'600'000;

} // namespace

TEST(LobbiesJson, TheLobbyReads)
{
	const auto lobby = TestLobby();
	EXPECT_EQ(lobby.id, "l1");
	EXPECT_EQ(lobby.host.name, "marek");
	EXPECT_TRUE(lobby.locked);
	EXPECT_EQ(lobby.godSlots, 6);
	ASSERT_EQ(lobby.slotKinds.size(), 6u);
	EXPECT_EQ(lobby.slotKinds[4], lobby_api::SlotKind::Computer);
	EXPECT_EQ(lobby.settings.timeLimitMinutes, 60);
	EXPECT_EQ(lobby.settings.conditions, (std::vector<int32_t> {1, 2, 3}));
	ASSERT_EQ(lobby.members.size(), 4u);
	EXPECT_EQ(lobby.members[0].joinedAt, k_TenOClock);
	EXPECT_EQ(lobby.members[3].role, lobby_api::MemberRole::Observer);
	EXPECT_FALSE(lobby.members[3].slot.has_value());
	EXPECT_TRUE(lobby.members[0].host);
	EXPECT_FALSE(lobby.members[3].haveMap);
	EXPECT_EQ(lobby.code, "K7Q2-MX9D");
	// A slot entry with an index out of the slots is not a lobby
	EXPECT_FALSE(lobby_api::ParseLobby(
	                 std::string(k_LobbyJson).replace(std::string(k_LobbyJson).find(R"("index":5)"), 9, R"("index":6)"))
	                 .has_value());
	EXPECT_FALSE(lobby_api::ParseLobby(R"({"id":"l1"})").has_value());
	EXPECT_FALSE(lobby_api::ParseLobby(std::string(k_LobbyJson).replace(0, 1, "[")).has_value());
}

TEST(LobbiesJson, TheListAndItsEventsRead)
{
	const auto list = lobby_api::ParseLobbyList(
	    R"({"lobbies":[{"id":"a","name":"A","host":{"id":1,"name":"h"},"map":{"file":"m","name":"","sha256":"","size":0},)"
	    R"("gods":1,"god_slots":2,"open_slots":2,"free_slots":1,"observers":0,"locked":false,"state":"queued",)"
	    R"("queue_position":2,"started_at":null,"build_id":"b","created":"2026-11-12T10:00:00+00:00"}],)"
	    R"("pool":{"free":0,"total":1}})");
	ASSERT_TRUE(list.has_value());
	ASSERT_EQ(list->lobbies.size(), 1u);
	EXPECT_EQ(list->lobbies[0].state, lobby_api::LobbyState::Queued);
	EXPECT_EQ(list->lobbies[0].queuePosition, 2);
	EXPECT_EQ(list->lobbies[0].freeSlots, 1);
	EXPECT_EQ(list->lobbies[0].created, k_TenOClock);
	ASSERT_TRUE(list->pool.has_value());
	EXPECT_EQ(list->pool->total, 1);
	EXPECT_TRUE(list->lobbies[0].players.empty());
	EXPECT_FALSE(lobby_api::ParseLobbyList(R"({"lobbies":[{"id":"a"}]})").has_value());
	// A game that plays gives the names of its players
	const auto playing = lobby_api::ParseLobbyList(
	    R"({"lobbies":[{"id":"a","name":"A","host":{"id":1,"name":"h"},"map":{"file":"m"},"gods":2,"god_slots":2,)"
	    R"("players":[{"id":1,"name":"h"},{"id":7,"name":"owl"}],"observers_allowed":false,"state":"in_game",)"
	    R"("started_at":"2026-11-12T10:00:00+00:00","build_id":"b"}]})");
	ASSERT_TRUE(playing.has_value());
	ASSERT_EQ(playing->lobbies.at(0).players.size(), 2u);
	EXPECT_EQ(playing->lobbies[0].players[1].name, "owl");
	EXPECT_EQ(playing->lobbies[0].players[1].id, 7);
	EXPECT_EQ(playing->lobbies[0].startedAt, k_TenOClock);
	EXPECT_FALSE(lobby_api::ParseLobbyList(R"({"lobbies":[{"id":"a","name":"A","host":{"id":1,"name":"h"},)"
	                                       R"("map":{"file":"m"},"gods":1,"god_slots":2,"players":[1],"state":"open"}]})")
	                 .has_value());

	const auto removed =
	    lobby_api::ParseLobbyListChange(lobby_api::lobby_events::k_LobbiesRemoved, R"({"id":"a","reason":"host_closed"})");
	ASSERT_TRUE(removed.has_value());
	EXPECT_FALSE(removed->summary.has_value());
	EXPECT_EQ(removed->reason, "host_closed");
	// "lobbies.changed" holds the entry of the list in "lobby"
	const auto changed = lobby_api::ParseLobbyListChange(
	    lobby_api::lobby_events::k_LobbiesChanged,
	    R"({"lobby":{"id":"a","name":"A","host":{"id":1,"name":"h"},"map":{"file":"m"},"gods":1,"god_slots":2,)"
	    R"("state":"open"}})");
	ASSERT_TRUE(changed.has_value());
	EXPECT_EQ(changed->id, "a");
	ASSERT_TRUE(changed->summary.has_value());
	EXPECT_FALSE(lobby_api::ParseLobbyListChange(lobby_api::lobby_events::k_LobbiesChanged, R"({"id":"a"})").has_value());
}

TEST(LobbiesJson, TheEventsOfTheServiceRead)
{
	const auto change =
	    lobby_api::ParseLobbyChange(R"({"lobby":"l1","fields":["map","settings"],"ready_cleared":true,"map_cleared":true})");
	ASSERT_TRUE(change.has_value());
	EXPECT_EQ(change->fields, (std::vector<std::string> {"map", "settings"}));
	EXPECT_TRUE(change->readyCleared);
	EXPECT_TRUE(change->mapCleared);
	EXPECT_FALSE(lobby_api::ParseLobbyChange(R"({"lobby":"l1","fields":{"name":"x"}})").has_value());

	const auto member = lobby_api::ParseMemberEvent(
	    R"({"lobby":"l1","account":{"id":2,"name":"owl"},"ready":false,"slot":1,"have_map":false})");
	ASSERT_TRUE(member.has_value());
	EXPECT_EQ(member->haveMap, false);
	const auto left = lobby_api::ParseMemberEvent(R"({"lobby":"l1","account":{"id":2,"name":"owl"},"reason":"left"})");
	ASSERT_TRUE(left.has_value());
	EXPECT_EQ(left->reason, "left");

	const auto kick = lobby_api::ParseLobbyKick(R"({"lobby":"l1","by":{"id":1,"name":"marek"},"banned":true})");
	ASSERT_TRUE(kick.has_value());
	EXPECT_TRUE(kick->banned);
	ASSERT_TRUE(kick->by.has_value());
	EXPECT_EQ(kick->by->name, "marek");
	// An administrator removes an account with no host
	const auto byAdmin = lobby_api::ParseLobbyKick(R"({"lobby":"l1","by":null,"banned":false})");
	ASSERT_TRUE(byAdmin.has_value());
	EXPECT_FALSE(byAdmin->by.has_value());

	const auto assigned = lobby_api::ParseLobbyStateEvent(R"({"lobby":"l1","game_id":"g7","url":"wss://x/game/g7"})");
	ASSERT_TRUE(assigned.has_value());
	EXPECT_EQ(assigned->url, "wss://x/game/g7");

	const auto ticket = lobby_api::ParseLobbyTicket(
	    R"({"url":"wss://x/game/g7","ticket":"v4.public.t","expires":"2026-11-12T10:01:00Z","role":"god","game_id":"g7"})");
	ASSERT_TRUE(ticket.has_value());
	EXPECT_EQ(ticket->gameId, "g7");
	EXPECT_EQ(ticket->ticket.ticket, "v4.public.t");
}

TEST(LobbiesJson, TheRequestsWrite)
{
	lobby_api::LobbyCreateRequest create {
	    .name = "g",
	    .map = {.file = "m", .sha256 = "ab"},
	    .godSlots = 2,
	    .slotKinds = {lobby_api::SlotKind::Open, lobby_api::SlotKind::Computer},
	    .password = "pw",
	    .settings = {.timeLimitMinutes = 30, .conditions = {1}},
	    .buildId = "b",
	};
	EXPECT_EQ(lobby_api::ToJson(create),
	          R"({"build_id":"b","god_slots":2,"map":{"file":"m","sha256":"ab"},"name":"g","password":"pw",)"
	          R"("settings":{"conditions":[1],"observers_allowed":true,"time_limit_minutes":30},)"
	          R"("slot_kinds":["open","computer"],"visibility":"public"})");
	// An empty password is no password
	create.password = "";
	EXPECT_EQ(lobby_api::ToJson(create).find("password"), std::string::npos);

	EXPECT_EQ(lobby_api::ToJson(lobby_api::LobbyJoinRequest {.password = "pw", .buildId = "b", .slot = 3}),
	          R"({"build_id":"b","password":"pw","role":"god","slot":3})");
	EXPECT_EQ(lobby_api::ToJson(lobby_api::LobbyJoinRequest {
	              .buildId = "b", .buildHash = "cd", .map = lobby_api::MapReport {.haveMap = true, .sha256 = "ab"}}),
	          R"({"build_hash":"cd","build_id":"b","have_map":true,"map_sha256":"ab","role":"god"})");
	EXPECT_EQ(lobby_api::ToJson(lobby_api::MemberChangeRequest {.ready = true}), R"({"ready":true})");
	// A land of the game has no hash: the report has have_map only
	EXPECT_EQ(lobby_api::ToJson(lobby_api::MemberChangeRequest {.map = lobby_api::MapReport {.haveMap = false}}),
	          R"({"have_map":false})");
	// A change with an empty password removes the password
	EXPECT_EQ(lobby_api::ToJson(lobby_api::LobbyChangeRequest {.password = ""}), R"({"password":""})");
	EXPECT_EQ(lobby_api::ToJson(lobby_api::QuickJoinRequest {.buildId = "b"}), R"({"build_id":"b"})");
}

TEST(LobbiesJson, TheCodeOfAPrivateLobby)
{
	EXPECT_EQ(lobby_api::NormalLobbyCode("k7q2-mx9d"), "K7Q2MX9D");
	// Crockford base32 reads O as 0 and I or L as 1
	EXPECT_EQ(lobby_api::NormalLobbyCode("O0IL-1111"), "00111111");
	EXPECT_FALSE(lobby_api::NormalLobbyCode("K7Q2-MX9").has_value());
	EXPECT_FALSE(lobby_api::NormalLobbyCode("K7Q2-MX9U").has_value());
	EXPECT_FALSE(lobby_api::NormalLobbyCode("../K7Q2MX9D").has_value());
	EXPECT_EQ(lobby_api::LobbyCodeText("K7Q2MX9D"), "K7Q2-MX9D");
}

TEST(LobbyEntry, ASignedInPlayerGoesToTheLobbyList)
{
	EXPECT_EQ(FirstBox(true, false), EntryBox::LobbyList);
	EXPECT_EQ(FirstBox(false, true), EntryBox::LobbyList);
	EXPECT_EQ(FirstBox(false, false), EntryBox::FindGames);
}

TEST(LobbyRoom, TheSlotsShowThePlayersAndTheKinds)
{
	const LobbyRoom room(TestLobby(), k_Me);
	const auto slots = room.Slots();
	ASSERT_EQ(slots.size(), 6u);
	EXPECT_EQ(slots[0].holder, SlotHolder::Player);
	EXPECT_TRUE(slots[0].host);
	EXPECT_TRUE(slots[1].ready);
	EXPECT_TRUE(slots[2].own);
	EXPECT_EQ(slots[3].holder, SlotHolder::Open);
	EXPECT_EQ(slots[4].holder, SlotHolder::Computer);
	EXPECT_EQ(slots[5].holder, SlotHolder::Closed);
	ASSERT_EQ(room.Observers().size(), 1u);
	EXPECT_EQ(room.Observers()[0].name, "kim");
	EXPECT_EQ(room.Channel(), "lobby-l1");
	EXPECT_FALSE(room.IsHost());
	EXPECT_EQ(room.GodsInSlots(), 3);
}

TEST(LobbyRoom, ReadyAndSlotRules)
{
	LobbyRoom room(TestLobby(), k_Me);
	EXPECT_TRUE(room.CanReady());
	EXPECT_TRUE(room.CanPickSlot(3));
	// A slot of a player, a computer slot, a closed slot and a slot out of the lobby
	EXPECT_FALSE(room.CanPickSlot(1));
	EXPECT_FALSE(room.CanPickSlot(4));
	EXPECT_FALSE(room.CanPickSlot(5));
	EXPECT_FALSE(room.CanPickSlot(6));
	room.Apply(Event("member.changed", fmt::format(R"({{"account":{},"ready":true}})", Account(k_Me))));
	EXPECT_TRUE(room.OwnMember()->ready);
	// A ready player stays in the slot
	EXPECT_FALSE(room.CanPickSlot(3));
}

TEST(LobbyRoom, ReadyNeedsTheMap)
{
	LobbyRoom room(TestLobby(), k_Me);
	room.Apply(Event("member.changed", fmt::format(R"({{"account":{},"ready":false,"have_map":false}})", Account(k_Me))));
	EXPECT_FALSE(room.CanReady());
	EXPECT_FALSE(room.Slots()[2].haveMap);
	room.Apply(Event("member.changed", fmt::format(R"({{"account":{},"have_map":true}})", Account(k_Me))));
	EXPECT_TRUE(room.CanReady());
	// A new map clears the map of each member
	room.Apply(Event("lobby.changed", R"({"fields":["map"],"ready_cleared":true,"map_cleared":true})"));
	EXPECT_FALSE(room.CanReady());
	EXPECT_TRUE(room.TakeReloadWanted());
	EXPECT_FALSE(room.TakeReloadWanted());
	// A host with no map cannot start
	LobbyRoom host(TestLobby(), k_Host);
	host.Apply(Event("member.changed", fmt::format(R"({{"account":{},"ready":true}})", Account(k_Me))));
	host.Apply(Event("member.changed", fmt::format(R"({{"account":{},"have_map":false}})", Account(k_Host))));
	EXPECT_FALSE(host.CanStart());
}

TEST(LobbyRoom, TheHostStartsWhenEachOtherGodIsReady)
{
	LobbyRoom host(TestLobby(), k_Host);
	EXPECT_TRUE(host.IsHost());
	// gavxn is not ready
	EXPECT_FALSE(host.CanStart());
	host.Apply(Event("member.changed", fmt::format(R"({{"account":{},"ready":true}})", Account(k_Me))));
	// The ready of the host comes with the start, and the observer needs none
	EXPECT_TRUE(host.CanStart());
	const LobbyRoom player(TestLobby(), k_Me);
	EXPECT_FALSE(player.CanStart());
}

TEST(LobbyRoom, MemberEventsWriteSystemLines)
{
	LobbyRoom room(TestLobby(), k_Me);
	room.Apply(Event("member.joined", R"({"account":{"id":5,"name":"Sunfish"},"role":"god","slot":3})"));
	ASSERT_NE(room.FindMember(5), nullptr);
	EXPECT_EQ(room.FindMember(5)->slot, 3);
	room.Apply(Event("member.kicked", fmt::format(R"({{"account":{{"id":5,"name":"Sunfish"}},"by":{}}})", Account(k_Host))));
	EXPECT_EQ(room.FindMember(5), nullptr);
	room.Apply(Event("member.left", fmt::format(R"({{"account":{}}})", Account(k_Owl))));
	room.Apply(Event("member.banned", R"({"account":{"id":9,"name":"kim"}})"));
	room.Apply(Event("host.changed", fmt::format(R"({{"account":{},"by":{},"line":"gavxn is now the host."}})", Account(k_Me),
	                                             Account(k_Host))));
	EXPECT_TRUE(room.IsHost());
	EXPECT_TRUE(room.OwnMember()->host);
	EXPECT_FALSE(room.FindMember(k_Host.id)->host);
	EXPECT_EQ(LineTexts(room), (std::vector<std::string> {
	                               "Sunfish joined the game.",
	                               "Sunfish was kicked by marek.",
	                               "owl left the game.",
	                               "kim was banned by marek.",
	                               "gavxn is now the host.",
	                           }));
	EXPECT_EQ(room.Phase(), RoomPhase::Open);
}

TEST(LobbyRoom, TheLobbyEndsWhenTheHostGoesBeforeTheStart)
{
	LobbyRoom room(TestLobby(), k_Me);
	room.Apply(Event("member.left", fmt::format(R"({{"account":{}}})", Account(k_Host))));
	EXPECT_EQ(room.Phase(), RoomPhase::Ended);
	ASSERT_TRUE(room.End().has_value());
	EXPECT_EQ(room.End()->reason, EndReason::HostLeft);
	EXPECT_EQ(EndMessage(*room.End(), lobby_api::ClientKind::Desktop), "The channel host left the channel that you were in.");
	// The room ignores events after its end
	const auto lines = room.Lines().size();
	room.Apply(Event("member.joined", R"({"account":{"id":5,"name":"Sunfish"}})"));
	EXPECT_EQ(room.Lines().size(), lines);

	LobbyRoom closed(TestLobby(), k_Me);
	closed.Apply(Event("lobby.closed", R"({"reason":"host_away"})"));
	EXPECT_EQ(closed.End()->reason, EndReason::HostLeft);
	LobbyRoom left(TestLobby(), k_Me);
	left.Apply(Event("lobby.closed", R"({"lobby":"l1","reason":"host_left"})"));
	EXPECT_EQ(left.End()->reason, EndReason::HostLeft);
}

TEST(LobbyRoom, AfterTheStartTheHostLeavesAsAnyPlayer)
{
	LobbyRoom room(TestLobby(), k_Me);
	room.Apply(Event("game.assigned", R"({"game_id":"g1"})"));
	room.Apply(Event("game.started", "{}"));
	EXPECT_EQ(room.Phase(), RoomPhase::InGame);
	room.Apply(Event("member.left", fmt::format(R"({{"account":{}}})", Account(k_Host))));
	EXPECT_EQ(room.Phase(), RoomPhase::InGame);
	room.Apply(Event("game.ended", "{}"));
	EXPECT_EQ(room.End()->reason, EndReason::GameEnded);
	EXPECT_FALSE(EndMessage(*room.End(), lobby_api::ClientKind::Desktop).has_value());
}

TEST(LobbyRoom, AKickedPlayerGoesBackWithTheVanillaText)
{
	LobbyRoom room(TestLobby(), k_Me);
	room.Apply(Event("member.kicked", fmt::format(R"({{"account":{},"by":{}}})", Account(k_Me), Account(k_Host))));
	ASSERT_EQ(room.Phase(), RoomPhase::Ended);
	EXPECT_EQ(room.End()->reason, EndReason::Kicked);
	EXPECT_EQ(EndMessage(*room.End(), lobby_api::ClientKind::Browser), "marek has kicked you from the game.");

	// The service tells the kicked account with "lobby.kicked", also after the start
	LobbyRoom inGame(TestLobby(), k_Me);
	inGame.Apply(Event("game.assigned", R"({"lobby":"l1","game_id":"g1","url":"wss://x/game/g1"})"));
	EXPECT_EQ(inGame.GameUrl(), "wss://x/game/g1");
	inGame.Apply(Event("lobby.kicked", R"({"lobby":"l2","by":{"id":1,"name":"marek"},"banned":true})"));
	EXPECT_EQ(inGame.Phase(), RoomPhase::Assigned);
	inGame.Apply(Event("lobby.kicked", R"({"lobby":"l1","by":{"id":1,"name":"marek"},"banned":true})"));
	ASSERT_EQ(inGame.Phase(), RoomPhase::Ended);
	EXPECT_EQ(inGame.End()->reason, EndReason::Banned);
	EXPECT_EQ(inGame.End()->by, "marek");
	EXPECT_EQ(inGame.OwnMember(), nullptr);
}

TEST(LobbyRoom, TheCloseReasons)
{
	const auto endOf = [](std::string_view reason, const lobby_api::AccountRef& self) {
		LobbyRoom room(TestLobby(), self);
		room.Apply(Event("lobby.closed", fmt::format(R"({{"reason":"{}"}})", reason)));
		return room.End().value_or(RoomEnd {}).reason;
	};
	EXPECT_EQ(endOf("host_closed", k_Me), EndReason::HostClosed);
	EXPECT_EQ(endOf("host_closed", k_Host), EndReason::OwnClose);
	EXPECT_EQ(endOf("new_version", k_Me), EndReason::NewVersion);
	EXPECT_EQ(endOf("server_stopped", k_Me), EndReason::GameFailed);
	EXPECT_EQ(endOf("ended", k_Me), EndReason::GameEnded);
	EXPECT_EQ(endOf("expired", k_Me), EndReason::Closed);
	EXPECT_EQ(EndMessage({.reason = EndReason::HostClosed}, lobby_api::ClientKind::Desktop), "The host closed the game.");
	EXPECT_EQ(EndMessage({.reason = EndReason::NewVersion}, lobby_api::ClientKind::Browser),
	          "A new version of openblack is live. Reload the page.");
	EXPECT_FALSE(EndMessage({.reason = EndReason::OwnClose}, lobby_api::ClientKind::Desktop).has_value());
	// An event of a different lobby does nothing
	LobbyRoom room(TestLobby(), k_Me);
	room.Apply(Event("lobby.closed", R"({"lobby":"l2","reason":"host_closed"})"));
	EXPECT_EQ(room.Phase(), RoomPhase::Open);
}

TEST(LobbyRoom, ChangesClearTheReady)
{
	LobbyRoom room(TestLobby(), k_Me);
	// The event names the fields: the ready clears at once, and the lines come with the lobby that the room gets again
	room.Apply(Event("lobby.changed", R"({"lobby":"l1","fields":["map"],"ready_cleared":true,"map_cleared":true})"));
	EXPECT_FALSE(room.FindMember(2)->ready);
	EXPECT_TRUE(LineTexts(room).empty());
	ASSERT_TRUE(room.TakeReloadWanted());
	auto lobby = TestLobby();
	lobby.map = {.file = "Online Maps/mpm_2p_1.map", .name = "Two Gods"};
	room.Reload(lobby);
	EXPECT_EQ(room.Data().map.name, "Two Gods");
	room.Apply(Event("lobby.changed", R"({"lobby":"l1","fields":["settings"],"ready_cleared":true,"map_cleared":false})"));
	lobby.settings.timeLimitMinutes = 90;
	room.Reload(lobby);
	EXPECT_EQ(room.Data().settings.timeLimitMinutes, 90);
	room.Apply(Event("lobby.changed", R"({"lobby":"l1","fields":["password"],"ready_cleared":false})"));
	lobby.locked = false;
	room.Reload(lobby);
	EXPECT_FALSE(room.Data().locked);
	// A reload with no change writes no line
	room.Reload(lobby);
	EXPECT_EQ(LineTexts(room), (std::vector<std::string> {
	                               "The host changed the map to Two Gods.",
	                               "Ready was cleared.",
	                               "The host changed the settings.",
	                               "Ready was cleared.",
	                               "The host changed the settings.",
	                           }));
}

TEST(LobbyRoom, EachChangeThatSetsTheMapCountsAsAMapSet)
{
	LobbyRoom room(TestLobby(), k_Me);
	EXPECT_EQ(room.MapSets(), 0u);
	// A failed download starts again at the next map set: a new map, or the same map that the service sets again
	room.Apply(Event("lobby.changed", R"({"lobby":"l1","fields":["map","slots"],"ready_cleared":true,"map_cleared":false})"));
	EXPECT_EQ(room.MapSets(), 1u);
	room.Apply(Event("lobby.changed", R"({"lobby":"l1","fields":["map"],"ready_cleared":true,"map_cleared":true})"));
	EXPECT_EQ(room.MapSets(), 2u);
	// A change of other settings does not set the map
	room.Apply(Event("lobby.changed", R"({"lobby":"l1","fields":["settings"],"ready_cleared":true})"));
	EXPECT_EQ(room.MapSets(), 2u);
}

TEST(LobbyRoom, TheCountdownWritesEachSecond)
{
	LobbyRoom room(TestLobby(), k_Me);
	room.Apply(Event("start.countdown", R"({"at":"2026-11-12T10:00:05Z"})"));
	EXPECT_EQ(room.Phase(), RoomPhase::Countdown);
	EXPECT_EQ(room.CountdownAt(), k_TenOClock + 5000);
	room.Tick(k_TenOClock);
	room.Tick(k_TenOClock + 200);
	room.Tick(k_TenOClock + 1500);
	// A long frame skips to the second of the time
	room.Tick(k_TenOClock + 3900);
	room.Tick(k_TenOClock + 4999);
	room.Tick(k_TenOClock + 5000);
	EXPECT_EQ(LineTexts(room), (std::vector<std::string> {
	                               "The game starts in 5...",
	                               "The game starts in 4...",
	                               "The game starts in 2...",
	                               "The game starts in 1...",
	                           }));
	room.Apply(Event("start.cancelled", fmt::format(R"({{"why":"not_ready","account":{}}})", Account(k_Owl))));
	EXPECT_EQ(room.Phase(), RoomPhase::Open);
	EXPECT_FALSE(room.CountdownAt().has_value());
	EXPECT_EQ(LineTexts(room).back(), "The start was cancelled: owl is not ready.");
	room.Apply(Event("start.cancelled", R"({"why":"server_failed"})"));
	EXPECT_EQ(LineTexts(room).back(), "The game could not start.");
	room.Apply(Event("start.cancelled", R"({"why":"not_enough_joined"})"));
	EXPECT_EQ(LineTexts(room).back(), "Not enough players joined the server.");
	room.Apply(Event("start.cancelled", R"({"why":"host_cancelled"})"));
	EXPECT_EQ(LineTexts(room).back(), "The start was cancelled.");
}

TEST(LobbyRoom, TheQueueShowsThePlace)
{
	LobbyRoom room(TestLobby(), k_Me);
	room.Apply(Event("start.queued", R"({"position":2})"));
	room.Apply(Event("start.queued", R"({"position":2})"));
	room.Apply(Event("start.queued", R"({"position":1})"));
	EXPECT_EQ(room.Phase(), RoomPhase::Queued);
	EXPECT_EQ(room.QueuePosition(), 1);
	EXPECT_EQ(LineTexts(room), (std::vector<std::string> {
	                               "Waiting for a free server: you are number 2 in the line.",
	                               "Waiting for a free server: you are number 1 in the line.",
	                           }));
	room.Apply(Event("start.countdown", R"({"at":"2026-11-12T10:00:05Z"})"));
	EXPECT_FALSE(room.QueuePosition().has_value());
	room.Apply(Event("game.assigned", R"({"game_id":"g7"})"));
	EXPECT_EQ(room.Phase(), RoomPhase::Assigned);
	EXPECT_EQ(room.GameId(), "g7");
}

TEST(LobbyRoom, ChatLinesOfTheRoom)
{
	LobbyRoom room(TestLobby(), k_Me);
	room.Apply(Event("chat.line", R"({"channel":"lobby-l1","from":{"id":2,"name":"owl"},"text":"hi all"})"));
	room.Apply(Event("chat.line", R"({"channel":"lobby-l1","from":{"id":3,"name":"gavxn"},"text":"ready in a sec"})"));
	room.Apply(Event("chat.line", R"({"channel":"lobby","from":{"id":2,"name":"owl"},"text":"elsewhere"})"));
	EXPECT_EQ(LineTexts(room), (std::vector<std::string> {"owl: hi all", "gavxn> ready in a sec"}));
	for (size_t line = 0; line < k_MostRoomLines + 3; ++line)
	{
		room.Apply(Event("chat.line", R"({"channel":"lobby-l1","from":{"id":2,"name":"owl"},"text":"x"})"));
	}
	EXPECT_EQ(room.Lines().size(), k_MostRoomLines);
}

TEST(LobbyRoom, AReloadKeepsTheLines)
{
	LobbyRoom room(TestLobby(), k_Me);
	room.Apply(Event("chat.line", R"({"channel":"lobby-l1","from":{"id":2,"name":"owl"},"text":"hi"})"));
	auto lobby = TestLobby();
	lobby.state = lobby_api::LobbyState::Queued;
	lobby.queuePosition = 3;
	room.Reload(lobby);
	EXPECT_EQ(room.Phase(), RoomPhase::Queued);
	EXPECT_EQ(room.Lines().size(), 1u);
	// A snapshot with no member for the player ends the room
	std::erase_if(lobby.members, [](const auto& member) { return member.account.id == k_Me.id; });
	room.Reload(lobby);
	EXPECT_EQ(room.Phase(), RoomPhase::Ended);
}

namespace
{

lobby_api::LobbySummary Summary(std::string id, int32_t gods, int32_t slots, lobby_api::LobbyState state, int64_t created,
                                std::string build = std::string(k_Build))
{
	return {
	    .id = id,
	    .name = "Game " + id,
	    .host = {.id = 1, .name = "Host" + id},
	    .map = {.file = "m" + id, .name = "Map " + id},
	    .gods = gods,
	    .godSlots = slots,
	    .state = state,
	    .created = created,
	    .buildId = std::move(build),
	};
}

} // namespace

TEST(LobbyList, TheFilterAndTheOrder)
{
	using lobby_api::LobbyState;
	const std::vector lobbies {
	    Summary("a", 1, 2, LobbyState::Open, 1),   Summary("b", 4, 4, LobbyState::Open, 2),
	    Summary("c", 3, 8, LobbyState::Open, 3),   Summary("d", 3, 8, LobbyState::Open, 4),
	    Summary("e", 2, 2, LobbyState::InGame, 5), Summary("f", 1, 2, LobbyState::Open, 6, "0.1.0+old/codec33"),
	    Summary("g", 6, 8, LobbyState::InGame, 7),
	};
	const auto ids = [](const std::vector<lobby_api::LobbySummary>& list) {
		std::string text;
		for (const auto& lobby : list)
		{
			text += lobby.id;
		}
		return text;
	};
	// Open first, then the other lobbies before their start, then the games that play. In each group the most gods
	// come first, then the newest.
	EXPECT_EQ(ids(VisibleLobbies(lobbies, {}, k_Build)), "dcabge");
	EXPECT_EQ(ids(VisibleLobbies(lobbies, {.showInGame = false}, k_Build)), "dcab");
	EXPECT_EQ(ids(VisibleLobbies(lobbies, {.showFull = false, .showInGame = false}, k_Build)), "dca");
	EXPECT_EQ(ids(VisibleLobbies(lobbies, {.showFull = false}, k_Build)), "dcag");
	EXPECT_EQ(ids(VisibleLobbies(lobbies, {.showInGame = true, .showOtherVersions = true}, k_Build)), "dcfabge");
	EXPECT_EQ(ids(VisibleLobbies(lobbies, {.search = "HOSTC"}, k_Build)), "c");
	EXPECT_EQ(ids(VisibleLobbies(lobbies, {.search = "map d"}, k_Build)), "d");
	EXPECT_EQ(ids(VisibleLobbies(lobbies, {.mapFile = "ma"}, k_Build)), "a");
	EXPECT_TRUE(IsWrongVersion(lobbies[5], k_Build));
	EXPECT_FALSE(CanJoin(lobbies[5], k_Build));
	EXPECT_FALSE(CanJoin(lobbies[1], k_Build));
	EXPECT_TRUE(CanJoin(lobbies[0], k_Build));
	// The computer slots and the closed slots take no player: the service gives the free slots
	auto computers = lobbies[0];
	computers.freeSlots = 0;
	EXPECT_TRUE(IsFull(computers));
}

TEST(LobbyList, TheEventsChangeTheList)
{
	LobbyListModel list;
	list.Replace({.lobbies = {Summary("a", 1, 2, lobby_api::LobbyState::Open, 1)}});
	const auto summary = R"({"id":"b","name":"B","host":{"id":1,"name":"h"},"map":{"file":"m"},"gods":1,)"
	                     R"("god_slots":2,"state":"open"})";
	EXPECT_TRUE(list.Apply(Event("lobbies.changed", fmt::format(R"({{"lobby":{}}})", summary))));
	EXPECT_EQ(list.Entries().size(), 2u);
	EXPECT_TRUE(list.Apply(Event("lobbies.removed", R"({"id":"a"})")));
	EXPECT_EQ(list.Find("a"), nullptr);
	EXPECT_FALSE(list.Apply(Event("lobbies.removed", R"({"id":"a"})")));
	EXPECT_TRUE(list.Apply(Event("pool.changed", R"({"free":1,"total":6})")));
	EXPECT_EQ(list.Pool()->free, 1);
	EXPECT_FALSE(list.Apply(Event("chat.line", "{}")));
}

namespace
{

/// A lobby client that keeps each call until the test answers it
class FakeLobbyClient final: public ecs::systems::LobbyClientSystemInterface
{
public:
	struct Call
	{
		HttpMethod method;
		std::string path;
		std::string body;
		std::vector<online::HttpHeader> headers;
		LobbyCallback<HttpResponse> callback;
	};

	void Update() override {}
	[[nodiscard]] bool IsConfigured() const override { return true; }
	[[nodiscard]] ecs::systems::LobbySessionState GetState() const override
	{
		return ecs::systems::LobbySessionState::LoggedIn;
	}
	[[nodiscard]] std::optional<lobby_api::AccountRef> GetAccount() const override { return account; }
	[[nodiscard]] bool IsPushOpen() const override { return true; }
	[[nodiscard]] bool IsEmailRequired() const override { return false; }
	void Login(std::string, std::string, LobbyCallback<lobby_api::AccountRef>) override {}
	void ResumeSession(LobbyCallback<lobby_api::AccountRef>) override {}
	void Logout(bool, LobbyCallback<LobbyDone>) override {}
	void GetMe(LobbyCallback<lobby_api::Me>) override {}
	void GetServers(LobbyCallback<lobby_api::ServerList>) override {}
	[[nodiscard]] std::span<const lobby_api::ServerEntry> GetKnownServers() const override { return {}; }
	void WatchServers(bool) override {}
	void WatchChannel(std::string channel, bool watch) override
	{
		// The client asks for each channel one time, as the real client does
		if (watch && std::ranges::find(channels, channel) == channels.end())
		{
			channels.push_back(std::move(channel));
		}
		else if (watch)
		{
			return;
		}
		else
		{
			std::erase(channels, channel);
		}
	}
	void RequestJoinTicket(std::string, game_socket::Role, LobbyCallback<lobby_api::JoinTicket>) override {}
	void SendChatLine(std::string channel, std::string text, LobbyCallback<LobbyDone> callback) override
	{
		chat.emplace_back(std::move(channel), std::move(text));
		if (callback)
		{
			callback({.value = LobbyDone {}});
		}
	}
	void UseClan(std::string, std::optional<int64_t>, LobbyCallback<lobby_api::UsedClan>) override {}
	void CheckAccountName(std::string, LobbyCallback<lobby_api::AccountNameCheck>) override {}
	void GetMyClans(LobbyCallback<lobby_api::ClanList>) override {}
	void GetClan(int64_t, LobbyCallback<lobby_api::ClanDetails>) override {}
	void GetClanLogo(int64_t, LobbyCallback<std::vector<uint8_t>>) override {}
	void GetPlayerRanking(lobby_api::RankingQuery, LobbyCallback<lobby_api::RankingPage>) override {}
	void GetClanRanking(lobby_api::RankingQuery, LobbyCallback<lobby_api::RankingPage>) override {}
	void GetPlayer(int64_t, LobbyCallback<lobby_api::PlayerDetails>) override {}
	void GetPlayerDetailLines(int64_t, std::string, LobbyCallback<lobby_api::PlayerDetailLines>) override {}
	void GetWeather(lobby_api::Date, LobbyCallback<lobby_api::RealWeather>) override {}
	void UploadCreature(lobby_api::CreatureUpload, LobbyCallback<lobby_api::CreatureUploadReply>) override {}
	void SendSkirmishPing(LobbyCallback<LobbyDone>) override {}
	void SendApiCall(HttpMethod method, std::string path, std::string body, std::vector<online::HttpHeader> headers,
	                 LobbyCallback<HttpResponse> callback) override
	{
		calls.push_back({method, std::move(path), std::move(body), std::move(headers), std::move(callback)});
	}
	uint32_t AddEventListener(ecs::systems::LobbyEventListener listener) override
	{
		listeners.push_back(std::move(listener));
		return static_cast<uint32_t>(listeners.size());
	}
	void RemoveEventListener(uint32_t id) override { listeners.at(id - 1) = nullptr; }

	/// Answers the oldest call with a success
	void Answer(int32_t status, std::string_view body, std::vector<online::HttpHeader> headers = {})
	{
		ASSERT_FALSE(calls.empty());
		auto call = std::move(calls.front());
		calls.pop_front();
		call.callback(
		    {.value = HttpResponse {.status = status, .headers = std::move(headers), .body = {body.begin(), body.end()}}});
	}
	/// Answers the oldest call with a refusal
	void Refuse(int32_t status, std::string code)
	{
		ASSERT_FALSE(calls.empty());
		auto call = std::move(calls.front());
		calls.pop_front();
		call.callback({.error = LobbyError {.kind = LobbyErrorKind::Refused, .status = status, .code = std::move(code)}});
	}
	void Deliver(const lobby_api::PushEvent& event)
	{
		for (const auto& listener : listeners)
		{
			if (listener)
			{
				listener(event);
			}
		}
	}

	std::optional<lobby_api::AccountRef> account {k_Me};
	std::deque<Call> calls;
	std::vector<std::string> channels;
	std::vector<std::pair<std::string, std::string>> chat;
	std::vector<ecs::systems::LobbyEventListener> listeners;
};

class LobbyRoomSystemTest: public ::testing::Test
{
protected:
	/// Joins the test lobby
	void JoinTestLobby()
	{
		_system.Join("l1", std::nullopt, lobby_api::MemberRole::God, std::nullopt, {});
		_client.Answer(201, R"({"account":{"id":3,"name":"gavxn"},"role":"god","slot":2})");
		_client.Answer(200, k_LobbyJson);
		ASSERT_NE(_system.Room(), nullptr);
	}

	FakeLobbyClient _client;
	int64_t _now {k_TenOClock};
	ecs::systems::LobbyRoomSystem _system {_client, std::string(k_Build), [this]() { return _now; }};
};

} // namespace

TEST_F(LobbyRoomSystemTest, JoinWithAPasswordOpensTheRoom)
{
	std::optional<LobbyResult<LobbyDone>> done;
	_system.Join("l1", "secret", lobby_api::MemberRole::God, 3, [&](auto&& result) { done = std::move(result); });
	ASSERT_EQ(_client.calls.size(), 1u);
	EXPECT_EQ(_client.calls[0].method, HttpMethod::Post);
	EXPECT_EQ(_client.calls[0].path, "/lobbies/l1/members");
	EXPECT_EQ(_client.calls[0].body, R"({"build_id":"0.1.0+abc/codec34","password":"secret","role":"god","slot":3})");
	_client.Answer(201, R"({"account":{"id":3,"name":"gavxn"},"role":"god","slot":3})");
	// The channel of the lobby opens before the snapshot
	EXPECT_EQ(_client.channels, (std::vector<std::string> {"lobby-l1"}));
	ASSERT_EQ(_client.calls.size(), 1u);
	EXPECT_EQ(_client.calls[0].method, HttpMethod::Get);
	EXPECT_EQ(_client.calls[0].path, "/lobbies/l1");
	_client.Answer(200, k_LobbyJson);
	ASSERT_TRUE(done.has_value() && done->Ok());
	ASSERT_NE(_system.Room(), nullptr);
	EXPECT_EQ(_system.Room()->Data().name, "Friday Ring");
	EXPECT_EQ(_client.channels, (std::vector<std::string> {"lobby-l1"}));
}

TEST_F(LobbyRoomSystemTest, AWrongPasswordOpensNoRoom)
{
	std::optional<LobbyError> error;
	_system.Join("l1", "bad", lobby_api::MemberRole::God, std::nullopt, [&](auto&& result) { error = result.error; });
	_client.Refuse(403, "wrong_password");
	ASSERT_TRUE(error.has_value());
	EXPECT_EQ(error->code, "wrong_password");
	EXPECT_EQ(_system.Room(), nullptr);
	EXPECT_TRUE(_client.channels.empty());
	// An id that is not a path part makes no call
	_system.Join("../admin", std::nullopt, lobby_api::MemberRole::God, std::nullopt,
	             [&](auto&& result) { error = result.error; });
	EXPECT_EQ(error->code, "bad_lobby");
	EXPECT_TRUE(_client.calls.empty());
}

TEST_F(LobbyRoomSystemTest, TheRoomCallsGoToTheLobby)
{
	JoinTestLobby();
	_system.SetReady(true, {});
	EXPECT_EQ(_client.calls.back().method, HttpMethod::Put);
	EXPECT_EQ(_client.calls.back().path, "/lobbies/l1/members/me");
	EXPECT_EQ(_client.calls.back().body, R"({"ready":true})");
	// The service gives the whole lobby back
	auto ready = std::string(k_LobbyJson);
	const auto own = ready.find(R"("id":3,"name":"gavxn"},"role":"god","slot":2,"ready":false)");
	ready.replace(ready.find(R"("ready":false)", own), 13, R"("ready":true)");
	_client.Answer(200, ready);
	EXPECT_TRUE(_system.Room()->OwnMember()->ready);

	_system.ReportMap({.haveMap = true, .sha256 = "ab"}, {});
	EXPECT_EQ(_client.calls.back().body, R"({"have_map":true,"map_sha256":"ab"})");
	_client.Answer(200, k_LobbyJson);

	_system.PickSlot(3, {});
	EXPECT_EQ(_client.calls.back().body, R"({"slot":3})");
	_client.Answer(200, "{}");
	_system.Kick(2, true, {});
	EXPECT_EQ(_client.calls.back().method, HttpMethod::Delete);
	EXPECT_EQ(_client.calls.back().path, "/lobbies/l1/members/2?ban=1");
	_client.Answer(204, "");
	_system.Kick(2, false, {});
	EXPECT_EQ(_client.calls.back().path, "/lobbies/l1/members/2");
	_client.Answer(204, "");
	_system.MakeHost(2, {});
	EXPECT_EQ(_client.calls.back().method, HttpMethod::Post);
	EXPECT_EQ(_client.calls.back().path, "/lobbies/l1/members/2/make-host");
	EXPECT_TRUE(_client.calls.back().body.empty());
	_client.Answer(200, "{}");
	_system.Change({.name = "New"}, {});
	EXPECT_EQ(_client.calls.back().method, HttpMethod::Patch);
	EXPECT_EQ(_client.calls.back().path, "/lobbies/l1");
	_client.Answer(200, "{}");
	_system.CancelStart({});
	EXPECT_EQ(_client.calls.back().path, "/lobbies/l1/start/cancel");
	_client.Answer(200, "{}");
	_system.SendChat("hi", {});
	ASSERT_EQ(_client.chat.size(), 1u);
	EXPECT_EQ(_client.chat[0].first, "lobby-l1");
}

TEST_F(LobbyRoomSystemTest, TheStartAndTheCountdown)
{
	JoinTestLobby();
	_system.Start({});
	EXPECT_EQ(_client.calls.back().path, "/lobbies/l1/start");
	_client.Answer(202, R"({"state":"countdown","at":"2026-11-12T10:00:05Z"})");
	EXPECT_EQ(_system.Room()->Phase(), RoomPhase::Countdown);
	_system.Update();
	ASSERT_FALSE(_system.Room()->Lines().empty());
	EXPECT_EQ(RoomLineText(_system.Room()->Lines().back()), "The game starts in 5...");
	_client.Deliver(Event("game.assigned", R"({"game_id":"g1"})"));
	EXPECT_EQ(_system.Room()->Phase(), RoomPhase::Assigned);

	std::optional<lobby_api::JoinTicket> ticket;
	std::optional<LobbyErrorKind> kind;
	_system.RequestTicket([&](auto&& result) {
		ticket = result.value;
		kind = result.error.kind;
	});
	EXPECT_EQ(_client.calls.back().path, "/lobbies/l1/tickets");
	_client.Answer(201, R"({"url":"https://bw.example/game/s1/g1","ticket":"v4.public.x","expires":"2026-11-12T10:01:00Z",)"
	                    R"("role":"god","game_id":"g1"})");
	// The game socket must be encrypted
	EXPECT_EQ(kind, LobbyErrorKind::AddressRefused);
	_system.RequestTicket([&](auto&& result) { ticket = result.value; });
	_client.Answer(201, R"({"url":"wss://bw.example/game/s1/g1","ticket":"v4.public.x","expires":"2026-11-12T10:01:00Z",)"
	                    R"("role":"god","game_id":"g1"})");
	ASSERT_TRUE(ticket.has_value());
	EXPECT_EQ(ticket->url, "wss://bw.example/game/s1/g1");
}

TEST_F(LobbyRoomSystemTest, TheEndStopsTheChannelOfTheRoom)
{
	JoinTestLobby();
	// A change of the lobby gets the lobby again
	_client.Deliver(Event("lobby.changed", R"({"lobby":"l1","fields":["name"],"ready_cleared":true})"));
	ASSERT_FALSE(_client.calls.empty());
	EXPECT_EQ(_client.calls.back().path, "/lobbies/l1");
	_client.Answer(200, k_LobbyJson);
	EXPECT_EQ(RoomLineText(_system.Room()->Lines().back()), "Ready was cleared.");
	_client.Deliver(Event("lobby.closed", R"({"reason":"host_closed"})"));
	ASSERT_NE(_system.Room(), nullptr);
	EXPECT_EQ(_system.Room()->End()->reason, EndReason::HostClosed);
	EXPECT_TRUE(_client.channels.empty());
	// A call of a room that ended fails at once
	std::optional<LobbyError> error;
	_system.SetReady(true, [&](auto&& result) { error = result.error; });
	EXPECT_EQ(error->code, "no_room");
	_system.DismissRoom();
	EXPECT_EQ(_system.Room(), nullptr);
}

TEST_F(LobbyRoomSystemTest, LeaveAndClose)
{
	JoinTestLobby();
	_system.Leave({});
	EXPECT_EQ(_client.calls.back().method, HttpMethod::Delete);
	EXPECT_EQ(_client.calls.back().path, "/lobbies/l1/members/me");
	_client.Answer(204, "");
	EXPECT_EQ(_system.Room()->End()->reason, EndReason::Left);
	EXPECT_TRUE(_client.channels.empty());

	// A new join forgets the room that ended
	JoinTestLobby();
	_system.Close({});
	EXPECT_EQ(_client.calls.back().path, "/lobbies/l1");
	_client.Refuse(410, "closed");
	EXPECT_EQ(_system.Room()->End()->reason, EndReason::Closed);
}

TEST_F(LobbyRoomSystemTest, CreateSendsTheBuild)
{
	_system.Create({.name = "g", .map = {.file = "m"}, .godSlots = 2, .buildId = "other"}, {});
	EXPECT_EQ(_client.calls.back().path, "/lobbies");
	EXPECT_NE(_client.calls.back().body.find(R"("build_id":"0.1.0+abc/codec34")"), std::string::npos);
	auto lobby = std::string(k_LobbyJson);
	_client.Answer(201, lobby);
	ASSERT_NE(_system.Room(), nullptr);
	EXPECT_EQ(_client.channels, (std::vector<std::string> {"lobby-l1"}));
	// A second lobby needs the player to leave the first
	std::optional<LobbyError> error;
	_system.Create({.name = "h"}, [&](auto&& result) { error = result.error; });
	EXPECT_EQ(error->code, "in_room");
}

TEST_F(LobbyRoomSystemTest, TheListUsesItsTagAndItsEvents)
{
	_system.WatchList(true);
	EXPECT_EQ(_client.channels, (std::vector<std::string> {"lobbies"}));
	_system.RefreshList({});
	// The list asks for the started lobbies and the other builds too, for the filters of the screen
	EXPECT_EQ(_client.calls.back().path, "/lobbies?in_game=1&version=1&build=0.1.0%2Babc%2Fcodec34");
	EXPECT_TRUE(_client.calls.back().headers.empty());
	_client.Answer(200,
	               R"({"lobbies":[{"id":"a","name":"A","host":{"id":1,"name":"h"},"map":{"file":"m"},"gods":1,)"
	               R"("god_slots":2,"state":"open","build_id":"0.1.0+abc/codec34"}]})",
	               {{"etag", "\"v1\""}});
	EXPECT_EQ(_system.List().Entries().size(), 1u);
	_system.RefreshList({});
	ASSERT_EQ(_client.calls.back().headers.size(), 1u);
	EXPECT_EQ(_client.calls.back().headers[0].second, "\"v1\"");
	_client.Answer(304, "");
	EXPECT_EQ(_system.List().Entries().size(), 1u);
	_client.Deliver(Event("lobbies.removed", R"({"id":"a"})"));
	EXPECT_TRUE(_system.List().Entries().empty());
	// A new connection of the push channel gets the list again with no tag
	_client.Deliver({.type = "hello", .data = R"({"epoch":"e2","seq":0})"});
	EXPECT_TRUE(_client.calls.back().headers.empty());
}

TEST_F(LobbyRoomSystemTest, FindByCode)
{
	std::optional<std::string> id;
	_system.FindByCode("k7q2-mx9d", [&](auto&& result) { id = result.value; });
	EXPECT_EQ(_client.calls.back().path, "/lobbies/by-code/K7Q2MX9D");
	_client.Answer(200, R"({"id":"l9"})");
	EXPECT_EQ(id, "l9");
	std::optional<LobbyError> error;
	_system.FindByCode("bad", [&](auto&& result) { error = result.error; });
	EXPECT_EQ(error->code, "bad_code");
}

TEST_F(LobbyRoomSystemTest, QuickJoinGetsTheLobbyAgain)
{
	_system.QuickJoin({});
	EXPECT_EQ(_client.calls.back().path, "/lobbies/quick-join");
	EXPECT_EQ(_client.calls.back().body, R"({"build_id":"0.1.0+abc/codec34"})");
	_client.Answer(200, k_LobbyJson);
	ASSERT_NE(_system.Room(), nullptr);
	ASSERT_EQ(_client.calls.size(), 1u);
	EXPECT_EQ(_client.calls[0].path, "/lobbies/l1");
	auto lobby = std::string(k_LobbyJson);
	lobby.replace(lobby.find("Friday Ring"), 11, "Monday Ring");
	_client.Answer(200, lobby);
	EXPECT_EQ(_system.Room()->Data().name, "Monday Ring");
}

namespace
{

/// The notice of one event of the room, from the snapshots before and after the event
LobbyNotice NoticeOf(LobbyRoom& room, const lobby_api::PushEvent& event)
{
	const auto before = SnapshotOf(room);
	room.Apply(event);
	return NoticeBetween(before, SnapshotOf(room));
}

constexpr LobbyNotice k_Sound {.sound = true, .flash = false};
constexpr LobbyNotice k_SoundAndFlash {.sound = true, .flash = true};

} // namespace

TEST(LobbyNotices, TheSnapshotHasTheOtherReadyPlayersAndTheCounts)
{
	LobbyRoom room(TestLobby(), k_Me);
	room.Apply(Event("member.changed", fmt::format(R"({{"account":{},"ready":true}})", Account(k_Me))));
	room.Apply(Event("member.joined", R"({"account":{"id":5,"name":"Sunfish"},"role":"god","slot":3})"));
	room.Apply(Event("member.left", fmt::format(R"({{"account":{}}})", Account(k_Owl))));
	const auto snapshot = SnapshotOf(room);
	EXPECT_EQ(snapshot.lobbyId, "l1");
	EXPECT_EQ(snapshot.phase, RoomPhase::Open);
	EXPECT_FALSE(snapshot.end.has_value());
	EXPECT_EQ(snapshot.joins, 1u);
	EXPECT_EQ(snapshot.leaves, 1u);
	EXPECT_EQ(snapshot.removals, 0u);
	// The player is ready too, but the snapshot keeps only the other players. Owl left.
	EXPECT_TRUE(snapshot.readyPlayers.empty());
}

TEST(LobbyNotices, TheEventsOfOtherPlayersPlayTheSound)
{
	LobbyRoom room(TestLobby(), k_Me);
	EXPECT_EQ(NoticeOf(room, Event("member.joined", R"({"account":{"id":5,"name":"Sunfish"},"role":"god","slot":3})")),
	          k_Sound);
	EXPECT_EQ(NoticeOf(room, Event("member.changed", R"({"account":{"id":5,"name":"Sunfish"},"ready":true})")), k_Sound);
	EXPECT_EQ(NoticeOf(room, Event("member.kicked",
	                               fmt::format(R"({{"account":{{"id":5,"name":"Sunfish"}},"by":{}}})", Account(k_Host)))),
	          k_Sound);
	EXPECT_EQ(NoticeOf(room, Event("member.left", fmt::format(R"({{"account":{}}})", Account(k_Owl)))), k_Sound);
}

TEST(LobbyNotices, TheEventsWithNoNoticeAreSilent)
{
	LobbyRoom room(TestLobby(), k_Me);
	// The ready of the player comes from the click of the player, which has its own sound
	EXPECT_FALSE(NoticeOf(room, Event("member.changed", fmt::format(R"({{"account":{},"ready":true}})", Account(k_Me)))).Any());
	// A player that is not ready now, a chat line and a change of the map give no notice
	EXPECT_FALSE(
	    NoticeOf(room, Event("member.changed", fmt::format(R"({{"account":{},"ready":false}})", Account(k_Owl)))).Any());
	EXPECT_FALSE(
	    NoticeOf(room, Event("chat.line", fmt::format(R"({{"channel":"lobby-l1","from":{},"text":"hi"}})", Account(k_Owl))))
	        .Any());
	EXPECT_FALSE(NoticeOf(room, Event("lobby.changed", R"({"fields":["map"],"ready_cleared":true})")).Any());
}

TEST(LobbyNotices, TheCountdownFlashesAtItsStartAndPlaysEachSecond)
{
	LobbyRoom room(TestLobby(), k_Me);
	EXPECT_EQ(NoticeOf(room, Event("start.countdown", R"({"at":"2026-11-12T10:00:05Z"})")), k_SoundAndFlash);
	auto before = SnapshotOf(room);
	room.Tick(k_TenOClock);
	EXPECT_EQ(NoticeBetween(before, SnapshotOf(room)), k_Sound);
	// The same second gives no second notice
	before = SnapshotOf(room);
	room.Tick(k_TenOClock + 200);
	EXPECT_FALSE(NoticeBetween(before, SnapshotOf(room)).Any());
	// The cancel of the start gives no notice
	EXPECT_FALSE(NoticeOf(room, Event("start.cancelled", R"({"why":"host_cancelled"})")).Any());
}

TEST(LobbyNotices, TheStartOfTheGameFlashesOnce)
{
	LobbyRoom room(TestLobby(), k_Me);
	room.Apply(Event("start.countdown", R"({"at":"2026-11-12T10:00:05Z"})"));
	EXPECT_EQ(NoticeOf(room, Event("game.assigned", R"({"game_id":"g1"})")), k_SoundAndFlash);
	EXPECT_FALSE(NoticeOf(room, Event("game.started", "{}")).Any());
	// The end of the game gives no notice
	EXPECT_FALSE(NoticeOf(room, Event("game.ended", "{}")).Any());
}

TEST(LobbyNotices, AKickOrABanOfThePlayerFlashes)
{
	LobbyRoom kicked(TestLobby(), k_Me);
	EXPECT_EQ(NoticeOf(kicked, Event("lobby.kicked", R"({"lobby":"l1","by":{"id":1,"name":"marek"},"banned":false})")),
	          k_SoundAndFlash);
	LobbyRoom banned(TestLobby(), k_Me);
	EXPECT_EQ(NoticeOf(banned, Event("member.banned", fmt::format(R"({{"account":{}}})", Account(k_Me)))), k_SoundAndFlash);
	// The leave of the player gives no notice
	LobbyRoom left(TestLobby(), k_Me);
	EXPECT_FALSE(NoticeOf(left, Event("member.left", fmt::format(R"({{"account":{}}})", Account(k_Me)))).Any());
}

TEST(LobbyNotices, TwoLobbiesGiveNoNotice)
{
	NoticeSnapshot before {.lobbyId = "l1"};
	NoticeSnapshot after {.lobbyId = "l2", .phase = RoomPhase::Countdown, .joins = 3};
	EXPECT_FALSE(NoticeBetween(before, after).Any());
	after.lobbyId = "l1";
	EXPECT_EQ(NoticeBetween(before, after), k_SoundAndFlash);
}

TEST(LobbyNotices, TheWindowFlashesOnlyWithNoFocus)
{
	EXPECT_TRUE(FlashesWindow(k_SoundAndFlash, false));
	EXPECT_FALSE(FlashesWindow(k_SoundAndFlash, true));
	EXPECT_FALSE(FlashesWindow(k_Sound, false));
}
