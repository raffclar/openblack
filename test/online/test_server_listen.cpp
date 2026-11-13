/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The listener of the game server. A new server must listen at once on the port of a server that stopped, also when
// a connection of the old server stays open in the system. The room launcher of the game container starts a new server
// on the same port a few seconds after the old one stops.

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Online/Desktop/CurlWebSocketClient.h"
#include "Server/Desktop/SocketWebSocketServer.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{

/// The server writes its log lines to the game's log
void MakeLog()
{
	if (spdlog::get("game") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_st>("game");
	}
}

/// Moves the server and the client on until the condition is true or the time is over
bool PollUntil(server::SocketWebSocketServer& sockets, online::CurlWebSocketClient& client,
               const std::function<bool()>& condition)
{
	const auto end = std::chrono::steady_clock::now() + 10s;
	while (std::chrono::steady_clock::now() < end)
	{
		sockets.Poll();
		client.Poll();
		if (condition())
		{
			return true;
		}
		std::this_thread::sleep_for(2ms);
	}
	return false;
}

/// The client of the test speaks TLS, so the server makes a certificate of its own
server::ListenSettings OnPort(uint16_t port)
{
	return {.address = "127.0.0.1", .port = port, .certificate = std::nullopt, .key = std::nullopt, .ownCertificate = true};
}

} // namespace

TEST(ServerListen, ANewServerListensAtOnceOnThePortOfAServerThatStoppedWithAnOpenConnection)
{
	MakeLog();
	online::CurlWebSocketClient client;
	uint16_t port = 0;
	{
		server::SocketWebSocketServer old;
		bool accepted = false;
		old.SetHandlers(
		    {.opened = [&accepted](server::ConnectionId) { accepted = true; }, .text = {}, .binary = {}, .closed = {}});
		ASSERT_FALSE(old.Listen(OnPort(0)).has_value());
		port = old.Port();
		bool opened = false;
		client.Connect({.url = "wss://127.0.0.1:" + std::to_string(port) + "/game",
		                .headers = {},
		                .subprotocols = {},
		                .serverKey = old.OwnKeyPin()},
		               {.opened = [&opened]() { opened = true; }, .text = {}, .binary = {}, .closed = {}});
		ASSERT_TRUE(PollUntil(old, client, [&opened, &accepted]() { return opened && accepted; }));
		// The old server stops. The client does not read its socket, so the connection of the old server stays in the
		// system.
	}
	server::SocketWebSocketServer next;
	const auto error = next.Listen(OnPort(port));
	EXPECT_FALSE(error.has_value()) << error.value_or("");
	EXPECT_EQ(next.Port(), port);
}

TEST(ServerListen, ASecondServerCannotListenOnThePortOfARunningServerAndTheErrorGivesTheSystemCode)
{
	MakeLog();
	server::SocketWebSocketServer first;
	ASSERT_FALSE(first.Listen(OnPort(0)).has_value());
	server::SocketWebSocketServer second;
	const auto error = second.Listen(OnPort(first.Port()));
	ASSERT_TRUE(error.has_value());
	EXPECT_NE(error->find("system error"), std::string::npos) << *error;
}
