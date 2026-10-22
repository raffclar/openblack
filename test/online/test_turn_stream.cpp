/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Commands/TurnCodec.h"
#include "FakeWebSocketClient.h"
#include "Online/TurnStream.h"

using namespace openblack;
using namespace openblack::player_commands;
using online::TurnStream;
using test_fakes::FakeWebSocketClient;

namespace
{

entt::entity Entity(uint32_t value)
{
	return static_cast<entt::entity>(value);
}

/// One computer of a player: its own WSS client and its end of the game socket
struct Player
{
	explicit Player(uint64_t& clock)
	    : stream(sockets, [&clock]() { return clock; })
	{
		stream.Open({.url = "wss://bw.example/game/1", .headers = {}, .subprotocols = {}},
		            online::TurnStreamHandlers {
		                .opened = {},
		                .turn = [this](const turn_codec::TurnBundle& turn) { turns.push_back(turn_codec::Encode(turn)); },
		                .text = [this](std::string_view text) { texts.emplace_back(text); },
		                .closed = {},
		            });
		connection = sockets.Last();
		sockets.Accept(connection);
		sockets.Poll();
	}

	FakeWebSocketClient sockets;
	TurnStream stream;
	online::WebSocketId connection {0};
	/// The bytes of each turn that the player got
	std::vector<std::vector<uint8_t>> turns;
	std::vector<std::string> texts;
};

/// A relay for the test, in place of the game server: it stamps each block with the player of its connection, and
/// sends one turn with all the blocks to every player
class FakeRelay
{
public:
	void Join(Player& player, PlayerNames slot) { _players.push_back({&player, slot}); }

	/// Closes the turn: takes what each player sent, and sends the turn to all
	void CloseTurn()
	{
		turn_codec::TurnBundle turn {.turn = _turn++, .blocks = {}};
		for (auto& [player, slot] : _players)
		{
			for (const auto& sent : player->sockets.TakeSent(player->connection))
			{
				ASSERT_FALSE(sent.text);
				auto decoded = turn_codec::Decode(sent.bytes);
				ASSERT_TRUE(decoded.frame.has_value());
				if (auto* block = std::get_if<turn_codec::CommandBlock>(&*decoded.frame))
				{
					// The stamp comes from the connection, never from the client
					turn.blocks.push_back({.sender = slot, .sequence = block->sequence, .actions = std::move(block->actions)});
				}
				else if (const auto* checksum = std::get_if<turn_codec::TurnChecksum>(&*decoded.frame))
				{
					checksums.emplace_back(slot, checksum->checksum);
				}
			}
		}
		const auto bytes = turn_codec::Encode(turn);
		for (auto& [player, slot] : _players)
		{
			player->sockets.ServerBinary(player->connection, bytes);
			player->sockets.Poll();
		}
	}

	std::vector<std::pair<PlayerNames, uint64_t>> checksums;

private:
	std::vector<std::pair<Player*, PlayerNames>> _players;
	uint32_t _turn {100};
};

} // namespace

TEST(TurnStream, TwoClientsGetTheSameTurnsThroughTheRelay)
{
	uint64_t clock = 0;
	Player one(clock);
	Player two(clock);
	FakeRelay relay;
	relay.Join(one, PlayerNames::PLAYER_ONE);
	relay.Join(two, PlayerNames::PLAYER_TWO);

	const std::vector<Action> fromOne {HandLetGo {.object = Entity(5), .velocity = {0.1f, -0.0f, 3.0e38f}}};
	const std::vector<Action> fromTwo {LeashOrder {.kind = LeashOrder::Kind::AtPlace, .object = entt::null, .place = {1, 2, 3}},
	                                   HandTap {.object = Entity(6)}};
	ASSERT_TRUE(one.stream.SendCommands(99, fromOne));
	ASSERT_TRUE(two.stream.SendCommands(99, fromTwo));
	ASSERT_TRUE(one.stream.SendCommands(99, std::vector<Action> {HandTake {.object = Entity(7)}}));
	ASSERT_TRUE(one.stream.SendChecksum(99, 0xABCDEF));
	relay.CloseTurn();
	relay.CloseTurn();

	ASSERT_EQ(one.turns.size(), 2u);
	EXPECT_EQ(one.turns, two.turns);
	const auto first = turn_codec::Decode(one.turns[0]);
	ASSERT_TRUE(first.frame.has_value());
	const auto& turn = std::get<turn_codec::TurnBundle>(*first.frame);
	EXPECT_EQ(turn.turn, 100u);
	ASSERT_EQ(turn.blocks.size(), 3u);
	EXPECT_EQ(turn.blocks[0].sender, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(turn.blocks[0].sequence, 0);
	EXPECT_EQ(turn.blocks[1].sender, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(turn.blocks[1].sequence, 1);
	EXPECT_EQ(turn.blocks[2].sender, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(turn.blocks[2].sequence, 0);
	EXPECT_EQ(turn.blocks[2].actions.size(), 2u);
	EXPECT_EQ(std::bit_cast<uint32_t>(std::get<HandLetGo>(turn.blocks[0].actions[0]).velocity.y), 0x80000000U);
	EXPECT_EQ(relay.checksums, (std::vector<std::pair<PlayerNames, uint64_t>> {{PlayerNames::PLAYER_ONE, 0xABCDEF}}));
	// The second turn has no commands
	const auto second = turn_codec::Decode(one.turns[1]);
	EXPECT_TRUE(std::get<turn_codec::TurnBundle>(*second.frame).blocks.empty());
}

TEST(TurnStream, TheClientAnswersThePingOfTheServerAndTimesItsOwnPing)
{
	uint64_t clock = 1'000'000;
	Player player(clock);
	player.sockets.ServerBinary(player.connection, turn_codec::Encode(turn_codec::Ping {.id = 4, .time = 77}));
	player.sockets.Poll();
	auto sent = player.sockets.TakeSent(player.connection);
	ASSERT_EQ(sent.size(), 1u);
	const auto pong = turn_codec::Decode(sent[0].bytes);
	ASSERT_TRUE(pong.frame.has_value());
	EXPECT_EQ(std::get<turn_codec::Pong>(*pong.frame).id, 4u);
	EXPECT_EQ(std::get<turn_codec::Pong>(*pong.frame).time, 77u);

	EXPECT_FALSE(player.stream.RoundTrip().has_value());
	ASSERT_TRUE(player.stream.SendPing());
	sent = player.sockets.TakeSent(player.connection);
	const auto ping = std::get<turn_codec::Ping>(*turn_codec::Decode(sent[0].bytes).frame);
	clock += 48'000;
	player.sockets.ServerBinary(player.connection, turn_codec::Encode(turn_codec::Pong {.id = ping.id, .time = ping.time}));
	player.sockets.Poll();
	EXPECT_EQ(player.stream.RoundTrip(), 48'000u);
}

TEST(TurnStream, TheClientRefusesBadFramesAndGivesTheJsonFrames)
{
	uint64_t clock = 0;
	Player player(clock);
	const std::vector<uint8_t> garbage {9, 9, 9};
	player.sockets.ServerBinary(player.connection, garbage);
	// A client never gets commands or checksums from the server
	player.sockets.ServerBinary(player.connection, turn_codec::Encode(turn_codec::TurnChecksum {.turn = 1, .checksum = 2}));
	player.sockets.ServerText(player.connection, R"({"type":"room","players":[]})");
	player.sockets.Poll();
	EXPECT_EQ(player.stream.RejectedFrames(), 2u);
	EXPECT_TRUE(player.turns.empty());
	ASSERT_EQ(player.texts.size(), 1u);
	EXPECT_EQ(player.texts[0], R"({"type":"room","players":[]})");
}

TEST(TurnStream, NothingGoesBeforeTheSocketOpensOrAfterItCloses)
{
	FakeWebSocketClient sockets;
	TurnStream stream(sockets, []() { return uint64_t {0}; });
	EXPECT_FALSE(stream.SendPing());
	bool closed = false;
	stream.Open(
	    {.url = "wss://bw.example/game/1", .headers = {}, .subprotocols = {}},
	    online::TurnStreamHandlers {.opened = {}, .turn = {}, .text = {}, .closed = [&](auto&& /*end*/) { closed = true; }});
	EXPECT_FALSE(stream.SendCommands(1, std::vector<Action> {HandTake {.object = Entity(1)}}));
	sockets.Accept(sockets.Last());
	sockets.Poll();
	EXPECT_TRUE(stream.IsOpen());
	// A block with too many commands breaks the format
	EXPECT_FALSE(stream.SendCommands(1, std::vector<Action>(turn_codec::k_MostBlockCommands + 1, HandTake {})));
	sockets.End(sockets.Last(), {.end = online::WebSocketEnd::Network, .code = 0, .httpStatus = 0});
	sockets.Poll();
	EXPECT_TRUE(closed);
	EXPECT_FALSE(stream.IsOpen());
	EXPECT_FALSE(stream.SendChecksum(1, 2));
}
