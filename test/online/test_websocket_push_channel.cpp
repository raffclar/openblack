/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "FakeWebSocketClient.h"
#include "Online/Backoff.h"
#include "Online/HttpClientInterface.h"
#include "Online/TokenStoreInterface.h"
#include "Online/WebSocketPushChannel.h"

// Enable this define because the test makes the system itself
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/LobbyClientSystem.h"

using namespace openblack;
using namespace std::chrono_literals;
using online::PushAuth;
using online::PushClose;
using online::WebSocketClosed;
using online::WebSocketEnd;
using test_fakes::FakeWebSocketClient;

namespace
{

/// A web client that keeps each request until the test answers it
class FakeHttp final: public online::HttpClientInterface
{
public:
	online::HttpRequestId Send(online::HttpRequest request, online::HttpCallback callback) override
	{
		sent.emplace_back(std::move(request), std::move(callback));
		return static_cast<online::HttpRequestId>(sent.size());
	}
	void Cancel(online::HttpRequestId /*id*/) override {}
	void Poll() override {}
	[[nodiscard]] size_t Pending() const override { return sent.size(); }

	void Answer(int32_t status, std::string_view body)
	{
		ASSERT_FALSE(sent.empty());
		auto next = std::move(sent.front());
		sent.pop_front();
		next.second(online::HttpResponse {
		    .error = online::HttpError::None,
		    .status = status,
		    .headers = {},
		    .body = {body.begin(), body.end()},
		});
	}

	std::deque<std::pair<online::HttpRequest, online::HttpCallback>> sent;
};

class NoTokens final: public online::TokenStoreInterface
{
public:
	[[nodiscard]] std::optional<std::string> Load() override { return std::nullopt; }
	bool Save(std::string_view /*token*/) override { return true; }
	void Clear() override {}
};

/// The access token of the login lasts until 10:15, and the test starts at 10:00
constexpr std::string_view k_Session =
    R"({"token_type":"Bearer","access_token":"access1","access_token_expires":"2026-10-22T10:15:00.000Z",)"
    R"("refresh_token":"refresh1","refresh_token_expires":"2026-11-21T10:00:00.000Z","account":{"id":7,"name":"abc"}})";
constexpr std::string_view k_Hello = R"({"type":"hello","data":{"epoch":"e1","seq":0,"account":{"id":7,"name":"abc"}}})";
constexpr std::string_view k_HelloAgain = R"({"type":"hello","data":{"epoch":"e1","seq":9,"account":{"id":7,"name":"abc"}}})";

[[nodiscard]] bool Has(const std::string& text, std::string_view part)
{
	return text.find(part) != std::string::npos;
}

/// The lobby client with the real push channel on the fake WSS client
class LobbyPushTest: public ::testing::Test
{
protected:
	void Make(bool configured = true, PushAuth auth = PushAuth::Header)
	{
		std::optional<ecs::systems::LobbyClientConfig> config;
		if (configured)
		{
			config = ecs::systems::LobbyClientConfig {
			    .origin = auth == PushAuth::Header ? "https://bw.example" : "",
			    .client = auth == PushAuth::Header ? lobby_api::ClientKind::Desktop : lobby_api::ClientKind::Browser,
			};
		}
		_client = std::make_unique<ecs::systems::LobbyClientSystem>(
		    config, _http, std::make_unique<online::WebSocketPushChannel>(_sockets, auth), std::make_unique<NoTokens>(),
		    [this]() { return _now; });
	}

	/// One frame of the game: the WSS client moves on, then the lobby client
	void Frame()
	{
		_sockets.Poll();
		_client->Update();
	}

	void LogIn()
	{
		_client->Login("abc", "secret", [](auto&& /*result*/) {});
		_http.Answer(201, k_Session);
	}

	FakeHttp _http;
	FakeWebSocketClient _sockets;
	/// 2026-10-22T10:00:00Z
	std::chrono::system_clock::time_point _now {std::chrono::seconds {1792663200}};
	std::unique_ptr<ecs::systems::LobbyClientSystem> _client;
};

} // namespace

TEST(ReconnectBackoff, TheWaitDoublesToTheMostAndAnOpenResetsIt)
{
	online::ReconnectBackoff backoff {1s, 60s};
	std::vector<std::chrono::milliseconds> waits;
	for (int i = 0; i < 8; ++i)
	{
		waits.push_back(backoff.Next());
	}
	EXPECT_EQ(waits, (std::vector<std::chrono::milliseconds> {1s, 2s, 4s, 8s, 16s, 32s, 60s, 60s}));
	backoff.Reset();
	EXPECT_EQ(backoff.Current(), 1s);
}

TEST(PushChannelHandshake, TheDesktopSendsTheTokenInTheHeader)
{
	const auto request = online::push_channel::Handshake("wss://bw.example/api/v1/events", "tok", PushAuth::Header);
	EXPECT_EQ(request.url, "wss://bw.example/api/v1/events");
	ASSERT_EQ(request.headers.size(), 1u);
	EXPECT_EQ(request.headers[0].first, "Authorization");
	EXPECT_EQ(request.headers[0].second, "Bearer tok");
	EXPECT_TRUE(request.subprotocols.empty());
}

TEST(PushChannelHandshake, TheBrowserSendsTheTokenInASubprotocol)
{
	const auto request = online::push_channel::Handshake("/api/v1/events", "tok", PushAuth::Subprotocol);
	EXPECT_TRUE(request.headers.empty());
	EXPECT_EQ(request.subprotocols, (std::vector<std::string> {"bwgame.v1", "bearer.tok"}));
}

TEST(PushChannelHandshake, TheCloseCodesOfTheLobbyEndTheSession)
{
	using online::push_channel::CloseReason;
	EXPECT_EQ(CloseReason({.end = WebSocketEnd::ClosedByServer, .code = 4001, .httpStatus = 0}), PushClose::SignedOut);
	EXPECT_EQ(CloseReason({.end = WebSocketEnd::ClosedByServer, .code = 4003, .httpStatus = 0}), PushClose::Banned);
	// Too many frames, text frames only, and a restart of the lobby: the client connects again later
	EXPECT_EQ(CloseReason({.end = WebSocketEnd::ClosedByServer, .code = 4008, .httpStatus = 0}), PushClose::Lost);
	EXPECT_EQ(CloseReason({.end = WebSocketEnd::ClosedByServer, .code = 1003, .httpStatus = 0}), PushClose::Lost);
	EXPECT_EQ(CloseReason({.end = WebSocketEnd::ClosedByServer, .code = 1001, .httpStatus = 0}), PushClose::Lost);
	EXPECT_EQ(CloseReason({.end = WebSocketEnd::HandshakeRefused, .code = 0, .httpStatus = 401}), PushClose::Refused);
	EXPECT_EQ(CloseReason({.end = WebSocketEnd::HandshakeRefused, .code = 0, .httpStatus = 403}), PushClose::Lost);
	EXPECT_EQ(CloseReason({.end = WebSocketEnd::HandshakeRefused, .code = 0, .httpStatus = 429}), PushClose::Lost);
	EXPECT_EQ(CloseReason({.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0}), PushClose::Lost);
	EXPECT_EQ(CloseReason({.end = WebSocketEnd::Tls, .code = 0, .httpStatus = 0}), PushClose::Lost);
	EXPECT_EQ(CloseReason({.end = WebSocketEnd::Timeout, .code = 0, .httpStatus = 0}), PushClose::Lost);
	EXPECT_EQ(CloseReason({.end = WebSocketEnd::TooLarge, .code = 0, .httpStatus = 0}), PushClose::Lost);
	EXPECT_EQ(CloseReason({.end = WebSocketEnd::AddressRefused, .code = 0, .httpStatus = 0}), PushClose::Unavailable);
}

TEST(WebSocketPushChannel, TheChannelGivesTheTextFramesAndTheReasonOfTheEnd)
{
	FakeWebSocketClient sockets;
	online::WebSocketPushChannel channel(sockets, PushAuth::Header);
	bool opened = false;
	std::vector<std::string> messages;
	std::optional<PushClose> closed;
	channel.Open("wss://bw.example/api/v1/events", "tok",
	             online::PushHandlers {
	                 .opened = [&]() { opened = true; },
	                 .message = [&](std::string_view text) { messages.emplace_back(text); },
	                 .closed = [&](PushClose reason) { closed = reason; },
	             });
	const auto id = sockets.Last();
	EXPECT_FALSE(channel.IsOpen());
	sockets.Accept(id);
	sockets.ServerText(id, k_Hello);
	sockets.Poll();
	EXPECT_TRUE(opened);
	EXPECT_TRUE(channel.IsOpen());
	ASSERT_EQ(messages.size(), 1u);
	EXPECT_EQ(messages[0], k_Hello);

	channel.Send(R"({"type":"subscribe","channel":"lobby"})");
	const auto sent = sockets.TakeSent(id);
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_TRUE(sent[0].text);

	sockets.End(id, {.end = WebSocketEnd::ClosedByServer, .code = 4003, .httpStatus = 0});
	sockets.Poll();
	EXPECT_EQ(closed, PushClose::Banned);
	EXPECT_FALSE(channel.IsOpen());
}

TEST(WebSocketPushChannel, AChannelThatTheGameClosesRunsNoMoreHandlers)
{
	FakeWebSocketClient sockets;
	bool called = false;
	{
		online::WebSocketPushChannel channel(sockets, PushAuth::Header);
		channel.Open("wss://bw.example/api/v1/events", "tok",
		             online::PushHandlers {
		                 .opened = [&]() { called = true; },
		                 .message = [&](std::string_view /*text*/) { called = true; },
		                 .closed = [&](PushClose /*reason*/) { called = true; },
		             });
		sockets.Accept(sockets.Last());
	}
	sockets.Poll();
	EXPECT_FALSE(called);
	EXPECT_EQ(sockets.sockets.begin()->second.closedWith, online::websocket_close::k_Normal);
	EXPECT_EQ(sockets.Count(), 0u);
}

TEST_F(LobbyPushTest, WithNoLobbyNoSocketOpens)
{
	Make(false);
	for (int i = 0; i < 10; ++i)
	{
		Frame();
	}
	EXPECT_TRUE(_sockets.order.empty());
	EXPECT_TRUE(_http.sent.empty());
}

TEST_F(LobbyPushTest, TheLoginOpensTheChannelWithTheAccessTokenAndSubscribesAfterHello)
{
	Make();
	_client->WatchChannel("lobby", true);
	LogIn();
	ASSERT_EQ(_sockets.order.size(), 1u);
	const auto id = _sockets.Last();
	const auto& request = _sockets.sockets[id].request;
	EXPECT_EQ(request.url, "wss://bw.example/api/v1/events");
	ASSERT_EQ(request.headers.size(), 1u);
	EXPECT_EQ(request.headers[0].second, "Bearer access1");

	_sockets.Accept(id);
	Frame();
	EXPECT_TRUE(_client->IsPushOpen());
	// The client waits for hello before it asks for its channels
	EXPECT_TRUE(_sockets.TakeSent(id).empty());
	_sockets.ServerText(id, k_Hello);
	Frame();
	const auto sent = _sockets.TakeSent(id);
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_TRUE(Has(sent[0].Text(), R"("type":"subscribe")"));
	EXPECT_TRUE(Has(sent[0].Text(), R"("channel":"lobby")"));
}

TEST_F(LobbyPushTest, TheBrowserOpensAPathOfTheOriginWithTheSubprotocols)
{
	Make(true, PushAuth::Subprotocol);
	LogIn();
	ASSERT_EQ(_sockets.order.size(), 1u);
	const auto& request = _sockets.sockets[_sockets.Last()].request;
	EXPECT_EQ(request.url, "/api/v1/events");
	EXPECT_TRUE(request.headers.empty());
	EXPECT_EQ(request.subprotocols, (std::vector<std::string> {"bwgame.v1", "bearer.access1"}));
}

TEST_F(LobbyPushTest, ALossConnectsAgainAfterTheBackoffAndResumesFromTheLastEvent)
{
	Make();
	_client->WatchChannel("lobby", true);
	LogIn();
	auto id = _sockets.Last();
	_sockets.Accept(id);
	_sockets.ServerText(id, k_Hello);
	_sockets.ServerText(id, R"({"seq":9,"type":"chat.line","time":"2026-10-22T10:00:01.000Z","data":{}})");
	Frame();
	_sockets.TakeSent(id);

	_sockets.End(id, {.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0});
	Frame();
	EXPECT_FALSE(_client->IsPushOpen());
	EXPECT_EQ(_sockets.order.size(), 1u);
	// The first wait is one second
	_now += 999ms;
	Frame();
	EXPECT_EQ(_sockets.order.size(), 1u);
	_now += 1ms;
	Frame();
	ASSERT_EQ(_sockets.order.size(), 2u);

	id = _sockets.Last();
	_sockets.Accept(id);
	_sockets.ServerText(id, k_HelloAgain);
	Frame();
	const auto sent = _sockets.TakeSent(id);
	ASSERT_EQ(sent.size(), 1u);
	const auto resume = sent[0].Text();
	EXPECT_TRUE(Has(resume, R"("type":"resume")"));
	EXPECT_TRUE(Has(resume, R"("epoch":"e1")"));
	EXPECT_TRUE(Has(resume, R"("seq":9)"));
	EXPECT_TRUE(Has(resume, R"("channels":["lobby"])"));

	// The lobby lost the events: the client continues from the number of the lobby. The listener also gets the resync
	// frame, which has no number.
	_sockets.ServerText(id, R"({"type":"resync","data":{"epoch":"e2","seq":40}})");
	std::vector<int64_t> seen;
	_client->AddEventListener([&](const lobby_api::PushEvent& event) { seen.push_back(event.seq.value_or(-1)); });
	_sockets.ServerText(id, R"({"seq":40,"type":"chat.line","time":"2026-10-22T10:00:02.000Z","data":{}})");
	_sockets.ServerText(id, R"({"seq":41,"type":"chat.line","time":"2026-10-22T10:00:03.000Z","data":{}})");
	Frame();
	EXPECT_EQ(seen, (std::vector<int64_t> {-1, 41}));
}

TEST_F(LobbyPushTest, EachLossDoublesTheWaitUntilAChannelOpens)
{
	Make();
	LogIn();
	std::vector<std::chrono::milliseconds> waits;
	for (int attempt = 0; attempt < 4; ++attempt)
	{
		const auto opens = _sockets.order.size();
		_sockets.End(_sockets.Last(), {.end = WebSocketEnd::Tls, .code = 0, .httpStatus = 0});
		Frame();
		std::chrono::milliseconds waited {0};
		while (_sockets.order.size() == opens)
		{
			_now += 100ms;
			waited += 100ms;
			Frame();
		}
		waits.push_back(waited);
	}
	EXPECT_EQ(waits, (std::vector<std::chrono::milliseconds> {1s, 2s, 4s, 8s}));
}

TEST_F(LobbyPushTest, CloseCode4001Or4003EndsTheSession)
{
	for (const uint16_t code : {uint16_t {4001}, uint16_t {4003}})
	{
		Make();
		LogIn();
		const auto id = _sockets.Last();
		_sockets.Accept(id);
		_sockets.ServerText(id, k_Hello);
		Frame();
		ASSERT_EQ(_client->GetState(), ecs::systems::LobbySessionState::LoggedIn);
		const auto opens = _sockets.order.size();
		_sockets.End(id, {.end = WebSocketEnd::ClosedByServer, .code = code, .httpStatus = 0});
		Frame();
		EXPECT_EQ(_client->GetState(), ecs::systems::LobbySessionState::LoggedOut) << code;
		_now += 120s;
		Frame();
		EXPECT_EQ(_sockets.order.size(), opens) << code;
	}
}

TEST_F(LobbyPushTest, ARefusedTokenGetsANewTokenBeforeTheNextAttempt)
{
	Make();
	LogIn();
	_sockets.End(_sockets.Last(), {.end = WebSocketEnd::HandshakeRefused, .code = 0, .httpStatus = 401});
	Frame();
	_now += 1s;
	Frame();
	// The client asks for a new access token first
	ASSERT_EQ(_http.sent.size(), 1u);
	EXPECT_TRUE(_http.sent.front().first.url.ends_with("/sessions/refresh"));
}
