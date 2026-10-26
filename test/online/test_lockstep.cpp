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

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Commands/TurnCodec.h"
#include "LoopbackSockets.h"
#include "Online/NetworkSession.h"
#include "Online/StartRules.h"
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

/// The land file of the server in the test
const std::vector<uint8_t> k_LandFile = {'L', 'A', 'N', 'D', '\n', '1', '\n'};

/// One client computer: its WSS client, its session and its world
struct Computer
{
	Computer(LoopbackSockets& network, int64_t& now, std::string ticket,
	         std::optional<std::vector<uint8_t>> landFile = k_LandFile, std::vector<uint8_t> creature = {'C'},
	         float alignment = 0.0f)
	    : sockets(network.MakeClient())
	    , session(*sockets, [&now] { return now; })
	{
		session.Join(online::NetworkSessionSettings {
		    .request = {.url = "wss://127.0.0.1/game/1", .headers = {}, .subprotocols = {}},
		    .ticket = std::move(ticket),
		    .gamePassword = {},
		    .comeBackMicroseconds = 5LL * 60 * 1'000'000,
		    .readLand = [this](std::string_view land) -> std::optional<std::vector<uint8_t>> {
			    askedLand = std::string(land);
			    return this->landFile;
		    },
		    .readCreature = [this] { return this->creature; },
		    .readAlignment = [alignment] { return alignment; },
		});
		this->landFile = std::move(landFile);
		this->creature = std::move(creature);
		session.SetReady(true);
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

	std::optional<std::vector<uint8_t>> landFile;
	std::vector<uint8_t> creature;
	std::string askedLand;
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
		    server::HostSettings {
		        .start = {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow", .godsRingMap = false},
		        .landChecksum = online::start_rules::LandChecksum(k_LandFile),
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

	/// The world state of the fake world: its hash and its turns
	void GiveWorldState()
	{
		host->SetWorldState([this] {
			std::vector<uint8_t> bytes(sizeof(uint64_t) + sizeof(uint32_t));
			std::memcpy(bytes.data(), &serverWorld.hash, sizeof(uint64_t));
			std::memcpy(bytes.data() + sizeof(uint64_t), &serverWorld.turns, sizeof(uint32_t));
			return bytes;
		});
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
	// The reply to the join gives the land, the seed and the start of the server's world before the start
	ASSERT_TRUE(alice.session.GetJoined()->start.has_value());
	EXPECT_EQ(alice.session.GetJoined()->start->land, "Land1.txt");
	EXPECT_EQ(alice.session.GetJoined()->start->seed, 1u);
	EXPECT_EQ(alice.session.GetJoined()->start->opening, "cow");
	EXPECT_FALSE(alice.session.GetStart().has_value());
	EXPECT_FALSE(host->Started());
	// No turn before the start
	EXPECT_EQ(serverWorld.turns, 0u);

	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 6);
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
	Frames({&alice, &bob}, 7);
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
	// The first turn names the humans of slots 0 and 1
	EXPECT_EQ(commands, 2 + 600u / 7 + 1 + 2 * (600u / 11 + 1));
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
	Frames({&watcher, &alice}, 6);
	ASSERT_TRUE(host->Started());
	// The observer gets the turns too, and sends no command
	watcher.session.Send(HandTap {.object = Entity(1)});
	Frames({&watcher, &alice}, 20);
	EXPECT_GT(watcher.world.turns, 10u);
	EXPECT_EQ(watcher.session.GetCompares().compared, watcher.session.GetCompares().same);

	// A player who comes after the start needs the live world. This server has no world state to send.
	Computer late(network, now, "god:dave");
	Frames({&watcher, &alice, &late}, 3);
	EXPECT_EQ(late.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(late.session.RefusedReason(), "started");
}

TEST_F(LockstepTest, AClientCannotSendTheLeaveOfTheServer)
{
	MakeHost(1);
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 6);
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
	// The tap that waited for the socket went to the server after the resume, after the humans of slots 0 and 1 in the
	// first turn
	size_t commands = 0;
	for (const auto count : alice.played)
	{
		commands += count;
	}
	EXPECT_EQ(commands, 3u);
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
	// The humans of slots 0 and 1 in the first turn, and the defeat of slot 1
	size_t commands = 0;
	for (const auto count : alice.played)
	{
		commands += count;
	}
	EXPECT_EQ(commands, 3u);
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

TEST_F(LockstepTest, TheHostTellsTheFirstTurnOfEachPart)
{
	MakeHost(1);
	std::vector<std::pair<uint8_t, uint32_t>> parts;
	host->SetPartListener([&parts](uint8_t seat, uint32_t turn) { parts.emplace_back(seat, turn); });
	Computer alice(network, now, "god:alice");
	alice.world.partsAfter = 20;
	for (int i = 0; i < 200 && alice.session.GetState() != NetworkSession::State::Ended; ++i)
	{
		Frame({&alice});
	}
	// The world stays parted for some turns after 21, but only the first turn of the part comes
	ASSERT_FALSE(parts.empty());
	EXPECT_EQ(parts.front(), std::make_pair(uint8_t {0}, 21u));
	EXPECT_LT(parts.size(), host->Seats()[0].compares.compared - host->Seats()[0].compares.same);
}

TEST_F(LockstepTest, AClientThatPartsLoadsTheWorldOfTheServerAndStaysInStep)
{
	MakeHost(1);
	GiveWorldState();
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

TEST_F(LockstepTest, TheGodsSendTheirCreaturesToAllPlayersBeforeTheStart)
{
	MakeHost(2);
	Computer alice(network, now, "god:alice", k_LandFile, {'A', 'L'}, -0.5f);
	Computer bob(network, now, "god:bob", k_LandFile, {}, 0.7f);
	Computer watcher(network, now, "observer:watcher", std::nullopt, {'W'}, 0.25f);
	Frames({&alice, &bob, &watcher}, 8);
	ASSERT_TRUE(host->Started());
	EXPECT_EQ(alice.askedLand, "Land1.txt");
	// The observer has no god: its reply does not stop the start, and it sends no creature
	EXPECT_EQ(watcher.askedLand, "Land1.txt");
	for (const auto* computer : {&alice, &bob, &watcher})
	{
		const auto& creatures = computer->session.StartCreatures();
		ASSERT_EQ(creatures.size(), 2u);
		EXPECT_EQ(creatures.at(0), (std::vector<uint8_t> {'A', 'L'}));
		EXPECT_TRUE(creatures.at(1).empty());
	}
	EXPECT_EQ(alice.session.TakeStartEvent(), online::StartEvent::HasLand);
	EXPECT_EQ(alice.session.TakeStartEvent(), online::StartEvent::Transfer);
	EXPECT_FALSE(alice.session.TakeStartEvent().has_value());

	// The server keeps the files for its own world, so each world has the same creature files for the turns
	for (const player_commands::TurnCommandSourceInterface* source :
	     std::initializer_list<const player_commands::TurnCommandSourceInterface*> {host.get(), &alice.session, &bob.session,
	                                                                                &watcher.session})
	{
		const auto first = source->StartCreatureFile(PlayerNames::PLAYER_ONE);
		EXPECT_EQ(std::vector<uint8_t>(first.begin(), first.end()), (std::vector<uint8_t> {'A', 'L'}));
		EXPECT_TRUE(source->StartCreatureFile(PlayerNames::PLAYER_TWO).empty());
		EXPECT_TRUE(source->StartCreatureFile(PlayerNames::PLAYER_THREE).empty());
		// Each god's alignment comes with its file; a slot with no player has none
		EXPECT_EQ(source->StartAlignment(PlayerNames::PLAYER_ONE), -0.5f);
		EXPECT_EQ(source->StartAlignment(PlayerNames::PLAYER_TWO), 0.7f);
		EXPECT_FALSE(source->StartAlignment(PlayerNames::PLAYER_THREE).has_value());
	}
	// Each client knows the god that it plays; the server and an observer play none
	EXPECT_EQ(alice.session.OwnGod(), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(bob.session.OwnGod(), PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(watcher.session.OwnGod().has_value());
	EXPECT_FALSE(host->OwnGod().has_value());
}

TEST_F(LockstepTest, AGodWithoutTheLandStopsTheStartAndIsNotReady)
{
	MakeHost(2);
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob", std::nullopt);
	Frames({&alice, &bob}, 8);
	EXPECT_FALSE(host->Started());
	EXPECT_FALSE(host->Starting());
	EXPECT_FALSE(host->Seats()[1].ready);
	EXPECT_TRUE(host->Seats()[0].ready);
	EXPECT_EQ(bob.session.TakeStartEvent(), online::StartEvent::LacksLand);
	EXPECT_EQ(alice.session.TakeStartEvent(), online::StartEvent::HasLand);
	EXPECT_EQ(alice.session.TakeStartEvent(), online::StartEvent::Cancelled);
	// Both players stay in the room
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Waiting);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Waiting);
}

TEST_F(LockstepTest, AGodWithAnotherLandFileIsRemovedFromTheRoom)
{
	MakeHost(2);
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob", std::vector<uint8_t> {'X'});
	Frames({&alice, &bob}, 8);
	EXPECT_FALSE(host->Started());
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Ended);
	EXPECT_EQ(bob.session.RefusedReason(), "kicked");
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Open);
	EXPECT_EQ(alice.session.TakeStartEvent(), online::StartEvent::HasLand);
	EXPECT_EQ(alice.session.TakeStartEvent(), online::StartEvent::Cancelled);
}

TEST_F(LockstepTest, AGodWhoLeavesDuringTheStartStopsIt)
{
	MakeHost(2);
	Computer alice(network, now, "god:alice");
	auto bob = std::make_unique<Computer>(network, now, "god:bob");
	// The start begins, then Bob leaves before the game starts
	for (int i = 0; i < 8 && !host->Starting(); ++i)
	{
		Frame({&alice, bob.get()});
	}
	ASSERT_TRUE(host->Starting());
	bob->session.Leave();
	bob->Frame();
	Frames({&alice}, 6);
	EXPECT_FALSE(host->Started());
	EXPECT_FALSE(host->Starting());
	bool stopped = false;
	while (const auto event = alice.session.TakeStartEvent())
	{
		stopped = stopped || *event == online::StartEvent::Stopped;
	}
	EXPECT_TRUE(stopped);
}

// The end box of a decided game opens on one computer: the server stops the turns, and every computer stops at the same
// turn with the same checksums
TEST_F(LockstepTest, AStopRequestStopsTheTurnsOfEveryComputerAtTheSameTurn)
{
	MakeHost(2);
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 7);
	ASSERT_TRUE(host->Started());
	// Every computer names each god by the name of its player
	ASSERT_TRUE(bob.session.GetStart().has_value());
	ASSERT_GE(bob.session.GetStart()->names.size(), 2u);
	EXPECT_EQ(bob.session.GetStart()->names[0], "alice");
	EXPECT_EQ(bob.session.GetStart()->names[1], "bob");
	Frames({&alice, &bob}, 50);
	bob.session.StopGame();
	bob.session.StopGame();
	Frames({&alice, &bob}, 50);
	ASSERT_TRUE(host->StoppedAfter().has_value());
	const auto last = *host->StoppedAfter();
	EXPECT_EQ(serverWorld.turns, last + 1);
	EXPECT_EQ(alice.world.turns, last + 1);
	EXPECT_EQ(bob.world.turns, last + 1);
	EXPECT_EQ(alice.session.StoppedAfter(), last);
	EXPECT_EQ(bob.session.StoppedAfter(), last);
	EXPECT_EQ(alice.world.hash, serverWorld.hash);
	EXPECT_EQ(bob.world.hash, serverWorld.hash);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	// The game ends when the gods leave, with no turn more
	alice.session.Leave();
	bob.session.Leave();
	Frames({&alice, &bob}, 5);
	EXPECT_TRUE(host->GameOver());
	EXPECT_EQ(serverWorld.turns, last + 1);
}

TEST_F(LockstepTest, ANewProcessOfAnAwayGodsPlayerComesBackIntoTheGodWithTheWorldOfTheServer)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	auto bob = std::make_unique<Computer>(network, now, "god:bob");
	Frames({&alice, bob.get()}, 30);
	ASSERT_TRUE(host->Started());
	const auto oldSession = bob->session.GetJoined()->session;

	// Bob's program stops: the god waits for him
	network.Cut(bob->sockets->Ids().back());
	bob.reset();
	Frames({&alice}, 20);
	ASSERT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Away);

	// Bob starts the program again with a new ticket of the same account
	Computer again(network, now, "god:bob");
	Frames({&alice, &again}, 40);
	ASSERT_TRUE(again.session.GetJoined().has_value());
	EXPECT_EQ(again.session.GetJoined()->role, game_socket::Role::God);
	EXPECT_EQ(again.session.GetJoined()->slot, std::optional<uint8_t>(1));
	EXPECT_NE(again.session.GetJoined()->session, oldSession);
	EXPECT_EQ(again.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(again.loads, 1u);
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Taken);
	EXPECT_EQ(host->Seats()[1].resumes, 1u);
	// The god plays on: its commands reach the server, and every turn is the same as the server's
	again.session.Send(HandTap {.object = Entity(5)});
	Frames({&alice, &again}, 20);
	EXPECT_EQ(again.world.hash, serverWorld.hash);
	EXPECT_EQ(again.world.turns, serverWorld.turns);
	EXPECT_GT(again.session.GetCompares().same, 40u);
	EXPECT_EQ(again.session.GetCompares().compared, again.session.GetCompares().same);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(host->Seats()[1].commands.size(), 1u);
}

TEST_F(LockstepTest, ANewProcessReplacesTheOldSocketOfTheSamePlayer)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(host->Started());

	// The old program of Bob still has its socket when the new one joins, as after a reload of the page
	Computer again(network, now, "god:bob");
	Frames({&alice, &bob, &again}, 40);
	EXPECT_EQ(again.session.GetJoined()->slot, std::optional<uint8_t>(1));
	EXPECT_EQ(again.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Taken);
	// The old program cannot take the god back with the secret of its session
	EXPECT_EQ(bob.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(bob.session.RefusedReason(), "resume");
	EXPECT_EQ(again.session.GetCompares().compared, again.session.GetCompares().same);
	EXPECT_GT(again.session.GetCompares().same, 20u);
}

TEST_F(LockstepTest, PlayersWhoJoinAfterTheStartAreObserversWithTheWorldOfTheServer)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 30);
	ASSERT_TRUE(host->Started());

	// A new god's account and an observer come after the start (MP-Q12)
	Computer dave(network, now, "god:dave");
	Computer carol(network, now, "observer:carol");
	Frames({&alice, &dave, &carol}, 30);
	for (const auto* late : {&dave, &carol})
	{
		ASSERT_TRUE(late->session.GetJoined().has_value());
		EXPECT_EQ(late->session.GetJoined()->role, game_socket::Role::Observer);
		EXPECT_FALSE(late->session.GetJoined()->slot.has_value());
		EXPECT_EQ(late->loads, 1u);
		EXPECT_EQ(late->world.turns, serverWorld.turns);
		EXPECT_EQ(late->world.hash, serverWorld.hash);
		EXPECT_EQ(late->session.GetCompares().compared, late->session.GetCompares().same);
	}
	EXPECT_EQ(host->Observers(), 2u);
	EXPECT_EQ(std::ranges::count(host->Seats(), LockstepHost::SeatState::Taken, &LockstepHost::Seat::state), 1);
	// An observer's commands do not reach the server
	dave.session.Send(HandTap {.object = Entity(3)});
	Frames({&alice, &dave, &carol}, 5);
	EXPECT_TRUE(host->Seats()[1].commands.empty());
}

TEST_F(LockstepTest, ThePlayerOfADefeatedGodComesBackAsAnObserver)
{
	MakeHost(2, 2 * 1'000'000);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	auto bob = std::make_unique<Computer>(network, now, "god:bob");
	Frames({&alice, bob.get()}, 10);
	network.Cut(bob->sockets->Ids().back());
	bob.reset();
	Frames({&alice}, 40);
	ASSERT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Closed);

	// The slot of a defeated god stays closed (MP-Q5): its player can only watch
	Computer again(network, now, "god:bob");
	Frames({&alice, &again}, 20);
	ASSERT_TRUE(again.session.GetJoined().has_value());
	EXPECT_EQ(again.session.GetJoined()->role, game_socket::Role::Observer);
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Closed);
	EXPECT_EQ(again.world.hash, serverWorld.hash);
}
