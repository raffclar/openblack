/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <utility>

#include <gtest/gtest.h>

#include "FakeWebSocketClient.h"
#include "Online/JoinRetryRule.h"
#include "Online/NetworkSession.h"

using namespace openblack;
using online::NetworkSession;
using online::WebSocketClosed;
using online::WebSocketEnd;

namespace
{

constexpr int64_t k_Second = 1'000'000;

WebSocketClosed Refused(int32_t status)
{
	return {.end = WebSocketEnd::HandshakeRefused, .code = 0, .httpStatus = status};
}

/// A client that joins a server behind a proxy, with a clock that the test moves
struct Client
{
	explicit Client(online::GameOrigin origin = online::GameOrigin::InternetLobby)
	    : session(sockets, [this] { return now; })
	{
		online::NetworkSessionSettings settings {
		    .request = {.url = "wss://localhost/game/s1/x", .headers = {}, .subprotocols = {}, .serverKey = std::nullopt},
		    .ticket = "ticket",
		};
		settings.origin = origin;
		session.Join(std::move(settings));
	}

	void Step(int64_t microseconds)
	{
		now += microseconds;
		sockets.Poll();
		session.Update();
		sockets.Poll();
	}

	int64_t now {0};
	test_fakes::FakeWebSocketClient sockets;
	NetworkSession session;
};

} // namespace

TEST(JoinRetry, OnlyAProxyWithNoServerOrNoConnectionGivesAnotherTry)
{
	using online::join_retry::ServerNotListening;
	EXPECT_TRUE(ServerNotListening(Refused(502)));
	EXPECT_TRUE(ServerNotListening(Refused(503)));
	EXPECT_TRUE(ServerNotListening(Refused(504)));
	// The browser cannot read the status
	EXPECT_TRUE(ServerNotListening(Refused(0)));
	EXPECT_TRUE(ServerNotListening({.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0}));
	EXPECT_TRUE(ServerNotListening({.end = WebSocketEnd::Timeout, .code = 0, .httpStatus = 0}));
	// A refusal of the server itself, or a bad certificate, stays an end
	EXPECT_FALSE(ServerNotListening(Refused(401)));
	EXPECT_FALSE(ServerNotListening(Refused(403)));
	EXPECT_FALSE(ServerNotListening({.end = WebSocketEnd::Tls, .code = 0, .httpStatus = 0}));
	EXPECT_FALSE(ServerNotListening({.end = WebSocketEnd::ClosedByServer, .code = 4000, .httpStatus = 0}));
}

TEST(JoinRetry, TheClientTriesAgainUntilTheServerListens)
{
	Client client;
	const auto first = client.sockets.Last();
	client.sockets.End(first, Refused(502));
	client.Step(0);
	EXPECT_EQ(client.session.GetState(), NetworkSession::State::Connecting);
	EXPECT_EQ(client.session.GetEnd(), NetworkSession::End::None);
	// The client waits before the next try
	client.Step(1000);
	client.Step(10 * k_Second);
	const auto second = client.sockets.Last();
	ASSERT_NE(second, first);
	// The server now listens: the client sends its join on the new socket
	client.sockets.Accept(second);
	client.Step(0);
	EXPECT_EQ(client.session.GetState(), NetworkSession::State::Joining);
	EXPECT_FALSE(client.sockets.TakeSent(second).empty());
}

TEST(JoinRetry, TheClientStopsAtTheEndOfTheTimeToTryAgain)
{
	Client client;
	for (int i = 0; i < 100 && client.session.GetState() == NetworkSession::State::Connecting; ++i)
	{
		client.sockets.End(client.sockets.Last(), Refused(502));
		client.Step(0);
		client.Step(5 * k_Second);
	}
	EXPECT_EQ(client.session.GetState(), NetworkSession::State::Ended);
	EXPECT_EQ(client.session.GetEnd(), NetworkSession::End::NoConnection);
	EXPECT_GE(client.now, 60 * k_Second);
}

TEST(JoinRetry, ARefusalOfTheServerEndsTheJoinAtOnce)
{
	Client client;
	client.sockets.End(client.sockets.Last(), Refused(403));
	client.Step(0);
	EXPECT_EQ(client.session.GetState(), NetworkSession::State::Ended);
	EXPECT_EQ(client.session.GetEnd(), NetworkSession::End::NoConnection);
	EXPECT_EQ(client.sockets.order.size(), 1U);
}

TEST(JoinRetry, AGameOnTheLocalNetworkDoesNotTryAgain)
{
	Client client(online::GameOrigin::Lan);
	client.sockets.End(client.sockets.Last(), Refused(502));
	client.Step(0);
	EXPECT_EQ(client.session.GetState(), NetworkSession::State::Ended);
	EXPECT_EQ(client.session.GetEnd(), NetworkSession::End::NoConnection);
}
