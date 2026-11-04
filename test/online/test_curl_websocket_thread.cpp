/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The desktop WSS client does the work on its sockets in a thread of its own. These tests use a game server on the
// loopback address with its own certificate. They make sure that the messages keep their order, and that the handlers
// run only from Poll and never after a close.

#include <cstdint>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Online/Desktop/CurlWebSocketClient.h"
#include "Server/Desktop/SocketWebSocketServer.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{

/// The number of text messages that each test sends in one frame
constexpr size_t k_Burst = 200;
/// The size of the large binary message
constexpr size_t k_LargeBytes = 1024 * 1024;
/// The close code of the server in the test
constexpr uint16_t k_ServerCode = 4000;

/// A game server that sends back each message of a client
class EchoServer
{
public:
	EchoServer()
	{
		// The server writes its log lines to the game's log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		sockets.SetHandlers(
		    {.opened = [this](server::ConnectionId connection) { client = connection; },
		     .text = [this](server::ConnectionId connection, std::string_view text) { sockets.SendText(connection, text); },
		     .binary = [this](server::ConnectionId connection,
		                      std::span<const uint8_t> bytes) { sockets.SendBinary(connection, bytes); },
		     .closed = [this](server::ConnectionId, uint16_t code) { closedCode = code; }});
		const auto error = sockets.Listen(
		    {.address = "127.0.0.1", .port = 0, .certificate = std::nullopt, .key = std::nullopt, .ownCertificate = true});
		EXPECT_FALSE(error.has_value());
	}

	[[nodiscard]] online::WebSocketRequest Request() const
	{
		return {.url = "wss://127.0.0.1:" + std::to_string(sockets.Port()) + "/game",
		        .headers = {},
		        .subprotocols = {},
		        .serverKey = sockets.OwnKeyPin()};
	}

	server::SocketWebSocketServer sockets;
	std::optional<server::ConnectionId> client;
	std::optional<uint16_t> closedCode;
};

/// What the handlers of one connection got
struct Seen
{
	bool opened {false};
	std::vector<std::string> texts;
	std::vector<std::vector<uint8_t>> binaries;
	std::optional<online::WebSocketClosed> closed;
	/// A handler ran out of Poll
	bool outOfPoll {false};
};

/// Makes handlers that record into the seen data. The flag tells if Poll runs now.
online::WebSocketHandlers Recorder(Seen& seen, const bool& inPoll)
{
	return {.opened =
	            [&seen, &inPoll]() {
		            seen.outOfPoll = seen.outOfPoll || !inPoll;
		            seen.opened = true;
	            },
	        .text =
	            [&seen, &inPoll](std::string_view text) {
		            seen.outOfPoll = seen.outOfPoll || !inPoll;
		            seen.texts.emplace_back(text);
	            },
	        .binary =
	            [&seen, &inPoll](std::span<const uint8_t> bytes) {
		            seen.outOfPoll = seen.outOfPoll || !inPoll;
		            seen.binaries.emplace_back(bytes.begin(), bytes.end());
	            },
	        .closed =
	            [&seen, &inPoll](const online::WebSocketClosed& closed) {
		            seen.outOfPoll = seen.outOfPoll || !inPoll;
		            seen.closed = closed;
	            }};
}

/// Moves the server and the client on until the condition is true or the time is over
bool PollUntil(EchoServer& server, online::CurlWebSocketClient& client, bool& inPoll, const std::function<bool()>& condition)
{
	const auto end = std::chrono::steady_clock::now() + 10s;
	while (std::chrono::steady_clock::now() < end)
	{
		server.sockets.Poll();
		inPoll = true;
		client.Poll();
		inPoll = false;
		if (condition())
		{
			return true;
		}
		std::this_thread::sleep_for(1ms);
	}
	return false;
}

} // namespace

TEST(CurlWebSocketThread, TheMessagesComeBackInTheirOrderAndOnlyFromPoll)
{
	EchoServer server;
	online::CurlWebSocketClient client;
	Seen seen;
	bool inPoll = false;
	const auto id = client.Connect(server.Request(), Recorder(seen, inPoll));
	EXPECT_EQ(client.State(id), online::WebSocketState::Connecting);
	ASSERT_TRUE(PollUntil(server, client, inPoll, [&seen]() { return seen.opened || seen.closed.has_value(); }));
	ASSERT_TRUE(seen.opened);
	EXPECT_EQ(client.State(id), online::WebSocketState::Open);
	EXPECT_EQ(client.Count(), 1u);

	std::vector<uint8_t> large(k_LargeBytes);
	for (size_t index = 0; index < large.size(); ++index)
	{
		large.at(index) = static_cast<uint8_t>(index * 7);
	}
	for (size_t index = 0; index < k_Burst; ++index)
	{
		ASSERT_TRUE(client.SendText(id, std::to_string(index)));
		if (index == k_Burst / 2)
		{
			ASSERT_TRUE(client.SendBinary(id, large));
		}
	}
	ASSERT_TRUE(
	    PollUntil(server, client, inPoll, [&seen]() { return seen.texts.size() == k_Burst || seen.closed.has_value(); }));
	ASSERT_EQ(seen.texts.size(), k_Burst);
	for (size_t index = 0; index < k_Burst; ++index)
	{
		EXPECT_EQ(seen.texts.at(index), std::to_string(index));
	}
	ASSERT_EQ(seen.binaries.size(), 1u);
	EXPECT_EQ(seen.binaries.front(), large);
	EXPECT_FALSE(seen.outOfPoll);
	EXPECT_FALSE(seen.closed.has_value());
}

TEST(CurlWebSocketThread, TheCloseOfTheServerComesAfterItsLastMessage)
{
	EchoServer server;
	online::CurlWebSocketClient client;
	Seen seen;
	bool inPoll = false;
	const auto id = client.Connect(server.Request(), Recorder(seen, inPoll));
	ASSERT_TRUE(PollUntil(server, client, inPoll, [&seen, &server]() { return seen.opened && server.client.has_value(); }));

	ASSERT_TRUE(server.sockets.SendText(*server.client, "last"));
	server.sockets.Close(*server.client, k_ServerCode);
	ASSERT_TRUE(PollUntil(server, client, inPoll, [&seen]() { return seen.closed.has_value(); }));
	ASSERT_EQ(seen.texts.size(), 1u);
	EXPECT_EQ(seen.texts.front(), "last");
	EXPECT_EQ(seen.closed->end, online::WebSocketEnd::ClosedByServer);
	EXPECT_EQ(seen.closed->code, k_ServerCode);
	EXPECT_EQ(client.State(id), online::WebSocketState::None);
	EXPECT_EQ(client.Count(), 0u);
	EXPECT_FALSE(client.SendText(id, "after"));
	EXPECT_FALSE(seen.outOfPoll);
}

TEST(CurlWebSocketThread, NoHandlerRunsAfterTheGameClosesAndTheServerGetsTheCode)
{
	EchoServer server;
	online::CurlWebSocketClient client;
	Seen seen;
	bool inPoll = false;
	const auto id = client.Connect(server.Request(), Recorder(seen, inPoll));
	ASSERT_TRUE(PollUntil(server, client, inPoll, [&seen, &server]() { return seen.opened && server.client.has_value(); }));

	for (size_t index = 0; index < k_Burst; ++index)
	{
		ASSERT_TRUE(client.SendText(id, std::to_string(index)));
	}
	client.Close(id, online::websocket_close::k_Normal);
	EXPECT_EQ(client.State(id), online::WebSocketState::None);
	EXPECT_EQ(client.Count(), 0u);
	const auto before = seen.texts.size();
	ASSERT_TRUE(PollUntil(server, client, inPoll, [&server]() { return server.closedCode.has_value(); }));
	EXPECT_EQ(*server.closedCode, online::websocket_close::k_Normal);
	EXPECT_EQ(seen.texts.size(), before);
	EXPECT_FALSE(seen.closed.has_value());
}

TEST(CurlWebSocketThread, TheServerGetsGoingAwayWhenTheClientStops)
{
	EchoServer server;
	auto client = std::make_unique<online::CurlWebSocketClient>();
	Seen seen;
	bool inPoll = false;
	client->Connect(server.Request(), Recorder(seen, inPoll));
	ASSERT_TRUE(PollUntil(server, *client, inPoll, [&seen, &server]() { return seen.opened && server.client.has_value(); }));

	client.reset();
	const auto end = std::chrono::steady_clock::now() + 10s;
	while (!server.closedCode.has_value() && std::chrono::steady_clock::now() < end)
	{
		server.sockets.Poll();
		std::this_thread::sleep_for(1ms);
	}
	ASSERT_TRUE(server.closedCode.has_value());
	EXPECT_EQ(*server.closedCode, online::websocket_close::k_GoingAway);
}

TEST(CurlWebSocketThread, AnAddressThatIsNotWssMakesNoThreadAndEndsInPoll)
{
	online::CurlWebSocketClient client;
	Seen seen;
	bool inPoll = false;
	const auto id = client.Connect({.url = "ws://127.0.0.1/game", .headers = {}, .subprotocols = {}, .serverKey = {}},
	                               Recorder(seen, inPoll));
	EXPECT_EQ(client.State(id), online::WebSocketState::None);
	EXPECT_FALSE(seen.closed.has_value());
	inPoll = true;
	client.Poll();
	inPoll = false;
	ASSERT_TRUE(seen.closed.has_value());
	EXPECT_EQ(seen.closed->end, online::WebSocketEnd::AddressRefused);
	EXPECT_FALSE(seen.outOfPoll);
}
