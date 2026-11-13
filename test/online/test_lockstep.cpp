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
#include <functional>
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
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

#include "Commands/TurnCodec.h"
#include "LoopbackSockets.h"
#include "Online/ConnectionLostNotice.h"
#include "Online/NetworkSession.h"
#include "Online/StartRules.h"
#include "Server/Desktop/Sha256SessionHasher.h"
#include "Server/LockstepHost.h"
#include "Server/ServerAdmin.h"
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
	/// The commands that bring the god of the observers into the game and take it out, in the order of the turns
	std::vector<Command> observersGod;
	/// The commands of the observers and the commands of their comes and goes, in the order of the turns
	std::vector<Command> observerCommands;

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
			if (std::holds_alternative<ObserversJoined>(command.action) ||
			    std::holds_alternative<ObserversLeft>(command.action))
			{
				observersGod.push_back(command);
			}
			if (command.observer != 0)
			{
				observerCommands.push_back(command);
			}
			hash = (hash ^ (static_cast<uint64_t>(command.player) + 1) * 31 + command.action.index()) * 1099511628211ull;
			hash = (hash ^ command.observer) * 1099511628211ull;
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
	         float alignment = 0.0f, std::string name = {}, std::string build = {}, std::string map = {},
	         int64_t loadDelayMicroseconds = 0)
	    : sockets(network.MakeClient())
	    , session(*sockets, [&now] { return now; })
	{
		session.Join(online::NetworkSessionSettings {
		    .request = {.url = "wss://127.0.0.1/game/1", .headers = {}, .subprotocols = {}, .serverKey = std::nullopt},
		    .ticket = std::move(ticket),
		    .gamePassword = {},
		    .name = std::move(name),
		    .build = std::move(build),
		    .map = std::move(map),
		    .comeBackMicroseconds = 5LL * 60 * 1'000'000,
		    .readLand = [this](std::string_view land) -> std::optional<std::vector<uint8_t>> {
			    askedLand = std::string(land);
			    return this->landFile;
		    },
		    .readCreature = [this] { return this->creature; },
		    .readAlignment = [alignment] { return alignment; },
		    .localParts = [this](uint32_t turn) { return parts ? parts(turn) : online::PartHashes {}; },
		    .testLoadDelayMicroseconds = loadDelayMicroseconds,
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
	/// The parts of the world of this computer at a turn, for the log of a part. None by default.
	std::function<online::PartHashes(uint32_t turn)> parts;
	NetworkSession session;
	FakeWorld world;
	std::vector<size_t> played;
	uint32_t loads {0};
};

server::TicketCheck::Result CheckFakeTicket(std::string_view token)
{
	// "god:name" or "observer:name", with "@lobby" after the name for a ticket of a lobby's game; anything else is not a
	// ticket
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
	auto subject = token.substr(colon + 1);
	if (const auto at = subject.find('@'); at != std::string_view::npos)
	{
		claims.lobby = std::string(subject.substr(at + 1));
		subject = subject.substr(0, at);
	}
	claims.subject = std::string(subject);
	claims.name = claims.subject;
	claims.role = *role;
	return {.problem = server::TicketProblem::None, .claims = claims};
}

/// The game server of the test, with its own world
class LockstepTest: public ::testing::Test
{
protected:
	void MakeHost(uint32_t gods, int64_t comeBack = 5LL * 60 * 1'000'000, uint8_t slots = 7, bool testCommands = false,
	              bool lan = false)
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
		        .sessionHasher = hasher,
		        .lanNames = lan});
	}

	/// The room of the game of one lobby: the members of the lobby, the build of the server and the SHA-256 of its land
	void MakeLobbyHost(std::vector<lobby_members::Member> members, uint32_t minGods, std::string build = "0.1.0+a/codec1",
	                   std::string map = {}, int64_t loadWindow = 120LL * 1'000'000)
	{
		host = std::make_unique<LockstepHost>(
		    network, CheckFakeTicket, [this] { return now; }, [this] { return "secret-" + std::to_string(++secrets); },
		    server::HostSettings {
		        .start = {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow", .godsRingMap = false},
		        .landChecksum = online::start_rules::LandChecksum(k_LandFile),
		        .minGods = minGods,
		        .godSlots = 4,
		        .comeBackMicroseconds = 5LL * 60 * 1'000'000,
		        .keptTurns = 3200,
		        .testCommands = false,
		        .sessionHasher = hasher,
		        .lanNames = false,
		        .mostObservers = server::k_MostObservers,
		        .relaysCreature = {},
		        .lobby = server::lobby_game::Lobby {.id = "lobby1", .members = std::move(members)},
		        .buildId = std::move(build),
		        .mapSha256 = std::move(map),
		        .loadWindowMicroseconds = loadWindow});
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

// A changed client can send the commands of a single player game in a network game. A real client never sends them, so
// the server leaves them out of its turns, and the other commands of the block stay.
TEST_F(LockstepTest, TheServerLeavesOutTheCommandsOfASinglePlayerGame)
{
	MakeHost(1);
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 6);
	alice.session.Send(QuickSave {});
	alice.session.Send(QuickLoad {});
	alice.session.Send(MusicMood {.creature = Entity(3), .track = 9, .mood = 0.5f});
	alice.session.Send(CreatureTattoo {.creature = Entity(3)});
	alice.session.Send(HandTap {.object = Entity(2)});
	Frames({&alice}, 3);
	EXPECT_EQ(host->RefusedBlocks(), 4u);
	const auto& commands = host->Seats()[0].commands;
	for (const auto tag : {turn_codec::CommandTag::QuickSave, turn_codec::CommandTag::QuickLoad,
	                       turn_codec::CommandTag::MusicMood, turn_codec::CommandTag::CreatureTattoo})
	{
		EXPECT_EQ(commands.count(static_cast<uint8_t>(tag)), 0u);
	}
	EXPECT_EQ(commands.at(static_cast<uint8_t>(turn_codec::CommandTag::HandTap)), 1u);
}

TEST(LockstepRules, TheServerLeavesOutOnlyTheCommandsThatAClientNeverSends)
{
	EXPECT_TRUE(server::LeavesOut(PlayerJoined {}, true));
	EXPECT_TRUE(server::LeavesOut(PlayerLeft {}, true));
	EXPECT_TRUE(server::LeavesOut(QuickSave {}, true));
	EXPECT_TRUE(server::LeavesOut(QuickLoad {}, true));
	EXPECT_TRUE(server::LeavesOut(MusicMood {}, true));
	EXPECT_TRUE(server::LeavesOut(CreatureTattoo {}, true));
	EXPECT_TRUE(server::LeavesOut(CreatureFightCheat {}, false));
	EXPECT_FALSE(server::LeavesOut(CreatureFightCheat {}, true));
	EXPECT_FALSE(server::LeavesOut(HandTap {}, false));
	EXPECT_FALSE(server::LeavesOut(TotemShare {}, false));
}

// A changed client can send bytes that are not a creature file. The server sends empty data in their place, so no
// computer reads them, and each computer brings no creature for that god, as it does for a file that does not read.
TEST_F(LockstepTest, TheServerSendsNoCreatureFileForAFileThatDoesNotRead)
{
	host = std::make_unique<LockstepHost>(
	    network, CheckFakeTicket, [this] { return now; }, [this] { return "secret-" + std::to_string(++secrets); },
	    server::HostSettings {
	        .start = {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow", .godsRingMap = false},
	        .landChecksum = online::start_rules::LandChecksum(k_LandFile),
	        .minGods = 2,
	        .godSlots = 7,
	        .comeBackMicroseconds = 5LL * 60 * 1'000'000,
	        .keptTurns = 3200,
	        .testCommands = false,
	        .sessionHasher = hasher,
	        .lanNames = false,
	        .relaysCreature = [](std::span<const uint8_t> data) { return data.empty() || data.front() == 'C'; }});
	Computer alice(network, now, "god:alice", k_LandFile, {'C', 'R'}, 0.0f);
	Computer bob(network, now, "god:bob", k_LandFile, {'X', 'X'}, 0.0f);
	Frames({&alice, &bob}, 8);
	ASSERT_TRUE(host->Started());
	for (const player_commands::TurnCommandSourceInterface* source :
	     std::initializer_list<const player_commands::TurnCommandSourceInterface*> {host.get(), &alice.session, &bob.session})
	{
		const auto first = source->StartCreatureFile(PlayerNames::PLAYER_ONE);
		EXPECT_EQ(std::vector<uint8_t>(first.begin(), first.end()), (std::vector<uint8_t> {'C', 'R'}));
		EXPECT_TRUE(source->StartCreatureFile(PlayerNames::PLAYER_TWO).empty());
	}
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

TEST_F(LockstepTest, AClientThatPartsSendsItsPartsAndBothLogsNameThePartThatDiffers)
{
	// The server's log goes to a sink that the test reads
	auto sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(256);
	auto logger = std::make_shared<spdlog::logger>("game", sink);
	spdlog::drop("game");
	spdlog::register_logger(logger);
	MakeHost(1);
	GiveWorldState();
	// The two worlds differ only in the part "Transform" from the turn of the part on
	host->SetPartsOfTurn(
	    [](uint32_t turn) { return online::PartHashes {{"entities", 1}, {"Transform", 10 + turn}, {"synced seed", 7}}; });
	Computer alice(network, now, "god:alice");
	// The world of the computer keeps the hashes of its parts as they were at each turn, also after it loads the server's
	// world
	constexpr uint32_t k_PartsAfter = 20;
	alice.parts = [](uint32_t turn) {
		const bool parted = turn > k_PartsAfter;
		return online::PartHashes {{"entities", 1}, {"Transform", 10 + turn + (parted ? 1 : 0)}, {"synced seed", 7}};
	};
	alice.world.partsAfter = k_PartsAfter;
	Frames({&alice}, 60);
	spdlog::drop("game");

	const auto serverLines = sink->last_formatted();
	const auto serverPart = std::ranges::find_if(serverLines, [](const std::string& line) {
		return line.find("parted at turn") != std::string::npos && line.find("1 parts differ: Transform") != std::string::npos;
	});
	ASSERT_NE(serverPart, serverLines.end());
	EXPECT_NE(serverPart->find("(alice, slot 0)"), std::string::npos) << *serverPart;
	// The checksums of the part are in the line of the compare
	EXPECT_TRUE(std::ranges::any_of(serverLines, [](const std::string& line) {
		return line.find("parts from the server's world at turn") != std::string::npos &&
		       line.find("client ") != std::string::npos;
	}));
	const auto& lines = alice.session.Log().Lines();
	EXPECT_TRUE(std::ranges::any_of(lines, [](const std::string& line) {
		return line.find("The worlds parted at turn") != std::string::npos &&
		       line.find("1 parts differ: Transform (this ") != std::string::npos;
	}));
	EXPECT_TRUE(std::ranges::any_of(
	    lines, [](const std::string& line) { return line.find("The client asks to resume from turn") != std::string::npos; }));
	EXPECT_EQ(alice.loads, 1u);
}

TEST_F(LockstepTest, TheServerLogTellsWhyItRefusesAResume)
{
	auto sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(64);
	auto logger = std::make_shared<spdlog::logger>("game", sink);
	spdlog::drop("game");
	spdlog::register_logger(logger);
	MakeHost(1);
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 5);
	auto client = network.MakeClient();
	const auto id =
	    client->Connect({.url = "wss://127.0.0.1/game/1", .headers = {}, .subprotocols = {}, .serverKey = std::nullopt}, {});
	Frames({&alice}, 1);
	client->Poll();
	client->SendText(id, game_socket::ToJson(game_socket::Resume {.session = "not-a-secret", .turn = 0, .world = false}));
	Frames({&alice}, 2);
	spdlog::drop("game");
	const auto lines = sink->last_formatted();
	EXPECT_TRUE(std::ranges::any_of(lines, [](const std::string& line) {
		return line.find("refuses connection") != std::string::npos &&
		       line.find("resume: no god or observer") != std::string::npos;
	}));
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

TEST(LockstepRules, ALanNameHasNoControlCharacterAndAMostLength)
{
	const auto claims = server::LanClaims("Alice");
	ASSERT_TRUE(claims.has_value());
	EXPECT_EQ(claims->name, "Alice");
	EXPECT_EQ(claims->subject, "lan:Alice");
	EXPECT_EQ(claims->role, game_socket::Role::God);
	EXPECT_FALSE(server::LanClaims("").has_value());
	EXPECT_FALSE(server::LanClaims("Al\x01ice").has_value());
	EXPECT_TRUE(server::LanClaims(std::string(server::k_MostLanNameBytes, 'a')).has_value());
	EXPECT_FALSE(server::LanClaims(std::string(server::k_MostLanNameBytes + 1, 'a')).has_value());
}

TEST_F(LockstepTest, OnTheLocalNetworkPlayersJoinByNameAndAFullRoomTakesAnObserver)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2, false, true);
	Computer alice(network, now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	alice.session.SetReady(false);
	Computer bob(network, now, "", k_LandFile, {'C'}, 0.0f, "Bob");
	Computer again(network, now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	Computer carol(network, now, "", k_LandFile, {'C'}, 0.0f, "Carol");
	Computer nameless(network, now, "", k_LandFile, {'C'}, 0.0f, "");
	Frames({&alice, &bob, &again, &carol, &nameless}, 3);
	ASSERT_TRUE(alice.session.GetJoined().has_value());
	EXPECT_EQ(alice.session.GetJoined()->name, "Alice");
	EXPECT_EQ(alice.session.GetJoined()->slot, 0);
	EXPECT_EQ(bob.session.GetJoined()->slot, 1);
	EXPECT_EQ(host->Seats()[0].name, "Alice");
	// A second player with the name of a god in the room is refused, as a second player of an account
	EXPECT_EQ(again.session.RefusedReason(), "already");
	// With no free slot, a player on the local network watches the game
	ASSERT_TRUE(carol.session.GetJoined().has_value());
	EXPECT_EQ(carol.session.GetJoined()->role, game_socket::Role::Observer);
	EXPECT_EQ(nameless.session.RefusedReason(), "ticket");
	alice.session.SetReady(true);
	Frames({&alice, &bob, &carol}, 6);
	EXPECT_TRUE(host->Started());
}

TEST_F(LockstepTest, ARoomOfTheLobbyRefusesAJoinWithANameAndNoTicket)
{
	MakeHost(1);
	Computer alice(network, now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	Frames({&alice}, 3);
	EXPECT_EQ(alice.session.RefusedReason(), "ticket");
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

TEST_F(LockstepTest, AnAwayGodThatTheLobbyKicksCannotComeBack)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(network, now, "god:100");
	Computer bob(network, now, "god:101");
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(host->Started());
	// Bob's network stops, then the lobby's host kicks him while his god waits
	network.Cut(bob.sockets->Ids().back());
	Frames({&alice}, 1);
	bob.Frame();
	Frames({&alice}, 3);
	ASSERT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Away);
	host->SetBans({101});
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Closed);
	// Bob's session tries to come back, and the server refuses it
	Frames({&alice, &bob}, 10);
	EXPECT_NE(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Closed);
	EXPECT_EQ(host->Seats()[1].resumes, 0u);
}

TEST_F(LockstepTest, ABanAfterTheStartTakesThePlayerOutOfTheGame)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(network, now, "god:100");
	Computer bob(network, now, "god:101");
	Computer carol(network, now, "observer:102");
	Frames({&alice, &bob, &carol}, 7);
	ASSERT_TRUE(host->Started());
	Frames({&alice, &bob, &carol}, 10);
	// The lobby's host kicks the god of bob and the observer carol
	host->SetBans({101, 102});
	Frames({&alice, &bob, &carol}, 3);
	EXPECT_EQ(host->Bans(), (std::set<int64_t> {101, 102}));
	// The server closes the sockets at once with "kicked", and the god leaves as a player who leaves
	EXPECT_EQ(bob.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(bob.session.RefusedReason(), "kicked");
	EXPECT_EQ(carol.session.RefusedReason(), "kicked");
	EXPECT_EQ(host->Seats()[1].state, LockstepHost::SeatState::Closed);
	EXPECT_EQ(host->Observers(), 0u);
	// The other god plays on
	Frames({&alice}, 10);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_FALSE(host->GameOver());

	// A new join of the kicked account gets "kicked" in every role, and the old session cannot come back
	Computer again(network, now, "god:101");
	Computer watch(network, now, "observer:101");
	Frames({&alice, &again, &watch}, 3);
	EXPECT_EQ(again.session.RefusedReason(), "kicked");
	EXPECT_EQ(watch.session.RefusedReason(), "kicked");

	// When the other god leaves too, the game is over
	alice.session.Leave();
	Frames({&alice}, 5);
	EXPECT_TRUE(host->GameOver());
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

TEST_F(LockstepTest, AnObserverThatJoinsTheRunningGameGetsTheNamesOfTheGods)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 30);
	ASSERT_TRUE(host->Started());
	Computer carol(network, now, "observer:carol");
	Frames({&alice, &carol}, 10);
	// The observer names the god of a slot as the players do, for example when the player of the god leaves
	ASSERT_TRUE(carol.session.GetStart().has_value());
	ASSERT_GE(carol.session.GetStart()->names.size(), 1u);
	EXPECT_EQ(carol.session.GetStart()->names[0], "alice");
}

// The god of the observers comes into the game with the first observer and goes when the last observer left. Each world
// gets the same commands from the god of the observers.
TEST_F(LockstepTest, TheGodOfTheObserversComesWithTheFirstObserverAndGoesWithTheLast)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 30);
	ASSERT_TRUE(host->Started());
	EXPECT_TRUE(serverWorld.observersGod.empty());
	// The room of the test has 7 god slots, so the god of the observers is the god of the eighth slot
	const auto god = static_cast<PlayerNames>(8);
	EXPECT_EQ(host->ObserversGod(), god);
	Computer carol(network, now, "observer:carol");
	Computer dave(network, now, "observer:dave");
	Frames({&alice, &carol, &dave}, 20);
	ASSERT_EQ(serverWorld.observersGod.size(), 1u);
	EXPECT_EQ(serverWorld.observersGod[0].player, god);
	EXPECT_TRUE(std::holds_alternative<ObserversJoined>(serverWorld.observersGod[0].action));
	EXPECT_TRUE(host->ObserversIn());
	dave.session.Leave();
	Frames({&alice, &carol, &dave}, 10);
	EXPECT_EQ(serverWorld.observersGod.size(), 1u);
	carol.session.Leave();
	Frames({&alice, &carol, &dave}, 10);
	ASSERT_EQ(serverWorld.observersGod.size(), 2u);
	EXPECT_EQ(serverWorld.observersGod[1].player, god);
	EXPECT_TRUE(std::holds_alternative<ObserversLeft>(serverWorld.observersGod[1].action));
	EXPECT_FALSE(host->ObserversIn());
	EXPECT_EQ(alice.world.observersGod.size(), 2u);
	EXPECT_EQ(alice.world.hash, serverWorld.hash);
}

// Each observer gets its own number. The turns tell every world of each observer that comes in, with its name, and
// stamp the report of its hand and camera and its lines of the chat with its number.
TEST_F(LockstepTest, EachObserverHasANumberAndSendsItsReportAndItsChat)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 30);
	ASSERT_TRUE(host->Started());
	Computer carol(network, now, "observer:carol");
	Computer dave(network, now, "observer:dave");
	Frames({&alice, &carol, &dave}, 20);
	EXPECT_EQ(carol.session.OwnObserver(), uint8_t {1});
	EXPECT_EQ(dave.session.OwnObserver(), uint8_t {2});
	EXPECT_FALSE(alice.session.OwnObserver().has_value());
	EXPECT_EQ(host->ObserverNumbers(), (std::vector<uint8_t> {1, 2}));
	const auto god = static_cast<PlayerNames>(8);
	ASSERT_EQ(serverWorld.observerCommands.size(), 2u);
	EXPECT_EQ(serverWorld.observerCommands[0].player, god);
	EXPECT_EQ(serverWorld.observerCommands[0].observer, 1u);
	EXPECT_EQ(std::get<ObserverIn>(serverWorld.observerCommands[0].action).name, u"carol");
	EXPECT_EQ(serverWorld.observerCommands[1].observer, 2u);
	EXPECT_EQ(std::get<ObserverIn>(serverWorld.observerCommands[1].action).name, u"dave");

	// The hand of an observer does not act on things, so its computer sends only its report and its chat
	const HandReport report {.collidePoint = {1.0f, 2.0f, 3.0f},
	                         .handPoint = {4.0f, 5.0f, 6.0f},
	                         .camera = {7.0f, 8.0f, 9.0f},
	                         .focus = {10.0f, 11.0f, 12.0f}};
	dave.session.Send(report);
	dave.session.Send(HandTap {.object = Entity(1)});
	dave.session.Send(ChatSpeech {.recipients = {}, .text = u"Hi"});
	Frames({&alice, &carol, &dave}, 10);
	ASSERT_EQ(serverWorld.observerCommands.size(), 4u);
	EXPECT_EQ(serverWorld.observerCommands[2].observer, 2u);
	EXPECT_EQ(serverWorld.observerCommands[2].player, god);
	EXPECT_EQ(std::get<HandReport>(serverWorld.observerCommands[2].action), report);
	EXPECT_EQ(serverWorld.observerCommands[3].observer, 2u);
	EXPECT_EQ(std::get<ChatSpeech>(serverWorld.observerCommands[3].action).text, u"Hi");
	EXPECT_TRUE(serverWorld.taps.empty());
	EXPECT_EQ(alice.world.hash, serverWorld.hash);
	EXPECT_EQ(carol.world.hash, serverWorld.hash);
	EXPECT_EQ(dave.world.hash, serverWorld.hash);
	EXPECT_EQ(dave.session.GetCompares().compared, dave.session.GetCompares().same);
}

// A changed client of an observer can send any command. The server leaves out each command that an observer does not
// send, and keeps the rest of the block.
TEST_F(LockstepTest, TheServerLeavesOutTheCommandsThatAnObserverDoesNotSend)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 30);
	Computer carol(network, now, "observer:carol");
	Frames({&alice, &carol}, 20);
	const auto refused = host->RefusedBlocks();
	const auto block = turn_codec::Encode(turn_codec::CommandBlock {
	    .sequence = 0, .turn = 0, .actions = {HandTap {.object = Entity(1)}, ChatTaunt {.recipients = {}, .taunt = 3}}});
	ASSERT_TRUE(carol.sockets->SendBinary(carol.sockets->Ids().back(), block));
	Frames({&alice, &carol}, 10);
	EXPECT_EQ(host->RefusedBlocks(), refused + 1);
	EXPECT_TRUE(serverWorld.taps.empty());
	ASSERT_FALSE(serverWorld.observerCommands.empty());
	EXPECT_EQ(std::get<ChatTaunt>(serverWorld.observerCommands.back().action).taunt, 3u);
	EXPECT_EQ(serverWorld.observerCommands.back().observer, 1u);
}

// The leave of an observer goes in the next turn. Its number is free again after that turn, so no other observer has
// the number while a world still knows the observer that left.
TEST_F(LockstepTest, TheLeaveOfAnObserverGoesInTheNextTurnAndThenItsNumberIsFree)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 30);
	Computer carol(network, now, "observer:carol");
	Computer dave(network, now, "observer:dave");
	Frames({&alice, &carol, &dave}, 20);
	carol.session.Leave();
	Frames({&alice, &carol, &dave}, 10);
	ASSERT_EQ(serverWorld.observerCommands.size(), 3u);
	EXPECT_TRUE(std::holds_alternative<ObserverOut>(serverWorld.observerCommands[2].action));
	EXPECT_EQ(serverWorld.observerCommands[2].observer, 1u);
	EXPECT_EQ(host->ObserverNumbers(), (std::vector<uint8_t> {2}));
	// The god of the observers stays while an observer is in the game
	EXPECT_EQ(serverWorld.observersGod.size(), 1u);
	Computer erin(network, now, "observer:erin");
	Frames({&alice, &dave, &erin}, 20);
	EXPECT_EQ(erin.session.OwnObserver(), uint8_t {1});
	EXPECT_EQ(std::get<ObserverIn>(serverWorld.observerCommands.back().action).name, u"erin");
	EXPECT_EQ(alice.world.hash, serverWorld.hash);
	EXPECT_EQ(erin.world.hash, serverWorld.hash);
}

// An observer that joins before the start comes into every world in the first turn, after the god of the observers
TEST_F(LockstepTest, AnObserverOfTheRoomComesInAfterItsGodInTheFirstTurn)
{
	MakeHost(1);
	Computer carol(network, now, "observer:carol");
	Computer alice(network, now, "god:alice");
	Frames({&carol, &alice}, 30);
	ASSERT_TRUE(host->Started());
	EXPECT_EQ(carol.session.OwnObserver(), uint8_t {1});
	ASSERT_EQ(serverWorld.observersGod.size(), 1u);
	ASSERT_EQ(serverWorld.observerCommands.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<ObserverIn>(serverWorld.observerCommands[0].action));
	EXPECT_EQ(carol.world.observerCommands.size(), 1u);
	EXPECT_EQ(carol.world.hash, serverWorld.hash);
}

// A restored server gives each observer its old number back, and keeps the blocks that the sent turns hold
TEST_F(LockstepTest, ARestoredServerKeepsTheNumbersOfItsObservers)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Computer carol(network, now, "observer:carol");
	Computer dave(network, now, "observer:dave");
	Frames({&alice, &bob, &carol, &dave}, 30);
	ASSERT_TRUE(host->Started());
	dave.session.Send(ChatTaunt {.recipients = {}, .taunt = 4});
	Frames({&alice, &bob, &carol, &dave}, 10);
	const auto room = host->RoomRecord();
	EXPECT_EQ(room.watcherNumbers, (std::vector<uint8_t> {1, 2}));
	const auto blocks = host->BlocksInTurns();
	EXPECT_TRUE(blocks.contains(static_cast<uint8_t>(server::snapshots::k_ObserverBlocksKey + 2)));
	RestartFrom(room, host->TurnsDone(), serverWorld, {&alice, &bob, &carol, &dave}, 0, blocks);
	EXPECT_EQ(host->ObserverNumbers(), (std::vector<uint8_t> {1, 2}));
	Frames({&alice, &bob, &carol, &dave}, 40);
	EXPECT_EQ(dave.session.Resumes(), 1u);
	const auto taunts = std::ranges::count_if(
	    serverWorld.observerCommands, [](const Command& command) { return std::holds_alternative<ChatTaunt>(command.action); });
	EXPECT_EQ(taunts, 1);
	dave.session.Send(ChatTaunt {.recipients = {}, .taunt = 5});
	Frames({&alice, &bob, &carol, &dave}, 10);
	ASSERT_FALSE(serverWorld.observerCommands.empty());
	EXPECT_EQ(serverWorld.observerCommands.back().observer, 2u);
	EXPECT_EQ(std::get<ChatTaunt>(serverWorld.observerCommands.back().action).taunt, 5u);
	EXPECT_EQ(alice.world.hash, serverWorld.hash);
}

// An observer that loses its socket can come back, so its god stays in the game until its session ends
TEST_F(LockstepTest, TheGodOfTheObserversWaitsForAnObserverThatCanComeBack)
{
	MakeHost(1, 3 * 1'000'000);
	Computer alice(network, now, "god:alice");
	auto watcher = std::make_unique<Computer>(network, now, "observer:carol");
	Frames({&alice, watcher.get()}, 30);
	ASSERT_EQ(serverWorld.observersGod.size(), 1u);
	network.Cut(watcher->sockets->Ids().back());
	watcher.reset();
	Frames({&alice}, 20);
	EXPECT_EQ(serverWorld.observersGod.size(), 1u);
	Frames({&alice}, 30);
	ASSERT_EQ(serverWorld.observersGod.size(), 2u);
	EXPECT_TRUE(std::holds_alternative<ObserversLeft>(serverWorld.observersGod[1].action));
}

// The god of the observers is the god of the slot after the last god slot of the room, and at least player seven
TEST_F(LockstepTest, TheGodOfTheObserversFollowsTheGodSlotsOfTheRoom)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2);
	EXPECT_EQ(host->ObserversGod(), PlayerNames::PLAYER_SEVEN);
	MakeHost(1, 5LL * 60 * 1'000'000, 32);
	EXPECT_EQ(host->ObserversGod(), static_cast<PlayerNames>(33));
}

// A server that a snapshot restores knows the names of its observers and whether their god is in the game. So the
// players still see the name of an observer that leaves after the restart.
TEST_F(LockstepTest, ARestoredServerTellsTheNameOfAnObserverThatLeaves)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Computer carol(network, now, "observer:carol");
	Frames({&alice, &bob, &carol}, 30);
	ASSERT_TRUE(host->Started());
	const auto room = host->RoomRecord();
	EXPECT_TRUE(room.observersIn);
	EXPECT_EQ(room.watcherNames, (std::vector<std::string> {"carol"}));
	RestartFrom(room, host->TurnsDone(), serverWorld, {&alice, &bob, &carol});
	EXPECT_TRUE(host->ObserversIn());
	ASSERT_EQ(host->Watchers().size(), 1u);
	EXPECT_EQ(host->Watchers()[0].name, "carol");
	Frames({&alice, &bob, &carol}, 40);
	EXPECT_EQ(carol.session.Resumes(), 1u);
	carol.session.Leave();
	Frames({&alice, &bob, &carol}, 10);
	EXPECT_EQ(alice.session.TakeObserversLeft(), (std::vector<std::string> {"carol"}));
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

// The other players see that an observer left the game, by its name, as they see any player who leaves
TEST_F(LockstepTest, ThePlayersSeeTheNameOfAnObserverThatLeavesTheGame)
{
	MakeHost(1);
	Computer alice(network, now, "god:alice");
	Computer carol(network, now, "observer:carol");
	Frames({&alice, &carol}, 30);
	ASSERT_TRUE(host->Started());
	carol.session.Leave();
	Frames({&alice, &carol}, 10);
	EXPECT_EQ(alice.session.TakeObserversLeft(), (std::vector<std::string> {"carol"}));
	EXPECT_TRUE(alice.session.TakeObserversLeft().empty());
}

// An observer that loses its socket can come back, so the others see its leave only when its session ends
TEST_F(LockstepTest, ThePlayersSeeTheLeaveOfAnObserverThatDoesNotComeBackWhenItsSessionEnds)
{
	MakeHost(1, 3 * 1'000'000);
	Computer alice(network, now, "god:alice");
	auto watcher = std::make_unique<Computer>(network, now, "observer:carol");
	Frames({&alice, watcher.get()}, 10);
	ASSERT_TRUE(host->Started());
	network.Cut(watcher->sockets->Ids().back());
	watcher.reset();
	Frames({&alice}, 20);
	EXPECT_TRUE(alice.session.TakeObserversLeft().empty());
	Frames({&alice}, 30);
	EXPECT_EQ(alice.session.TakeObserversLeft(), (std::vector<std::string> {"carol"}));
}

// Before the start, the room shows who is in it, so no player is told of an observer that leaves
TEST_F(LockstepTest, NoPlayerIsToldOfAnObserverThatLeavesBeforeTheStart)
{
	MakeHost(2);
	Computer alice(network, now, "god:alice");
	Computer leaver(network, now, "observer:leaver");
	Frames({&alice, &leaver}, 5);
	ASSERT_FALSE(host->Started());
	leaver.session.Leave();
	Frames({&alice, &leaver}, 10);
	EXPECT_TRUE(alice.session.TakeObserversLeft().empty());
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
	// The clients do the turns a frame after the server, and each checksum is the same as the server's
	for (const auto* computer : {&alice, &bob})
	{
		EXPECT_EQ(computer->loads, 0u);
		EXPECT_GT(computer->session.GetCompares().compared, 60u);
		EXPECT_EQ(computer->session.GetCompares().compared, computer->session.GetCompares().same);
	}
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
	for (const auto* computer : {&alice, &bob})
	{
		EXPECT_GT(computer->session.GetCompares().compared, 60u);
		EXPECT_EQ(computer->session.GetCompares().compared, computer->session.GetCompares().same);
	}
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

TEST_F(LockstepTest, TheReportOfTheRoomGivesTheGodsAndTheTurnsForTheOperator)
{
	MakeHost(2);
	host->SetBans({7, 3});
	auto report = server::admin::RoomReportOf(*host);
	EXPECT_FALSE(report.started);
	EXPECT_EQ(report.turnsDone, 0u);
	EXPECT_EQ(report.bans, (std::vector<int64_t> {3, 7}));
	ASSERT_EQ(report.seats.size(), 7u);
	EXPECT_TRUE(std::ranges::all_of(report.seats, [](const auto& seat) { return seat.state == "open"; }));

	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 12);
	ASSERT_TRUE(host->Started());
	report = server::admin::RoomReportOf(*host);
	EXPECT_TRUE(report.started);
	EXPECT_FALSE(report.paused);
	EXPECT_EQ(report.turnsDone, host->TurnsDone());
	EXPECT_GT(report.turnsDone, 0u);
	const auto& first = report.seats.at(0);
	EXPECT_EQ(first.state, "taken");
	EXPECT_EQ(first.account, "alice");
	EXPECT_TRUE(first.connected);
	EXPECT_TRUE(first.ready);
	EXPECT_EQ(first.checksumsCompared, host->Seats().at(0).compares.compared);
	EXPECT_EQ(report.seats.at(1).account, "bob");
	EXPECT_EQ(report.seats.at(2).state, "open");
}

TEST(LockstepRules, ALineOfTheRoomsChatHasAVisibleCharacterAndTheMostCharactersOfTheChatBox)
{
	EXPECT_TRUE(server::IsRoomChatLine("Hi"));
	EXPECT_TRUE(server::IsRoomChatLine("  a  "));
	EXPECT_FALSE(server::IsRoomChatLine(""));
	EXPECT_FALSE(server::IsRoomChatLine("   "));
	EXPECT_FALSE(server::IsRoomChatLine("\t\r\n"));
	EXPECT_TRUE(server::IsRoomChatLine(std::string(game_socket::k_MostRoomChatCharacters, 'a')));
	EXPECT_FALSE(server::IsRoomChatLine(std::string(game_socket::k_MostRoomChatCharacters + 1, 'a')));
	// A letter of two bytes is one code unit; a letter of four bytes is two code units
	std::string accents;
	for (size_t i = 0; i < game_socket::k_MostRoomChatCharacters; ++i)
	{
		accents += "\xC3\xA9";
	}
	EXPECT_TRUE(server::IsRoomChatLine(accents));
	std::string faces;
	for (size_t i = 0; i < game_socket::k_MostRoomChatCharacters / 2; ++i)
	{
		faces += "\xF0\x9F\x98\x80";
	}
	EXPECT_TRUE(server::IsRoomChatLine(faces));
	EXPECT_FALSE(server::IsRoomChatLine(faces + "a"));
}

TEST_F(LockstepTest, OnTheLocalNetworkTheRoomsChatGoesToTheOtherPlayersInTheRoomUntilTheStart)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2, false, true);
	Computer alice(network, now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	alice.session.SetReady(false);
	Computer bob(network, now, "", k_LandFile, {'C'}, 0.0f, "Bob");
	bob.session.SetReady(false);
	Computer carol(network, now, "", k_LandFile, {'C'}, 0.0f, "Carol");
	Frames({&alice, &bob, &carol}, 3);
	ASSERT_EQ(carol.session.GetJoined()->role, game_socket::Role::Observer);

	alice.session.SendRoomChat("Hello, all!");
	carol.session.SendRoomChat("I watch.");
	// The server sends no line of only spaces and no line that is longer than the chat box
	bob.session.SendRoomChat("   ");
	bob.session.SendRoomChat(std::string(game_socket::k_MostRoomChatCharacters + 1, 'b'));
	Frames({&alice, &bob, &carol}, 3);
	// Each line goes to the other players in the room with the sender's name. The sender's game shows its own line.
	const game_socket::RoomChat fromAlice {.from = "Alice", .text = "Hello, all!"};
	const game_socket::RoomChat fromCarol {.from = "Carol", .text = "I watch."};
	EXPECT_EQ(alice.session.TakeRoomChat(), std::vector<game_socket::RoomChat> {fromCarol});
	EXPECT_EQ(bob.session.TakeRoomChat(), (std::vector<game_socket::RoomChat> {fromAlice, fromCarol}));
	EXPECT_EQ(carol.session.TakeRoomChat(), std::vector<game_socket::RoomChat> {fromAlice});
	EXPECT_TRUE(bob.session.TakeRoomChat().empty());

	// The chat of the room ends when the game starts
	alice.session.SetReady(true);
	bob.session.SetReady(true);
	Frames({&alice, &bob, &carol}, 6);
	ASSERT_TRUE(host->Started());
	alice.session.SendRoomChat("Too late");
	Frames({&alice, &bob, &carol}, 3);
	for (auto* computer : {&alice, &bob, &carol})
	{
		EXPECT_TRUE(computer->session.TakeRoomChat().empty());
	}
}

TEST_F(LockstepTest, OnTheLocalNetworkAPrivateRoomChatLineGoesOnlyToItsPlayer)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2, false, true);
	Computer alice(network, now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	alice.session.SetReady(false);
	Computer bob(network, now, "", k_LandFile, {'C'}, 0.0f, "Bob");
	bob.session.SetReady(false);
	Computer carol(network, now, "", k_LandFile, {'C'}, 0.0f, "Carol");
	Frames({&alice, &bob, &carol}, 3);
	ASSERT_EQ(carol.session.GetJoined()->role, game_socket::Role::Observer);

	// A private line goes only to the named player, also to an observer and to the sender self. A team line keeps its
	// mark. A name that is not in the room gets no line.
	bob.session.SendRoomChat("Our team", "Alice", true);
	Frames({&alice, &bob, &carol}, 3);
	alice.session.SendRoomChat("Hi Carol", "Carol");
	alice.session.SendRoomChat("Note to self", "Alice");
	alice.session.SendRoomChat("Anyone?", "Zed");
	Frames({&alice, &bob, &carol}, 3);
	const game_socket::RoomChat toCarol {.from = "Alice", .text = "Hi Carol", .to = {}, .privateLine = true, .team = false};
	const game_socket::RoomChat toAlice {.from = "Bob", .text = "Our team", .to = {}, .privateLine = true, .team = true};
	const game_socket::RoomChat toSelf {.from = "Alice", .text = "Note to self", .to = {}, .privateLine = true, .team = false};
	EXPECT_EQ(alice.session.TakeRoomChat(), (std::vector<game_socket::RoomChat> {toAlice, toSelf}));
	EXPECT_TRUE(bob.session.TakeRoomChat().empty());
	EXPECT_EQ(carol.session.TakeRoomChat(), std::vector<game_socket::RoomChat> {toCarol});
}

TEST_F(LockstepTest, ARoomOfTheLobbyHasNoChatOnTheGameSocket)
{
	MakeHost(1);
	Computer alice(network, now, "god:alice");
	alice.session.SetReady(false);
	Computer bob(network, now, "god:bob");
	bob.session.SetReady(false);
	Frames({&alice, &bob}, 3);
	ASSERT_TRUE(alice.session.GetJoined().has_value());
	// On the internet the lobby's channel of the room has the chat
	alice.session.SendRoomChat("Hello");
	Frames({&alice, &bob}, 3);
	EXPECT_TRUE(alice.session.TakeRoomChat().empty());
	EXPECT_TRUE(bob.session.TakeRoomChat().empty());
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::None);
}

// A changed client of an observer asks the server to stop the turns of a ranked game. Only the player of a god can do
// that.
TEST_F(LockstepTest, AnObserverCannotStopTheTurns)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(network, now, "god:alice");
	Frames({&alice}, 7);
	ASSERT_TRUE(host->Started());
	Computer watcher(network, now, "observer:carol");
	Frames({&alice, &watcher}, 10);
	ASSERT_EQ(watcher.session.GetJoined()->role, game_socket::Role::Observer);
	watcher.sockets->SendText(watcher.sockets->Ids().back(), game_socket::ToJson(game_socket::StopRequest {}));
	Frames({&alice, &watcher}, 5);
	EXPECT_FALSE(host->StoppedAfter().has_value());
	EXPECT_FALSE(watcher.session.StoppedAfter().has_value());
	// The god's player can still stop the turns of the decided game
	alice.session.StopGame();
	Frames({&alice, &watcher}, 5);
	EXPECT_TRUE(host->StoppedAfter().has_value());
}

// A room of computer gods has no human god. The end box of its observer stops the turns of the decided game.
TEST_F(LockstepTest, AnObserverStopsTheTurnsOfARoomWithNoHumanGod)
{
	MakeAllComputerHost(1);
	GiveWorldState();
	Computer watcher(network, now, "observer:one");
	Frames({&watcher}, 20);
	ASSERT_GT(serverWorld.turns, 0u);
	watcher.session.StopGame();
	Frames({&watcher}, 5);
	EXPECT_TRUE(host->StoppedAfter().has_value());
	EXPECT_TRUE(watcher.session.StoppedAfter().has_value());
}

// On the local network a name is only a label. A computer that gives the name of a god of the running game does not take
// the god: it watches the game, and the player of the god keeps playing.
TEST_F(LockstepTest, OnTheLocalNetworkANameDoesNotTakeTheGodOfTheRunningGame)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2, false, true);
	GiveWorldState();
	Computer alice(network, now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	Frames({&alice}, 8);
	ASSERT_TRUE(host->Started());
	const auto aliceConnection = host->Seats()[0].connection;
	Computer impostor(network, now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	Frames({&alice, &impostor}, 10);
	ASSERT_TRUE(impostor.session.GetJoined().has_value());
	EXPECT_EQ(impostor.session.GetJoined()->role, game_socket::Role::Observer);
	EXPECT_EQ(host->Seats()[0].connection, aliceConnection);
	EXPECT_EQ(host->Seats()[0].state, LockstepHost::SeatState::Taken);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	alice.session.Send(HandTap {.object = Entity(4)});
	Frames({&alice, &impostor}, 5);
	EXPECT_EQ(serverWorld.taps[4], 1u);
}

// One god sends more commands than a turn can hold. The server refuses the commands of that god over the limit, and the
// commands of the other god still come in the turn on every computer.
TEST_F(LockstepTest, AFloodOfOneGodDoesNotPushTheCommandsOfAnotherGodOutOfTheTurn)
{
	MakeHost(2);
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 7);
	ASSERT_TRUE(host->Started());
	const ChatSpeech line {.recipients = {PlayerNames::PLAYER_TWO}, .text = std::u16string(1000, u'a')};
	const std::vector<Action> lines(turn_codec::k_MostBlockCommands, line);
	for (uint16_t sequence = 1000; sequence < 1040; ++sequence)
	{
		const auto bytes = turn_codec::Encode(turn_codec::CommandBlock {.sequence = sequence, .turn = 0, .actions = lines});
		ASSERT_FALSE(bytes.empty());
		alice.sockets->SendBinary(alice.sockets->Ids().back(), bytes);
	}
	bob.session.Send(HandTap {.object = Entity(5)});
	Frames({&alice, &bob}, 5);
	EXPECT_GT(host->RefusedBlocks(), 0u);
	EXPECT_EQ(serverWorld.taps[5], 1u);
	EXPECT_EQ(alice.world.taps[5], 1u);
	EXPECT_EQ(bob.world.taps[5], 1u);
	Frames({&alice, &bob}, 5);
	EXPECT_EQ(alice.world.hash, bob.world.hash);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
}

// The room takes no more observers than its limit
TEST_F(LockstepTest, TheRoomRefusesAnObserverOverItsLimit)
{
	MakeHost(1);
	std::vector<std::unique_ptr<Computer>> watchers;
	for (size_t i = 0; i <= server::k_MostObservers; ++i)
	{
		watchers.push_back(std::make_unique<Computer>(network, now, "observer:w" + std::to_string(i)));
	}
	std::vector<Computer*> computers;
	std::ranges::transform(watchers, std::back_inserter(computers), [](const auto& watcher) { return watcher.get(); });
	Frames(computers, 3);
	EXPECT_EQ(host->Observers(), server::k_MostObservers);
	EXPECT_EQ(watchers.back()->session.RefusedReason(), "full");
}

// A socket that never joins does not stay open
TEST_F(LockstepTest, AConnectionThatDoesNotJoinInItsTimeCloses)
{
	MakeHost(1);
	auto client = network.MakeClient();
	const auto id =
	    client->Connect({.url = "wss://127.0.0.1/game/1", .headers = {}, .subprotocols = {}, .serverKey = std::nullopt}, {});
	Frames({}, 2);
	EXPECT_EQ(network.Count(), 1u);
	Frames({}, 200);
	EXPECT_EQ(network.Count(), 0u);
	client->Close(id, server::server_close::k_Normal);
}

namespace
{

/// A game server that a test controls: it sends the frames that the test gives to the client that connected
struct HostileServer
{
	explicit HostileServer(LoopbackSockets& sockets)
	    : network(sockets)
	{
		network.SetHandlers(server::ServerSocketHandlers {
		    .opened = [this](server::ConnectionId connection) { opened.push_back(connection); },
		    .text = {},
		    .binary = {},
		    .closed = {},
		});
	}

	void Text(const game_socket::Frame& frame) { network.SendText(opened.back(), game_socket::ToJson(frame)); }
	void Binary(const turn_codec::Frame& frame) { network.SendBinary(opened.back(), turn_codec::Encode(frame)); }

	LoopbackSockets& network;
	std::vector<server::ConnectionId> opened;
};

void Exchange(LoopbackSockets& network, Computer& computer, int frames = 3)
{
	for (int i = 0; i < frames; ++i)
	{
		network.Poll();
		computer.Frame();
	}
}

} // namespace

// A changed server names a land that is not a plain name of a land of the game: a network share, a drive, a device or a
// path out of the folder. The client opens no file and leaves the game.
TEST(HostileGameServer, ALandThatIsNotAPlainNameOfALandEndsTheJoin)
{
	for (const std::string land : {"//host/share/x.txt", "\\\\host\\share\\x.txt", "C:/Windows/win.ini", "../../x.txt",
	                               "Scripts/../../x.txt", "NUL", "con.txt", "Land1.txt:stream", "a/./b.txt", "x.txt."})
	{
		SCOPED_TRACE(land);
		LoopbackSockets network;
		HostileServer server(network);
		int64_t now = 0;
		Computer alice(network, now, "god:alice");
		Exchange(network, alice);
		ASSERT_FALSE(server.opened.empty());
		server.Text(game_socket::Joined {.role = game_socket::Role::God,
		                                 .slot = 0,
		                                 .session = "s",
		                                 .name = "alice",
		                                 .start = game_socket::Start {.land = land, .seed = 1}});
		Exchange(network, alice);
		EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::ServerLimit);
		EXPECT_TRUE(alice.askedLand.empty());
	}
}

TEST(HostileGameServer, ALandQueryWithAPathOutOfTheGameEndsTheJoin)
{
	LoopbackSockets network;
	HostileServer server(network);
	int64_t now = 0;
	Computer alice(network, now, "god:alice");
	Exchange(network, alice);
	server.Text(game_socket::Joined {.role = game_socket::Role::God, .slot = 0, .session = "s", .name = "alice"});
	Exchange(network, alice);
	server.Text(game_socket::StartQuery {.land = "\\\\host\\share\\land.txt", .testbed = false, .checksum = 1});
	Exchange(network, alice);
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::ServerLimit);
	EXPECT_TRUE(alice.askedLand.empty());
}

// A changed server sends turns far ahead of the game
TEST(HostileGameServer, TurnsFarAheadEndTheGame)
{
	LoopbackSockets network;
	HostileServer server(network);
	int64_t now = 0;
	Computer alice(network, now, "god:alice");
	Exchange(network, alice);
	const game_socket::Start start {.land = "Land1.txt", .seed = 1};
	server.Text(
	    game_socket::Joined {.role = game_socket::Role::God, .slot = 0, .session = "s", .name = "alice", .start = start});
	server.Text(start);
	Exchange(network, alice);
	ASSERT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	server.Binary(turn_codec::TurnBundle {.turn = 0, .blocks = {}});
	Exchange(network, alice);
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::None);
	server.Binary(turn_codec::TurnBundle {.turn = 0x7FFFFFFF, .blocks = {}});
	Exchange(network, alice);
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::ServerLimit);
}

// A changed server sends a world state larger than the client keeps
TEST(HostileGameServer, AWorldStateLargerThanTheLimitEndsTheGame)
{
	LoopbackSockets network;
	HostileServer server(network);
	int64_t now = 0;
	Computer alice(network, now, "god:alice");
	Exchange(network, alice);
	const game_socket::Start start {.land = "Land1.txt", .seed = 1};
	server.Text(
	    game_socket::Joined {.role = game_socket::Role::God, .slot = 0, .session = "s", .name = "alice", .start = start});
	server.Text(start);
	server.Text(game_socket::Resumed {.turn = 0, .world = true});
	Exchange(network, alice);
	ASSERT_TRUE(alice.session.WaitsForWorld());
	server.Binary(turn_codec::WorldPart {.turn = 0, .part = 0, .parts = 0xFFFF, .bytes = {1, 2, 3}});
	Exchange(network, alice);
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::ServerLimit);
}

TEST_F(LockstepTest, TheStartGivesTheAccountOfEachGodWhoseTicketHasOne)
{
	MakeHost(2);
	Computer alice(network, now, "god:100");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 9);
	ASSERT_TRUE(alice.session.GetStart().has_value());
	const auto& gods = alice.session.GetStart()->gods;
	ASSERT_EQ(gods.size(), 2);
	// A subject that is not an account id gives no account
	EXPECT_EQ(gods[0].account, std::optional<int64_t>(100));
	EXPECT_EQ(gods[1].account, std::nullopt);
}

TEST_F(LockstepTest, AClientThatCannotComeBackInTimeShowsTheBoxOfTheLostConnectionOnce)
{
	MakeHost(2);
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(host->Started());
	online::ConnectionLostNotice notice;
	const auto facts = [&alice] {
		return online::ConnectionLostFacts {.hasSession = true,
		                                    .started = alice.session.GetStart().has_value(),
		                                    .state = alice.session.GetState(),
		                                    .end = alice.session.GetEnd(),
		                                    .toolRun = false};
	};

	// The network to the server goes: each try to come back gets no connection
	int told = 0;
	int64_t toldAt = 0;
	const auto lostAt = now;
	for (int frame = 0; frame < 3100; ++frame)
	{
		now += k_Turn;
		network.Cut(alice.sockets->Ids().back());
		network.Poll();
		alice.Frame();
		if (notice.Update(facts()))
		{
			++told;
			toldAt = now;
		}
		// The box waits while the client tries to come back
		if (now - lostAt < 5LL * 60 * 1'000'000)
		{
			ASSERT_EQ(told, 0);
			ASSERT_NE(alice.session.GetState(), NetworkSession::State::Ended);
		}
	}
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::Lost);
	EXPECT_EQ(told, 1);
	EXPECT_GT(toldAt - lostAt, 5LL * 60 * 1'000'000);
}

namespace
{
/// The members of the lobby of the tests: two gods with a slot each and an observer
std::vector<lobby_members::Member> TwoGodsAndAnObserver()
{
	return {
	    {.account = "alice", .role = game_socket::Role::God, .slot = 2},
	    {.account = "bob", .role = game_socket::Role::God, .slot = 0},
	    {.account = "carol", .role = game_socket::Role::Observer, .slot = std::nullopt},
	};
}

constexpr std::string_view k_ServerBuild = "0.1.0+a/codec1";
} // namespace

TEST_F(LockstepTest, TheRoomOfALobbyTakesOnlyItsMembersInTheirRolesAndSlots)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 1);
	const std::string build(k_ServerBuild);
	Computer alice(network, now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer carol(network, now, "observer:carol@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer dave(network, now, "god:dave@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer other(network, now, "god:bob@lobby2", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer noLobby(network, now, "god:bob", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer wrongRole(network, now, "observer:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice, &carol, &dave, &other, &noLobby, &wrongRole}, 3);
	// Each member takes the slot that the lobby gave, not the first free slot
	ASSERT_TRUE(alice.session.GetJoined().has_value());
	EXPECT_EQ(alice.session.GetJoined()->slot, 2);
	ASSERT_TRUE(carol.session.GetJoined().has_value());
	EXPECT_EQ(carol.session.GetJoined()->role, game_socket::Role::Observer);
	// An account that is not a member, a ticket of another lobby or of no lobby, and a ticket of another role
	for (const auto* refused : {&dave, &other, &noLobby, &wrongRole})
	{
		EXPECT_EQ(refused->session.GetEnd(), NetworkSession::End::Refused);
		EXPECT_EQ(refused->session.RefusedReason(), "ticket");
	}
}

TEST_F(LockstepTest, TheRoomOfALobbyRefusesAClientOfAnotherBuildOrWithNoBuild)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 1);
	Computer other(network, now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, "0.1.0+b/codec1");
	Computer none(network, now, "god:bob@lobby1");
	Frames({&other, &none}, 3);
	EXPECT_EQ(other.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(other.session.RefusedReason(), "version");
	EXPECT_EQ(none.session.RefusedReason(), "version");
	EXPECT_EQ(host->GodsInRoom(), 0u);
}

TEST_F(LockstepTest, ARoomWithNoLobbyRefusesAnotherBuildAndTakesAClientWithNoBuild)
{
	MakeHost(2);
	// A room with no build id of its own takes every build
	Computer any(network, now, "god:alice", k_LandFile, {'C'}, 0.0f, {}, "0.1.0+b/codec1");
	Frames({&any}, 3);
	EXPECT_TRUE(any.session.GetJoined().has_value());

	host.reset();
	host = std::make_unique<LockstepHost>(
	    network, CheckFakeTicket, [this] { return now; }, [this] { return "secret-" + std::to_string(++secrets); },
	    server::HostSettings {.start = {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow"},
	                          .landChecksum = online::start_rules::LandChecksum(k_LandFile),
	                          .minGods = 2,
	                          .godSlots = 4,
	                          .comeBackMicroseconds = 5LL * 60 * 1'000'000,
	                          .keptTurns = 3200,
	                          .sessionHasher = hasher,
	                          .buildId = std::string(k_ServerBuild)});
	Computer older(network, now, "god:bob");
	Computer other(network, now, "god:carol", k_LandFile, {'C'}, 0.0f, {}, "0.1.0+b/codec1");
	Frames({&older, &other}, 3);
	EXPECT_TRUE(older.session.GetJoined().has_value());
	EXPECT_EQ(other.session.RefusedReason(), "version");
}

TEST_F(LockstepTest, TheRoomOfALobbyRefusesAClientWithAnotherFileOfTheLand)
{
	const std::string map(64, 'a');
	MakeLobbyHost(TwoGodsAndAnObserver(), 1, std::string(k_ServerBuild), map);
	const std::string build(k_ServerBuild);
	Computer same(network, now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build, map);
	Computer other(network, now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build, std::string(64, 'b'));
	Frames({&same, &other}, 3);
	EXPECT_TRUE(same.session.GetJoined().has_value());
	EXPECT_EQ(other.session.RefusedReason(), "map");
}

TEST_F(LockstepTest, TheRoomOfALobbyWaitsForEachGodUntilTheTimeToJoinEnds)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 1);
	EXPECT_EQ(host->GodsToWaitFor(), 2u);
	const std::string build(k_ServerBuild);
	Computer alice(network, now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice}, 10);
	// One god of two is ready: the room waits for the other god of the lobby
	EXPECT_EQ(host->GodsInRoom(), 1u);
	EXPECT_FALSE(host->Started());
	EXPECT_FALSE(host->Starting());
	host->EndJoinWindow();
	EXPECT_EQ(host->GodsToWaitFor(), 1u);
	Frames({&alice}, 10);
	EXPECT_TRUE(host->Started());
}

TEST_F(LockstepTest, TheRoomOfALobbyStartsWhenEachGodOfTheLobbyJoined)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 1);
	const std::string build(k_ServerBuild);
	Computer alice(network, now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer bob(network, now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice, &bob}, 10);
	EXPECT_TRUE(host->Started());
	EXPECT_EQ(bob.session.GetJoined()->slot, 0);
}

TEST_F(LockstepTest, TheTurnsOfALobbyGameWaitUntilEachGodsPlayerLoadedTheWorld)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 2);
	const std::string build(k_ServerBuild);
	Computer alice(network, now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer bob(network, now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(host->Started());
	// No turn runs: the game is paused for every computer, and each sees the players who load
	EXPECT_TRUE(host->WaitsForLoads());
	EXPECT_EQ(serverWorld.turns, 0u);
	EXPECT_EQ(alice.world.turns, 0u);
	ASSERT_TRUE(alice.session.LoadWait().has_value());
	EXPECT_EQ(alice.session.LoadWait()->loading.size(), 2u);
	// The seconds count down from the server's end of the wait, 120 seconds after the room was made
	EXPECT_GT(alice.session.LoadWait()->secondsLeft, 110u);
	EXPECT_LE(alice.session.LoadWait()->secondsLeft, 120u);
	const auto before = alice.session.LoadWait()->secondsLeft;
	Frames({&alice, &bob}, 20);
	EXPECT_EQ(alice.session.LoadWait()->secondsLeft, before - 2);
	// A command of the world does not go during the pause
	alice.session.Send(HandTap {.object = Entity(5)});

	// Alice loaded: the turns still wait for bob, and both computers see only bob's name
	alice.session.ReportLoaded();
	Frames({&alice, &bob}, 5);
	EXPECT_EQ(serverWorld.turns, 0u);
	ASSERT_TRUE(bob.session.LoadWait().has_value());
	EXPECT_EQ(bob.session.LoadWait()->loading, (std::vector<std::string> {"bob"}));
	EXPECT_EQ(alice.session.LoadWait()->loading, (std::vector<std::string> {"bob"}));

	// Bob loaded: the pause ends for all, and the turns run
	bob.session.ReportLoaded();
	Frames({&alice, &bob}, 10);
	EXPECT_FALSE(host->WaitsForLoads());
	EXPECT_FALSE(alice.session.LoadWait().has_value());
	EXPECT_FALSE(bob.session.LoadWait().has_value());
	EXPECT_GT(serverWorld.turns, 5u);
	EXPECT_GT(alice.world.turns, 5u);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	// The tap of the pause did not go into a turn
	EXPECT_TRUE(host->Seats()[2].commands.empty());
}

TEST_F(LockstepTest, AReportOfTheLoadThatTheTestsDelayGoesLateAndTheTurnsWaitForIt)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 2);
	const std::string build(k_ServerBuild);
	Computer alice(network, now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer bob(network, now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build, {}, 3LL * 1'000'000);
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(host->Started());
	alice.session.ReportLoaded();
	bob.session.ReportLoaded();
	// Bob's computer loaded, but it tells the server 3 seconds late: alice sees the pause with bob's name
	Frames({&alice, &bob}, 20);
	EXPECT_TRUE(host->WaitsForLoads());
	EXPECT_EQ(serverWorld.turns, 0u);
	ASSERT_TRUE(alice.session.LoadWait().has_value());
	EXPECT_EQ(alice.session.LoadWait()->loading, (std::vector<std::string> {"bob"}));
	// After the delay the report goes, and the turns run
	Frames({&alice, &bob}, 20);
	EXPECT_FALSE(host->WaitsForLoads());
	EXPECT_FALSE(alice.session.LoadWait().has_value());
	EXPECT_GT(serverWorld.turns, 5u);
}

TEST_F(LockstepTest, AClientOfARoomThatTheServerEndsBeforeTheStartDoesNotTryToComeBack)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 2);
	const std::string build(k_ServerBuild);
	Computer alice(network, now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice}, 10);
	ASSERT_FALSE(host->Started());
	ASSERT_TRUE(alice.session.GetJoined().has_value());
	host->EndRoom("ended");
	Frames({&alice}, 10);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Ended);
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(alice.session.RefusedReason(), "ended");
}

TEST_F(LockstepTest, AGodWhosePlayerLeavesDuringTheLoadsLeavesAndTheTurnsStartWithTheOthers)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 2);
	const std::string build(k_ServerBuild);
	Computer alice(network, now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer bob(network, now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(host->Started());
	alice.session.ReportLoaded();
	Frames({&alice, &bob}, 5);
	EXPECT_EQ(serverWorld.turns, 0u);
	// Bob leaves before his world loaded: his god leaves the game, and the game goes on with alice
	bob.session.Leave();
	Frames({&alice, &bob}, 10);
	EXPECT_EQ(host->Seats()[0].state, LockstepHost::SeatState::Closed);
	EXPECT_FALSE(host->WaitsForLoads());
	EXPECT_FALSE(alice.session.LoadWait().has_value());
	EXPECT_GT(serverWorld.turns, 5u);
	EXPECT_FALSE(host->GameOver());
}

TEST_F(LockstepTest, AGodWhosePlayerDoesNotLoadInTimeLeavesTheGame)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 2, std::string(k_ServerBuild), {}, 5LL * 1'000'000);
	const std::string build(k_ServerBuild);
	Computer alice(network, now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer bob(network, now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(host->Started());
	alice.session.ReportLoaded();
	// The time to load counts 5 seconds from the time when the room was made
	Frames({&alice, &bob}, 30);
	EXPECT_TRUE(host->WaitsForLoads());
	EXPECT_EQ(serverWorld.turns, 0u);
	Frames({&alice, &bob}, 30);
	// Bob did not load in time: the server closes his socket with "load", and his god leaves as a player who leaves
	EXPECT_EQ(bob.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(bob.session.RefusedReason(), "load");
	EXPECT_EQ(host->Seats()[0].state, LockstepHost::SeatState::Closed);
	EXPECT_FALSE(host->WaitsForLoads());
	EXPECT_GT(serverWorld.turns, 5u);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
}

TEST_F(LockstepTest, ARoomWithNoLobbyDoesNotWaitForTheLoads)
{
	MakeHost(2);
	Computer alice(network, now, "god:alice");
	Computer bob(network, now, "god:bob");
	Frames({&alice, &bob}, 20);
	ASSERT_TRUE(host->Started());
	EXPECT_FALSE(host->WaitsForLoads());
	EXPECT_FALSE(alice.session.LoadWait().has_value());
	EXPECT_GT(serverWorld.turns, 5u);
}

TEST_F(LockstepTest, AGameThatStartsAfterTheTimeToLoadGetsTheWholeTimeFromItsStart)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 2, std::string(k_ServerBuild), {}, 5LL * 1'000'000);
	// 6 seconds go by before the players join
	Frames({}, 60);
	const std::string build(k_ServerBuild);
	Computer alice(network, now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer bob(network, now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(host->Started());
	ASSERT_TRUE(alice.session.LoadWait().has_value());
	// The wait gives the whole 5 seconds from the start
	EXPECT_GE(alice.session.LoadWait()->secondsLeft, 4u);
	EXPECT_LE(alice.session.LoadWait()->secondsLeft, 5u);
	Frames({&alice, &bob}, 30);
	EXPECT_TRUE(host->WaitsForLoads());
	Frames({&alice, &bob}, 30);
	EXPECT_FALSE(host->WaitsForLoads());
	EXPECT_EQ(alice.session.RefusedReason(), "load");
	EXPECT_EQ(bob.session.RefusedReason(), "load");
}
