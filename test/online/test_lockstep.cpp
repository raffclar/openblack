/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Commands/TurnCodec.h"
#include "LoopbackSockets.h"
#include "Online/NetworkSession.h"
#include "Server/LockstepHost.h"

using namespace openblack;
using namespace openblack::player_commands;
using online::NetworkSession;
using server::LockstepHost;
using test_fakes::LoopbackSockets;

namespace
{

constexpr int64_t k_Turn = 100'000;

entt::entity Entity(uint32_t value)
{
	return static_cast<entt::entity>(value);
}

/// The world of a computer, for the test: the checksum of a turn follows every command that the world did
struct FakeWorld
{
	uint64_t hash {1469598103934665603ull};
	uint32_t turns {0};
	/// A world whose checksum parts from the others after this turn
	std::optional<uint32_t> partsAfter;

	uint64_t Play(uint32_t turn, const std::vector<Command>& commands)
	{
		turns = turn + 1;
		hash = (hash ^ (turn + 1)) * 1099511628211ull;
		for (const auto& command : commands)
		{
			hash = (hash ^ (static_cast<uint64_t>(command.player) + 1) * 31 + command.action.index()) * 1099511628211ull;
		}
		return partsAfter.has_value() && turn >= *partsAfter ? hash ^ 0xFF : hash;
	}
};

/// One client computer: its WSS client, its session and its world
struct Computer
{
	Computer(LoopbackSockets& network, int64_t& now, std::string ticket)
	    : sockets(network.MakeClient())
	    , session(*sockets, [&now] { return now; })
	{
		session.Join(online::NetworkSessionSettings {
		    .request = {.url = "wss://127.0.0.1/game/1", .headers = {}, .subprotocols = {}},
		    .ticket = std::move(ticket),
		    .gamePassword = {},
		    .comeBackMicroseconds = 5LL * 60 * 1'000'000,
		});
		session.SetReady();
	}

	/// The frame of the computer: the socket, the session, then the turns that are due
	void Frame()
	{
		sockets->Poll();
		session.Update();
		while (session.HasTurn(world.turns))
		{
			const auto turn = world.turns;
			const auto commands = session.TakeTurn(turn);
			played.push_back(commands.size());
			session.LocalChecksum(turn + 1, world.Play(turn, commands));
		}
		// After a resync the server's world comes: the computer loads it and goes on from its turn
		if (auto state = session.TakeWorld(); state.has_value())
		{
			ASSERT_EQ(state->bytes.size(), sizeof(uint64_t) + sizeof(uint32_t));
			std::memcpy(&world.hash, state->bytes.data(), sizeof(uint64_t));
			std::memcpy(&world.turns, state->bytes.data() + sizeof(uint64_t), sizeof(uint32_t));
			world.partsAfter.reset();
			++loads;
			session.WorldLoaded(state->turn);
		}
	}

	std::unique_ptr<LoopbackSockets::Client> sockets;
	NetworkSession session;
	FakeWorld world;
	std::vector<size_t> played;
	uint32_t loads {0};
};

server::TicketCheck::Result CheckFakeTicket(std::string_view token)
{
	// "god:name" or "observer:name"; anything else is not a ticket
	const auto colon = token.find(':');
	if (colon == std::string_view::npos)
	{
		return {.problem = server::TicketProblem::Signature, .claims = std::nullopt};
	}
	const auto role = game_socket::RoleFrom(token.substr(0, colon));
	if (!role.has_value())
	{
		return {.problem = server::TicketProblem::Format, .claims = std::nullopt};
	}
	join_ticket::Claims claims;
	claims.subject = std::string(token.substr(colon + 1));
	claims.name = claims.subject;
	claims.role = *role;
	return {.problem = server::TicketProblem::None, .claims = claims};
}

/// The game server of the test, with its own world
class LockstepTest: public ::testing::Test
{
protected:
	void MakeHost(uint32_t gods, int64_t comeBack = 5LL * 60 * 1'000'000)
	{
		host = std::make_unique<LockstepHost>(
		    network, CheckFakeTicket, [this] { return now; }, [this] { return "secret-" + std::to_string(++secrets); },
		    server::HostSettings {.start = {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0},
		                          .godsToStart = gods,
		                          .godSlots = 7,
		                          .comeBackMicroseconds = comeBack,
		                          .keptTurns = 3200});
	}

	/// One frame of the server and of each computer: 100 ms of time, and a turn when the world turns
	void Frame(std::vector<Computer*> computers)
	{
		now += k_Turn;
		network.Poll();
		host->Update();
		if (host->HasTurn(serverWorld.turns))
		{
			const auto turn = serverWorld.turns;
			const auto commands = host->TakeTurn(turn);
			host->TurnChecksum(turn + 1, serverWorld.Play(turn, commands));
		}
		for (auto* computer : computers)
		{
			computer->Frame();
		}
	}

	void Frames(std::vector<Computer*> computers, int count)
	{
		for (int i = 0; i < count; ++i)
		{
			Frame(computers);
		}
	}

	int64_t now {1'000'000};
	int secrets {0};
	LoopbackSockets network;
	std::unique_ptr<LockstepHost> host;
	FakeWorld serverWorld;
};

} // namespace

TEST_F(LockstepTest, TwoGodsJoinAndTheGameStartsWhenBothAreReady)
{
	MakeHost(2);
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 3);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Waiting);
	ASSERT_TRUE(alice.session.GetJoined().has_value());
	EXPECT_EQ(alice.session.GetJoined()->slot, 0);
	EXPECT_FALSE(host->Started());
	// No turn before the start
	EXPECT_EQ(serverWorld.turns, 0u);

	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 3);
	EXPECT_TRUE(host->Started());
	EXPECT_EQ(bob.session.GetJoined()->slot, 1);
	ASSERT_TRUE(alice.session.GetStart().has_value());
	EXPECT_EQ(alice.session.GetStart()->land, "Land1.txt");
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
}

TEST_F(LockstepTest, TheCommandsOfBothGodsComeInTheSameTurnsOnEveryComputer)
{
	MakeHost(2);
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 4);
	ASSERT_TRUE(host->Started());
	for (int i = 0; i < 600; ++i)
	{
		if (i % 7 == 0)
		{
			alice.session.Send(HandTap {.object = Entity(static_cast<uint32_t>(i))});
		}
		if (i % 11 == 0)
		{
			bob.session.Send(HandTake {.object = Entity(static_cast<uint32_t>(i))});
			bob.session.Send(LeashPostTap {.post = Entity(3)});
		}
		Frame({&alice, &bob});
	}
	EXPECT_GE(serverWorld.turns, 590u);
	EXPECT_GE(alice.world.turns + 3, serverWorld.turns);
	EXPECT_EQ(alice.played,
	          std::vector(bob.played.begin(),
	                      bob.played.begin() + static_cast<std::ptrdiff_t>(std::min(alice.played.size(), bob.played.size()))));
	// The worlds stay in step: every compared checksum is the same on the clients and on the server
	EXPECT_GT(alice.session.GetCompares().compared, 580u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
	const auto& seats = host->Seats();
	EXPECT_GT(seats[0].compares.compared, 580u);
	EXPECT_EQ(seats[0].compares.compared, seats[0].compares.same);
	EXPECT_EQ(seats[1].compares.compared, seats[1].compares.same);
	// The server stamped every block with the slot of its god
	size_t commands = 0;
	for (const auto count : alice.played)
	{
		commands += count;
	}
	// The first turn gives the god of slot 1 its hands
	EXPECT_EQ(commands, 1 + 600u / 7 + 1 + 2 * (600u / 11 + 1));
}

TEST_F(LockstepTest, TheServerRefusesBadTicketsLateJoinersAndTheCommandsOfObservers)
{
	MakeHost(1);
	Computer bad(network, now, "nothing");
	Computer watcher(network, now, "observer:carol");
	Frames({&bad, &watcher}, 3);
	EXPECT_EQ(bad.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(bad.session.RefusedReason(), "ticket");
	EXPECT_EQ(watcher.session.GetJoined()->role, game_socket::Role::Observer);
	EXPECT_EQ(host->Observers(), 1u);
	// An observer alone does not start the game
	EXPECT_FALSE(host->Started());

	Computer alice(network, now, "god:alice");
	Frames({&watcher, &alice}, 3);
	ASSERT_TRUE(host->Started());
	// The observer gets the turns too, and sends no command
	watcher.session.Send(HandTap {.object = Entity(1)});
	Frames({&watcher, &alice}, 20);
	EXPECT_GT(watcher.world.turns, 10u);
	EXPECT_EQ(watcher.session.GetCompares().compared, watcher.session.GetCompares().same);

	// A god who comes after the start needs the live world, which the server does not send yet
	Computer late(network, now, "god:dave");
	Frames({&watcher, &alice, &late}, 3);
	EXPECT_EQ(late.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(late.session.RefusedReason(), "started");
}

TEST_F(LockstepTest, AClientCannotSendTheLeaveOfTheServer)
{
	MakeHost(1);
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 3);
	alice.session.Send(PlayerLeft {});
	alice.session.Send(PlayerJoined {});
	alice.session.Send(HandTap {.object = Entity(2)});
	Frames({&alice}, 3);
	EXPECT_EQ(host->RefusedBlocks(), 2u);
	EXPECT_EQ(host->Seats()[0].state, LockstepHost::SeatState::Taken);
}

TEST_F(LockstepTest, AClientThatLosesTheSocketComesBackAndGetsTheTurnsThatItMissed)
{
	MakeHost(2);
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(host->Started());

	// Bob's network stops; his commands wait and are not lost
	network.Cut(bob.sockets->Ids().back());
	bob.session.Send(HandTap {.object = Entity(5)});
	Frames({&alice}, 1);
	bob.Frame();
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Reconnecting);
	Frames({&alice}, 10);
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Away);
	// Alice still plays: the world turns while a god's player is connected
	const auto turnsWhileAway = serverWorld.turns;
	EXPECT_GT(turnsWhileAway, 35u);

	Frames({&alice, &bob}, 10);
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.session.Resumes(), 1u);
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Taken);
	EXPECT_EQ(host->Seats()[1].resumes, 1u);
	EXPECT_GE(bob.world.turns, turnsWhileAway);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
	// The tap that waited for the socket went to the server after the resume, after the hands of slot 1 in the first
	// turn
	size_t commands = 0;
	for (const auto count : alice.played)
	{
		commands += count;
	}
	EXPECT_EQ(commands, 2u);
}

TEST_F(LockstepTest, TheWorldWaitsWithNoPlayersAndAGodThatDoesNotComeBackIsDefeated)
{
	MakeHost(1, 2 * 1'000'000);
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 10);
	ASSERT_TRUE(host->Started());
	alice.session.Leave();
	alice.Frame();
	Frames({}, 3);
	// The leave closed the slot; the defeat goes in the next turn, and then the world waits with no god
	EXPECT_EQ(host->Seats()[0].state, LockstepHost::SeatState::Closed);
	EXPECT_TRUE(host->Paused());
	const auto turns = serverWorld.turns;
	Frames({}, 20);
	EXPECT_EQ(serverWorld.turns, turns);
	EXPECT_FALSE(host->HasTurn(serverWorld.turns));
}

TEST_F(LockstepTest, AnAwayGodIsDefeatedAfterTheTimeToComeBack)
{
	MakeHost(2, 3 * 1'000'000);
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 10);
	network.Cut(bob.sockets->Ids().back());
	std::vector<Command> leaves;
	// Bob's computer stops: no frame of his any more
	for (int i = 0; i < 50; ++i)
	{
		Frame({&alice});
	}
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Closed);
	// Every computer did the defeat at the same turn: Alice is still in step with the server
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	// The hands of slot 1 in the first turn, and its defeat
	size_t commands = 0;
	for (const auto count : alice.played)
	{
		commands += count;
	}
	EXPECT_EQ(commands, 2u);
}

TEST_F(LockstepTest, AClientThatPartsTriesThreeRejoinsAndThenLeaves)
{
	MakeHost(1);
	Computer alice(network, now, "god:alice");
	alice.world.partsAfter = 20;
	for (int i = 0; i < 200 && alice.session.GetState() != NetworkSession::State::Ended; ++i)
	{
		Frame({&alice});
	}
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::OutOfSync);
	EXPECT_EQ(alice.session.Resync().Tries(), online::ResyncRule::k_MostTries);
	EXPECT_EQ(alice.session.Resumes(), online::ResyncRule::k_MostTries);
	EXPECT_EQ(alice.session.GetCompares().firstDifferent, 21u);
	ASSERT_TRUE(host->Seats()[0].compares.firstDifferent.has_value());
	EXPECT_EQ(*host->Seats()[0].compares.firstDifferent, 21u);
}

TEST_F(LockstepTest, AClientThatPartsLoadsTheWorldOfTheServerAndStaysInStep)
{
	MakeHost(1);
	// The world state of the fake world: its hash and its turns
	host->SetWorldState([this] {
		std::vector<uint8_t> bytes(sizeof(uint64_t) + sizeof(uint32_t));
		std::memcpy(bytes.data(), &serverWorld.hash, sizeof(uint64_t));
		std::memcpy(bytes.data() + sizeof(uint64_t), &serverWorld.turns, sizeof(uint32_t));
		return bytes;
	});
	Computer alice(network, now, "god:alice");
	alice.world.partsAfter = 20;
	Frames({&alice}, 120);
	EXPECT_EQ(alice.loads, 1u);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(alice.session.Resumes(), 1u);
	EXPECT_EQ(alice.session.Resync().GetState(), online::ResyncRule::State::InStep);
	EXPECT_FALSE(alice.session.ShowsResync());
	// After the load every turn is the same as the server's again
	EXPECT_EQ(alice.world.hash, serverWorld.hash);
	EXPECT_EQ(alice.world.turns, serverWorld.turns);
	EXPECT_GT(alice.session.GetCompares().same, 90u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same + 1);
}
