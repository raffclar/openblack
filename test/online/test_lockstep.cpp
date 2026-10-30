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
#include <array>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
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
#include "Server/Desktop/Sha256SessionHasher.h"
#include "Server/LockstepHost.h"
#include "Server/SessionHash.h"

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
	/// The players of the commands that the world did
	std::set<PlayerNames> players;
	/// The taps of the hand that the world did, by the object
	std::map<uint32_t, uint32_t> taps;

	uint64_t Play(uint32_t turn, const std::vector<Command>& commands)
	{
		turns = turn + 1;
		hash = (hash ^ (turn + 1)) * 1099511628211ull;
		for (const auto& command : commands)
		{
			players.insert(command.player);
			if (const auto* tap = std::get_if<HandTap>(&command.action))
			{
				++taps[entt::to_integral(tap->object)];
			}
			hash = (hash ^ (static_cast<uint64_t>(command.player) + 1) * 31 + command.action.index()) * 1099511628211ull;
		}
		return partsAfter.has_value() && turn >= *partsAfter ? hash ^ 0xFF : hash;
	}
};

/// True when the bytes have the text in them
bool HasText(std::span<const uint8_t> bytes, std::string_view text)
{
	return !std::ranges::search(bytes, text, [](uint8_t byte, char letter) {
		        return byte == static_cast<uint8_t>(letter);
	        }).empty();
}

/// The hashes of the secrets of sessions, for the test: each hash has a new salt, and the digest is a FNV-1a hash
class FakeSessionHasher final: public server::SessionHasherInterface
{
public:
	[[nodiscard]] std::optional<server::SessionHash> Hash(std::string_view secret) const override
	{
		server::SessionHash hash;
		hash.salt.fill(++_salts);
		hash.digest = Digest(hash.salt, secret);
		return hash;
	}

	[[nodiscard]] bool Matches(const server::SessionHash& hash, std::string_view secret) const override
	{
		return Digest(hash.salt, secret) == hash.digest;
	}

private:
	[[nodiscard]] static std::array<uint8_t, server::k_SessionDigestBytes> Digest(std::span<const uint8_t> salt,
	                                                                              std::string_view secret)
	{
		uint64_t value = 1469598103934665603ull;
		for (const auto byte : salt)
		{
			value = (value ^ byte) * 1099511628211ull;
		}
		for (const auto letter : secret)
		{
			value = (value ^ static_cast<uint8_t>(letter)) * 1099511628211ull;
		}
		std::array<uint8_t, server::k_SessionDigestBytes> digest {};
		for (size_t i = 0; i < digest.size(); ++i)
		{
			digest[i] = static_cast<uint8_t>(value >> (8 * (i % sizeof(uint64_t))));
		}
		return digest;
	}

	mutable uint8_t _salts {0};
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
	void MakeHost(uint32_t gods, int64_t comeBack = 5LL * 60 * 1'000'000, uint8_t slots = 7, bool testCommands = false)
	{
		host = std::make_unique<LockstepHost>(
		    network, CheckFakeTicket, [this] { return now; }, [this] { return "secret-" + std::to_string(++secrets); },
		    server::HostSettings {
		        .start = {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow", .godsRingMap = false},
		        .landChecksum = online::start_rules::LandChecksum(k_LandFile),
		        .minGods = gods,
		        .godSlots = slots,
		        .comeBackMicroseconds = comeBack,
		        .keptTurns = 3200,
		        .testCommands = testCommands,
		        .sessionHasher = hasher});
	}

	/// A room with no human god (tools): the game starts at once, and the world turns when this many observers joined
	void MakeAllComputerHost(uint32_t observers, int64_t comeBack = 5LL * 60 * 1'000'000)
	{
		game_socket::Start start {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow"};
		start.allComputerGods = true;
		start.aiSeed = 3u;
		host = std::make_unique<LockstepHost>(
		    network, CheckFakeTicket, [this] { return now; }, [this] { return "secret-" + std::to_string(++secrets); },
		    server::HostSettings {.start = std::move(start),
		                          .landChecksum = online::start_rules::LandChecksum(k_LandFile),
		                          .minGods = observers,
		                          .godSlots = 7,
		                          .comeBackMicroseconds = comeBack,
		                          .keptTurns = 3200,
		                          .sessionHasher = hasher});
	}

	/// A room of the tools whose game plays a testbed scenario from the first turn
	void MakeScenarioHost(uint32_t gods)
	{
		game_socket::Start start {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow"};
		start.scenario = "mind.computer_god_rivals";
		host = std::make_unique<LockstepHost>(
		    network, CheckFakeTicket, [this] { return now; }, [this] { return "secret-" + std::to_string(++secrets); },
		    server::HostSettings {.start = std::move(start),
		                          .landChecksum = online::start_rules::LandChecksum(k_LandFile),
		                          .minGods = gods,
		                          .godSlots = 7,
		                          .comeBackMicroseconds = 5LL * 60 * 1'000'000,
		                          .keptTurns = 3200,
		                          .sessionHasher = hasher});
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

	/// The world of the away client parts, and the client drops its socket. The god of the stopper stops the turns before
	/// the away client comes back with the world of the server. Returns the last turn of the game.
	uint32_t StopWhileAClientComesBackWithTheWorld(Computer& stopper, Computer& away)
	{
		away.world.partsAfter = away.world.turns + 2;
		for (int i = 0; i < 20 && away.session.GetState() == NetworkSession::State::Playing; ++i)
		{
			Frame({&stopper, &away});
		}
		EXPECT_EQ(away.session.GetState(), NetworkSession::State::Reconnecting);
		stopper.session.StopGame();
		Frames({&stopper}, 5);
		return stopper.session.StoppedAfter().value_or(0);
	}

	/// The server keeps each turn that it sends in a journal, as the snapshots of the game server do
	void KeepJournal()
	{
		host->SetTurnJournal(
		    [this](uint32_t turn, std::span<const uint8_t> bytes, const server::snapshots::BlockSequences& blocks) {
			    journal[turn].assign(bytes.begin(), bytes.end());
			    journalBlocks[turn] = blocks;
		    });
	}

	/// The server reads its sockets, but closes no turn
	void ServerReads()
	{
		now += k_Turn;
		network.Poll();
		host->Update();
	}

	/// The server reads its sockets and closes a turn, but no computer reads its socket
	void ServerTurn()
	{
		ServerReads();
		ASSERT_TRUE(host->HasTurn(serverWorld.turns));
		const auto turn = serverWorld.turns;
		const auto commands = host->TakeTurn(turn);
		host->TurnChecksum(turn + 1, serverWorld.Play(turn, commands));
	}

	/// The server stops with no warning. A new server continues the game from the room and the world of a snapshot, and
	/// does again the turns of the journal after the snapshot, less the last turns that the journal lost.
	void RestartFrom(const server::snapshots::Room& room, uint32_t turn, const FakeWorld& world,
	                 const std::vector<Computer*>& computers, uint32_t lostTurns = 0,
	                 const server::snapshots::BlockSequences& snapshotBlocks = {})
	{
		for (auto* computer : computers)
		{
			network.Cut(computer->sockets->Ids().back());
		}
		host.reset();
		MakeHost(2);
		GiveWorldState();
		serverWorld = world;
		auto replayed = journal;
		for (uint32_t i = 0; i < lostTurns && !replayed.empty(); ++i)
		{
			replayed.erase(std::prev(replayed.end()));
		}
		const auto savedRoom = server::snapshots::DecodeRoom(server::snapshots::EncodeRoom(room));
		ASSERT_TRUE(savedRoom.has_value());
		// The last blocks of the gods come from the last turn that the journal kept, else from the snapshot
		auto blocks = snapshotBlocks;
		if (const auto last = replayed.empty() ? journalBlocks.end() : journalBlocks.find(replayed.rbegin()->first);
		    last != journalBlocks.end() && last->first >= turn)
		{
			blocks = last->second;
		}
		host->Restore(*savedRoom, turn, std::move(replayed), blocks);
		KeepJournal();
		while (host->Replaying())
		{
			const auto next = serverWorld.turns;
			ASSERT_TRUE(host->HasTurn(next));
			const auto commands = host->TakeTurn(next);
			host->TurnChecksum(next + 1, serverWorld.Play(next, commands));
		}
	}

	std::map<uint32_t, std::vector<uint8_t>> journal;
	std::map<uint32_t, server::snapshots::BlockSequences> journalBlocks;
	std::shared_ptr<const server::SessionHasherInterface> hasher = std::make_shared<FakeSessionHasher>();
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
	// The start names the gods of the game for the chat
	const auto& gods = alice.session.GetStart()->gods;
	ASSERT_EQ(gods.size(), 2);
	EXPECT_EQ(gods[0].slot, 0);
	EXPECT_EQ(gods[1].slot, 1);
	EXPECT_EQ(gods[0].name, alice.session.GetJoined()->name);
	EXPECT_EQ(gods[1].name, bob.session.GetJoined()->name);
	EXPECT_NE(gods[0].name, gods[1].name);
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

TEST_F(LockstepTest, AServerLeavesOutTheFightCheatWithoutItsTestCommands)
{
	MakeHost(1);
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 6);
	alice.session.Send(CreatureFightCheat {});
	Frames({&alice}, 3);
	EXPECT_EQ(host->RefusedBlocks(), 1u);
	EXPECT_EQ(host->Seats()[0].commands.count(static_cast<uint8_t>(turn_codec::CommandTag::CreatureFightCheat)), 0u);
}

TEST_F(LockstepTest, AServerWithItsTestCommandsOnTakesTheFightCheat)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 7, true);
	Computer bob(network, now, "god:bob");
	Frames({&bob}, 6);
	bob.session.Send(CreatureFightCheat {});
	Frames({&bob}, 3);
	EXPECT_EQ(host->RefusedBlocks(), 0u);
	EXPECT_EQ(host->Seats()[0].commands.at(static_cast<uint8_t>(turn_codec::CommandTag::CreatureFightCheat)), 1u);
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

// The state of a testbed scenario is not in the world: every computer plays the scenario from the first turn. An observer
// in the room before the start gets the scenario with the start, and nobody joins the game as it turns.
TEST_F(LockstepTest, TheStartGivesTheScenarioAndNobodyJoinsTheRunningGameOfAScenario)
{
	MakeScenarioHost(1);
	GiveWorldState();
	Computer watcher(network, now, "observer:carol");
	Computer alice(network, now, "god:alice");
	Frames({&watcher, &alice}, 30);
	ASSERT_TRUE(host->Started());
	for (const auto* computer : {&watcher, &alice})
	{
		ASSERT_TRUE(computer->session.GetStart().has_value());
		EXPECT_EQ(computer->session.GetStart()->scenario, "mind.computer_god_rivals");
		EXPECT_GT(computer->world.turns, 10u);
		EXPECT_EQ(computer->session.GetCompares().compared, computer->session.GetCompares().same);
	}

	Computer late(network, now, "observer:dave");
	Frames({&watcher, &alice, &late}, 3);
	EXPECT_EQ(late.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(late.session.RefusedReason(), "started");
	EXPECT_EQ(late.loads, 0u);
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

TEST(LockstepRoom, TheRoomStartsWhenEachGodInItIsReadyAndHasTheFewestGods)
{
	EXPECT_TRUE(server::RoomStarts(1, 1, 1));
	EXPECT_TRUE(server::RoomStarts(2, 2, 1));
	EXPECT_TRUE(server::RoomStarts(2, 2, 2));
	// A god who is not ready holds the start
	EXPECT_FALSE(server::RoomStarts(2, 1, 1));
	// Too few gods
	EXPECT_FALSE(server::RoomStarts(1, 1, 2));
	// An empty room does not start, also with no fewest gods
	EXPECT_FALSE(server::RoomStarts(0, 0, 0));
	EXPECT_TRUE(server::RoomStarts(1, 1, 0));
}

TEST(LockstepRoom, TheRoomHasASlotForEachCitadelUpToTheLastCitadel)
{
	EXPECT_EQ(server::GodSlotsOfLand(std::array {true, true, false, false, false, false, false}), 2);
	EXPECT_EQ(server::GodSlotsOfLand(std::array {true, true, true, true, false, false, false}), 4);
	// A slot with no citadel before the last citadel stays a slot, so that each citadel can have a human
	EXPECT_EQ(server::GodSlotsOfLand(std::array {true, false, true, false, false, false, false}), 3);
	// A land with no citadel keeps the default slots
	EXPECT_EQ(server::GodSlotsOfLand(std::array {false, false, false, false, false, false, false}), server::k_DefaultGodSlots);
	EXPECT_EQ(server::GodSlotsOfLand({}), server::k_DefaultGodSlots);
}

TEST_F(LockstepTest, OneHumanStartsARoomOfTwoSlots)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2);
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 6);
	ASSERT_TRUE(host->Started());
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(host->Seats().size(), 2u);
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Open);
}

TEST_F(LockstepTest, TwoHumansPlayInARoomOfTwoSlotsThatStartsWithOne)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2);
	Computer alice(network, now, "god:alice");
	alice.session.SetReady(false);
	Frames({&alice}, 3);
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 6);
	// Bob is ready, but Alice in the room is not: the start waits for her
	EXPECT_FALSE(host->Started());
	EXPECT_FALSE(host->Starting());
	alice.session.SetReady(true);
	Frames({&alice, &bob}, 6);
	ASSERT_TRUE(host->Started());
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.session.GetJoined()->slot, 1);
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Taken);
}

TEST_F(LockstepTest, AThirdGodIsRefusedFromARoomOfTwoSlotsAndAnObserverIsLetIn)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2);
	Computer alice(network, now, "god:alice");
	alice.session.SetReady(false);
	Computer bob(network, now, "god:bob");
	Computer carol(network, now, "god:carol");
	Computer watcher(network, now, "observer:dave");
	Frames({&alice, &bob, &carol, &watcher}, 3);
	EXPECT_EQ(carol.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(carol.session.RefusedReason(), "full");
	EXPECT_EQ(watcher.session.GetJoined()->role, game_socket::Role::Observer);
	alice.session.SetReady(true);
	Frames({&alice, &bob, &watcher}, 6);
	EXPECT_TRUE(host->Started());
}

TEST_F(LockstepTest, ARoomWithNoHumanGodStartsAtOnceAndTurnsWhenTheObserversJoined)
{
	MakeAllComputerHost(2);
	GiveWorldState();
	Frames({}, 3);
	EXPECT_TRUE(host->Started());
	EXPECT_FALSE(host->Paused());
	EXPECT_FALSE(host->GameOver());
	// No observer yet: the world waits
	EXPECT_FALSE(host->HasTurn(serverWorld.turns));
	EXPECT_EQ(serverWorld.turns, 0u);

	Computer first(network, now, "observer:one");
	Frames({&first}, 20);
	ASSERT_TRUE(first.session.GetJoined().has_value());
	EXPECT_EQ(first.session.GetJoined()->role, game_socket::Role::Observer);
	// The start that the observer gets names the tool modes
	ASSERT_TRUE(first.session.GetJoined()->start.has_value());
	EXPECT_TRUE(first.session.GetJoined()->start->allComputerGods);
	EXPECT_EQ(first.session.GetJoined()->start->aiSeed, std::optional<uint32_t>(3u));
	EXPECT_EQ(serverWorld.turns, 0u);

	Computer second(network, now, "observer:two");
	Frames({&first, &second}, 40);
	EXPECT_EQ(host->Observers(), 2u);
	EXPECT_GT(serverWorld.turns, 0u);
	for (const auto* observer : {&first, &second})
	{
		EXPECT_EQ(observer->world.turns, serverWorld.turns);
		EXPECT_EQ(observer->world.hash, serverWorld.hash);
		EXPECT_EQ(observer->session.GetCompares().compared, observer->session.GetCompares().same);
	}
	EXPECT_EQ(std::ranges::count(host->Seats(), LockstepHost::SeatState::Taken, &LockstepHost::Seat::state), 0);
}

TEST_F(LockstepTest, ARoomWithNoHumanGodAndNoObserverToWaitForTurnsAtOnce)
{
	MakeAllComputerHost(0);
	GiveWorldState();
	Frames({}, 10);
	EXPECT_GT(serverWorld.turns, 0u);
	// With no observer seen yet the room is not over
	EXPECT_FALSE(host->GameOver());
}

TEST_F(LockstepTest, ARoomWithNoHumanGodIsOverWhenTheObserversThatJoinedAllLeft)
{
	MakeAllComputerHost(1);
	GiveWorldState();
	Computer watcher(network, now, "observer:one");
	Frames({&watcher}, 20);
	ASSERT_EQ(host->Observers(), 1u);
	EXPECT_FALSE(host->GameOver());
	// An observer that leaves cannot come back, so the room ends at once
	watcher.session.Leave();
	Frames({&watcher}, 2);
	EXPECT_EQ(host->Observers(), 0u);
	EXPECT_TRUE(host->Watchers().empty());
	EXPECT_TRUE(host->GameOver());
}

TEST_F(LockstepTest, ARoomWithNoHumanGodWaitsForItsLastObserverToComeBack)
{
	MakeAllComputerHost(1);
	GiveWorldState();
	Computer watcher(network, now, "observer:one");
	Frames({&watcher}, 20);
	ASSERT_GT(serverWorld.turns, 0u);

	// The socket of the only observer closes: the room stays open, and the observer gets the turns that it missed
	network.Cut(watcher.sockets->Ids().back());
	Frames({}, 10);
	EXPECT_EQ(host->Observers(), 0u);
	EXPECT_FALSE(host->GameOver());
	Frames({&watcher}, 10);
	EXPECT_EQ(host->Observers(), 1u);
	EXPECT_EQ(watcher.session.GetState(), NetworkSession::State::Playing);

	// The world of the only observer parts: it comes back with the world of the server and stays in step
	watcher.world.partsAfter = watcher.world.turns + 3;
	Frames({&watcher}, 60);
	EXPECT_FALSE(host->GameOver());
	EXPECT_EQ(watcher.loads, 2u);
	EXPECT_EQ(watcher.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(watcher.world.turns, serverWorld.turns);
	EXPECT_EQ(watcher.world.hash, serverWorld.hash);
	ASSERT_EQ(host->Watchers().size(), 1u);
	EXPECT_EQ(host->Watchers()[0].resumes, 2u);
}

TEST_F(LockstepTest, ARoomWithNoHumanGodEndsWhenItsLastObserverDoesNotComeBackInTime)
{
	MakeAllComputerHost(1, 3 * 1'000'000);
	GiveWorldState();
	auto watcher = std::make_unique<Computer>(network, now, "observer:one");
	Frames({watcher.get()}, 20);
	ASSERT_EQ(host->Observers(), 1u);
	network.Cut(watcher->sockets->Ids().back());
	watcher.reset();
	Frames({}, 20);
	EXPECT_FALSE(host->GameOver());
	Frames({}, 20);
	EXPECT_TRUE(host->Watchers().empty());
	EXPECT_TRUE(host->GameOver());
}

TEST_F(LockstepTest, TheRoomRefusesABannedGodBeforeTheStartWithAKick)
{
	MakeHost(3);
	host->SetBans({101});
	Computer alice(network, now, "god:100");
	Computer banned(network, now, "god:101");
	Frames({&alice, &banned}, 3);
	EXPECT_EQ(banned.session.GetState(), NetworkSession::State::Ended);
	EXPECT_EQ(banned.session.RefusedReason(), "kicked");
	EXPECT_EQ(alice.session.GetJoined()->slot, 0);
	EXPECT_TRUE(std::ranges::none_of(host->Seats(), [](const LockstepHost::Seat& seat) { return seat.account == "101"; }));
	// The lobby lifted the ban: the account joins again
	host->SetBans({});
	Computer again(network, now, "god:101");
	Frames({&alice, &again}, 3);
	ASSERT_TRUE(again.session.GetJoined().has_value());
	EXPECT_EQ(again.session.GetJoined()->slot, 1);
}

TEST_F(LockstepTest, ABanTakesAGodOutOfTheRoomBeforeTheStart)
{
	MakeHost(3);
	Computer alice(network, now, "god:100");
	Computer bob(network, now, "god:101");
	Frames({&alice, &bob}, 3);
	ASSERT_EQ(bob.session.GetJoined()->slot, 1);
	host->SetBans({101, 555});
	Frames({&alice, &bob}, 2);
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Ended);
	EXPECT_EQ(bob.session.RefusedReason(), "kicked");
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Open);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Waiting);
	EXPECT_EQ(host->Seats()[0].state, LockstepHost::SeatState::Taken);
}

TEST_F(LockstepTest, ABanDuringTheStartStopsTheStart)
{
	MakeHost(2);
	Computer alice(network, now, "god:100");
	Computer bob(network, now, "god:101");
	for (int i = 0; i < 8 && !host->Starting(); ++i)
	{
		Frame({&alice, &bob});
	}
	ASSERT_TRUE(host->Starting());
	host->SetBans({101});
	Frames({&alice, &bob}, 6);
	EXPECT_FALSE(host->Started());
	EXPECT_FALSE(host->Starting());
	EXPECT_EQ(bob.session.RefusedReason(), "kicked");
	bool stopped = false;
	while (const auto event = alice.session.TakeStartEvent())
	{
		stopped = stopped || *event == online::StartEvent::Stopped;
	}
	EXPECT_TRUE(stopped);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Waiting);
}

TEST_F(LockstepTest, ABanAfterTheStartChangesNothing)
{
	MakeHost(2);
	Computer alice(network, now, "god:100");
	Computer bob(network, now, "god:101");
	Frames({&alice, &bob}, 7);
	ASSERT_TRUE(host->Started());
	host->SetBans({101});
	Frames({&alice, &bob}, 50);
	EXPECT_EQ(host->Bans(), std::set<int64_t> {101});
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Taken);
	EXPECT_GE(bob.world.turns + 3, serverWorld.turns);
	// The banned god stays in step with the server
	EXPECT_GT(host->Seats()[1].compares.compared, 40u);
	EXPECT_EQ(host->Seats()[1].compares.same, host->Seats()[1].compares.compared);
}

TEST_F(LockstepTest, AnObserverThatPartsLoadsTheWorldOfTheServerAndStaysInStep)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Computer watcher(network, now, "observer:carol");
	watcher.world.partsAfter = 20;
	Frames({&alice, &watcher}, 120);
	ASSERT_TRUE(watcher.session.GetJoined().has_value());
	EXPECT_FALSE(watcher.session.GetJoined()->session.empty());
	EXPECT_EQ(watcher.loads, 1u);
	EXPECT_EQ(watcher.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(watcher.session.Resumes(), 1u);
	EXPECT_EQ(watcher.session.Resync().GetState(), online::ResyncRule::State::InStep);
	// After the load every turn is the same as the server's again
	EXPECT_EQ(watcher.world.hash, serverWorld.hash);
	EXPECT_EQ(watcher.world.turns, serverWorld.turns);
	EXPECT_EQ(watcher.session.GetCompares().compared, watcher.session.GetCompares().same + 1);
	ASSERT_EQ(host->Watchers().size(), 1u);
	EXPECT_EQ(host->Watchers()[0].resumes, 1u);
	EXPECT_EQ(host->Observers(), 1u);
	// The observer did not take a god, and the god plays on in step
	EXPECT_EQ(std::ranges::count(host->Seats(), LockstepHost::SeatState::Taken, &LockstepHost::Seat::state), 1);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
}

TEST_F(LockstepTest, AnObserverThatJoinedTheRunningGameComesBackAfterItsWorldParts)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 30);
	ASSERT_TRUE(host->Started());
	Computer first(network, now, "observer:one");
	Computer second(network, now, "observer:two");
	Frames({&alice, &first, &second}, 10);
	ASSERT_EQ(second.loads, 1u);
	// The world of a late joiner comes as a resume, so each observer counts one resume already
	ASSERT_EQ(first.session.Resumes(), 1u);
	ASSERT_EQ(second.session.Resumes(), 1u);
	second.world.partsAfter = second.world.turns + 5;
	Frames({&alice, &first, &second}, 80);
	EXPECT_EQ(second.loads, 2u);
	EXPECT_EQ(second.session.Resumes(), 2u);
	EXPECT_EQ(second.session.GetState(), NetworkSession::State::Playing);
	for (const auto* observer : {&first, &second})
	{
		EXPECT_EQ(observer->world.turns, serverWorld.turns);
		EXPECT_EQ(observer->world.hash, serverWorld.hash);
	}
	EXPECT_EQ(first.session.Resumes(), 1u);
	EXPECT_EQ(first.session.GetCompares().compared, first.session.GetCompares().same);
	EXPECT_EQ(host->Observers(), 2u);
	ASSERT_EQ(host->Watchers().size(), 2u);
	EXPECT_EQ(host->Watchers()[0].resumes, 0u);
	EXPECT_EQ(host->Watchers()[1].resumes, 1u);
}

TEST_F(LockstepTest, AnObserverThatLosesTheSocketComesBackAndGetsTheTurnsThatItMissed)
{
	MakeHost(1);
	Computer alice(network, now, "god:alice");
	Computer watcher(network, now, "observer:carol");
	Frames({&alice, &watcher}, 30);
	ASSERT_TRUE(host->Started());

	network.Cut(watcher.sockets->Ids().back());
	Frames({&alice}, 1);
	watcher.Frame();
	EXPECT_EQ(watcher.session.GetState(), NetworkSession::State::Reconnecting);
	Frames({&alice}, 10);
	EXPECT_EQ(host->Observers(), 0u);
	ASSERT_EQ(host->Watchers().size(), 1u);
	EXPECT_FALSE(host->Watchers()[0].connection.has_value());
	const auto turnsWhileAway = serverWorld.turns;

	Frames({&alice, &watcher}, 10);
	EXPECT_EQ(watcher.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(watcher.session.Resumes(), 1u);
	EXPECT_EQ(watcher.loads, 0u);
	EXPECT_EQ(host->Observers(), 1u);
	EXPECT_EQ(host->Watchers()[0].resumes, 1u);
	// The observer did the turns that it missed, and each of its checksums is the same as the server's
	EXPECT_GE(watcher.world.turns, turnsWhileAway);
	EXPECT_GT(watcher.session.GetCompares().same, 35u);
	EXPECT_EQ(watcher.session.GetCompares().compared, watcher.session.GetCompares().same);
	// The observer is not a god: the server takes no command from it
	watcher.session.Send(HandTap {.object = Entity(4)});
	Frames({&alice, &watcher}, 5);
	EXPECT_TRUE(host->Seats()[1].commands.empty());
}

TEST_F(LockstepTest, AnObserverThatDoesNotComeBackInTimeLosesItsSession)
{
	MakeHost(1, 3 * 1'000'000);
	Computer alice(network, now, "god:alice");
	auto watcher = std::make_unique<Computer>(network, now, "observer:carol");
	Frames({&alice, watcher.get()}, 10);
	ASSERT_EQ(host->Watchers().size(), 1u);
	network.Cut(watcher->sockets->Ids().back());
	watcher.reset();
	Frames({&alice}, 20);
	ASSERT_EQ(host->Watchers().size(), 1u);
	Frames({&alice}, 30);
	EXPECT_TRUE(host->Watchers().empty());
}

TEST_F(LockstepTest, AnObserverThatLeavesOrPartsBeforeTheStartCannotComeBack)
{
	MakeHost(2);
	Computer alice(network, now, "god:alice");
	Computer early(network, now, "observer:early");
	Computer leaver(network, now, "observer:leaver");
	Frames({&alice, &early, &leaver}, 5);
	ASSERT_FALSE(host->Started());
	ASSERT_EQ(host->Watchers().size(), 2u);
	// Before the start, an observer that loses its socket has no session to come back to
	network.Cut(early.sockets->Ids().back());
	leaver.session.Leave();
	Frames({&alice, &early, &leaver}, 10);
	EXPECT_TRUE(host->Watchers().empty());
	EXPECT_EQ(early.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(early.session.RefusedReason(), "resume");
}

TEST_F(LockstepTest, TheSessionOfAnObserverDoesNotTakeAGod)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	auto bob = std::make_unique<Computer>(network, now, "god:bob");
	Computer watcher(network, now, "observer:carol");
	Frames({&alice, bob.get(), &watcher}, 30);
	ASSERT_TRUE(host->Started());
	// Bob is away, and the observer comes back while the god of slot 1 waits
	network.Cut(bob->sockets->Ids().back());
	bob.reset();
	network.Cut(watcher.sockets->Ids().back());
	Frames({&alice, &watcher}, 20);
	EXPECT_EQ(watcher.session.Resumes(), 1u);
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Away);
	EXPECT_EQ(host->Seats()[1].resumes, 0u);
	EXPECT_FALSE(watcher.session.GetJoined()->slot.has_value());
	EXPECT_EQ(watcher.session.GetJoined()->role, game_socket::Role::Observer);
}

TEST_F(LockstepTest, AGodThatComesBackWithTheWorldAfterTheStopGetsTheStop)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(host->Started());
	const auto last = StopWhileAClientComesBackWithTheWorld(alice, bob);
	ASSERT_EQ(host->StoppedAfter(), std::optional<uint32_t>(last));
	EXPECT_FALSE(bob.session.StoppedAfter().has_value());
	Frames({&alice, &bob}, 20);
	EXPECT_EQ(bob.loads, 1u);
	EXPECT_EQ(bob.session.StoppedAfter(), std::optional<uint32_t>(last));
	EXPECT_EQ(bob.world.turns, last + 1);
	EXPECT_EQ(bob.world.hash, serverWorld.hash);
	EXPECT_EQ(serverWorld.turns, last + 1);
}

TEST_F(LockstepTest, AnObserverThatComesBackWithTheWorldAfterTheStopGetsTheStop)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Computer watcher(network, now, "observer:carol");
	Frames({&alice, &watcher}, 30);
	ASSERT_TRUE(host->Started());
	const auto last = StopWhileAClientComesBackWithTheWorld(alice, watcher);
	ASSERT_EQ(host->StoppedAfter(), std::optional<uint32_t>(last));
	EXPECT_FALSE(watcher.session.StoppedAfter().has_value());
	Frames({&alice, &watcher}, 20);
	EXPECT_EQ(watcher.loads, 1u);
	EXPECT_EQ(watcher.session.StoppedAfter(), std::optional<uint32_t>(last));
	EXPECT_EQ(watcher.world.turns, last + 1);
	EXPECT_EQ(watcher.world.hash, serverWorld.hash);
}

TEST_F(LockstepTest, AnObserverThatJoinsAfterTheStopGetsTheStopWithTheWorld)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 30);
	alice.session.StopGame();
	Frames({&alice}, 5);
	ASSERT_TRUE(host->StoppedAfter().has_value());
	Computer late(network, now, "observer:late");
	Frames({&alice, &late}, 10);
	EXPECT_EQ(late.loads, 1u);
	EXPECT_EQ(late.session.StoppedAfter(), host->StoppedAfter());
	EXPECT_EQ(late.world.turns, serverWorld.turns);
}

TEST_F(LockstepTest, TheGodsAfterTheSeventhTakeTheNumbersAfterTheNeutralPlayer)
{
	constexpr uint8_t k_Gods = 9;
	MakeHost(k_Gods, 5LL * 60 * 1'000'000, k_Gods);
	std::vector<std::unique_ptr<Computer>> gods;
	std::vector<Computer*> computers;
	for (uint8_t i = 0; i < k_Gods; ++i)
	{
		gods.push_back(std::make_unique<Computer>(network, now, "god:god" + std::to_string(i), k_LandFile,
		                                          std::vector<uint8_t> {static_cast<uint8_t>('a' + i)}));
		computers.push_back(gods.back().get());
	}
	Frames(computers, 12);
	ASSERT_TRUE(host->Started());
	// The first 7 gods have the numbers of the gods of a land; the eighth and the ninth come after the neutral player
	EXPECT_EQ(gods[0]->session.OwnGod(), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(gods[6]->session.OwnGod(), PlayerNames::PLAYER_SEVEN);
	EXPECT_EQ(gods[7]->session.OwnGod(), static_cast<PlayerNames>(8));
	EXPECT_EQ(gods[8]->session.OwnGod(), static_cast<PlayerNames>(9));
	for (const player_commands::TurnCommandSourceInterface* source :
	     std::initializer_list<const player_commands::TurnCommandSourceInterface*> {host.get(), &gods[0]->session})
	{
		const auto eighth = source->StartCreatureFile(static_cast<PlayerNames>(8));
		EXPECT_EQ(std::vector<uint8_t>(eighth.begin(), eighth.end()), (std::vector<uint8_t> {'h'}));
		EXPECT_TRUE(source->StartCreatureFile(PlayerNames::NEUTRAL).empty());
	}
	// The joins of the first turn come from each god's own number, never from the neutral player
	EXPECT_TRUE(serverWorld.players.contains(static_cast<PlayerNames>(8)));
	EXPECT_TRUE(serverWorld.players.contains(static_cast<PlayerNames>(9)));
	EXPECT_FALSE(serverWorld.players.contains(PlayerNames::NEUTRAL));
	EXPECT_EQ(gods[8]->world.players, serverWorld.players);
}

TEST_F(LockstepTest, AServerThatRestartsFromItsSnapshotGetsItsPlayersBackWithNoStepBack)
{
	hasher = std::make_shared<server::Sha256SessionHasher>();
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(host->Started());
	// The snapshot of the server, then more turns with a command of a player
	const auto room = host->RoomRecord();
	const auto snapshotTurn = host->TurnsDone();
	const auto snapshotWorld = serverWorld;
	// The snapshot file keeps the hashes of the secrets, not the secrets
	const auto file = server::snapshots::EncodeSnapshot({.turn = snapshotTurn, .room = room, .world = {}, .blocks = {}});
	EXPECT_EQ(secrets, 2);
	EXPECT_FALSE(HasText(file, "secret-"));
	for (const auto& seat : host->Seats())
	{
		EXPECT_TRUE(seat.session.empty() || !HasText(file, seat.session));
	}
	alice.session.Send(HandTap {.object = Entity(5)});
	Frames({&alice, &bob}, 20);
	const auto lastTurn = host->TurnsDone();
	const auto lastHash = serverWorld.hash;

	RestartFrom(room, snapshotTurn, snapshotWorld, {&alice, &bob});
	// The new server did again every turn that the old server sent, with the same commands
	EXPECT_EQ(host->TurnsDone(), lastTurn);
	EXPECT_EQ(serverWorld.hash, lastHash);
	EXPECT_EQ(host->Seats()[0].state, LockstepHost::SeatState::Away);
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Away);
	EXPECT_FALSE(host->HasTurn(lastTurn));

	// The players come back with the secrets of their sessions and get only the turns that they missed
	Frames({&alice, &bob}, 40);
	for (const auto* computer : {&alice, &bob})
	{
		EXPECT_EQ(computer->session.GetState(), NetworkSession::State::Playing);
		EXPECT_EQ(computer->session.Resumes(), 1u);
		EXPECT_EQ(computer->loads, 0u);
		EXPECT_EQ(computer->world.hash, serverWorld.hash);
		EXPECT_EQ(computer->world.turns, serverWorld.turns);
		EXPECT_EQ(computer->session.GetCompares().compared, computer->session.GetCompares().same);
	}
	EXPECT_EQ(host->Seats()[0].state, LockstepHost::SeatState::Taken);
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Taken);
	EXPECT_GT(serverWorld.turns, lastTurn + 20);
}

TEST_F(LockstepTest, AfterARestartAPlayerWithTheWrongSecretIsRefused)
{
	hasher = std::make_shared<server::Sha256SessionHasher>();
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(host->Started());
	auto room = host->RoomRecord();
	const auto snapshotTurn = host->TurnsDone();
	const auto snapshotWorld = serverWorld;
	Frames({&alice, &bob}, 10);

	// The slot of Bob keeps the hash of another secret, so the secret of Bob does not match it
	room.seats[1].session = hasher->Hash("another-secret");
	RestartFrom(room, snapshotTurn, snapshotWorld, {&alice, &bob});
	Frames({&alice, &bob}, 40);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(host->Seats()[0].state, LockstepHost::SeatState::Taken);
	EXPECT_EQ(bob.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(bob.session.RefusedReason(), "resume");
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Away);
}

TEST_F(LockstepTest, AClientAheadOfTheRestoredServerGetsTheWorldOfTheServer)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(host->Started());
	const auto room = host->RoomRecord();
	const auto snapshotTurn = host->TurnsDone();
	const auto snapshotWorld = serverWorld;
	Frames({&alice, &bob}, 20);
	const auto lastTurn = host->TurnsDone();

	// The journal lost its last turns, so the clients are at turns after the turn of the new server
	RestartFrom(room, snapshotTurn, snapshotWorld, {&alice, &bob}, 5);
	EXPECT_EQ(host->TurnsDone(), lastTurn - 5);
	Frames({&alice, &bob}, 40);
	for (const auto* computer : {&alice, &bob})
	{
		EXPECT_EQ(computer->session.GetState(), NetworkSession::State::Playing);
		EXPECT_EQ(computer->loads, 1u);
		EXPECT_EQ(computer->world.hash, serverWorld.hash);
		EXPECT_EQ(computer->world.turns, serverWorld.turns);
	}
}

TEST_F(LockstepTest, ACommandThatTheServerHadInNoTurnAtItsStopComesToTheNewServerOnce)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(host->Started());
	const auto room = host->RoomRecord();
	const auto snapshotTurn = host->TurnsDone();
	const auto snapshotWorld = serverWorld;
	const auto snapshotBlocks = host->BlocksInTurns();
	alice.session.Send(HandTap {.object = Entity(5)});
	Frames({&alice, &bob}, 20);
	EXPECT_EQ(serverWorld.taps[5], 1u);
	EXPECT_TRUE(alice.session.Sent().Kept().empty());

	// The server gets a second tap, then stops before a turn holds it
	alice.session.Send(HandTap {.object = Entity(6)});
	ServerReads();
	ASSERT_EQ(alice.session.Sent().Kept().size(), 1u);
	RestartFrom(room, snapshotTurn, snapshotWorld, {&alice, &bob}, 0, snapshotBlocks);
	EXPECT_EQ(serverWorld.taps[6], 0u);

	// Alice comes back and sends the tap again. Each world does each tap one time.
	Frames({&alice, &bob}, 40);
	EXPECT_EQ(alice.session.Resumes(), 1u);
	EXPECT_TRUE(alice.session.Sent().Kept().empty());
	EXPECT_EQ(host->Seats()[0].blocksAgain, 0u);
	for (const auto* world : {&serverWorld, &alice.world, &bob.world})
	{
		EXPECT_EQ(world->taps.at(5), 1u);
		EXPECT_EQ(world->taps.at(6), 1u);
	}
	EXPECT_EQ(alice.world.hash, serverWorld.hash);
	EXPECT_EQ(bob.world.hash, serverWorld.hash);
}

TEST_F(LockstepTest, ACommandInASentTurnThatTheClientDidNotGetIsNotDoneTwiceAfterARestart)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(host->Started());
	const auto room = host->RoomRecord();
	const auto snapshotTurn = host->TurnsDone();
	const auto snapshotWorld = serverWorld;
	const auto snapshotBlocks = host->BlocksInTurns();
	Frames({&alice, &bob}, 5);

	// The server puts the tap in a turn and writes the turn in its journal. It stops before Alice gets the turn.
	alice.session.Send(HandTap {.object = Entity(8)});
	ServerTurn();
	EXPECT_EQ(serverWorld.taps[8], 1u);
	RestartFrom(room, snapshotTurn, snapshotWorld, {&alice, &bob}, 0, snapshotBlocks);
	EXPECT_EQ(serverWorld.taps[8], 1u);

	// Alice sends the tap again, because no turn that she got holds it. The new server had it in its sent turns.
	Frames({&alice, &bob}, 40);
	EXPECT_EQ(host->Seats()[0].blocksAgain, 1u);
	EXPECT_TRUE(alice.session.Sent().Kept().empty());
	for (const auto* world : {&serverWorld, &alice.world, &bob.world})
	{
		EXPECT_EQ(world->taps.at(8), 1u);
	}
	EXPECT_EQ(alice.world.hash, serverWorld.hash);
}

TEST_F(LockstepTest, AClientThatLostItsSocketSendsAgainTheBlocksThatNoTurnHeldAndTheServerTakesThemOnce)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(host->Started());

	// The server gets the tap, then the socket of Alice stops before the turn comes to her
	alice.session.Send(HandTap {.object = Entity(4)});
	ServerReads();
	network.Cut(alice.sockets->Ids().back());
	Frames({&alice, &bob}, 40);
	EXPECT_EQ(alice.session.Resumes(), 1u);
	EXPECT_EQ(host->Seats()[0].blocksAgain, 1u);
	EXPECT_TRUE(alice.session.Sent().Kept().empty());
	for (const auto* world : {&serverWorld, &alice.world, &bob.world})
	{
		EXPECT_EQ(world->taps.at(4), 1u);
	}
	EXPECT_EQ(alice.world.hash, serverWorld.hash);
}

TEST_F(LockstepTest, ANewProcessOfAPlayerCountsItsBlocksFromTheStartAgain)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	auto bob = std::make_unique<Computer>(network, now, "god:bob");
	Frames({&alice, bob.get()}, 30);
	ASSERT_TRUE(host->Started());
	for (uint32_t object = 1; object <= 3; ++object)
	{
		bob->session.Send(HandTap {.object = Entity(object)});
		Frames({&alice, bob.get()}, 2);
	}
	EXPECT_EQ(host->Seats()[1].lastBlock, std::optional<uint16_t>(2));

	// Bob's program stops. He starts it again with a new ticket, and its first block has the number 0.
	network.Cut(bob->sockets->Ids().back());
	bob.reset();
	Frames({&alice}, 5);
	Computer again(network, now, "god:bob");
	Frames({&alice, &again}, 40);
	ASSERT_EQ(again.session.GetState(), NetworkSession::State::Playing);
	again.session.Send(HandTap {.object = Entity(9)});
	Frames({&alice, &again}, 10);
	EXPECT_EQ(host->Seats()[1].blocksAgain, 0u);
	EXPECT_EQ(host->Seats()[1].lastBlock, std::optional<uint16_t>(0));
	EXPECT_EQ(serverWorld.taps[9], 1u);
	EXPECT_EQ(alice.world.taps[9], 1u);
}

TEST_F(LockstepTest, AGodWhoseProgramStoppedWithTheServerComesBackWithANewTicket)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(network, now, "god:alice");
	auto bob = std::make_unique<Computer>(network, now, "god:bob");
	Frames({&alice, bob.get()}, 30);
	ASSERT_TRUE(host->Started());
	const auto room = host->RoomRecord();
	const auto snapshotTurn = host->TurnsDone();
	const auto snapshotWorld = serverWorld;
	Frames({&alice, bob.get()}, 10);

	// Bob's program stopped with the server. He starts it again with a new ticket of the same account.
	network.Cut(bob->sockets->Ids().back());
	bob.reset();
	RestartFrom(room, snapshotTurn, snapshotWorld, {&alice});
	Computer again(network, now, "god:bob");
	Frames({&alice, &again}, 40);
	ASSERT_TRUE(again.session.GetJoined().has_value());
	EXPECT_EQ(again.session.GetJoined()->slot, std::optional<uint8_t>(1));
	EXPECT_EQ(again.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(again.world.hash, serverWorld.hash);
	// Alice came back with the secret of her session and stays in step with the server
	EXPECT_GT(alice.session.GetCompares().same, 60u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(alice.loads, 0u);
	EXPECT_EQ(alice.session.Resumes(), 1u);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Taken);
}
