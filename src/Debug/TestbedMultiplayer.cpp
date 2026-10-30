/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedMultiplayer.h"

#include <cstdint>

#include <algorithm>
#include <array>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <GameSocket.h>
#include <LobbyApi.h>
#include <LobbyApiJson.h>

#include "Commands/TurnCodec.h"
#include "Common/BuildVersion.h"
#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "Gui/MultiplayerFrontEnd.h"
#include "Online/LanDiscoveryInterface.h"
#include "Online/NetworkSession.h"
#include "Online/WebSocketInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::testbed_multiplayer;
using openblack::gui::MultiplayerFrontEnd;
using openblack::gui::multiplayer::Box;
using openblack::gui::multiplayer::LobbyKind;
using openblack::gui::multiplayer::RoomTab;

namespace
{

constexpr int64_t k_PlayerId = 7;
/// The other gods in the big room; the player is the last of its 32 gods
constexpr int64_t k_BigRoomOthers = 31;
/// The time limit of the game of the demonstration server that picks winning conditions, in minutes
constexpr uint32_t k_DemoTimeLimitMinutes = 90;

lobby_api::RoomPlayer Player(int64_t id, std::string name, std::string role)
{
	return {.account = {.id = id, .name = std::move(name)}, .role = std::move(role), .clan = std::nullopt};
}

lobby_api::ServerEntry Server(std::string id, std::string name, std::string land, int32_t most)
{
	lobby_api::ServerEntry server;
	server.id = std::move(id);
	server.hostname = std::move(name);
	server.land = std::move(land);
	server.gamemode = "openstaging";
	server.maxplayers = most;
	server.gamever = std::string(build_version::Text());
	server.url = "wss://demo.invalid/game/" + server.id;
	return server;
}

std::vector<lobby_api::ServerEntry> DemoServers()
{
	// The game server sends the path of the map file of its land, as a real server does
	auto bob = Server("w1", "Bob's game", "Online Maps/mpm_2p_1.map", 4);
	bob.players = {Player(3, "Bob", "god"), Player(k_PlayerId, "Alice", "god"), Player(9, "Eve", "observer")};
	auto fun = Server("w2", "Fun", "mpm_3p_1", 2);
	fun.gamemode = "closedplaying";
	fun.players = {Player(11, "Carol", "god")};
	auto test = Server("w3", "Test", "mpm_4p_1", 4);
	test.gamever = "older";
	test.players = {Player(12, "Dan", "god")};
	auto clans = Server("w4", "Clan war", "mpm_4p_1", 4);
	clans.password = true;
	clans.locked = true;
	clans.players = {Player(13, "Fay", "god"), Player(14, "Gus", "god"), Player(15, "Hal", "god")};
	auto big = Server("w5", "Thirty-two gods", "mpm_4p_1", 32);
	for (int64_t i = 0; i < k_BigRoomOthers; ++i)
	{
		big.players.push_back(Player(100 + i, "God" + std::to_string(i + 1), "god"));
	}
	// The player is the last god of the big room
	big.players.push_back(Player(k_PlayerId, "Alice", "god"));
	// A room whose gods are all there: the lobby gives only an observer's ticket for it
	auto full = Server("w6", "Full game", "mpm_2p_1", 2);
	full.players = {Player(16, "Ivy", "god"), Player(17, "Jon", "god")};
	std::vector servers = {bob, fun, test, clans, big, full};
	for (auto& server : servers)
	{
		server.numplayers =
		    static_cast<int32_t>(std::ranges::count(server.players, std::string("god"), &lobby_api::RoomPlayer::role));
	}
	return servers;
}

/// A 64 by 64 grey BMP of 8 bits a pixel: a ring and a bar, in 16 greys
std::vector<uint8_t> DemoLogo()
{
	constexpr int32_t k_Size = 64;
	constexpr size_t k_Palette = 256 * 4;
	constexpr size_t k_Offset = 54 + k_Palette;
	std::vector<uint8_t> bmp(k_Offset + (k_Size * k_Size), 0);
	const auto put32 = [&bmp](size_t at, uint32_t value) {
		for (size_t i = 0; i < 4; ++i)
		{
			bmp[at + i] = static_cast<uint8_t>(value >> (i * 8));
		}
	};
	bmp[0] = 'B';
	bmp[1] = 'M';
	put32(2, static_cast<uint32_t>(bmp.size()));
	put32(10, static_cast<uint32_t>(k_Offset));
	put32(14, 40);
	put32(18, k_Size);
	put32(22, k_Size);
	bmp[26] = 1;
	bmp[28] = 8;
	put32(46, 256);
	for (size_t i = 0; i < 256; ++i)
	{
		bmp[54 + (i * 4)] = static_cast<uint8_t>(i);
		bmp[54 + (i * 4) + 1] = static_cast<uint8_t>(i);
		bmp[54 + (i * 4) + 2] = static_cast<uint8_t>(i);
	}
	for (int32_t y = 0; y < k_Size; ++y)
	{
		for (int32_t x = 0; x < k_Size; ++x)
		{
			const auto dx = x - 32;
			const auto dy = y - 32;
			const auto distance = (dx * dx) + (dy * dy);
			uint8_t grey = 0;
			if (distance > 18 * 18 && distance < 28 * 28)
			{
				grey = static_cast<uint8_t>(255 - (y * 2));
			}
			if (x > 28 && x < 36 && y > 10 && y < 54)
			{
				grey = 255;
			}
			bmp[k_Offset + static_cast<size_t>((y * k_Size) + x)] = grey;
		}
	}
	return bmp;
}

/// The tickets of the demonstration, which say the role
constexpr std::string_view k_GodTicket = "demo";
constexpr std::string_view k_ObserverTicket = "demo-observer";

/// A lobby with no network: each call answers at once with the demonstration's data
class DemoLobby final: public LobbyClientSystemInterface
{
public:
	void Update() override {}
	[[nodiscard]] bool IsConfigured() const override { return true; }
	[[nodiscard]] LobbySessionState GetState() const override
	{
		return _account.has_value() ? LobbySessionState::LoggedIn : LobbySessionState::LoggedOut;
	}
	[[nodiscard]] std::optional<lobby_api::AccountRef> GetAccount() const override { return _account; }
	[[nodiscard]] bool IsPushOpen() const override { return _account.has_value(); }
	[[nodiscard]] bool IsEmailRequired() const override { return false; }
	void Login(std::string name, std::string, LobbyCallback<lobby_api::AccountRef> callback) override
	{
		// Every name but the demonstration's has no account
		if (name != "Alice")
		{
			callback({.value = std::nullopt, .error = {.kind = LobbyErrorKind::Refused, .status = 401}});
			return;
		}
		_account = lobby_api::AccountRef {.id = k_PlayerId, .name = std::move(name)};
		callback({.value = _account, .error = {}});
	}
	void ResumeSession(LobbyCallback<lobby_api::AccountRef>) override {}
	void Logout(bool, LobbyCallback<LobbyDone>) override { _account.reset(); }
	void GetMe(LobbyCallback<lobby_api::Me>) override {}
	void GetServers(LobbyCallback<lobby_api::ServerList> callback) override
	{
		callback({.value = lobby_api::ServerList {.servers = _servers}, .error = {}});
	}
	[[nodiscard]] std::span<const lobby_api::ServerEntry> GetKnownServers() const override { return _servers; }
	void WatchServers(bool) override {}
	void WatchChannel(std::string channel, bool watch) override
	{
		// The room of Bob's game has a few lines of chat when the player comes in
		if (watch && channel == "server-w1")
		{
			_pending.push_back(Line(3, "Bob", "Hello, and welcome to the game."));
			_pending.push_back(Line(9, "Eve", "I am only watching."));
			_pending.push_back(Line(k_PlayerId, "Alice", "Hi, all!"));
		}
		// In the big room each of the other gods says a line, so that the chat shows the colour of each god
		if (watch && channel == "server-w5")
		{
			for (int64_t i = 0; i < k_BigRoomOthers; ++i)
			{
				const auto name = "God" + std::to_string(i + 1);
				_pending.push_back(Line(100 + i, name, "I am god " + std::to_string(i + 1) + ".", channel));
			}
		}
	}
	void RequestJoinTicket(std::string serverId, game_socket::Role role, LobbyCallback<lobby_api::JoinTicket> callback) override
	{
		// A god's ticket for a full room is refused, as the lobby refuses it
		const auto server = std::ranges::find(_servers, serverId, &lobby_api::ServerEntry::id);
		if (role == game_socket::Role::God && server != _servers.end() && server->numplayers >= server->maxplayers &&
		    std::ranges::none_of(server->players, [](const lobby_api::RoomPlayer& p) { return p.account.id == k_PlayerId; }))
		{
			callback({.value = std::nullopt,
			          .error = {.kind = LobbyErrorKind::Refused,
			                    .status = 409,
			                    .detail = "The room is full. You can still join as an observer.",
			                    .code = std::string(lobby_api::problems::k_ServerFull)}});
			return;
		}
		callback({.value = lobby_api::JoinTicket {.url = "wss://demo.invalid/game/" + serverId,
		                                          .ticket = std::string(role == game_socket::Role::Observer ? k_ObserverTicket
		                                                                                                    : k_GodTicket),
		                                          .expires = 0,
		                                          .role = std::string(game_socket::RoleName(role))},
		          .error = {}});
	}
	void SendChatLine(std::string, std::string text, LobbyCallback<LobbyDone> callback) override
	{
		// The line comes back on the room's channel, as the lobby sends it to everyone in the room
		_pending.push_back(Line(k_PlayerId, "Alice", text));
		callback({.value = LobbyDone {}, .error = {}});
	}
	void UseClan(std::string, std::optional<int64_t> clanId, LobbyCallback<lobby_api::UsedClan> callback) override
	{
		lobby_api::UsedClan used;
		if (clanId.has_value())
		{
			used.clan = lobby_api::ClanRef {.id = *clanId, .name = ClanName(*clanId), .role = "leader"};
		}
		callback({.value = std::move(used), .error = {}});
	}
	void CheckAccountName(std::string name, LobbyCallback<lobby_api::AccountNameCheck> callback) override
	{
		callback(
		    {.value = lobby_api::AccountNameCheck {.name = name, .valid = true, .available = name != "Alice"}, .error = {}});
	}
	void GetMyClans(LobbyCallback<lobby_api::ClanList> callback) override
	{
		callback({.value = lobby_api::ClanList {.clans = {{.id = 4, .name = "The Clan", .points = 12.5},
		                                                  {.id = 5, .name = "Other Clan", .points = 3.0}}},
		          .error = {}});
	}
	void GetClan(int64_t clanId, LobbyCallback<lobby_api::ClanDetails> callback) override
	{
		callback({.value = lobby_api::ClanDetails {.id = clanId,
		                                           .name = ClanName(clanId),
		                                           .description = "Our motto",
		                                           .leader = "Alice",
		                                           .url = "www.example.com",
		                                           .points = clanId == 4 ? 12.5 : 3.0,
		                                           .members = 3},
		          .error = {}});
	}
	void GetClanLogo(int64_t, LobbyCallback<std::vector<uint8_t>> callback) override
	{
		callback({.value = DemoLogo(), .error = {}});
	}
	void GetPlayerRanking(int32_t, LobbyCallback<lobby_api::RankingPage>) override {}
	void GetClanRanking(int32_t, LobbyCallback<lobby_api::RankingPage>) override {}
	void GetPlayer(int64_t, LobbyCallback<lobby_api::PlayerDetails>) override {}
	void GetWeather(lobby_api::Date, LobbyCallback<lobby_api::RealWeather>) override {}
	void UploadCreature(lobby_api::CreatureUpload, LobbyCallback<lobby_api::CreatureUploadReply>) override {}
	void SendSkirmishPing(LobbyCallback<LobbyDone>) override {}
	uint32_t AddEventListener(LobbyEventListener listener) override
	{
		_listeners[++_nextListener] = std::move(listener);
		return _nextListener;
	}
	void RemoveEventListener(uint32_t id) override { _listeners.erase(id); }

	/// The events wait for the next frame, as they come on the push channel
	void Poll()
	{
		auto pending = std::move(_pending);
		_pending.clear();
		for (const auto& event : pending)
		{
			for (auto& [id, listener] : _listeners)
			{
				listener(event);
			}
		}
	}

private:
	static std::string ClanName(int64_t clanId) { return clanId == 4 ? "The Clan" : "Other Clan"; }

	lobby_api::PushEvent Line(int64_t id, const std::string& name, const std::string& text,
	                          const std::string& channel = "server-w1")
	{
		// The text goes through the JSON writer, which escapes it: {"text":"..."} loses its first brace here
		const auto textMember = lobby_api::ToJson(lobby_api::ChatLineRequest {.text = text}).substr(1);
		const std::string data = R"({"channel":")" + channel + R"(","from":{"id":)" + std::to_string(id) + R"(,"name":")" +
		                         name + R"("},)" + textMember;
		return {.seq = ++_seq, .type = std::string(lobby_api::events::k_ChatLine), .time = {}, .data = data};
	}

	std::optional<lobby_api::AccountRef> _account;
	std::vector<lobby_api::ServerEntry> _servers = DemoServers();
	std::map<uint32_t, LobbyEventListener> _listeners;
	uint32_t _nextListener {0};
	std::vector<lobby_api::PushEvent> _pending;
	int64_t _seq {0};
};

/// A game socket with no network: each connection opens at the next frame, and the server lets each join in at the
/// frame after it. The clock of the demonstration moves on by the same time at each frame, and the server answers each
/// ping at the next frame. Thus each ping has the same round trip.
class DemoSockets final: public online::WebSocketClientInterface
{
public:
	/// The time that the clock of the demonstration moves on at each frame, in microseconds
	static constexpr int64_t k_FrameMicroseconds = 48'000;

	[[nodiscard]] int64_t Now() const noexcept { return _now; }
	/// The start that the server gives when it lets a player in: its time limit and its winning conditions
	void SetJoinStart(std::optional<game_socket::Start> start) { _joinStart = std::move(start); }

	online::WebSocketId Connect(online::WebSocketRequest, online::WebSocketHandlers handlers) override
	{
		const auto id = ++_nextId;
		_sockets[id] = {.handlers = std::move(handlers), .open = false};
		_opening.push_back(id);
		return id;
	}
	bool SendText(online::WebSocketId id, std::string_view text) override
	{
		if (!IsOpen(id))
		{
			return false;
		}
		const auto frame = game_socket::Parse(text);
		if (frame.has_value() && std::holds_alternative<game_socket::Join>(*frame))
		{
			_joins.emplace_back(id, std::get<game_socket::Join>(*frame).ticket == k_ObserverTicket);
		}
		return true;
	}
	bool SendBinary(online::WebSocketId id, std::span<const uint8_t> bytes) override
	{
		if (!IsOpen(id))
		{
			return false;
		}
		const auto decoded = turn_codec::Decode(bytes);
		if (decoded.frame.has_value())
		{
			if (const auto* ping = std::get_if<turn_codec::Ping>(&*decoded.frame))
			{
				_pongs.emplace_back(id, turn_codec::Encode(turn_codec::Pong {.id = ping->id, .time = ping->time}));
			}
		}
		return true;
	}
	void Close(online::WebSocketId id, uint16_t) override { _sockets.erase(id); }
	/// The server sends a frame to each open socket at the next frame
	void ServerSends(const game_socket::Frame& frame) { _toClients.push_back(game_socket::ToJson(frame)); }
	void Poll() override
	{
		_now += k_FrameMicroseconds;
		auto pongs = std::move(_pongs);
		_pongs.clear();
		for (const auto& [id, bytes] : pongs)
		{
			const auto found = _sockets.find(id);
			if (found != _sockets.end() && found->second.handlers.binary)
			{
				found->second.handlers.binary(bytes);
			}
		}
		auto toClients = std::move(_toClients);
		_toClients.clear();
		for (const auto& text : toClients)
		{
			for (auto& [id, socket] : _sockets)
			{
				if (socket.open && socket.handlers.text)
				{
					socket.handlers.text(text);
				}
			}
		}
		auto joins = std::move(_joins);
		_joins.clear();
		for (const auto& [id, observer] : joins)
		{
			const auto found = _sockets.find(id);
			if (found != _sockets.end() && found->second.handlers.text)
			{
				found->second.handlers.text(game_socket::ToJson(
				    game_socket::Joined {.role = observer ? game_socket::Role::Observer : game_socket::Role::God,
				                         .slot = observer ? std::nullopt : std::optional<uint8_t>(1),
				                         .session = "demo",
				                         .name = "Alice",
				                         .start = _joinStart}));
			}
		}
		auto opening = std::move(_opening);
		_opening.clear();
		for (const auto id : opening)
		{
			const auto found = _sockets.find(id);
			if (found == _sockets.end())
			{
				continue;
			}
			found->second.open = true;
			if (found->second.handlers.opened)
			{
				found->second.handlers.opened();
			}
		}
	}
	[[nodiscard]] online::WebSocketState State(online::WebSocketId id) const override
	{
		const auto found = _sockets.find(id);
		if (found == _sockets.end())
		{
			return online::WebSocketState::None;
		}
		return found->second.open ? online::WebSocketState::Open : online::WebSocketState::Connecting;
	}
	[[nodiscard]] size_t Count() const override { return _sockets.size(); }

private:
	struct Socket
	{
		online::WebSocketHandlers handlers;
		bool open {false};
	};
	[[nodiscard]] bool IsOpen(online::WebSocketId id) const
	{
		const auto found = _sockets.find(id);
		return found != _sockets.end() && found->second.open;
	}

	std::map<online::WebSocketId, Socket> _sockets;
	std::vector<online::WebSocketId> _opening;
	/// The joins to let in, and whether the ticket is an observer's
	std::vector<std::pair<online::WebSocketId, bool>> _joins;
	std::optional<game_socket::Start> _joinStart;
	std::vector<std::string> _toClients;
	/// The answers to the pings, for the next frame
	std::vector<std::pair<online::WebSocketId, std::vector<uint8_t>>> _pongs;
	online::WebSocketId _nextId {0};
	int64_t _now {0};
};

/// The network game of the demonstration: a session on the demonstration's sockets. Its server never starts a game.
class DemoNetworkGame final: public ecs::systems::NetworkGameSystemInterface
{
public:
	explicit DemoNetworkGame(DemoSockets& sockets)
	    : _session(sockets, [&sockets] { return sockets.Now(); })
	{
	}

	bool Join(ecs::systems::NetworkJoin join) override
	{
		_session.Join(online::NetworkSessionSettings {
		    .request = {.url = std::move(join.url), .headers = {}, .subprotocols = {}},
		    .ticket = std::move(join.ticket),
		    .gamePassword = std::move(join.gamePassword),
		    .name = std::move(join.name),
		    .readLand = {},
		    .readCreature = {},
		    .readAlignment = {},
		    .parts = {},
		    .origin = join.origin,
		});
		return true;
	}
	void SetReady(bool ready) override { _session.SetReady(ready); }
	void Leave() override { _session.Leave(); }
	void Update() override { _session.Update(); }
	[[nodiscard]] std::optional<game_socket::Start> TakeStart() override { return std::nullopt; }
	void BeginPlay() override {}
	void PlayAlone() override {}
	[[nodiscard]] bool InGame() const override { return _session.GetState() != online::NetworkSession::State::Ended; }
	[[nodiscard]] const online::NetworkSession* Session() const override { return &_session; }
	[[nodiscard]] std::optional<online::StartEvent> TakeStartEvent() override { return _session.TakeStartEvent(); }

private:
	online::NetworkSession _session;
};

/// The servers on the demonstration's local network
class DemoLan final: public online::LanDiscoveryInterface
{
public:
	[[nodiscard]] bool IsAvailable() const override { return true; }
	void Start() override
	{
		_servers = {Server("l1", "Office game", "mpm_2p_1", 4), Server("l2", "Upstairs", "mpm_3p_1", 32)};
		_servers[0].players = {Player(21, "Ian", "god")};
		_servers[0].numplayers = 1;
	}
	void Stop() override { _servers.clear(); }
	void Poll() override {}
	[[nodiscard]] std::span<const lobby_api::ServerEntry> GetServers() const override { return _servers; }

private:
	std::vector<lobby_api::ServerEntry> _servers;
};

struct DemoServices
{
	DemoLobby lobby;
	DemoSockets sockets;
	DemoNetworkGame network {sockets};
	DemoLan lan;
	Stage stage {Stage::ChooseConnection};
	/// The steps after the join wait for the game socket to open
	bool staged {false};
};

/// The steps that go to a stage, from the first box
void Begin(MultiplayerFrontEnd& frontEnd, DemoServices& demo)
{
	const auto stage = demo.stage;
	if (stage == Stage::ChooseConnection)
	{
		demo.staged = true;
		return;
	}
	frontEnd.Next();
	const bool lan = stage == Stage::LanGameList;
	frontEnd.SetLobbyKind(lan ? LobbyKind::Lan : LobbyKind::Internet);
	if (stage == Stage::FindGames)
	{
		demo.staged = true;
		return;
	}
	frontEnd.Next();
	if (stage == Stage::Login || stage == Stage::LanGameList)
	{
		demo.staged = true;
		return;
	}
	if (stage == Stage::NotRegistered)
	{
		frontEnd.Login(u"Zed", u"secret");
		demo.staged = true;
		return;
	}
	frontEnd.Login(u"Alice", u"secret");
	if (stage == Stage::GameList)
	{
		frontEnd.SelectGame(0);
		demo.staged = true;
		return;
	}
	if (stage == Stage::RoomChat32 || stage == Stage::FullRoomObserver)
	{
		const auto games = frontEnd.GetGames();
		const auto room =
		    std::ranges::find(games, std::string(stage == Stage::RoomChat32 ? "w5" : "w6"), &lobby_api::ServerEntry::id);
		frontEnd.SelectGame(static_cast<size_t>(std::distance(games.begin(), room)));
	}
	else
	{
		frontEnd.SelectGame(0);
	}
	frontEnd.Join(u"");
}

/// Once the room is open: the tab or the page of the stage
void Finish(MultiplayerFrontEnd& frontEnd, DemoServices& demo)
{
	demo.staged = true;
	switch (demo.stage)
	{
	case Stage::RoomLandscape:
		frontEnd.SelectTab(RoomTab::Landscape);
		break;
	case Stage::RoomClans:
		frontEnd.SelectTab(RoomTab::Clans);
		frontEnd.ShowClan(0);
		break;
	case Stage::RoomPlayerDetails:
		frontEnd.SelectTab(RoomTab::PlayerDetails);
		frontEnd.ShowSeat(0);
		break;
	case Stage::Conditions:
	case Stage::ConditionsPicked:
		frontEnd.Next();
		break;
	case Stage::WaitingForStart:
		frontEnd.Next();
		frontEnd.SetReady(true);
		break;
	case Stage::StartTransfer:
	case Stage::StartCancelled:
	case Stage::KickedFromGame:
	{
		// The demonstration's server makes the steps of the start that the stage shows
		frontEnd.Next();
		frontEnd.SetReady(true);
		auto& sockets = demo.sockets;
		sockets.ServerSends(game_socket::StartQuery {.land = {}, .testbed = true, .checksum = 0});
		if (demo.stage == Stage::StartTransfer)
		{
			sockets.ServerSends(game_socket::StartTransfer {});
		}
		else if (demo.stage == Stage::StartCancelled)
		{
			sockets.ServerSends(game_socket::StartCancel {});
		}
		else
		{
			sockets.ServerSends(game_socket::StartCancel {});
			sockets.ServerSends(game_socket::Refused {.reason = "kicked"});
		}
		break;
	}
	default:
		break;
	}
}

} // namespace

std::unique_ptr<MultiplayerFrontEnd> testbed_multiplayer::MakeDemoFrontEnd(Stage stage, std::u16string profileName,
                                                                           std::string localMaps, std::string conditionTemplate)
{
	auto demo = std::make_shared<DemoServices>();
	demo->stage = stage;
	if (stage == Stage::ConditionsPicked)
	{
		// The server picked three winning conditions in the limits of the land, and a time limit of an hour and a half
		demo->sockets.SetJoinStart(game_socket::Start {
		    .land = {},
		    .testbed = true,
		    .seed = 0,
		    .turn = 0,
		    .opening = {},
		    .godsRingMap = false,
		    .allComputerGods = false,
		    .aiSeed = std::nullopt,
		    .scenario = std::nullopt,
		    .timeLimit = k_DemoTimeLimitMinutes,
		    .conditions = {{.condition = 3, .value = 20}, {.condition = 5, .value = 9000}, {.condition = 14, .value = 100}},
		    .names = {},
		    .gods = {},
		});
	}
	bool begun = false;
	MultiplayerFrontEnd::Services services {
	    .lobby = &demo->lobby,
	    .network = &demo->network,
	    .lan = &demo->lan,
	    .openPage = [](std::string_view) {},
	    .owner = demo,
	    .poll =
	        [state = demo.get(), begun](MultiplayerFrontEnd& frontEnd) mutable {
		        state->network.Update();
		        state->sockets.Poll();
		        state->lobby.Poll();
		        if (!begun)
		        {
			        begun = true;
			        Begin(frontEnd, *state);
		        }
		        else if (!state->staged && frontEnd.IsJoined())
		        {
			        Finish(frontEnd, *state);
		        }
	        },
	};
	MultiplayerFrontEnd::Settings settings {
	    .registerPage = {},
	    .accountPage = {},
	    .internetName = u"demo.invalid",
	    .profileName = std::move(profileName),
	    .gameVersion = std::string(build_version::Text()),
	    .browser = false,
	    .localMaps = std::move(localMaps),
	    .conditionTemplate = std::move(conditionTemplate),
	    .showPing = stage == Stage::RoomPing,
	};
	return std::make_unique<MultiplayerFrontEnd>(std::move(services), std::move(settings));
}
