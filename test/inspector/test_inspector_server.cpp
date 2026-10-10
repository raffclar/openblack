/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <thread>

#include <Inspector.h>
#include <InspectorServer.h>
#include <gtest/gtest.h>

using namespace openblack::inspector;
using namespace std::chrono_literals;

namespace
{

/// Polls the server as the game does once a frame, until the client has its line
std::optional<std::string> Exchange(Server& server, Client& client, const std::string& line)
{
	if (!client.SendLine(line))
	{
		return std::nullopt;
	}
	const Inspector inspector;
	for (int frame = 0; frame < 200; ++frame)
	{
		server.Poll([&inspector](std::string_view request) { return inspector.Handle(request); });
		if (auto answer = client.ReceiveLine(5ms); answer.has_value())
		{
			return answer;
		}
	}
	return std::nullopt;
}

} // namespace

TEST(InspectorServer, AnswersALineWithALine)
{
	std::string error;
	auto server = Server::Listen(0, error);
	ASSERT_NE(server, nullptr) << error;
	EXPECT_NE(server->Port(), 0);

	auto client = Client::Connect(server->Port());
	ASSERT_TRUE(client.has_value());

	const auto answer = Exchange(*server, *client, R"({"id": 1, "query": "ping"})");
	ASSERT_TRUE(answer.has_value());
	const auto parsed = Parse(*answer);
	ASSERT_TRUE(parsed.has_value());
	EXPECT_EQ((*parsed)["id"], 1);
	EXPECT_EQ((*parsed)["result"]["pong"], true);

	// The same connection takes another
	const auto again = Exchange(*server, *client, R"({"id": 2, "query": "nothing.here"})");
	ASSERT_TRUE(again.has_value());
	EXPECT_EQ((*Parse(*again))["ok"], false);
}

TEST(InspectorServer, NeverWaitsWithoutClients)
{
	std::string error;
	auto server = Server::Listen(0, error);
	ASSERT_NE(server, nullptr) << error;
	const auto start = std::chrono::steady_clock::now();
	EXPECT_EQ(server->Poll([](std::string_view) { return std::string(); }), 0u);
	EXPECT_LT(std::chrono::steady_clock::now() - start, 100ms);
}

TEST(InspectorServer, ClientsThatGoAreDropped)
{
	std::string error;
	auto server = Server::Listen(0, error);
	ASSERT_NE(server, nullptr) << error;
	{
		auto client = Client::Connect(server->Port());
		ASSERT_TRUE(client.has_value());
		ASSERT_TRUE(Exchange(*server, *client, R"({"query": "ping"})").has_value());
		EXPECT_EQ(server->ClientCount(), 1u);
	}
	for (int frame = 0; frame < 100 && server->ClientCount() != 0; ++frame)
	{
		server->Poll([](std::string_view) { return std::string(); });
	}
	EXPECT_EQ(server->ClientCount(), 0u);
}

TEST(InspectorServer, ATakenPortIsReported)
{
	std::string error;
	auto first = Server::Listen(0, error);
	ASSERT_NE(first, nullptr) << error;
	auto second = Server::Listen(first->Port(), error);
	EXPECT_EQ(second, nullptr);
	EXPECT_FALSE(error.empty());
}

// A client that only pings is looking for games, not driving one: only a client that asks something else counts as
// controlling, and only once the filter says its line takes control
TEST(InspectorServer, OnlyClientsTakingControlCountAsControlling)
{
	std::string error;
	auto server = Server::Listen(0, error);
	ASSERT_NE(server, nullptr) << error;
	server->SetControlFilter(&Inspector::TakesControl);

	auto looking = Client::Connect(server->Port());
	auto driving = Client::Connect(server->Port());
	ASSERT_TRUE(looking.has_value());
	ASSERT_TRUE(driving.has_value());

	ASSERT_TRUE(Exchange(*server, *looking, R"({"id": 1, "query": "ping"})").has_value());
	EXPECT_EQ(server->ClientCount(), 2);
	EXPECT_EQ(server->ControllingClientCount(), 0);

	ASSERT_TRUE(Exchange(*server, *driving, R"({"id": 2, "query": "describe"})").has_value());
	EXPECT_EQ(server->ControllingClientCount(), 1);
	// It stays controlling while connected, whatever it asks next
	ASSERT_TRUE(Exchange(*server, *driving, R"({"id": 3, "query": "ping"})").has_value());
	EXPECT_EQ(server->ControllingClientCount(), 1);
}

// Without a filter every line takes control, as before
TEST(InspectorServer, EveryLineTakesControlWithoutAFilter)
{
	std::string error;
	auto server = Server::Listen(0, error);
	ASSERT_NE(server, nullptr) << error;
	auto client = Client::Connect(server->Port());
	ASSERT_TRUE(client.has_value());
	ASSERT_TRUE(Exchange(*server, *client, R"({"id": 1, "query": "ping"})").has_value());
	EXPECT_EQ(server->ControllingClientCount(), 1);
}

// While the game's frame is busy answering one line (a land loading), a helper polling from another thread answers
// other clients meanwhile; the slow answer still reaches its client once made
TEST(InspectorServer, AnotherThreadAnswersWhileOneHandlerIsBusy)
{
	std::string error;
	auto server = Server::Listen(0, error);
	ASSERT_NE(server, nullptr) << error;
	auto loading = Client::Connect(server->Port());
	auto waiting = Client::Connect(server->Port());
	ASSERT_TRUE(loading.has_value());
	ASSERT_TRUE(waiting.has_value());

	std::atomic<bool> busy {false};
	std::atomic<bool> release {false};
	ASSERT_TRUE(loading->SendLine("slow"));
	std::thread frame([&server, &busy, &release] {
		const auto deadline = std::chrono::steady_clock::now() + 5s;
		while (server->Poll([&busy, &release, deadline](std::string_view) {
			busy = true;
			while (!release && std::chrono::steady_clock::now() < deadline)
			{
				std::this_thread::sleep_for(1ms);
			}
			return std::string("loaded");
		}) == 0 &&
		       std::chrono::steady_clock::now() < deadline)
		{
			std::this_thread::sleep_for(1ms);
		}
	});
	while (!busy)
	{
		std::this_thread::sleep_for(1ms);
	}

	ASSERT_TRUE(waiting->SendLine("quick"));
	std::optional<std::string> quick;
	for (int poll = 0; poll < 500 && !quick.has_value(); ++poll)
	{
		server->Poll([](std::string_view) { return std::string("loading"); });
		quick = waiting->ReceiveLine(5ms);
	}
	EXPECT_EQ(quick, "loading");
	EXPECT_FALSE(release);

	release = true;
	frame.join();
	std::optional<std::string> slow;
	for (int poll = 0; poll < 200 && !slow.has_value(); ++poll)
	{
		server->Poll([](std::string_view) { return std::string(); });
		slow = loading->ReceiveLine(5ms);
	}
	EXPECT_EQ(slow, "loaded");
}
