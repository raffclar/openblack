/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <GameSocket.h>
#include <gtest/gtest.h>
#include <spdlog/common.h>

#include "ECS/PartHashHistory.h"
#include "Online/ConnectionLog.h"

using namespace openblack;
using online::ConnectionLog;

TEST(ConnectionLog, MasksTicketsTokensAndSecrets)
{
	EXPECT_EQ(online::MaskSecrets("ticket v4.public.eyJhYmMi.sig sent"), "ticket <hidden> sent");
	EXPECT_EQ(online::MaskSecrets("Authorization: Bearer abc.def"), "Authorization: <hidden>");
	EXPECT_EQ(online::MaskSecrets(R"({"type":"room.resume","session":"secret-1","turn":4})"),
	          R"({"type":"room.resume","session":"<hidden>","turn":4})");
	EXPECT_EQ(online::MaskSecrets("game_password=hunter2 next"), "game_password=<hidden> next");
	EXPECT_EQ(online::MaskSecrets("refreshToken: abc"), "refreshToken: <hidden>");
	// Words of secrets with no value after them stay
	EXPECT_EQ(online::MaskSecrets("The client sends its join (with a ticket)"), "The client sends its join (with a ticket)");
	EXPECT_EQ(online::MaskSecrets("The lobby gave a ticket after 12 ms"), "The lobby gave a ticket after 12 ms");
}

TEST(ConnectionLog, DescribesWhoClosedTheSocketAndWhy)
{
	using online::WebSocketEnd;
	EXPECT_EQ(online::DescribeClosed({.end = WebSocketEnd::ClosedByServer, .code = 4003, .httpStatus = 0}),
	          "the server closed it with code 4003 (refused)");
	EXPECT_EQ(online::DescribeClosed({.end = WebSocketEnd::ClosedByServer, .code = 4999, .httpStatus = 0}),
	          "the server closed it with code 4999");
	EXPECT_EQ(online::DescribeClosed({.end = WebSocketEnd::HandshakeRefused, .code = 0, .httpStatus = 502}),
	          "the server refused the handshake with HTTP status 502");
	EXPECT_EQ(online::DescribeClosed({.end = WebSocketEnd::Timeout, .code = 0, .httpStatus = 0}),
	          "the client ended it: no data from the server in time");
	EXPECT_EQ(online::DescribeClosed({.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0}),
	          "the network or the browser ended it with no close frame");
}

TEST(ConnectionLog, NamesThePartsThatDifferWithBothValues)
{
	const std::vector<std::pair<std::string, uint64_t>> local {{"entities", 1}, {"Transform", 2}, {"Villager", 3}};
	const std::vector<std::pair<std::string, uint64_t>> other {{"entities", 1}, {"Transform", 5}, {"Creature", 4}};
	const auto parts = online::DifferentParts(local, other);
	ASSERT_EQ(parts.size(), 3u);
	EXPECT_EQ(parts[0].name, "Transform");
	EXPECT_EQ(parts[0].local, 2u);
	EXPECT_EQ(parts[0].other, 5u);
	// A part that only one world has differs too
	EXPECT_EQ(parts[1].name, "Villager");
	EXPECT_EQ(parts[1].other, 0u);
	EXPECT_EQ(parts[2].name, "Creature");
	EXPECT_EQ(parts[2].local, 0u);
	EXPECT_EQ(online::DescribeParts(std::span(parts).first(1), "this", "server"),
	          "1 parts differ: Transform (this 0000000000000002, server 0000000000000005)");
	EXPECT_EQ(online::DescribeParts(parts, "a", "b", 1),
	          "3 parts differ: Transform (a 0000000000000002, b 0000000000000005) and 2 more");
	EXPECT_EQ(online::DescribeParts({}, "a", "b"), "no part differs");
}

TEST(ConnectionLog, KeepsTheLastLinesWithTheirTimesForTheReport)
{
	int64_t now = 5'000'000;
	std::vector<std::pair<spdlog::level::level_enum, std::string>> sunk;
	ConnectionLog log([&now] { return now; },
	                  [&sunk](spdlog::level::level_enum level, std::string_view line) { sunk.emplace_back(level, line); });
	now += 1'234'000;
	log.Info("The game socket is open");
	log.Warn("ticket=abc");
	ASSERT_EQ(sunk.size(), 2u);
	EXPECT_EQ(sunk[1].first, spdlog::level::warn);
	EXPECT_EQ(sunk[1].second, "ticket=<hidden>");
	ASSERT_EQ(log.Lines().size(), 2u);
	EXPECT_EQ(log.Lines()[0], "       1.234 The game socket is open");
	EXPECT_EQ(log.Lines()[1], "       1.234 ! ticket=<hidden>");
	for (size_t i = 0; i < ConnectionLog::k_MostLines + 10; ++i)
	{
		log.Info("line");
	}
	EXPECT_EQ(log.Lines().size(), ConnectionLog::k_MostLines);
	const std::vector<std::string> header {"Build: x", "password: y"};
	const auto report = log.Report(header, 2);
	EXPECT_EQ(report, "Build: x\npassword: <hidden>\nThe last 2 lines of the connection log (seconds since the log "
	                  "started):\n       1.234 line\n       1.234 line\n");
}

TEST(PartHashHistory, KeepsThePartsOfTheLastTurnsAndForgetsTurnsAfterAStepBack)
{
	synced_state::PartHashHistory history;
	const std::vector<std::pair<std::string, uint64_t>> first {{"entities", 1}, {"Transform", 2}};
	history.Keep(10, first);
	history.Keep(11, std::vector<std::pair<std::string, uint64_t>> {{"entities", 3}, {"Transform", 4}});
	EXPECT_EQ(history.Of(10), first);
	EXPECT_EQ(history.Of(11)[1].second, 4u);
	EXPECT_TRUE(history.Of(12).empty());
	// A world that loads the server's state goes on from an earlier turn: the later turns go
	history.Keep(11, std::vector<std::pair<std::string, uint64_t>> {{"entities", 5}});
	EXPECT_EQ(history.Turns(), 2u);
	EXPECT_EQ(history.Of(11).size(), 1u);
	EXPECT_EQ(history.Of(10), first);
	for (uint32_t turn = 12; turn < 12 + synced_state::PartHashHistory::k_KeptTurns; ++turn)
	{
		history.Keep(turn, first);
	}
	EXPECT_EQ(history.Turns(), synced_state::PartHashHistory::k_KeptTurns);
	EXPECT_TRUE(history.Of(10).empty());
}

TEST(GameSocketSyncParts, GoesThroughJsonWithEveryBitOfTheHashes)
{
	const game_socket::SyncParts parts {.turn = 42, .parts = {{"entities", 0xFFFFFFFFFFFFFFFFull}, {"Transform", 0x10}}};
	const auto text = game_socket::ToJson(parts);
	const auto back = game_socket::Parse(text);
	ASSERT_TRUE(back.has_value());
	ASSERT_TRUE(std::holds_alternative<game_socket::SyncParts>(*back));
	EXPECT_EQ(std::get<game_socket::SyncParts>(*back), parts);
	// A hash that is not hexadecimal does not read
	EXPECT_FALSE(game_socket::Parse(R"({"type":"sync.parts","turn":1,"parts":[["a","xyz"]]})").has_value());
	EXPECT_FALSE(game_socket::Parse(R"({"type":"sync.parts","turn":1,"parts":[["a"]]})").has_value());
}
