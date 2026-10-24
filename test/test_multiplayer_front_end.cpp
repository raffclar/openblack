/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rules of the multiplayer front end over the temple's future room, and its calls to the lobby client and the network
// game, with a fake lobby, a fake network game and a fake socket

#include <cmath>
#include <cstdint>

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <LobbyApiJson.h>
#include <gtest/gtest.h>

#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "Gui/MultiplayerFrontEnd.h"
#include "Gui/MultiplayerFrontEndRules.h"
#include "Gui/TextDatabase.h"
#include "Online/LanDiscoveryInterface.h"
#include "Online/NetworkSession.h"
#include "online/FakeWebSocketClient.h"

using namespace openblack;
using namespace openblack::gui;
using namespace openblack::gui::multiplayer;
using namespace openblack::ecs::systems;

namespace
{
/// The version of the client in these tests, as the build writes it
constexpr std::string_view k_GameVersion = "0.1.0+0123456789ab";
} // namespace

namespace
{

lobby_api::ServerEntry Server(std::string id, std::string name, int32_t players, int32_t most)
{
	lobby_api::ServerEntry server;
	server.id = std::move(id);
	server.hostname = std::move(name);
	server.numplayers = players;
	server.maxplayers = most;
	server.gamemode = "openstaging";
	server.gamever = std::string(k_GameVersion);
	server.url = "wss://lobby.test/game/" + server.id;
	server.land = "mpm_2p_1";
	return server;
}

lobby_api::RoomPlayer Player(int64_t id, std::string name, std::string role)
{
	return {.account = {.id = id, .name = std::move(name)}, .role = std::move(role), .clan = std::nullopt};
}

/// A lobby for the tests: it keeps each call's callback, and the test answers the calls
class FakeLobby final: public LobbyClientSystemInterface
{
public:
	void Update() override {}
	[[nodiscard]] bool IsConfigured() const override { return true; }
	[[nodiscard]] LobbySessionState GetState() const override
	{
		return account ? LobbySessionState::LoggedIn : LobbySessionState::LoggedOut;
	}
	[[nodiscard]] std::optional<lobby_api::AccountRef> GetAccount() const override { return account; }
	[[nodiscard]] bool IsPushOpen() const override { return true; }
	[[nodiscard]] bool IsEmailRequired() const override { return emailRequired; }
	void Login(std::string name, std::string password, LobbyCallback<lobby_api::AccountRef> callback) override
	{
		loginName = std::move(name);
		loginPassword = std::move(password);
		login = std::move(callback);
	}
	void ResumeSession(LobbyCallback<lobby_api::AccountRef>) override {}
	void Logout(bool, LobbyCallback<LobbyDone>) override {}
	void GetMe(LobbyCallback<lobby_api::Me>) override {}
	void GetServers(LobbyCallback<lobby_api::ServerList> callback) override { servers = std::move(callback); }
	[[nodiscard]] std::span<const lobby_api::ServerEntry> GetKnownServers() const override { return known; }
	void WatchServers(bool watch) override { watchingServers = watch; }
	void WatchChannel(std::string channel, bool watch) override { watched[channel] = watch; }
	void RequestJoinTicket(std::string serverId, game_socket::Role role, LobbyCallback<lobby_api::JoinTicket> callback) override
	{
		ticketFor = std::move(serverId);
		ticketRoles.push_back(role);
		ticket = std::move(callback);
	}
	void SendChatLine(std::string channel, std::string text, LobbyCallback<LobbyDone>) override
	{
		chatChannel = std::move(channel);
		chatText = std::move(text);
	}
	void UseClan(std::string serverId, std::optional<int64_t> clanId, LobbyCallback<lobby_api::UsedClan> callback) override
	{
		useClanServer = std::move(serverId);
		useClanId = clanId;
		useClan = std::move(callback);
	}
	void CheckAccountName(std::string name, LobbyCallback<lobby_api::AccountNameCheck> callback) override
	{
		checkedName = std::move(name);
		nameCheck = std::move(callback);
	}
	void GetMyClans(LobbyCallback<lobby_api::ClanList> callback) override { myClans = std::move(callback); }
	void GetClan(int64_t clanId, LobbyCallback<lobby_api::ClanDetails> callback) override
	{
		clanAsked = clanId;
		clan = std::move(callback);
	}
	void GetClanLogo(int64_t, LobbyCallback<std::vector<uint8_t>> callback) override { logo = std::move(callback); }
	void GetPlayerRanking(int32_t, LobbyCallback<lobby_api::RankingPage>) override {}
	void GetClanRanking(int32_t, LobbyCallback<lobby_api::RankingPage>) override {}
	void GetPlayer(int64_t, LobbyCallback<lobby_api::PlayerDetails>) override {}
	void GetWeather(lobby_api::Date, LobbyCallback<lobby_api::RealWeather>) override {}
	void UploadCreature(lobby_api::CreatureUpload, LobbyCallback<lobby_api::CreatureUploadReply>) override {}
	uint32_t AddEventListener(LobbyEventListener listener) override
	{
		listeners.push_back(std::move(listener));
		return static_cast<uint32_t>(listeners.size());
	}
	void RemoveEventListener(uint32_t id) override { removed.push_back(id); }

	void Event(std::string_view type, std::string_view data)
	{
		const lobby_api::PushEvent event {.seq = 1, .type = std::string(type), .time = {}, .data = std::string(data)};
		for (auto& listener : listeners)
		{
			listener(event);
		}
	}

	template <typename Value>
	static void Fail(LobbyCallback<Value>& callback, LobbyErrorKind kind)
	{
		auto done = std::move(callback);
		callback = {};
		done(LobbyResult<Value> {.value = std::nullopt, .error = {.kind = kind}});
	}
	template <typename Value>
	static void Give(LobbyCallback<Value>& callback, Value value)
	{
		auto done = std::move(callback);
		callback = {};
		done(LobbyResult<Value> {.value = std::move(value), .error = {}});
	}

	std::optional<lobby_api::AccountRef> account;
	std::vector<lobby_api::ServerEntry> known;
	std::string loginName;
	std::string loginPassword;
	LobbyCallback<lobby_api::AccountRef> login;
	LobbyCallback<lobby_api::ServerList> servers;
	bool watchingServers {false};
	std::map<std::string, bool> watched;
	std::string ticketFor;
	LobbyCallback<lobby_api::JoinTicket> ticket;
	std::vector<game_socket::Role> ticketRoles;
	bool emailRequired {false};
	std::string chatChannel;
	std::string chatText;
	std::string useClanServer;
	std::optional<int64_t> useClanId;
	LobbyCallback<lobby_api::UsedClan> useClan;
	std::string checkedName;
	LobbyCallback<lobby_api::AccountNameCheck> nameCheck;
	LobbyCallback<lobby_api::ClanList> myClans;
	std::optional<int64_t> clanAsked;
	LobbyCallback<lobby_api::ClanDetails> clan;
	LobbyCallback<std::vector<uint8_t>> logo;
	std::vector<LobbyEventListener> listeners;
	std::vector<uint32_t> removed;
};

/// A network game for the tests: a real session on the fake socket, with a clock that the test moves
class FakeNetworkGame final: public NetworkGameSystemInterface
{
public:
	explicit FakeNetworkGame(online::WebSocketClientInterface& sockets)
	    : session(sockets, [this] { return now; })
	{
	}

	bool Join(NetworkJoin join) override
	{
		joins.push_back(join);
		session.Join(online::NetworkSessionSettings {
		    .request = {.url = join.url, .headers = {}, .subprotocols = {}},
		    .ticket = join.ticket,
		    .gamePassword = join.gamePassword,
		    .name = join.name,
		});
		return true;
	}
	void SetReady(bool ready) override { session.SetReady(ready); }
	void Leave() override
	{
		++leaves;
		session.Leave();
	}
	void Update() override { session.Update(); }
	[[nodiscard]] std::optional<game_socket::Start> TakeStart() override { return std::nullopt; }
	void BeginPlay() override {}
	[[nodiscard]] bool InGame() const override { return session.GetState() != online::NetworkSession::State::Ended; }
	[[nodiscard]] const online::NetworkSession* Session() const override { return &session; }

	int64_t now {0};
	online::NetworkSession session;
	std::vector<NetworkJoin> joins;
	int leaves {0};
};

class FakeLan final: public online::LanDiscoveryInterface
{
public:
	[[nodiscard]] bool IsAvailable() const override { return true; }
	void Start() override { ++starts; }
	void Stop() override { ++stops; }
	void Poll() override { ++polls; }
	[[nodiscard]] std::span<const lobby_api::ServerEntry> GetServers() const override { return servers; }

	std::vector<lobby_api::ServerEntry> servers;
	int starts {0};
	int stops {0};
	int polls {0};
};

constexpr std::string_view k_LocalMaps =
    "1:Bombardment - 2 players:mpm_2p_1:2:\r\n2:King of the hill - 3 players:mpm_3p_1:3:\r\n";
constexpr std::string_view k_Conditions = "FORMAP mpm_2p_1.txt\n"
                                          "//\tDefaultValue\tMinValue\n"
                                          "WC_KILL_NUM_VILLAGERS:\t\t1\t1\t500\t1 \t25\n"
                                          "WC_GROW_TREES:\t1\t1\t1000\t1\t100\n"
                                          "\n"
                                          "FORMAP mpm_3p_1.txt\n"
                                          "WC_KILL_NUM_VILLAGERS:\t7\t1\t500\t1\t25\n";

struct FrontEndTest: public ::testing::Test
{
	FakeLobby lobby;
	test_fakes::FakeWebSocketClient sockets;
	FakeNetworkGame network {sockets};
	FakeLan lan;
	std::vector<std::string> opened;
	std::unique_ptr<MultiplayerFrontEnd> frontEnd;

	void Make(bool withLan = false)
	{
		MultiplayerFrontEnd::Services services {
		    .lobby = &lobby,
		    .network = &network,
		    .lan = withLan ? &lan : nullptr,
		    .openPage = [this](std::string_view url) { opened.emplace_back(url); },
		    .owner = {},
		    .poll = {},
		};
		MultiplayerFrontEnd::Settings settings {
		    .registerPage = "https://lobby.test/register/",
		    .accountPage = "https://lobby.test/account/",
		    .internetName = u"lobby.test",
		    .profileName = u"Alice",
		    .gameVersion = std::string(k_GameVersion),
		    .browser = false,
		    .localMaps = std::string(k_LocalMaps),
		    .conditionTemplate = std::string(k_Conditions),
		};
		frontEnd = std::make_unique<MultiplayerFrontEnd>(std::move(services), std::move(settings));
	}

	/// The player goes through the boxes to the game list of the internet, logged in as Alice
	void ToGameList()
	{
		Make();
		frontEnd->Next();
		frontEnd->SetLobbyKind(LobbyKind::Internet);
		frontEnd->Next();
		ASSERT_EQ(frontEnd->GetBox(), Box::Login);
		frontEnd->Login(u"Alice", u"secret");
		lobby.account = lobby_api::AccountRef {.id = 7, .name = "Alice"};
		FakeLobby::Give(lobby.login, *lobby.account);
		ASSERT_EQ(frontEnd->GetBox(), Box::GameList);
		lobby.known = {Server("w1", "Bob's game", 2, 4), Server("w2", "Fun", 1, 2)};
		lobby.known[0].players = {Player(3, "Bob", "god"), Player(7, "Alice", "god"), Player(9, "Eve", "observer")};
		FakeLobby::Give(lobby.servers, lobby_api::ServerList {.servers = lobby.known});
	}

	/// A frame: the socket, the network game and the front end move on
	void Step()
	{
		sockets.Poll();
		network.Update();
		frontEnd->Update();
	}

	/// The game server lets the player in on the last socket
	void ServerJoins(game_socket::Role role = game_socket::Role::God)
	{
		const std::optional<uint8_t> slot = role == game_socket::Role::God ? std::optional<uint8_t>(1) : std::nullopt;
		sockets.ServerText(sockets.order.back(), game_socket::ToJson(game_socket::Joined {
		                                             .role = role, .slot = slot, .session = "s", .name = "Alice"}));
		Step();
	}

	/// The player joins the first game, its socket opens and the server lets the player in
	void ToRoom()
	{
		ToGameList();
		frontEnd->SelectGame(0);
		frontEnd->Join(u"");
		ASSERT_EQ(lobby.ticketFor, "w1");
		FakeLobby::Give(
		    lobby.ticket,
		    lobby_api::JoinTicket {.url = "wss://lobby.test/game/w1", .ticket = "v4.public.t", .expires = 0, .role = "god"});
		ASSERT_FALSE(sockets.order.empty());
		sockets.Accept(sockets.order.back());
		Step();
		// The room opens only when the server lets the player in
		ASSERT_EQ(frontEnd->GetBox(), Box::GameList);
		ServerJoins();
		ASSERT_EQ(frontEnd->GetBox(), Box::Room);
	}
};

} // namespace

TEST(MultiplayerRules, LoginFieldsFollowTheLobbyRules)
{
	EXPECT_EQ(CheckLogin(u"Alice", u"secret"), LoginProblem::None);
	EXPECT_EQ(CheckLogin(u"Al", u"secret"), LoginProblem::InvalidDetails);
	EXPECT_EQ(CheckLogin(u"Al ice", u"secret"), LoginProblem::InvalidDetails);
	EXPECT_EQ(CheckLogin(u"Alice_1", u"secret"), LoginProblem::InvalidDetails);
	EXPECT_EQ(CheckLogin(u"Aléx", u"secret"), LoginProblem::InvalidDetails);
	EXPECT_EQ(CheckLogin(std::u16string(32, u'a'), u"secret"), LoginProblem::None);
	EXPECT_EQ(CheckLogin(std::u16string(33, u'a'), u"secret"), LoginProblem::InvalidDetails);
	// An empty password gets the answer of a wrong password
	EXPECT_EQ(CheckLogin(u"Alice", u""), LoginProblem::IncorrectPassword);
	EXPECT_EQ(CheckLogin(u"Alice", std::u16string(33, u'x')), LoginProblem::IncorrectPassword);
}

TEST(MultiplayerRules, LoginFailuresGiveTheGamesMessages)
{
	EXPECT_EQ(LoginFailureMessage(LobbyErrorKind::Refused, true), Message::NotRegistered);
	EXPECT_EQ(LoginFailureMessage(LobbyErrorKind::Refused, false), Message::IncorrectPassword);
	EXPECT_EQ(LoginFailureMessage(LobbyErrorKind::Refused, std::nullopt), Message::IncorrectPassword);
	EXPECT_EQ(LoginFailureMessage(LobbyErrorKind::InvalidName, std::nullopt), Message::InvalidDetails);
	EXPECT_EQ(LoginFailureMessage(LobbyErrorKind::Network, std::nullopt), Message::IncorrectPassword);
	EXPECT_TRUE(AsksAboutName(LobbyErrorKind::Refused));
	EXPECT_FALSE(AsksAboutName(LobbyErrorKind::Timeout));
	EXPECT_EQ(ButtonsOf(Message::NotRegistered), MessageButtons::YesNo);
	EXPECT_EQ(ButtonsOf(Message::WaitingForStart), MessageButtons::Cancel);
}

TEST(MultiplayerRules, JoinFailuresGiveTheGamesMessages)
{
	// On the internet the game shows "Connection Failed." for each failed join
	EXPECT_EQ(TicketFailureMessage(LobbyErrorKind::Refused, "banned_from_server"), Message::ConnectionFailed);
	EXPECT_EQ(TicketFailureMessage(LobbyErrorKind::Refused, "game_started"), Message::ConnectionFailed);
	EXPECT_EQ(TicketFailureMessage(LobbyErrorKind::Network, ""), Message::ConnectionFailed);
	EXPECT_EQ(TicketFailureMessage(LobbyErrorKind::Refused, "email_required"), Message::EmailRequired);
	EXPECT_EQ(JoinFailureMessage("already", LobbyKind::Internet), Message::AlreadyInRoom);
	EXPECT_EQ(JoinFailureMessage("started", LobbyKind::Internet), Message::ConnectionFailed);
	EXPECT_EQ(JoinFailureMessage("", LobbyKind::Internet), Message::ConnectionFailed);
	EXPECT_EQ(JoinFailureMessage("", LobbyKind::Lan), Message::CouldNotConnect);
	EXPECT_EQ(ButtonsOf(Message::IncorrectPassword), MessageButtons::Ok);
}

TEST(MultiplayerRules, FormatTextFillsTheCodesInOrder)
{
	const std::array<TextArgument, 4> arguments = {std::u16string_view(u"Alice"), std::u16string_view(u"Bob's game"),
	                                               std::u16string_view(u"Bombardment"), int64_t {2}};
	EXPECT_EQ(FormatText(u"%s, you are in %s, landscape: %s (%d players).", arguments),
	          u"Alice, you are in Bob's game, landscape: Bombardment (2 players).");
	const std::array<TextArgument, 1> one = {int64_t {5}};
	EXPECT_EQ(FormatText(u"God %d 100%%", one), u"God 5 100%");
	// A code with no argument stays as it is
	EXPECT_EQ(FormatText(u"%s and %s", std::span<const TextArgument>(arguments).first(1)), u"Alice and %s");
	EXPECT_EQ(TimeLimitText(90), u"1:30");
	EXPECT_EQ(TimeLimitText(5), u"0:05");
}

TEST(MultiplayerRules, GameLinesShowTheStatesOfAGame)
{
	const GameLineTexts texts {.playing = u" - PLAYING",
	                           .wrongVersion = u" (Wrong Version)",
	                           .needsPassword = u"(Needs Password)",
	                           .locked = u"Locked"};
	auto server = Server("w1", "Bob's game", 2, 4);
	EXPECT_EQ(GameLine(server, texts, k_GameVersion, LobbyKind::Internet), u"Bob's game (2/4)");
	server.gamemode = "closedplaying";
	server.gamever = "other";
	server.password = true;
	server.locked = true;
	EXPECT_EQ(GameLine(server, texts, k_GameVersion, LobbyKind::Internet),
	          u"Bob's game (2/4) - PLAYING (Wrong Version)(Needs Password)-Locked");
	// Only the internet list shows a lock
	EXPECT_EQ(GameLine(server, texts, k_GameVersion, LobbyKind::Lan),
	          u"Bob's game (2/4) - PLAYING (Wrong Version)(Needs Password)");
	// 32 players fit in the line, and a game with no maximum shows its players alone
	auto big = Server("w2", "Big", 32, 32);
	EXPECT_EQ(GameLine(big, texts, k_GameVersion, LobbyKind::Internet), u"Big (32/32)");
	big.maxplayers = 0;
	EXPECT_EQ(GameLine(big, texts, k_GameVersion, LobbyKind::Internet), u"Big (32)");
}

TEST(MultiplayerRules, TheFilterFindsNamesWithoutRegardToCase)
{
	EXPECT_TRUE(MatchesFilter(u"Bob's Game", u"game"));
	EXPECT_TRUE(MatchesFilter(u"Bob's Game", u""));
	EXPECT_FALSE(MatchesFilter(u"Bob's Game", u"clan"));
	const std::vector servers = {Server("a", "Clan war", 1, 4), Server("b", "Fun", 1, 2), Server("c", "CLAN night", 1, 4)};
	EXPECT_EQ(FilteredGames(servers, u"clan", true), (std::vector<size_t> {0, 2}));
	// With a game picked, the filter does not apply
	EXPECT_EQ(FilteredGames(servers, u"clan", false), (std::vector<size_t> {0, 1, 2}));
}

TEST(MultiplayerRules, ConnectionLinesAreNumbered)
{
	EXPECT_EQ(ConnectionLines(true, u"lobby.test", u"Local Area Network", u"(default)"),
	          (std::vector<std::u16string> {u"1.Local Area Network", u"2. lobby.test (default)"}));
	EXPECT_EQ(ConnectionLines(false, u"lobby.test", u"Local Area Network", u"(default)"),
	          (std::vector<std::u16string> {u"1. lobby.test (default)"}));
	EXPECT_TRUE(ConnectionLines(false, std::nullopt, u"Local Area Network", u"(default)").empty());
}

TEST(MultiplayerRules, SeatsNumberTheGodsInOrder)
{
	const std::vector players = {Player(1, "A", "god"), Player(2, "B", "observer"), Player(3, "C", "god")};
	const auto seats = Seats(players);
	ASSERT_EQ(seats.size(), 3u);
	EXPECT_EQ(seats[0].god, 1);
	EXPECT_EQ(seats[1].god, 0);
	EXPECT_EQ(seats[2].god, 2);
}

TEST(MultiplayerRules, ChatLinesHaveTheGodsColours)
{
	const auto own = ChatLine(u"Alice", u"hi", true, 2);
	EXPECT_EQ(own.text, u"Alice> hi");
	EXPECT_EQ(own.colour, OwnLineColour());
	const auto other = ChatLine(u"Bob", u"hello", false, 1);
	EXPECT_EQ(other.text, u"Bob: hello");
	EXPECT_EQ(other.colour, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
	EXPECT_EQ(GodColour(0), GodColour(-1));
	EXPECT_EQ(GodColour(4), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
	// The fifth god has a colour of its own, and a number past the last god is grey as the observers
	EXPECT_NE(GodColour(5), GodColour(1));
	EXPECT_EQ(GodColour(33), GodColour(0));
	EXPECT_EQ(GodColourBits(1), 0xFFFF0000u);
	EXPECT_EQ(GodColourBits(2), 0xFF00FF00u);
	EXPECT_EQ(GodColourBits(3), 0xFF0000FFu);
	EXPECT_EQ(GodColourBits(4), 0xFFFFFF00u);
	EXPECT_EQ(GodColourBits(0), 0xFF808080u);
	EXPECT_EQ(ClampChat(std::u16string(300, u'x')).size(), 254u);
}

TEST(MultiplayerRules, LocalMapsAndTheirConditions)
{
	const auto maps = ParseLocalMaps(k_LocalMaps);
	ASSERT_EQ(maps.size(), 2u);
	EXPECT_EQ(maps[0].name, u"Bombardment - 2 players");
	EXPECT_EQ(maps[1].file, "mpm_3p_1");
	EXPECT_EQ(maps[1].players, 3);
	EXPECT_EQ(FindLand(maps, "mpm_3p_1"), 1u);
	EXPECT_EQ(FindLand(maps, "Bombardment - 2 players"), 0u);
	EXPECT_FALSE(FindLand(maps, "mpm_9p_1").has_value());
	EXPECT_EQ(LandInfoText(0), "HELP_TEXT_DYK_RULES_02");
	EXPECT_EQ(LandInfoText(2), "HELP_TEXT_DYK_RULES_11");
	EXPECT_FALSE(LandInfoText(3).has_value());

	const auto conditions = FindConditions(k_Conditions, "mpm_2p_1");
	ASSERT_TRUE(conditions.has_value());
	EXPECT_EQ(conditions->at(0).value, 1);
	EXPECT_EQ(conditions->at(0).most, 500);
	EXPECT_EQ(conditions->at(0).rightStep, 25);
	EXPECT_EQ(conditions->at(14).most, 1000);
	EXPECT_EQ(FindConditions(k_Conditions, "mpm_3p_1")->at(0).value, 7);
	EXPECT_FALSE(FindConditions(k_Conditions, "mpm_4p_1").has_value());
	EXPECT_EQ(ConditionText(12), "HELP_TEXT_MULTIPLAYER_14");
}

TEST(MultiplayerRules, ClanInfoLines)
{
	const lobby_api::ClanDetails clan {.id = 1,
	                                   .name = "The Clan",
	                                   .description = "Our motto",
	                                   .leader = "Bob",
	                                   .url = "www.example.com",
	                                   .points = 12.5,
	                                   .members = 3};
	const ClanInfoTexts texts {
	    .description = u"Clan Description:", .leader = u"Clan Leader:", .url = u"Clan URL:", .credits = u"Credits"};
	EXPECT_EQ(ClanInfoLines(clan, texts), (std::vector<std::u16string> {u"Clan Description: Our motto", u"Clan Leader: Bob",
	                                                                    u"Clan URL: www.example.com", u"Credits 12.50"}));
}

TEST(MultiplayerRules, ClanLogosKeepSixteenGreys)
{
	// A bottom-up 8-bit BMP of 2 by 2 with a palette of 4 colours: white, black, a mid grey and red
	std::vector<uint8_t> bmp(54 + 16 + 8, 0);
	const auto put32 = [&bmp](size_t at, uint32_t value) {
		for (size_t i = 0; i < 4; ++i)
		{
			bmp[at + i] = static_cast<uint8_t>(value >> (i * 8));
		}
	};
	bmp[0] = 'B';
	bmp[1] = 'M';
	put32(10, 54 + 16);
	put32(14, 40);
	put32(18, 2);
	put32(22, 2);
	bmp[26] = 1;
	bmp[28] = 8;
	put32(46, 4);
	const std::array<std::array<uint8_t, 4>, 4> palette = {
	    {{255, 255, 255, 0}, {0, 0, 0, 0}, {128, 128, 128, 0}, {0, 0, 255, 0}}};
	for (size_t i = 0; i < palette.size(); ++i)
	{
		std::copy(palette[i].begin(), palette[i].end(), bmp.begin() + 54 + static_cast<ptrdiff_t>(i * 4));
	}
	// The bottom row first, each row padded to 4 bytes: bottom = grey, red; top = white, black
	bmp[70] = 2;
	bmp[71] = 3;
	bmp[74] = 0;
	bmp[75] = 1;
	const auto logo = DecodeClanLogo(bmp);
	ASSERT_TRUE(logo.has_value());
	EXPECT_EQ(logo->at(0), 15);
	EXPECT_EQ(logo->at(1), 0);
	EXPECT_EQ(logo->at(static_cast<size_t>(k_LogoSize)), 8);
	// Red is the grey 85, whose top bits are 5
	EXPECT_EQ(logo->at(static_cast<size_t>(k_LogoSize) + 1), 5);
	// Past the picture the logo has no opacity
	EXPECT_EQ(logo->at(2), 0);
	EXPECT_FALSE(DecodeClanLogo(std::span(bmp).first(20)).has_value());
}

TEST(MultiplayerRules, FlowGoesThroughTheBoxesInOrder)
{
	Flow flow;
	Next(flow);
	EXPECT_EQ(flow.box, Box::FindGames);
	Next(flow);
	EXPECT_EQ(flow.box, Box::Login);
	// The login box moves on only with the lobby's answer
	Next(flow);
	EXPECT_EQ(flow.box, Box::Login);
	Back(flow);
	EXPECT_EQ(flow.box, Box::FindGames);
	flow.kind = LobbyKind::Lan;
	Next(flow);
	EXPECT_EQ(flow.box, Box::GameList);
	Back(flow);
	EXPECT_EQ(flow.box, Box::FindGames);
	flow.kind = LobbyKind::Internet;
	flow.box = Box::GameList;
	Back(flow);
	EXPECT_EQ(flow.box, Box::Login);
	flow.box = Box::Room;
	Next(flow);
	EXPECT_EQ(flow.box, Box::Conditions);
	Back(flow);
	EXPECT_EQ(flow.box, Box::GameList);
	flow.box = Box::ChooseConnection;
	Back(flow);
	EXPECT_TRUE(flow.left);
}

TEST(MultiplayerJson, RoomFramesAndLobbyShapes)
{
	EXPECT_EQ(lobby_api::RoomJoinFrame("t", "", ""), R"({"ticket":"t","type":"room.join"})");
	EXPECT_EQ(lobby_api::RoomJoinFrame("", "pw", "Alice"), R"({"game_password":"pw","name":"Alice","type":"room.join"})");
	EXPECT_EQ(lobby_api::RoomReadyFrame(true), R"({"ready":true,"type":"room.ready"})");
	EXPECT_EQ(lobby_api::ToJson(lobby_api::ChatLineRequest {.text = "hi"}), R"({"text":"hi"})");
	EXPECT_EQ(lobby_api::ToJson(lobby_api::UseClanRequest {.clan = std::nullopt}), R"({"clan":null})");
	EXPECT_EQ(lobby_api::ToJson(lobby_api::UseClanRequest {.clan = 4}), R"({"clan":4})");
	const auto line = lobby_api::ParseChatLine(R"({"channel":"server-w1","from":{"id":3,"name":"Bob"},"text":"hello"})");
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(line->from.name, "Bob");
	const auto name = lobby_api::ParseAccountNameCheck(R"({"name":"Zed","valid":true,"available":true,"problems":[]})");
	ASSERT_TRUE(name.has_value());
	EXPECT_TRUE(name->available);
	const auto used = lobby_api::ParseUsedClan(R"({"clan":{"id":4,"name":"The Clan","role":"leader"}})");
	ASSERT_TRUE(used.has_value() && used->clan.has_value());
	EXPECT_EQ(used->clan->id, 4);
	EXPECT_FALSE(lobby_api::ParseUsedClan(R"({"clan":null})")->clan.has_value());
	const auto room = lobby_api::ParseRoomChange(
	    R"({"server":"w1","gamemode":"openstaging","players":[{"account":{"id":3,"name":"Bob"},"role":"god","clan":null}]})");
	ASSERT_TRUE(room.has_value());
	EXPECT_EQ(room->players.size(), 1u);
	// The clan's details in the shape of the lobby's contract
	const auto clan = lobby_api::ParseClanDetails(
	    R"({"id":4,"name":"The Clan","motto":"Our motto","website":"www.example.com","leader":{"id":3,"name":"Bob"},"points":12.5,"members":[{},{}]})");
	ASSERT_TRUE(clan.has_value());
	EXPECT_EQ(clan->description, "Our motto");
	EXPECT_EQ(clan->leader, "Bob");
	EXPECT_EQ(clan->url, "www.example.com");
	EXPECT_EQ(clan->members, 2);
}

TEST_F(FrontEndTest, TheFirstBoxListsTheConnections)
{
	Make(true);
	EXPECT_EQ(frontEnd->GetBox(), Box::ChooseConnection);
	EXPECT_EQ(frontEnd->GetConnections(), (std::vector<LobbyKind> {LobbyKind::Lan, LobbyKind::Internet}));
	// The internet connection is the default
	EXPECT_EQ(frontEnd->GetConnection(), 1u);
	frontEnd->Back();
	EXPECT_TRUE(frontEnd->TakeLeft());
}

TEST_F(FrontEndTest, LoginGoesToTheGameList)
{
	ToGameList();
	EXPECT_EQ(lobby.loginName, "Alice");
	EXPECT_EQ(lobby.loginPassword, "secret");
	EXPECT_TRUE(lobby.watchingServers);
	EXPECT_EQ(frontEnd->GetMessage(), Message::None);
	EXPECT_EQ(frontEnd->GetShownGames(), (std::vector<size_t> {0, 1}));
	// Back goes to the login box and stops the list's changes
	frontEnd->Back();
	EXPECT_EQ(frontEnd->GetBox(), Box::Login);
	EXPECT_FALSE(lobby.watchingServers);
}

TEST_F(FrontEndTest, AWrongLoginAsksAboutTheName)
{
	Make();
	frontEnd->Next();
	frontEnd->Next();
	frontEnd->Login(u"Zed", u"secret");
	FakeLobby::Fail(lobby.login, LobbyErrorKind::Refused);
	EXPECT_EQ(lobby.checkedName, "Zed");
	EXPECT_TRUE(frontEnd->IsBusy());
	FakeLobby::Give(lobby.nameCheck, lobby_api::AccountNameCheck {.name = "Zed", .valid = true, .available = true});
	EXPECT_EQ(frontEnd->GetMessage(), Message::NotRegistered);
	// Yes opens the registration page
	frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::Yes);
	EXPECT_EQ(opened, (std::vector<std::string> {"https://lobby.test/register/"}));
	EXPECT_EQ(frontEnd->GetMessage(), Message::None);
	EXPECT_EQ(frontEnd->GetBox(), Box::Login);

	// A taken name has an account: the password was wrong
	frontEnd->Login(u"Bob", u"wrong");
	FakeLobby::Fail(lobby.login, LobbyErrorKind::Refused);
	FakeLobby::Give(lobby.nameCheck, lobby_api::AccountNameCheck {.name = "Bob", .valid = true, .available = false});
	EXPECT_EQ(frontEnd->GetMessage(), Message::IncorrectPassword);
	frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::Ok);
	EXPECT_TRUE(opened.size() == 1);

	// The box finds a bad name itself
	frontEnd->Login(u"A b", u"secret");
	EXPECT_EQ(frontEnd->GetMessage(), Message::InvalidDetails);
	EXPECT_FALSE(lobby.login);
}

TEST_F(FrontEndTest, TheFilterAppliesWithNoGamePicked)
{
	ToGameList();
	frontEnd->SetFilter(u"FUN");
	EXPECT_EQ(frontEnd->GetShownGames(), (std::vector<size_t> {1}));
	frontEnd->SelectGame(0);
	EXPECT_EQ(frontEnd->GetShownGames(), (std::vector<size_t> {0, 1}));
	frontEnd->SelectGame(5);
	EXPECT_FALSE(frontEnd->GetSelectedGame().has_value());
}

TEST_F(FrontEndTest, AFailedListShowsConnectionFailed)
{
	Make();
	frontEnd->Next();
	frontEnd->Next();
	frontEnd->Login(u"Alice", u"secret");
	FakeLobby::Give(lobby.login, lobby_api::AccountRef {.id = 7, .name = "Alice"});
	FakeLobby::Fail(lobby.servers, LobbyErrorKind::Network);
	EXPECT_EQ(frontEnd->GetMessage(), Message::ConnectionFailed);
}

TEST_F(FrontEndTest, JoinTakesATicketThenOpensTheGameSocket)
{
	ToRoom();
	const auto& socket = sockets.sockets.at(sockets.order.back());
	EXPECT_EQ(socket.request.url, "wss://lobby.test/game/w1");
	ASSERT_EQ(socket.sent.size(), 1u);
	EXPECT_EQ(socket.sent[0].Text(), R"({"ticket":"v4.public.t","type":"room.join"})");
	// The network game joins for the front end: the server's land loads only after the start
	ASSERT_EQ(network.joins.size(), 1u);
	EXPECT_FALSE(network.joins[0].landLoaded);
	EXPECT_EQ(lobby.ticketRoles, (std::vector<game_socket::Role> {game_socket::Role::God}));
	EXPECT_TRUE(frontEnd->IsJoined());
	EXPECT_FALSE(frontEnd->IsObserver());
	EXPECT_TRUE(lobby.watched.at("server-w1"));
	ASSERT_EQ(frontEnd->GetSeats().size(), 3u);
	ASSERT_NE(frontEnd->GetOwnSeat(), nullptr);
	EXPECT_EQ(frontEnd->GetOwnSeat()->god, 2);
	EXPECT_EQ(frontEnd->GetServerLand(), 0u);
	ASSERT_TRUE(frontEnd->GetConditions().has_value());
	// Back leaves the room and its channel, and the server gets the leave
	frontEnd->Back();
	EXPECT_EQ(frontEnd->GetBox(), Box::GameList);
	EXPECT_FALSE(lobby.watched.at("server-w1"));
	EXPECT_EQ(network.leaves, 1);
	EXPECT_EQ(socket.sent.back().Text(), R"({"type":"room.leave"})");
	Step();
	EXPECT_EQ(sockets.Count(), 0u);
}

TEST_F(FrontEndTest, ARefusedTicketShowsConnectionFailed)
{
	ToGameList();
	frontEnd->SelectGame(1);
	frontEnd->Join(u"");
	FakeLobby::Fail(lobby.ticket, LobbyErrorKind::Refused);
	EXPECT_EQ(frontEnd->GetMessage(), Message::ConnectionFailed);
	EXPECT_EQ(frontEnd->GetBox(), Box::GameList);
	EXPECT_TRUE(network.joins.empty());
}

TEST_F(FrontEndTest, AServerBanShowsConnectionFailed)
{
	ToGameList();
	frontEnd->SelectGame(0);
	frontEnd->Join(u"");
	auto done = std::move(lobby.ticket);
	done({.value = std::nullopt,
	      .error = {.kind = LobbyErrorKind::Refused, .status = 403, .detail = {}, .code = "banned_from_server"}});
	EXPECT_EQ(frontEnd->GetMessage(), Message::ConnectionFailed);
	EXPECT_EQ(lobby.ticketRoles.size(), 1u);
}

TEST_F(FrontEndTest, AFullRoomIsJoinedAsAnObserver)
{
	ToGameList();
	frontEnd->SelectGame(0);
	frontEnd->Join(u"");
	auto full = std::move(lobby.ticket);
	full({.value = std::nullopt,
	      .error = {.kind = LobbyErrorKind::Refused, .status = 409, .detail = {}, .code = "server_full"}});
	// The front end asks again, for an observer's ticket, with no message
	EXPECT_EQ(frontEnd->GetMessage(), Message::None);
	EXPECT_EQ(lobby.ticketRoles, (std::vector<game_socket::Role> {game_socket::Role::God, game_socket::Role::Observer}));
	FakeLobby::Give(
	    lobby.ticket,
	    lobby_api::JoinTicket {.url = "wss://lobby.test/game/w1", .ticket = "v4.public.o", .expires = 0, .role = "observer"});
	sockets.Accept(sockets.order.back());
	Step();
	ServerJoins(game_socket::Role::Observer);
	EXPECT_EQ(frontEnd->GetBox(), Box::Room);
	EXPECT_TRUE(frontEnd->IsObserver());
	// A second full answer for the observer is a failure, not a loop
	frontEnd->Back();
	FakeLobby::Give(lobby.servers, lobby_api::ServerList {.servers = lobby.known});
	frontEnd->SelectGame(0);
	frontEnd->Join(u"");
	full = std::move(lobby.ticket);
	full({.value = std::nullopt,
	      .error = {.kind = LobbyErrorKind::Refused, .status = 409, .detail = {}, .code = "server_full"}});
	auto again = std::move(lobby.ticket);
	again({.value = std::nullopt,
	       .error = {.kind = LobbyErrorKind::Refused, .status = 409, .detail = {}, .code = "server_full"}});
	EXPECT_EQ(frontEnd->GetMessage(), Message::ConnectionFailed);
}

TEST_F(FrontEndTest, TheServerRefusesAnAccountThatIsInTheRoom)
{
	ToGameList();
	frontEnd->SelectGame(0);
	frontEnd->Join(u"");
	FakeLobby::Give(lobby.ticket, lobby_api::JoinTicket {
	                                  .url = "wss://lobby.test/game/w1", .ticket = "v4.public.t", .expires = 0, .role = "god"});
	sockets.Accept(sockets.order.back());
	Step();
	sockets.ServerText(sockets.order.back(), game_socket::ToJson(game_socket::Refused {.reason = "already"}));
	Step();
	EXPECT_EQ(frontEnd->GetMessage(), Message::AlreadyInRoom);
	EXPECT_EQ(frontEnd->GetBox(), Box::GameList);
	EXPECT_FALSE(frontEnd->IsBusy());
}

TEST_F(FrontEndTest, ASocketThatNeverOpensShowsConnectionFailed)
{
	ToGameList();
	frontEnd->SelectGame(0);
	frontEnd->Join(u"");
	FakeLobby::Give(lobby.ticket, lobby_api::JoinTicket {
	                                  .url = "wss://lobby.test/game/w1", .ticket = "v4.public.t", .expires = 0, .role = "god"});
	sockets.End(sockets.order.back(), {.end = online::WebSocketEnd::Network, .code = 0, .httpStatus = 0});
	Step();
	EXPECT_EQ(frontEnd->GetMessage(), Message::ConnectionFailed);
}

TEST_F(FrontEndTest, AnAccountWithNoEmailIsSentToTheAccountPage)
{
	Make();
	frontEnd->Next();
	frontEnd->Next();
	frontEnd->Login(u"Alice", u"secret");
	lobby.account = lobby_api::AccountRef {.id = 7, .name = "Alice"};
	lobby.emailRequired = true;
	FakeLobby::Give(lobby.login, *lobby.account);
	// The lobby would refuse the list: the box stays on the login
	EXPECT_EQ(frontEnd->GetBox(), Box::Login);
	EXPECT_EQ(frontEnd->GetMessage(), Message::EmailRequired);
	EXPECT_EQ(ButtonsOf(Message::EmailRequired), MessageButtons::YesNo);
	EXPECT_FALSE(lobby.servers);
	frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::Yes);
	EXPECT_EQ(opened, (std::vector<std::string> {"https://lobby.test/account/"}));
}

TEST_F(FrontEndTest, ALostLobbySessionGoesBackToTheLogin)
{
	ToRoom();
	// The lobby ended the session, for example for a ban
	lobby.account.reset();
	frontEnd->Update();
	EXPECT_EQ(frontEnd->GetBox(), Box::Login);
	EXPECT_EQ(frontEnd->GetMessage(), Message::ConnectionFailed);
	EXPECT_EQ(network.leaves, 1);
}

TEST_F(FrontEndTest, TheStartGivesTheSessionToTheNetworkGame)
{
	ToRoom();
	frontEnd->Next();
	frontEnd->SetReady(true);
	sockets.ServerText(sockets.order.back(), game_socket::ToJson(game_socket::Start {
	                                             .land = {}, .testbed = true, .seed = 3, .turn = 0, .opening = {}}));
	Step();
	EXPECT_TRUE(frontEnd->IsStarted());
	// The front end closes, and the network game keeps the socket for the game
	frontEnd.reset();
	EXPECT_EQ(network.leaves, 0);
	EXPECT_EQ(sockets.Count(), 1u);
	EXPECT_EQ(network.session.GetState(), online::NetworkSession::State::Playing);
}

TEST_F(FrontEndTest, TheRoomChatUsesTheServersChannel)
{
	ToRoom();
	frontEnd->SendChat(std::u16string(300, u'a'));
	EXPECT_EQ(lobby.chatChannel, "server-w1");
	EXPECT_EQ(lobby.chatText.size(), 254u);
	lobby.Event("chat.line", R"({"channel":"server-w1","from":{"id":3,"name":"Bob"},"text":"hello"})");
	lobby.Event("chat.line", R"({"channel":"server-w1","from":{"id":7,"name":"Alice"},"text":"hi"})");
	// A line of another channel is not the room's
	lobby.Event("chat.line", R"({"channel":"lobby","from":{"id":3,"name":"Bob"},"text":"elsewhere"})");
	const auto& chat = frontEnd->GetChat();
	ASSERT_EQ(chat.size(), 2u);
	EXPECT_EQ(chat[0].text, u"Bob: hello");
	EXPECT_EQ(chat[0].colour, GodColour(1));
	EXPECT_EQ(chat[1].text, u"Alice> hi");
	EXPECT_EQ(chat[1].colour, OwnLineColour());
}

TEST_F(FrontEndTest, RoomChangesUpdateTheSeats)
{
	ToRoom();
	lobby.Event(
	    "room.changed",
	    R"({"server":"w1","gamemode":"openstaging","players":[{"account":{"id":7,"name":"Alice"},"role":"god","clan":null}]})");
	ASSERT_EQ(frontEnd->GetSeats().size(), 1u);
	EXPECT_EQ(frontEnd->GetOwnSeat()->god, 1);
	EXPECT_EQ(frontEnd->GetRoomServer()->numplayers, 1);
}

TEST_F(FrontEndTest, UseThisClan)
{
	ToRoom();
	FakeLobby::Give(lobby.myClans, lobby_api::ClanList {.clans = {{.id = 4, .name = "The Clan", .points = 0.0}}});
	ASSERT_EQ(frontEnd->GetMyClans().size(), 1u);
	frontEnd->ShowClan(0);
	EXPECT_EQ(lobby.clanAsked, 4);
	FakeLobby::Give(lobby.clan, lobby_api::ClanDetails {.id = 4, .name = "The Clan"});
	ASSERT_TRUE(frontEnd->GetShownClan().has_value());
	frontEnd->UseShownClan();
	EXPECT_EQ(lobby.useClanServer, "w1");
	EXPECT_EQ(lobby.useClanId, 4);
	FakeLobby::Give(lobby.useClan,
	                lobby_api::UsedClan {.clan = lobby_api::ClanRef {.id = 4, .name = "The Clan", .role = "leader"}});
	EXPECT_EQ(frontEnd->GetUsedClan(), 4);
	frontEnd->UseNoClan();
	EXPECT_FALSE(lobby.useClanId.has_value());
	FakeLobby::Give(lobby.useClan, lobby_api::UsedClan {});
	EXPECT_FALSE(frontEnd->GetUsedClan().has_value());
}

TEST_F(FrontEndTest, ReadyWaitsForTheStart)
{
	ToRoom();
	frontEnd->Next();
	EXPECT_EQ(frontEnd->GetBox(), Box::Conditions);
	frontEnd->SetReady(true);
	EXPECT_TRUE(frontEnd->IsReady());
	EXPECT_EQ(frontEnd->GetMessage(), Message::WaitingForStart);
	const auto& socket = sockets.sockets.at(sockets.order.back());
	EXPECT_EQ(socket.sent.back().Text(), R"({"ready":true,"type":"room.ready"})");
	// Cancel takes the tick away
	frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::Cancel);
	EXPECT_FALSE(frontEnd->IsReady());
	EXPECT_EQ(frontEnd->GetMessage(), Message::None);
	EXPECT_EQ(socket.sent.back().Text(), R"({"ready":false,"type":"room.ready"})");
}

TEST_F(FrontEndTest, TheServerGoingSendsThePlayerToTheList)
{
	ToRoom();
	sockets.End(sockets.order.back(), {.end = online::WebSocketEnd::ClosedByServer, .code = 1000, .httpStatus = 0});
	Step();
	// The session tries to come back to the server for its time, then gives up
	EXPECT_EQ(frontEnd->GetMessage(), Message::None);
	network.now += 6LL * 60 * 1'000'000;
	Step();
	EXPECT_EQ(frontEnd->GetMessage(), Message::HostLeft);
	frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::Ok);
	EXPECT_EQ(frontEnd->GetBox(), Box::GameList);
}

TEST_F(FrontEndTest, TheLanListComesFromTheSearch)
{
	Make(true);
	lan.servers = {Server("l1", "LAN game", 1, 32)};
	lan.servers[0].url = "wss://192.168.1.5/game/l1";
	frontEnd->Next();
	frontEnd->SetLobbyKind(LobbyKind::Lan);
	frontEnd->Next();
	EXPECT_EQ(frontEnd->GetBox(), Box::GameList);
	EXPECT_EQ(lan.starts, 1);
	frontEnd->Update();
	EXPECT_EQ(lan.polls, 1);
	ASSERT_EQ(frontEnd->GetGames().size(), 1u);
	frontEnd->SelectGame(0);
	frontEnd->Join(u"");
	// On the LAN there is no ticket: the profile's name goes in the join
	ASSERT_FALSE(sockets.order.empty());
	sockets.Accept(sockets.order.back());
	Step();
	EXPECT_EQ(sockets.sockets.at(sockets.order.back()).sent[0].Text(), R"({"name":"Alice","type":"room.join"})");
	ServerJoins();
	EXPECT_EQ(frontEnd->GetBox(), Box::Room);
	frontEnd->Back();
	EXPECT_EQ(frontEnd->GetBox(), Box::GameList);
	frontEnd->Back();
	EXPECT_EQ(frontEnd->GetBox(), Box::FindGames);
	EXPECT_EQ(lan.stops, 1);
}

TEST_F(FrontEndTest, TheListenerGoesWithTheFrontEnd)
{
	Make();
	frontEnd.reset();
	EXPECT_EQ(lobby.removed, (std::vector<uint32_t> {1}));
}

namespace
{
/// The light of one part of a colour, from 0 to 1, as the sRGB curve has it
double LinearPart(uint32_t part)
{
	const auto value = static_cast<double>(part) / 255.0;
	return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}

/// The relative luminance of a colour, 0 for black and 1 for white
double Luminance(uint32_t argb)
{
	return (0.2126 * LinearPart((argb >> 16u) & 0xFFu)) + (0.7152 * LinearPart((argb >> 8u) & 0xFFu)) +
	       (0.0722 * LinearPart(argb & 0xFFu));
}

/// A colour in the CIE L*a*b* space, where the distance between two colours is near how different they look
std::array<double, 3> Lab(uint32_t argb)
{
	const auto r = LinearPart((argb >> 16u) & 0xFFu);
	const auto g = LinearPart((argb >> 8u) & 0xFFu);
	const auto b = LinearPart(argb & 0xFFu);
	const auto x = ((0.4124 * r) + (0.3576 * g) + (0.1805 * b)) / 0.95047;
	const auto y = (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
	const auto z = ((0.0193 * r) + (0.1192 * g) + (0.9505 * b)) / 1.08883;
	const auto f = [](double t) { return t > 216.0 / 24389.0 ? std::cbrt(t) : ((24389.0 / 27.0 * t) + 16.0) / 116.0; };
	return {(116.0 * f(y)) - 16.0, 500.0 * (f(x) - f(y)), 200.0 * (f(y) - f(z))};
}

double Difference(uint32_t first, uint32_t second)
{
	const auto a = Lab(first);
	const auto b = Lab(second);
	return std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}
} // namespace

TEST(MultiplayerRules, The32GodsHaveDistinctReadableChatColours)
{
	// Each pair of the 32 gods, the observers' grey and the player's own lines differ clearly
	std::vector<uint32_t> colours;
	for (int32_t god = 1; god <= k_MostGods; ++god)
	{
		EXPECT_EQ(GodColourBits(god) >> 24u, 0xFFu) << "god " << god;
		colours.push_back(GodColourBits(god));
	}
	colours.push_back(GodColourBits(0));
	colours.push_back(OwnLineColourBits());
	constexpr double k_LeastDifference = 25.0;
	for (size_t i = 0; i < colours.size(); ++i)
	{
		for (size_t j = i + 1; j < colours.size(); ++j)
		{
			EXPECT_GE(Difference(colours[i], colours[j]), k_LeastDifference) << std::hex << colours[i] << " and " << colours[j];
		}
	}
	// Gods 5 to 32 are bright enough to read on the dark chat box: a contrast of 4.5 to 1 or more with black
	for (int32_t god = 5; god <= k_MostGods; ++god)
	{
		const auto contrast = (Luminance(GodColourBits(god)) + 0.05) / 0.05;
		EXPECT_GE(contrast, 4.5) << "god " << god;
	}
}
