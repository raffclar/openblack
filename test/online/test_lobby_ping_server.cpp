/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <LobbyApi.h>
#include <LobbyApiJson.h>
#include <gtest/gtest.h>

#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "Online/FriendList.h"
#include "Online/LobbyPingServer.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using Message = online::PingServerInterface::Message;

namespace
{

/// A lobby that keeps the pings, the lines and the listener of the push events
class FakeLobby final: public LobbyClientSystemInterface
{
public:
	void Update() override {}
	[[nodiscard]] bool IsConfigured() const override { return true; }
	[[nodiscard]] LobbySessionState GetState() const override { return state; }
	[[nodiscard]] std::optional<lobby_api::AccountRef> GetAccount() const override { return std::nullopt; }
	[[nodiscard]] bool IsPushOpen() const override { return pushOpen; }
	[[nodiscard]] bool IsEmailRequired() const override { return false; }
	void Login(std::string, std::string, LobbyCallback<lobby_api::AccountRef>) override {}
	void ResumeSession(LobbyCallback<lobby_api::AccountRef>) override {}
	void Logout(bool, LobbyCallback<LobbyDone>) override {}
	void GetMe(LobbyCallback<lobby_api::Me>) override {}
	void GetServers(LobbyCallback<lobby_api::ServerList>) override {}
	[[nodiscard]] std::span<const lobby_api::ServerEntry> GetKnownServers() const override { return {}; }
	void WatchServers(bool) override {}
	void WatchChannel(std::string, bool) override {}
	void RequestJoinTicket(std::string, game_socket::Role, LobbyCallback<lobby_api::JoinTicket>) override {}
	void SendChatLine(std::string, std::string, LobbyCallback<LobbyDone>) override {}
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
	void SendPing(std::vector<int64_t> friends, LobbyCallback<lobby_api::PingReply> callback) override
	{
		pings.push_back(std::move(friends));
		pingAnswers.push_back(std::move(callback));
	}
	void SendPersonLine(int64_t account, std::string text, LobbyCallback<LobbyDone> callback) override
	{
		lines.emplace_back(account, std::move(text));
		callback({.value = LobbyDone {}, .error = {}});
	}
	uint32_t AddEventListener(LobbyEventListener added) override
	{
		listener = std::move(added);
		return 7;
	}
	void RemoveEventListener(uint32_t id) override
	{
		removed.push_back(id);
		listener = {};
	}

	void Push(std::string_view type, std::string data) const
	{
		ASSERT_TRUE(listener);
		listener(lobby_api::PushEvent {.seq = 1, .type = std::string(type), .time = {}, .data = std::move(data)});
	}

	LobbySessionState state {LobbySessionState::LoggedIn};
	bool pushOpen {true};
	std::vector<std::vector<int64_t>> pings;
	std::vector<LobbyCallback<lobby_api::PingReply>> pingAnswers;
	std::vector<std::pair<int64_t, std::string>> lines;
	LobbyEventListener listener;
	std::vector<uint32_t> removed;
};

online::LobbyPingServer MakeServer(FakeLobby& lobby)
{
	return online::LobbyPingServer([&lobby]() -> LobbyClientSystemInterface* { return &lobby; });
}

} // namespace

TEST(LobbyPingServer, ConnectedWithASessionAndAnOpenPushChannel)
{
	FakeLobby lobby;
	const auto server = MakeServer(lobby);
	EXPECT_TRUE(server.IsConnected());
	lobby.pushOpen = false;
	EXPECT_FALSE(server.IsConnected());
	lobby.pushOpen = true;
	lobby.state = LobbySessionState::LoggingIn;
	EXPECT_FALSE(server.IsConnected());
	const online::LobbyPingServer none([]() -> LobbyClientSystemInterface* { return nullptr; });
	EXPECT_FALSE(none.IsConnected());
}

TEST(LobbyPingServer, ThePingAndTheFriendQuestionGoInOneCall)
{
	FakeLobby lobby;
	auto server = MakeServer(lobby);
	const std::vector<gathering_box::UserId> friends {301, 302};
	server.Ping();
	server.AskOnline(friends);
	EXPECT_TRUE(lobby.pings.empty());
	EXPECT_TRUE(server.TakeMessages().empty());
	ASSERT_EQ(lobby.pings.size(), 1u);
	EXPECT_EQ(lobby.pings[0], (std::vector<int64_t> {301, 302}));

	// A ping with no friends asks for none, and the next frame sends nothing more
	server.Ping();
	std::ignore = server.TakeMessages();
	std::ignore = server.TakeMessages();
	ASSERT_EQ(lobby.pings.size(), 2u);
	EXPECT_TRUE(lobby.pings[1].empty());
}

TEST(LobbyPingServer, TheAnswerGivesTheOnlineFriendsAndTheAnnouncements)
{
	FakeLobby lobby;
	auto server = MakeServer(lobby);
	server.Ping();
	std::ignore = server.TakeMessages();
	ASSERT_EQ(lobby.pingAnswers.size(), 1u);
	lobby.pingAnswers[0](
	    {.value = lobby_api::PingReply {.online = {301, 5'000'000'000}, .here = {{.id = 10000000, .name = "Hello"}}},
	     .error = {}});
	const auto messages = server.TakeMessages();
	ASSERT_EQ(messages.size(), 2u);
	const auto* online = std::get_if<online::PingServerInterface::Online>(&messages[0]);
	ASSERT_NE(online, nullptr);
	EXPECT_EQ(online->users, std::vector<gathering_box::UserId> {301});
	const auto* here = std::get_if<online::PingServerInterface::Here>(&messages[1]);
	ASSERT_NE(here, nullptr);
	EXPECT_EQ(here->user, 10000000u);
	EXPECT_EQ(here->name, u"Hello");
}

TEST(LobbyPingServer, AFailedPingGivesNothing)
{
	FakeLobby lobby;
	auto server = MakeServer(lobby);
	server.Ping();
	std::ignore = server.TakeMessages();
	lobby.pingAnswers[0]({.value = std::nullopt, .error = {.kind = LobbyErrorKind::Network}});
	EXPECT_TRUE(server.TakeMessages().empty());
}

TEST(LobbyPingServer, PushEventsBecomeLinesAndPeopleThatAreHere)
{
	FakeLobby lobby;
	auto server = MakeServer(lobby);
	lobby.Push(lobby_api::events::k_PingLine, R"({"from": {"id": 9999999, "name": "Sam"}, "text": "!SaMuElGRReady?"})");
	lobby.Push(lobby_api::events::k_PingHere, R"({"people": [{"id": 10000000, "name": "A"}, {"id": 10000001, "name": "B"}]})");
	lobby.Push(lobby_api::events::k_ChatLine, R"({"channel": "lobby", "from": {"id": 5, "name": "x"}, "text": "y"})");
	lobby.Push(lobby_api::events::k_PingLine, R"({"text": "no sender"})");
	const auto messages = server.TakeMessages();
	ASSERT_EQ(messages.size(), 3u);
	const auto* line = std::get_if<online::PingServerInterface::Line>(&messages[0]);
	ASSERT_NE(line, nullptr);
	EXPECT_EQ(line->user, 9999999u);
	EXPECT_EQ(line->name, u"Sam");
	EXPECT_EQ(line->text, u"!SaMuElGRReady?");
	EXPECT_EQ(std::get<online::PingServerInterface::Here>(messages[2]).name, u"B");
}

TEST(LobbyPingServer, ALineGoesToTheAccountInUtf8)
{
	FakeLobby lobby;
	auto server = MakeServer(lobby);
	server.SendLine(301, u"héllo");
	ASSERT_EQ(lobby.lines.size(), 1u);
	EXPECT_EQ(lobby.lines[0].first, 301);
	EXPECT_EQ(lobby.lines[0].second, "h\xc3\xa9llo");
	// The game server sends the report of the host, so a game sends nothing
	const std::vector<uint8_t> report(0x48, 0);
	server.SendHostReport(report);
	EXPECT_TRUE(lobby.pings.empty());
}

TEST(LobbyPingServer, TheEndRemovesTheListenerAndALateAnswerGoesNowhere)
{
	FakeLobby lobby;
	{
		auto server = MakeServer(lobby);
		server.Ping();
		std::ignore = server.TakeMessages();
	}
	EXPECT_EQ(lobby.removed, std::vector<uint32_t> {7});
	lobby.pingAnswers[0]({.value = lobby_api::PingReply {.online = {301}, .here = {}}, .error = {}});
}

TEST(LobbyPing, UserOfKeepsTheIdsThatFit)
{
	EXPECT_EQ(online::lobby_ping::UserOf(1), std::optional<gathering_box::UserId>(1));
	EXPECT_EQ(online::lobby_ping::UserOf(4294967295), std::optional<gathering_box::UserId>(4294967295u));
	EXPECT_EQ(online::lobby_ping::UserOf(4294967296), std::nullopt);
	EXPECT_EQ(online::lobby_ping::UserOf(0), std::nullopt);
	EXPECT_EQ(online::lobby_ping::UserOf(-3), std::nullopt);
}

TEST(LobbyApiJson, PingShapes)
{
	EXPECT_EQ(lobby_api::ToJson(lobby_api::PingRequest {.friends = {3, 4}}), R"({"friends":[3,4]})");
	const auto reply = lobby_api::ParsePingReply(R"({"online": [3], "here": [{"id": 10000000, "name": "Hi"}]})");
	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->online, std::vector<int64_t> {3});
	ASSERT_EQ(reply->here.size(), 1u);
	EXPECT_EQ(reply->here[0].name, "Hi");
	EXPECT_TRUE(lobby_api::ParsePingReply(R"({"online": []})").has_value());
	EXPECT_FALSE(lobby_api::ParsePingReply(R"({"online": ["3"]})").has_value());
	EXPECT_FALSE(lobby_api::ParsePingReply(R"({"here": []})").has_value());
	EXPECT_FALSE(lobby_api::ParsePingHere(R"({"people": [{"id": 1}]})").has_value());
	EXPECT_FALSE(lobby_api::ParsePingLine(R"({"from": {"id": 1, "name": "a"}})").has_value());
}

TEST(FriendList, EncodeAndDecodeKeepTheOrderAndTheNames)
{
	const std::vector<chat::people::FriendRecord> friends {{.user = 42, .name = u"Anné"}, {.user = 7, .name = u"B\tob"}};
	const auto text = online::friend_list::Encode(friends);
	EXPECT_EQ(text, "openblack friend list 1\n42\tAnn\xc3\xa9\n7\tB ob\n");
	const std::span bytes(reinterpret_cast<const uint8_t*>(text.data()), text.size());
	const auto decoded = online::friend_list::Decode(bytes);
	EXPECT_EQ(decoded, (std::vector<chat::people::FriendRecord> {{.user = 42, .name = u"Anné"}, {.user = 7, .name = u"B ob"}}));
}

TEST(FriendList, DecodeIgnoresBadLinesAndKeeps25)
{
	std::string text = "openblack friend list 1\r\n0\tZero\nabc\tLetters\n9\n";
	for (int i = 1; i <= 30; ++i)
	{
		text += std::to_string(i) + "\tP" + std::to_string(i) + "\n";
	}
	const auto decoded = online::friend_list::Decode({reinterpret_cast<const uint8_t*>(text.data()), text.size()});
	ASSERT_EQ(decoded.size(), 25u);
	EXPECT_EQ(decoded.front().user, 1u);
	EXPECT_EQ(decoded.back().name, u"P25");

	const std::string other = "something else\n1\tA\n";
	EXPECT_TRUE(online::friend_list::Decode({reinterpret_cast<const uint8_t*>(other.data()), other.size()}).empty());
	EXPECT_TRUE(online::friend_list::Decode({}).empty());
}
