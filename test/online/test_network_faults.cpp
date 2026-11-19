/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Server/NetworkFaults.h"

using namespace openblack::server;

namespace
{

/// The real sockets of the server, as the tests see them: each send and close goes into a list, and the test calls the
/// handlers as a client does
class FakeServerSockets final: public WebSocketServerInterface
{
public:
	void SetHandlers(ServerSocketHandlers handlers) override { this->handlers = std::move(handlers); }
	bool SendText(ConnectionId connection, std::string_view text) override
	{
		sent.push_back(std::to_string(connection) + ":" + std::string(text));
		return true;
	}
	bool SendBinary(ConnectionId connection, std::span<const uint8_t> bytes) override
	{
		sent.push_back(std::to_string(connection) + ":binary" + std::to_string(bytes.size()));
		return true;
	}
	void Close(ConnectionId connection, uint16_t code) override
	{
		sent.push_back(std::to_string(connection) + ":close" + std::to_string(code));
	}
	void Poll() override { ++polls; }
	[[nodiscard]] size_t Count() const override { return 0; }

	ServerSocketHandlers handlers;
	std::vector<std::string> sent;
	int polls {0};
};

struct Received
{
	std::vector<std::string> lines;

	ServerSocketHandlers Handlers()
	{
		return {
		    .opened = [this](ConnectionId connection) { lines.push_back(std::to_string(connection) + ":opened"); },
		    .text = [this](ConnectionId connection,
		                   std::string_view text) { lines.push_back(std::to_string(connection) + ":" + std::string(text)); },
		    .binary = {},
		    .closed = [this](ConnectionId connection,
		                     uint16_t code) { lines.push_back(std::to_string(connection) + ":closed" + std::to_string(code)); },
		};
	}
};

} // namespace

TEST(NetworkFaults, WithNoFaultEachCallGoesToTheSocketsAtOnce)
{
	FakeServerSockets sockets;
	int64_t now = 0;
	FaultyWebSocketServer faulty(sockets, NetworkFaults {}, [&now] { return now; });
	Received received;
	faulty.SetHandlers(received.Handlers());
	sockets.handlers.opened(1);
	sockets.handlers.text(1, "hello");
	EXPECT_EQ(received.lines, (std::vector<std::string> {"1:opened", "1:hello"}));
	EXPECT_TRUE(faulty.SendText(1, "turn"));
	EXPECT_EQ(sockets.sent, std::vector<std::string> {"1:turn"});
}

TEST(NetworkFaults, ADelayHoldsEachMessageInEachDirectionAndKeepsTheOrder)
{
	FakeServerSockets sockets;
	int64_t now = 0;
	FaultyWebSocketServer faulty(sockets, NetworkFaults {.delayMilliseconds = 50}, [&now] { return now; });
	Received received;
	faulty.SetHandlers(received.Handlers());
	sockets.handlers.opened(1);
	sockets.handlers.text(1, "a");
	now = 10'000;
	sockets.handlers.text(1, "b");
	EXPECT_TRUE(faulty.SendText(1, "turn 1"));
	faulty.Poll();
	EXPECT_EQ(received.lines, std::vector<std::string> {"1:opened"});
	EXPECT_TRUE(sockets.sent.empty());
	now = 50'000;
	faulty.Poll();
	EXPECT_EQ(received.lines, (std::vector<std::string> {"1:opened", "1:a"}));
	now = 60'000;
	faulty.Poll();
	EXPECT_EQ(received.lines, (std::vector<std::string> {"1:opened", "1:a", "1:b"}));
	EXPECT_EQ(sockets.sent, std::vector<std::string> {"1:turn 1"});
}

TEST(NetworkFaults, ALostPacketComesAfterTheTimeToSendAgainAndTheNextMessagesWait)
{
	EXPECT_EQ(ResendMicroseconds(0), 0);
	EXPECT_EQ(ResendMicroseconds(1), 200'000);
	EXPECT_EQ(ResendMicroseconds(2), 600'000);
	EXPECT_EQ(ResendMicroseconds(k_MostLossesInARow + 3), ResendMicroseconds(k_MostLossesInARow));

	FakeServerSockets sockets;
	int64_t now = 0;
	// Each packet is lost the most times in a row
	FaultyWebSocketServer faulty(sockets, NetworkFaults {.lossPercent = 100}, [&now] { return now; });
	faulty.SetHandlers(Received {}.Handlers());
	sockets.handlers.opened(1);
	EXPECT_TRUE(faulty.SendText(1, "turn 1"));
	now = 1'000;
	EXPECT_TRUE(faulty.SendText(1, "turn 2"));
	EXPECT_EQ(faulty.Losses(), 2u * k_MostLossesInARow);
	now = ResendMicroseconds(k_MostLossesInARow) - 1;
	faulty.Poll();
	EXPECT_TRUE(sockets.sent.empty());
	now = ResendMicroseconds(k_MostLossesInARow);
	faulty.Poll();
	EXPECT_EQ(sockets.sent, std::vector<std::string> {"1:turn 1"});
	now += 1'000;
	faulty.Poll();
	EXPECT_EQ(sockets.sent, (std::vector<std::string> {"1:turn 1", "1:turn 2"}));
}

TEST(NetworkFaults, SomeLossesMakeSomePacketsLateAndKeepTheOrder)
{
	FakeServerSockets sockets;
	int64_t now = 0;
	FaultyWebSocketServer faulty(sockets, NetworkFaults {.lossPercent = 30, .seed = 7}, [&now] { return now; });
	faulty.SetHandlers(Received {}.Handlers());
	sockets.handlers.opened(1);
	std::vector<std::string> expected;
	for (int i = 0; i < 200; ++i)
	{
		now = i * 100'000LL;
		EXPECT_TRUE(faulty.SendText(1, std::to_string(i)));
		expected.push_back("1:" + std::to_string(i));
		faulty.Poll();
	}
	now += 60'000'000;
	faulty.Poll();
	EXPECT_EQ(sockets.sent, expected);
	// About 30% of 200 packets, and a lost packet can be lost again
	EXPECT_GT(faulty.Losses(), 40u);
	EXPECT_LT(faulty.Losses(), 140u);
}

TEST(NetworkFaults, ACloseComesAfterTheMessagesBeforeItAndNoSendGoesAfterIt)
{
	FakeServerSockets sockets;
	int64_t now = 0;
	FaultyWebSocketServer faulty(sockets, NetworkFaults {.delayMilliseconds = 20}, [&now] { return now; });
	Received received;
	faulty.SetHandlers(received.Handlers());
	sockets.handlers.opened(1);
	sockets.handlers.opened(2);
	// The client of connection 1 sends a line and closes
	sockets.handlers.text(1, "bye");
	sockets.handlers.closed(1, 1000);
	// The server sends a line to connection 2 and closes it
	EXPECT_TRUE(faulty.SendText(2, "refused"));
	faulty.Close(2, 4003);
	EXPECT_FALSE(faulty.SendText(2, "late"));
	now = 20'000;
	faulty.Poll();
	EXPECT_EQ(received.lines, (std::vector<std::string> {"1:opened", "2:opened", "1:bye", "1:closed1000"}));
	EXPECT_EQ(sockets.sent, (std::vector<std::string> {"2:refused", "2:close4003"}));
	EXPECT_FALSE(faulty.SendText(1, "after the close"));
}
