/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Online/DatagramSocketInterface.h"
#include "Online/LanDiscovery.h"
#include "Online/LanResponder.h"
#include "Online/LanSearcher.h"
#include "Server/LanRoom.h"

using namespace openblack;
namespace lan_discovery = online::lan_discovery;
using lan_discovery::Address;
using lan_discovery::V4;
using lan_discovery::V6;

namespace
{

lobby_api::LanAnswer Answer()
{
	lobby_api::LanAnswer answer;
	answer.hostname = "Office game";
	answer.land = "Online Maps/mpm_2p_1.map";
	answer.gamemode = "openstaging";
	answer.numplayers = 1;
	answer.maxplayers = 2;
	answer.gamever = "0.1.0";
	answer.port = 4433;
	answer.players = {{.account = {.id = 0, .name = "Alice"}, .role = "god", .clan = std::nullopt}};
	return answer;
}

std::vector<uint8_t> Bytes(std::string_view text)
{
	return {text.begin(), text.end()};
}

/// A home network: 192.168.1.0/24, a link-local IPv6 address and a global IPv6 prefix on interface 4
std::vector<lan_discovery::NetworkInterface> Home()
{
	return {
	    {.address = V4(127, 0, 0, 1), .prefixLength = 8, .index = 1, .loopback = true, .multicast = true},
	    {.address = V6({0, 0, 0, 0, 0, 0, 0, 1}), .prefixLength = 128, .index = 1, .loopback = true, .multicast = true},
	    {.address = V4(192, 168, 1, 20), .prefixLength = 24, .index = 4, .loopback = false, .multicast = true},
	    {.address = V6({0xFE80, 0, 0, 0, 0, 0, 0, 0x20}, 4),
	     .prefixLength = 64,
	     .index = 4,
	     .loopback = false,
	     .multicast = true},
	    {.address = V6({0x2001, 0xDB8, 1, 0, 0, 0, 0, 0x20}),
	     .prefixLength = 64,
	     .index = 4,
	     .loopback = false,
	     .multicast = true},
	};
}

struct SentDatagram
{
	Address to;
	uint16_t port {0};
	std::vector<uint8_t> bytes;
};

/// A socket that keeps what goes and gives what the test puts in
class FakeSocket final: public online::DatagramSocketInterface
{
public:
	struct State
	{
		bool open {true};
		std::vector<SentDatagram> sent;
		std::deque<online::Datagram> inbox;
	};

	explicit FakeSocket(std::shared_ptr<State> state)
	    : _state(std::move(state))
	{
	}

	[[nodiscard]] bool IsOpen() const override { return _state->open; }
	bool SendTo(const Address& to, uint16_t port, std::span<const uint8_t> bytes) override
	{
		_state->sent.push_back({.to = to, .port = port, .bytes = {bytes.begin(), bytes.end()}});
		return true;
	}
	[[nodiscard]] std::optional<online::Datagram> Receive() override
	{
		if (_state->inbox.empty())
		{
			return std::nullopt;
		}
		auto datagram = std::move(_state->inbox.front());
		_state->inbox.pop_front();
		return datagram;
	}

private:
	std::shared_ptr<State> _state;
};

/// The fakes of one side: two sockets, a clock and random numbers that the test controls
struct Side
{
	std::shared_ptr<FakeSocket::State> v4 = std::make_shared<FakeSocket::State>();
	std::shared_ptr<FakeSocket::State> v6 = std::make_shared<FakeSocket::State>();
	std::shared_ptr<int64_t> now = std::make_shared<int64_t>(1'000'000);
	std::shared_ptr<uint64_t> random = std::make_shared<uint64_t>(0);

	online::LanSearchParts SearchParts() const
	{
		return {.v4 = std::make_unique<FakeSocket>(v4),
		        .v6 = std::make_unique<FakeSocket>(v6),
		        .interfaces = &Home,
		        .clock = [now = now] { return *now; },
		        .random = [random = random] { return (*random)++; }};
	}

	online::LanResponderParts ResponderParts() const
	{
		return {.v4 = std::make_unique<FakeSocket>(v4),
		        .v6 = std::make_unique<FakeSocket>(v6),
		        .interfaces = &Home,
		        .clock = [now = now] { return *now; },
		        .random = [random = random] { return (*random)++; }};
	}
};

/// The session in the last search that went on the socket
uint64_t LastSession(const FakeSocket::State& state)
{
	return lan_discovery::ParseSearch(state.sent.back().bytes).value();
}

} // namespace

TEST(LanDiscovery, TheSearchHasItsVersionAndSession)
{
	const auto bytes = lan_discovery::SearchBytes(0x0123456789ABCDEF);
	EXPECT_EQ(std::string(bytes.begin(), bytes.end()), "openblack-lan-search 2 0123456789abcdef\n");
	EXPECT_EQ(lan_discovery::ParseSearch(bytes), 0x0123456789ABCDEFu);
	// The search of another version, the search of the first build and other bytes are not a search
	EXPECT_FALSE(lan_discovery::ParseSearch(Bytes("openblack-lan-search 3 0123456789abcdef\n")).has_value());
	EXPECT_FALSE(lan_discovery::ParseSearch(Bytes("openblack-lan-search 1\n")).has_value());
	EXPECT_FALSE(lan_discovery::ParseSearch(Bytes("openblack-lan-search 2 0123\n")).has_value());
	EXPECT_FALSE(lan_discovery::ParseSearch(Bytes("abc")).has_value());
	EXPECT_FALSE(lan_discovery::ParseSearch(lan_discovery::AnswerBytes(1, 2, Answer())).has_value());
}

TEST(LanDiscovery, TheAnswerGivesTheRoomBack)
{
	const auto parsed = lan_discovery::ParseAnswer(lan_discovery::AnswerBytes(7, 0xABC, Answer()));
	ASSERT_TRUE(parsed.has_value());
	EXPECT_EQ(parsed->session, 7u);
	EXPECT_EQ(parsed->server, 0xABCu);
	EXPECT_EQ(parsed->room.hostname, "Office game");
	EXPECT_EQ(parsed->room.port, 4433);
	ASSERT_EQ(parsed->room.players.size(), 1u);
	EXPECT_EQ(parsed->room.players[0].account.name, "Alice");
	// An answer with no port of a game socket is not an answer
	auto noPort = Answer();
	noPort.port = 0;
	EXPECT_FALSE(lan_discovery::ParseAnswer(lan_discovery::AnswerBytes(7, 1, noPort)).has_value());
	EXPECT_FALSE(lan_discovery::ParseAnswer(lan_discovery::SearchBytes(7)).has_value());
}

TEST(LanDiscovery, AnAnswerOfAnotherVersionIsIgnored)
{
	auto bytes = lan_discovery::AnswerBytes(7, 1, Answer());
	const std::string_view prefix = "openblack-lan-room 2";
	ASSERT_EQ(std::string_view(reinterpret_cast<const char*>(bytes.data()), prefix.size()), prefix);
	bytes[prefix.size() - 1] = '3';
	EXPECT_FALSE(lan_discovery::ParseAnswer(bytes).has_value());
	// The answer of the first build has no version 2 header
	EXPECT_FALSE(lan_discovery::ParseAnswer(Bytes("openblack-lan-room 1\n{}")).has_value());
	// A datagram larger than the limit is not an answer
	std::vector<uint8_t> large(lan_discovery::k_MostDatagramBytes + 1, ' ');
	EXPECT_FALSE(lan_discovery::ParseAnswer(large).has_value());
}

TEST(LanDiscovery, AddressesAsText)
{
	EXPECT_EQ(lan_discovery::ToText(V4(192, 168, 1, 20)), "192.168.1.20");
	EXPECT_EQ(lan_discovery::ToText(V6({0xFE80, 0, 0, 0, 0, 0, 0, 1})), "fe80::1");
	EXPECT_EQ(lan_discovery::ToText(V6({0, 0, 0, 0, 0, 0, 0, 1})), "::1");
	EXPECT_EQ(lan_discovery::ToText(V6({0x2001, 0xDB8, 0, 0, 1, 0, 0, 1})), "2001:db8::1:0:0:1");
	EXPECT_EQ(lan_discovery::ToText(V6({0x2001, 0xDB8, 1, 2, 3, 4, 5, 6})), "2001:db8:1:2:3:4:5:6");
	EXPECT_EQ(lan_discovery::ToText(lan_discovery::SearchGroup(4)), "ff12::2613");
	EXPECT_TRUE(lan_discovery::IsLoopback(V4(127, 0, 0, 5)));
	EXPECT_TRUE(lan_discovery::IsLoopback(V6({0, 0, 0, 0, 0, 0, 0, 1})));
	EXPECT_FALSE(lan_discovery::IsLoopback(V6({0, 0, 0, 0, 0, 0, 0, 2})));
	EXPECT_TRUE(lan_discovery::IsLinkLocal(V6({0xFEBF, 0, 0, 0, 0, 0, 0, 1})));
	EXPECT_FALSE(lan_discovery::IsLinkLocal(V6({0xFEC0, 0, 0, 0, 0, 0, 0, 1})));
}

TEST(LanDiscovery, OnlyTheLocalNetworkIsLocal)
{
	const auto home = Home();
	EXPECT_TRUE(lan_discovery::IsOnLocalNetwork(V4(192, 168, 1, 77), home));
	EXPECT_TRUE(lan_discovery::IsOnLocalNetwork(V4(127, 0, 0, 1), home));
	EXPECT_FALSE(lan_discovery::IsOnLocalNetwork(V4(192, 168, 2, 77), home));
	EXPECT_FALSE(lan_discovery::IsOnLocalNetwork(V4(8, 8, 8, 8), home));
	EXPECT_TRUE(lan_discovery::IsOnLocalNetwork(V6({0xFE80, 0, 0, 0, 0, 0, 0, 0x77}, 4), home));
	EXPECT_TRUE(lan_discovery::IsOnLocalNetwork(V6({0x2001, 0xDB8, 1, 0, 0, 0, 0, 0x77}), home));
	EXPECT_FALSE(lan_discovery::IsOnLocalNetwork(V6({0x2001, 0xDB8, 2, 0, 0, 0, 0, 0x77}), home));
	// With no interfaces, only this computer and the link are local
	EXPECT_FALSE(lan_discovery::IsOnLocalNetwork(V4(192, 168, 1, 77), {}));
}

TEST(LanDiscovery, TheSearchGoesToEachNetworkOfTheComputer)
{
	const auto home = Home();
	const auto v4 = lan_discovery::SearchTargets(lan_discovery::Family::V4, home);
	ASSERT_EQ(v4.size(), 2u);
	EXPECT_EQ(v4[0], V4(127, 0, 0, 1));
	EXPECT_EQ(v4[1], V4(192, 168, 1, 255));
	const auto v6 = lan_discovery::SearchTargets(lan_discovery::Family::V6, home);
	ASSERT_EQ(v6.size(), 2u);
	EXPECT_EQ(v6[0], V6({0, 0, 0, 0, 0, 0, 0, 1}));
	EXPECT_EQ(v6[1], lan_discovery::SearchGroup(4));
	// With no network, the search goes to the broadcast address of all networks
	const auto alone = lan_discovery::SearchTargets(lan_discovery::Family::V4, {});
	ASSERT_EQ(alone.size(), 2u);
	EXPECT_EQ(alone[1], V4(255, 255, 255, 255));
	// A network of 10.1.0.0/16 has the broadcast address 10.1.255.255
	const std::array<lan_discovery::NetworkInterface, 1> office {
	    {{.address = V4(10, 1, 2, 3), .prefixLength = 16, .index = 2, .loopback = false, .multicast = true}}};
	EXPECT_EQ(lan_discovery::SearchTargets(lan_discovery::Family::V4, office)[1], V4(10, 1, 255, 255));
}

TEST(LanDiscovery, TheSearchesStartAtARandomTimeAndThenWaitLonger)
{
	EXPECT_EQ(lan_discovery::SearchDelay(0, 0), lan_discovery::k_LeastStartDelayMicroseconds);
	EXPECT_EQ(lan_discovery::SearchDelay(0, 100'000), lan_discovery::k_MostStartDelayMicroseconds);
	for (uint64_t random = 0; random < 300'000; random += 7'919)
	{
		const auto first = lan_discovery::SearchDelay(1, random);
		EXPECT_GE(first, 450'000);
		EXPECT_LE(first, 550'000);
		const auto second = lan_discovery::SearchDelay(2, random);
		EXPECT_GE(second, 900'000);
		EXPECT_LE(second, 1'100'000);
		for (const int index : {3, 4, 10, 1000})
		{
			const auto steady = lan_discovery::SearchDelay(index, random);
			EXPECT_GE(steady, 1'800'000);
			EXPECT_LE(steady, 2'200'000);
		}
		const auto answer = lan_discovery::AnswerDelay(random);
		EXPECT_GE(answer, lan_discovery::k_LeastAnswerDelayMicroseconds);
		EXPECT_LE(answer, lan_discovery::k_MostAnswerDelayMicroseconds);
	}
}

TEST(LanDiscovery, TheLineOfARoomHasTheAddressOfItsGameSocket)
{
	const auto entry = lan_discovery::EntryOf(Answer(), 0xAB, V4(192, 168, 1, 20));
	EXPECT_EQ(entry.id, "lan-00000000000000ab");
	EXPECT_EQ(entry.url, "wss://192.168.1.20:4433/game");
	EXPECT_EQ(entry.hostname, "Office game");
	EXPECT_EQ(entry.numplayers, 1);
	EXPECT_EQ(entry.gamever, "0.1.0");
	EXPECT_EQ(lan_discovery::EntryOf(Answer(), 1, V6({0xFE80, 0, 0, 0, 0, 0, 0, 7}, 4)).url, "wss://[fe80::7%254]:4433/game");
	EXPECT_EQ(lan_discovery::EntryOf(Answer(), 1, V6({0x2001, 0xDB8, 1, 0, 0, 0, 0, 7})).url,
	          "wss://[2001:db8:1::7]:4433/game");
	// A server with a computer name for its certificate gives that name in the address
	auto named = Answer();
	named.host = "game.office.lan";
	EXPECT_EQ(lan_discovery::EntryOf(named, 1, V4(192, 168, 1, 20)).url, "wss://game.office.lan:4433/game");
}

TEST(LanDiscovery, TheListKeepsTheOrderAndTheBestAddressOfEachServer)
{
	lan_discovery::ServerList list;
	list.Heard({.session = 1, .server = 10, .room = Answer()}, V6({0xFE80, 0, 0, 0, 0, 0, 0, 7}, 4), 0);
	list.Heard({.session = 1, .server = 20, .room = Answer()}, V4(192, 168, 1, 30), 1);
	// The same server on IPv4: one line, with the IPv4 address
	list.Heard({.session = 1, .server = 10, .room = Answer()}, V4(192, 168, 1, 7), 2);
	ASSERT_EQ(list.Servers().size(), 2u);
	EXPECT_EQ(list.Servers()[0].url, "wss://192.168.1.7:4433/game");
	EXPECT_EQ(list.Servers()[1].id, "lan-0000000000000014");
	// A later answer on IPv6 changes the room but keeps the IPv4 address
	auto changed = Answer();
	changed.hostname = "new name";
	list.Heard({.session = 1, .server = 10, .room = changed}, V6({0xFE80, 0, 0, 0, 0, 0, 0, 7}, 4), 3);
	EXPECT_EQ(list.Servers()[0].hostname, "new name");
	EXPECT_EQ(list.Servers()[0].url, "wss://192.168.1.7:4433/game");
	list.Clear();
	EXPECT_TRUE(list.Servers().empty());
}

TEST(LanDiscovery, ARoomGoesAfterSomeMissedAnswers)
{
	lan_discovery::ServerList list;
	list.Searched(0);
	list.Heard({.session = 1, .server = 10, .room = Answer()}, V4(192, 168, 1, 7), 50);
	list.Heard({.session = 1, .server = 20, .room = Answer()}, V4(192, 168, 1, 8), 50);
	int64_t now = 0;
	// The miss of a search shows when the next search goes
	for (int search = 1; search <= lan_discovery::k_MostMissedAnswers + 1; ++search)
	{
		now += 1'000'000;
		list.Searched(now);
		// Only server 20 answers
		list.Heard({.session = 1, .server = 20, .room = Answer()}, V4(192, 168, 1, 8), now + 50);
		const auto expected = search <= lan_discovery::k_MostMissedAnswers ? 2u : 1u;
		ASSERT_EQ(list.Servers().size(), expected) << "search " << search;
	}
	EXPECT_EQ(list.Servers()[0].id, "lan-0000000000000014");
}

TEST(LanDiscovery, TheListHasALimit)
{
	lan_discovery::ServerList list;
	for (uint64_t server = 0; server < lan_discovery::k_MostRooms + 10; ++server)
	{
		list.Heard({.session = 1, .server = server, .room = Answer()}, V4(192, 168, 1, 7), 0);
	}
	EXPECT_EQ(list.Servers().size(), lan_discovery::k_MostRooms);
}

TEST(LanSearcher, TheFirstSearchWaitsForTheRandomStartTime)
{
	Side side;
	online::LanSearcher searcher(side.SearchParts());
	EXPECT_TRUE(searcher.IsAvailable());
	*side.random = 50'000;
	searcher.Start();
	// The session takes the first random number, the start time the second: 20 ms + 50'001 microseconds
	searcher.Poll();
	EXPECT_TRUE(side.v4->sent.empty());
	*side.now += 70'000;
	searcher.Poll();
	EXPECT_TRUE(side.v4->sent.empty());
	*side.now += 2;
	searcher.Poll();
	ASSERT_EQ(side.v4->sent.size(), 2u);
	EXPECT_EQ(side.v4->sent[0].to, V4(127, 0, 0, 1));
	EXPECT_EQ(side.v4->sent[1].to, V4(192, 168, 1, 255));
	EXPECT_EQ(side.v4->sent[1].port, lan_discovery::k_Port);
	ASSERT_EQ(side.v6->sent.size(), 2u);
	EXPECT_EQ(side.v6->sent[1].to, lan_discovery::SearchGroup(4));
	EXPECT_EQ(LastSession(*side.v4), 50'000u);
	// The next search waits about half a second
	*side.now += 400'000;
	searcher.Poll();
	EXPECT_EQ(side.v4->sent.size(), 2u);
	*side.now += 200'000;
	searcher.Poll();
	EXPECT_EQ(side.v4->sent.size(), 4u);
}

TEST(LanSearcher, OnlyAnswersToItsSessionFromTheLocalNetworkShow)
{
	Side side;
	online::LanSearcher searcher(side.SearchParts());
	searcher.Start();
	*side.now += lan_discovery::k_MostStartDelayMicroseconds;
	searcher.Poll();
	const auto session = LastSession(*side.v4);
	auto otherVersion = lan_discovery::AnswerBytes(session, 3, Answer());
	otherVersion[std::string_view("openblack-lan-room ").size()] = '9';
	side.v4->inbox.push_back(
	    {.bytes = lan_discovery::AnswerBytes(session, 1, Answer()), .from = V4(192, 168, 1, 30), .port = 2613});
	side.v4->inbox.push_back(
	    {.bytes = lan_discovery::AnswerBytes(session + 1, 2, Answer()), .from = V4(192, 168, 1, 31), .port = 2613});
	side.v4->inbox.push_back({.bytes = otherVersion, .from = V4(192, 168, 1, 32), .port = 2613});
	side.v4->inbox.push_back(
	    {.bytes = lan_discovery::AnswerBytes(session, 4, Answer()), .from = V4(203, 0, 113, 9), .port = 2613});
	side.v6->inbox.push_back({.bytes = lan_discovery::AnswerBytes(session, 5, Answer()),
	                          .from = V6({0xFE80, 0, 0, 0, 0, 0, 0, 5}, 4),
	                          .port = 2613});
	searcher.Poll();
	const auto servers = searcher.GetServers();
	ASSERT_EQ(servers.size(), 2u);
	EXPECT_EQ(servers[0].id, "lan-0000000000000001");
	EXPECT_EQ(servers[1].url, "wss://[fe80::5%254]:4433/game");
}

TEST(LanSearcher, ManyAnswersAllShowInTheirOrder)
{
	Side side;
	online::LanSearcher searcher(side.SearchParts());
	searcher.Start();
	*side.now += lan_discovery::k_MostStartDelayMicroseconds;
	searcher.Poll();
	const auto session = LastSession(*side.v4);
	for (uint8_t i = 0; i < 40; ++i)
	{
		auto room = Answer();
		room.hostname = "Room " + std::to_string(i);
		side.v4->inbox.push_back({.bytes = lan_discovery::AnswerBytes(session, 100 + i, room),
		                          .from = V4(192, 168, 1, static_cast<uint8_t>(100 + i)),
		                          .port = 2613});
	}
	searcher.Poll();
	const auto servers = searcher.GetServers();
	ASSERT_EQ(servers.size(), 40u);
	EXPECT_EQ(servers[0].hostname, "Room 0");
	EXPECT_EQ(servers[39].hostname, "Room 39");
	EXPECT_EQ(servers[39].url, "wss://192.168.1.139:4433/game");
}

TEST(LanSearcher, ASilentRoomGoesAfterThreeSearches)
{
	Side side;
	online::LanSearcher searcher(side.SearchParts());
	searcher.Start();
	*side.now += lan_discovery::k_MostStartDelayMicroseconds;
	searcher.Poll();
	side.v4->inbox.push_back(
	    {.bytes = lan_discovery::AnswerBytes(LastSession(*side.v4), 1, Answer()), .from = V4(192, 168, 1, 30), .port = 2613});
	searcher.Poll();
	ASSERT_EQ(searcher.GetServers().size(), 1u);
	int searches = 0;
	for (int step = 0; step < 100 && !searcher.GetServers().empty(); ++step)
	{
		const auto before = side.v4->sent.size();
		*side.now += 100'000;
		searcher.Poll();
		searches += side.v4->sent.size() > before ? 1 : 0;
	}
	EXPECT_TRUE(searcher.GetServers().empty());
	// The room misses three searches. The search after them removes it.
	EXPECT_EQ(searches, lan_discovery::k_MostMissedAnswers + 1);
	// A new start forgets the old rooms and takes a new session
	const auto oldSession = LastSession(*side.v4);
	searcher.Start();
	*side.now += lan_discovery::k_MostStartDelayMicroseconds;
	searcher.Poll();
	EXPECT_NE(LastSession(*side.v4), oldSession);
}

TEST(LanSearcher, WithNoOpenSocketTheSearchIsNotAvailable)
{
	Side side;
	side.v4->open = false;
	side.v6->open = false;
	online::LanSearcher searcher(side.SearchParts());
	EXPECT_FALSE(searcher.IsAvailable());
	// With IPv4 only, the search goes on IPv4
	side.v4->open = true;
	EXPECT_TRUE(searcher.IsAvailable());
	searcher.Start();
	*side.now += lan_discovery::k_MostStartDelayMicroseconds;
	searcher.Poll();
	EXPECT_FALSE(side.v4->sent.empty());
	EXPECT_TRUE(side.v6->sent.empty());
}

TEST(LanResponder, TheAnswerGoesAfterARandomTime)
{
	Side side;
	*side.random = 30'000;
	online::LanResponder responder(side.ResponderParts());
	EXPECT_EQ(responder.Server(), 30'000u);
	side.v4->inbox.push_back({.bytes = lan_discovery::SearchBytes(77), .from = V4(192, 168, 1, 50), .port = 50'000});
	const auto room = [] { return Answer(); };
	responder.Poll(room);
	EXPECT_TRUE(side.v4->sent.empty());
	// The delay is 20 ms + 30'001 microseconds
	*side.now += 50'000;
	responder.Poll(room);
	EXPECT_TRUE(side.v4->sent.empty());
	*side.now += 1;
	responder.Poll(room);
	ASSERT_EQ(side.v4->sent.size(), 1u);
	EXPECT_EQ(side.v4->sent[0].to, V4(192, 168, 1, 50));
	EXPECT_EQ(side.v4->sent[0].port, 50'000);
	const auto answer = lan_discovery::ParseAnswer(side.v4->sent[0].bytes);
	ASSERT_TRUE(answer.has_value());
	EXPECT_EQ(answer->session, 77u);
	EXPECT_EQ(answer->server, responder.Server());
}

TEST(LanResponder, AForeignSenderOrAnotherVersionGetsNoAnswer)
{
	Side side;
	online::LanResponder responder(side.ResponderParts());
	side.v4->inbox.push_back({.bytes = lan_discovery::SearchBytes(1), .from = V4(203, 0, 113, 9), .port = 50'000});
	side.v6->inbox.push_back(
	    {.bytes = lan_discovery::SearchBytes(1), .from = V6({0x2001, 0xDB8, 9, 0, 0, 0, 0, 1}), .port = 50'000});
	side.v4->inbox.push_back({.bytes = Bytes("openblack-lan-search 1\n"), .from = V4(192, 168, 1, 50), .port = 50'000});
	side.v4->inbox.push_back(
	    {.bytes = Bytes("openblack-lan-search 3 0000000000000001\n"), .from = V4(192, 168, 1, 51), .port = 50'000});
	const auto room = [] { return Answer(); };
	responder.Poll(room);
	*side.now += 1'000'000;
	responder.Poll(room);
	EXPECT_TRUE(side.v4->sent.empty());
	EXPECT_TRUE(side.v6->sent.empty());
}

TEST(LanResponder, OneSenderGetsOneAnswerInAShortTime)
{
	Side side;
	online::LanResponder responder(side.ResponderParts());
	const auto room = [] { return Answer(); };
	const auto search = [&side] {
		side.v6->inbox.push_back(
		    {.bytes = lan_discovery::SearchBytes(5), .from = V6({0xFE80, 0, 0, 0, 0, 0, 0, 9}, 4), .port = 40'000});
	};
	search();
	search();
	responder.Poll(room);
	*side.now += lan_discovery::k_MostAnswerDelayMicroseconds;
	responder.Poll(room);
	ASSERT_EQ(side.v6->sent.size(), 1u);
	EXPECT_EQ(side.v6->sent[0].to, V6({0xFE80, 0, 0, 0, 0, 0, 0, 9}, 4));
	EXPECT_TRUE(side.v4->sent.empty());
	// A search soon after the answer gets no answer
	search();
	*side.now += 10'000;
	responder.Poll(room);
	*side.now += lan_discovery::k_MostAnswerDelayMicroseconds;
	responder.Poll(room);
	EXPECT_EQ(side.v6->sent.size(), 1u);
	// After the gap, the sender gets an answer again
	*side.now += lan_discovery::k_LeastAnswerGapMicroseconds;
	search();
	responder.Poll(room);
	*side.now += lan_discovery::k_MostAnswerDelayMicroseconds;
	responder.Poll(room);
	EXPECT_EQ(side.v6->sent.size(), 2u);
}

TEST(LanResponder, ManySendersAreAnsweredUpToTheLimit)
{
	Side side;
	online::LanResponder responder(side.ResponderParts());
	for (int i = 0; i < 100; ++i)
	{
		side.v4->inbox.push_back({.bytes = lan_discovery::SearchBytes(static_cast<uint64_t>(i)),
		                          .from = V4(192, 168, 1, 50),
		                          .port = static_cast<uint16_t>(40'000 + i)});
	}
	const auto room = [] { return Answer(); };
	responder.Poll(room);
	*side.now += lan_discovery::k_MostAnswerDelayMicroseconds;
	responder.Poll(room);
	EXPECT_EQ(side.v4->sent.size(), lan_discovery::k_MostWaitingAnswers);
}

TEST(LanResponder, TheSearcherFindsTheResponder)
{
	Side client;
	Side server;
	*server.random = 900;
	online::LanSearcher searcher(client.SearchParts());
	online::LanResponder responder(server.ResponderParts());
	const auto room = [] { return Answer(); };
	searcher.Start();
	for (int step = 0; step < 10; ++step)
	{
		*client.now += 50'000;
		*server.now += 50'000;
		searcher.Poll();
		// The fake network: the searches to the broadcast address reach the server, and the answers reach the client
		for (auto& sent : client.v4->sent)
		{
			if (sent.to == V4(192, 168, 1, 255))
			{
				server.v4->inbox.push_back({.bytes = sent.bytes, .from = V4(192, 168, 1, 20), .port = 51'000});
			}
		}
		client.v4->sent.clear();
		responder.Poll(room);
		for (auto& sent : server.v4->sent)
		{
			client.v4->inbox.push_back({.bytes = sent.bytes, .from = V4(192, 168, 1, 40), .port = 2613});
		}
		server.v4->sent.clear();
	}
	const auto servers = searcher.GetServers();
	ASSERT_EQ(servers.size(), 1u);
	EXPECT_EQ(servers[0].id, "lan-0000000000000384");
	EXPECT_EQ(servers[0].url, "wss://192.168.1.40:4433/game");
}

TEST(LanRoom, TheAnswerListsTheGodsByTheNamesOfTheirPlayers)
{
	const server::lobby_status::Room room {.hostname = "Office game", .land = "Land1.txt", .maxGods = 2, .version = "0.1.0"};
	std::vector<server::LockstepHost::Seat> seats(3);
	seats[0].state = server::LockstepHost::SeatState::Taken;
	seats[0].name = "Alice";
	seats[1].state = server::LockstepHost::SeatState::Closed;
	seats[1].name = "Gone";
	seats[2].state = server::LockstepHost::SeatState::Away;
	seats[2].name = "Bob";
	const auto answer = server::lan_room::AnswerOf(room, false, seats, 4433, "");
	EXPECT_EQ(answer.gamemode, "openstaging");
	EXPECT_EQ(answer.numplayers, 2);
	EXPECT_EQ(answer.maxplayers, 2);
	EXPECT_EQ(answer.port, 4433);
	ASSERT_EQ(answer.players.size(), 2u);
	EXPECT_EQ(answer.players[0].account.name, "Alice");
	EXPECT_EQ(answer.players[1].account.name, "Bob");
	EXPECT_EQ(server::lan_room::AnswerOf(room, true, seats, 4433, "").gamemode, "closedplaying");
}
