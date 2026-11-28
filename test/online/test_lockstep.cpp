/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
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
#include "Editor/EditorMindLoads.h"
#include "Editor/EditorScriptFiles.h"
#include "Editor/LandBrush.h"
#include "LoopbackSockets.h"
#include "Online/ConnectionLostNotice.h"
#include "Online/GameBuildId.h"
#include "Online/GodsRingMapFile.h"
#include "Online/NetworkSession.h"
#include "Online/RoomClock.h"
#include "Online/StartRules.h"
#include "Server/Desktop/Sha256SessionHasher.h"
#include "Server/LockstepHost.h"
#include "Server/ServerAdmin.h"
#include "Server/ServerLogNames.h"
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
	/// Whether the computer has the mind file of the editor with the hash
	std::function<bool(uint64_t hash)> mindFileHere;
	/// For each mind load of the editor that the world did, whether the computer had its file at the turn
	std::vector<bool> mindFilesAtTurn;
	/// Whether the computer has the file of the script tabs of the editor of the kind with the hash
	std::function<bool(uint8_t kind, uint64_t hash)> scriptFileHere;
	/// For each act of the script tabs that uses a file, whether the computer had its file at the turn
	std::vector<bool> scriptFilesAtTurn;
	/// The world loads a new land before this turn, and its turns start again at 0, as a command of a testbed scenario does
	std::optional<uint32_t> landBefore;
	/// The lands that the world loaded before a turn
	uint32_t lands {0};
	/// The joins of the gods that the world did, each with the count of the lands that the world loaded before it
	std::vector<std::pair<uint32_t, PlayerNames>> joins;

	/// Loads the new land when it is due before the turn. Gives true when the land loaded.
	bool LoadLandBefore(uint32_t turn)
	{
		if (landBefore != turn)
		{
			return false;
		}
		landBefore.reset();
		turns = 0;
		hash = 7;
		++lands;
		return true;
	}

	uint64_t Play(uint32_t turn, const std::vector<Command>& commands)
	{
		turns = turn + 1;
		hash = (hash ^ (turn + 1)) * 1099511628211ull;
		for (const auto& command : commands)
		{
			players.insert(command.player);
			if (std::holds_alternative<PlayerJoined>(command.action))
			{
				joins.emplace_back(lands, command.player);
			}
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
			if (const auto* load = std::get_if<EditorMindLoad>(&command.action))
			{
				mindFilesAtTurn.push_back(mindFileHere && mindFileHere(load->file));
			}
			if (const auto* program = std::get_if<EditorScriptProgram>(&command.action);
			    program != nullptr && program->use != EditorScriptProgram::Use::GamesProgram)
			{
				const auto kind = program->use == EditorScriptProgram::Use::Open ? game_socket::EditorScriptFile::Kind::Program
				                                                                 : game_socket::EditorScriptFile::Kind::Source;
				scriptFilesAtTurn.push_back(scriptFileHere && scriptFileHere(static_cast<uint8_t>(kind), program->file));
			}
			if (const auto* land = std::get_if<EditorLandScript>(&command.action);
			    land != nullptr && land->act == EditorLandScript::Act::Open)
			{
				scriptFilesAtTurn.push_back(
				    scriptFileHere &&
				    scriptFileHere(static_cast<uint8_t>(game_socket::EditorScriptFile::Kind::LandScript), land->file));
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
		    .readSymbol = [this] { return this->symbol; },
		    .localParts = [this](uint32_t turn) { return parts ? parts(turn) : online::PartHashes {}; },
		    .origin = online::GameOrigin::Lan,
		    .testLoadDelayMicroseconds = loadDelayMicroseconds,
		    .desynced = [this](uint32_t turn, uint64_t local,
		                       uint64_t server) { desyncs.push_back({.turn = turn, .local = local, .server = server}); },
		    .partedParts = [this](std::span<const std::string> names) { partedNames.assign(names.begin(), names.end()); },
		    .printData =
		        [this](std::span<const std::string> files) { return printData ? printData(files) : game_socket::DataPrint {}; },
		    .entities =
		        [this](std::span<const std::string> names) {
			        return entities ? entities(names) : std::vector<synced_state::PartEntities> {};
		        },
		    .saveWorld = [this] { return saveWorld ? saveWorld() : std::vector<uint8_t> {}; },
		});
		this->landFile = std::move(landFile);
		this->creature = std::move(creature);
		world.mindFileHere = [this](uint64_t hash) { return !session.EditorMindFile(hash).empty(); };
		world.scriptFileHere = [this](uint8_t kind, uint64_t hash) { return !session.EditorScriptFile(kind, hash).empty(); };
		session.SetReady(true);
	}

	/// The frame of the computer: the socket, the session, then the turns that are due
	void Frame()
	{
		sockets->Poll();
		session.Update();
		// The server started the game again: the computer loads the land of the new start, then does its turns
		if (session.TakeRestart())
		{
			world = FakeWorld {};
			world.mindFileHere = [this](uint64_t hash) { return !session.EditorMindFile(hash).empty(); };
			world.turns = session.GetStart()->turn;
			++restarts;
			session.ReportLoaded();
		}
		while (session.HasTurn(world.turns))
		{
			if (const auto after = world.turns; world.LoadLandBefore(after))
			{
				session.TurnsStartAgain(after, world.turns);
			}
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
	/// The symbol of the player's profile, which goes with the creature
	game_socket::StartSymbol symbol;
	std::string askedLand;
	std::unique_ptr<LoopbackSockets::Client> sockets;
	/// The parts of the world of this computer at a turn, for the log of a part. None by default.
	std::function<online::PartHashes(uint32_t turn)> parts;
	/// The desyncs that the session told, and the names of the parts that differ from the last parts of the server
	struct Desync
	{
		uint32_t turn {0};
		uint64_t local {0};
		uint64_t server {0};
	};
	std::vector<Desync> desyncs;
	std::vector<std::string> partedNames;
	/// The print of the game data, the hash of each entity and the world of this computer, for the capture of a desync
	std::function<game_socket::DataPrint(std::span<const std::string> files)> printData;
	std::function<std::vector<synced_state::PartEntities>(std::span<const std::string> names)> entities;
	std::function<std::vector<uint8_t>()> saveWorld;
	NetworkSession session;
	FakeWorld world;
	std::vector<size_t> played;
	uint32_t loads {0};
	/// The starts of the game again that the computer did
	uint32_t restarts {0};
};

server::TicketCheck::Result CheckFakeTicket(std::string_view token)
{
	// "god:name" or "observer:name", with "+editor" after the role for a player with the editor claim and "@lobby" after the
	// name for a ticket of a lobby's game, and "#gods" after the lobby for the gods of the lobby's game; anything else is not a
	// ticket
	const auto colon = token.find(':');
	if (colon == std::string_view::npos)
	{
		return {.problem = server::TicketProblem::Signature, .claims = std::nullopt};
	}
	// "+debugger" after the role gives the debugger claim, and "+editor" after the role gives the editor claim
	auto roleText = token.substr(0, colon);
	constexpr std::string_view k_Debugger = "+debugger";
	const auto debugger = roleText.ends_with(k_Debugger);
	if (debugger)
	{
		roleText.remove_suffix(k_Debugger.size());
	}
	constexpr std::string_view k_Editor = "+editor";
	const auto editor = roleText.ends_with(k_Editor);
	if (editor)
	{
		roleText.remove_suffix(k_Editor.size());
	}
	const auto role = game_socket::RoleFrom(roleText);
	if (!role.has_value())
	{
		return {.problem = server::TicketProblem::Format, .claims = std::nullopt};
	}
	join_ticket::Claims claims;
	auto subject = token.substr(colon + 1);
	// "god:alice@lobby1#2%open,closed" gives the kinds of the slots of the lobby's game
	if (const auto percent = subject.find('%'); percent != std::string_view::npos)
	{
		auto kinds = server::lobby_game::ParseSlotKinds(subject.substr(percent + 1));
		claims.slotKinds = std::get<std::vector<lobby_api::SlotKind>>(kinds);
		subject = subject.substr(0, percent);
	}
	// "god:alice@lobby1#3!2" gives the slot that the lobby gave the player
	if (const auto bang = subject.find('!'); bang != std::string_view::npos)
	{
		claims.slot = static_cast<uint8_t>(std::stoul(std::string(subject.substr(bang + 1))));
		subject = subject.substr(0, bang);
	}
	if (const auto at = subject.find('@'); at != std::string_view::npos)
	{
		auto lobby = subject.substr(at + 1);
		if (const auto hash = lobby.find('#'); hash != std::string_view::npos)
		{
			claims.gods = static_cast<uint32_t>(std::stoul(std::string(lobby.substr(hash + 1))));
			lobby = lobby.substr(0, hash);
		}
		claims.lobby = std::string(lobby);
		subject = subject.substr(0, at);
	}
	claims.subject = std::string(subject);
	claims.name = claims.subject;
	claims.role = *role;
	claims.editor = editor;
	claims.debugger = debugger;
	return {.problem = server::TicketProblem::None, .claims = claims};
}

/// The game server of the test, with its own world
class LockstepTest: public ::testing::Test
{
protected:
	void MakeHost(uint32_t gods, int64_t comeBack = 5LL * 60 * 1'000'000, uint8_t slots = 7, bool testCommands = false,
	              bool lan = false, std::function<bool(std::span<const uint8_t>)> readsMindFile = {},
	              std::function<bool(std::span<const uint8_t>)> readsScriptProgram = {})
	{
		_host = std::make_unique<LockstepHost>(
		    _network, CheckFakeTicket, [this] { return _now; }, [this] { return "secret-" + std::to_string(++_secrets); },
		    server::HostSettings {
		        .start = {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow", .godsRingMap = false},
		        .landChecksum = online::start_rules::LandChecksum(k_LandFile),
		        .minGods = gods,
		        .godSlots = slots,
		        .comeBackMicroseconds = comeBack,
		        .keptTurns = _keptTurns,
		        .testCommands = testCommands,
		        .sessionHasher = _hasher,
		        .lanNames = lan,
		        .readsMindFile = std::move(readsMindFile),
		        .readsScriptProgram = std::move(readsScriptProgram)});
	}

	/// The room of the game of one lobby: the members of the lobby, the build of the server and the SHA-256 of its land
	void MakeLobbyHost(std::vector<lobby_members::Member> members, uint32_t minGods, std::string build = "0.1.0+a/codec1",
	                   std::string map = {}, int64_t loadWindow = 120LL * 1'000'000)
	{
		_host = std::make_unique<LockstepHost>(
		    _network, CheckFakeTicket, [this] { return _now; }, [this] { return "secret-" + std::to_string(++_secrets); },
		    server::HostSettings {
		        .start = {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow", .godsRingMap = false},
		        .landChecksum = online::start_rules::LandChecksum(k_LandFile),
		        .minGods = minGods,
		        .godSlots = 4,
		        .comeBackMicroseconds = 5LL * 60 * 1'000'000,
		        .keptTurns = 3200,
		        .testCommands = false,
		        .sessionHasher = _hasher,
		        .lanNames = false,
		        .mostObservers = server::k_MostObservers,
		        .relaysCreature = {},
		        .lobby = server::lobby_game::Lobby {.id = "lobby1", .members = std::move(members)},
		        .buildId = std::move(build),
		        .mapSha256 = std::move(map),
		        .loadWindowMicroseconds = loadWindow});
	}

	/// A room of four slots whose kinds the lobby set. The start names the closed slots.
	void MakeKindsHost(std::vector<lobby_api::SlotKind> kinds, std::vector<uint8_t> closedSlots)
	{
		game_socket::Start start {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow"};
		start.closedSlots = std::move(closedSlots);
		_host = std::make_unique<LockstepHost>(
		    _network, CheckFakeTicket, [this] { return _now; }, [this] { return "secret-" + std::to_string(++_secrets); },
		    server::HostSettings {.start = std::move(start),
		                          .landChecksum = online::start_rules::LandChecksum(k_LandFile),
		                          .minGods = 1,
		                          .godSlots = 4,
		                          .slotKinds = std::move(kinds),
		                          .comeBackMicroseconds = 5LL * 60 * 1'000'000,
		                          .keptTurns = 3200,
		                          .sessionHasher = _hasher});
	}

	/// A room with no human god (tools): the game starts at once, and the world turns when this many observers joined
	void MakeAllComputerHost(uint32_t observers, int64_t comeBack = 5LL * 60 * 1'000'000)
	{
		game_socket::Start start {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow"};
		start.allComputerGods = true;
		start.aiSeed = 3u;
		_host = std::make_unique<LockstepHost>(
		    _network, CheckFakeTicket, [this] { return _now; }, [this] { return "secret-" + std::to_string(++_secrets); },
		    server::HostSettings {.start = std::move(start),
		                          .landChecksum = online::start_rules::LandChecksum(k_LandFile),
		                          .minGods = observers,
		                          .godSlots = 7,
		                          .comeBackMicroseconds = comeBack,
		                          .keptTurns = 3200,
		                          .sessionHasher = _hasher});
	}

	/// A room of the tools whose game plays a testbed scenario from the first turn
	void MakeScenarioHost(uint32_t gods)
	{
		game_socket::Start start {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow"};
		start.scenario = "mind.computer_god_rivals";
		_host = std::make_unique<LockstepHost>(
		    _network, CheckFakeTicket, [this] { return _now; }, [this] { return "secret-" + std::to_string(++_secrets); },
		    server::HostSettings {.start = std::move(start),
		                          .landChecksum = online::start_rules::LandChecksum(k_LandFile),
		                          .minGods = gods,
		                          .godSlots = 7,
		                          .comeBackMicroseconds = 5LL * 60 * 1'000'000,
		                          .keptTurns = 3200,
		                          .sessionHasher = _hasher});
	}

	/// One frame of the server and of each computer: 100 ms of time, and a turn when the world turns
	void Frame(std::vector<Computer*> computers)
	{
		_now += k_Turn;
		_network.Poll();
		_host->Update();
		if (_host->HasTurn(_serverWorld.turns))
		{
			if (const auto after = _serverWorld.turns; _serverWorld.LoadLandBefore(after))
			{
				_host->TurnsStartAgain(after, _serverWorld.turns);
			}
			const auto turn = _serverWorld.turns;
			const auto commands = _host->TakeTurn(turn);
			_serverWorld.mindFileHere = [this](uint64_t hash) { return !_host->EditorMindFile(hash).empty(); };
			_serverWorld.scriptFileHere = [this](uint8_t kind, uint64_t hash) {
				return !_host->EditorScriptFile(kind, hash).empty();
			};
			_host->TurnChecksum(turn + 1, _serverWorld.Play(turn, commands));
		}
		for (auto* computer : computers)
		{
			computer->Frame();
		}
	}

	/// The world state of the fake world: its hash and its turns
	void GiveWorldState()
	{
		_host->SetWorldState([this] {
			std::vector<uint8_t> bytes(sizeof(uint64_t) + sizeof(uint32_t));
			std::memcpy(bytes.data(), &_serverWorld.hash, sizeof(uint64_t));
			std::memcpy(bytes.data() + sizeof(uint64_t), &_serverWorld.turns, sizeof(uint32_t));
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
		_host->SetTurnJournal(
		    [this](uint32_t turn, std::span<const uint8_t> bytes, const server::snapshots::BlockSequences& blocks) {
			    _journal[turn].assign(bytes.begin(), bytes.end());
			    _journalBlocks[turn] = blocks;
		    });
	}

	/// The server reads its sockets, but closes no turn
	void ServerReads()
	{
		_now += k_Turn;
		_network.Poll();
		_host->Update();
	}

	/// The server reads its sockets and closes a turn, but no computer reads its socket
	void ServerTurn()
	{
		ServerReads();
		ASSERT_TRUE(_host->HasTurn(_serverWorld.turns));
		const auto turn = _serverWorld.turns;
		const auto commands = _host->TakeTurn(turn);
		_host->TurnChecksum(turn + 1, _serverWorld.Play(turn, commands));
	}

	/// The server stops with no warning. A new server continues the game from the room and the world of a snapshot, and
	/// does again the turns of the journal after the snapshot, less the last turns that the journal lost. The new server
	/// keeps the files of the editor that the snapshot and the journal kept.
	void RestartFrom(const server::snapshots::Room& room, uint32_t turn, const FakeWorld& world,
	                 const std::vector<Computer*>& computers, uint32_t lostTurns = 0,
	                 const server::snapshots::BlockSequences& snapshotBlocks = {},
	                 std::vector<server::snapshots::EditorFile> editorFiles = {})
	{
		for (auto* computer : computers)
		{
			_network.Cut(computer->sockets->Ids().back());
		}
		_host.reset();
		MakeHost(2);
		GiveWorldState();
		_serverWorld = world;
		auto replayed = _journal;
		for (uint32_t i = 0; i < lostTurns && !replayed.empty(); ++i)
		{
			replayed.erase(std::prev(replayed.end()));
		}
		const auto savedRoom = server::snapshots::DecodeRoom(server::snapshots::EncodeRoom(room));
		ASSERT_TRUE(savedRoom.has_value());
		// The last blocks of the gods come from the last turn that the journal kept, else from the snapshot
		auto blocks = snapshotBlocks;
		if (const auto last = replayed.empty() ? _journalBlocks.end() : _journalBlocks.find(replayed.rbegin()->first);
		    last != _journalBlocks.end() && last->first >= turn)
		{
			blocks = last->second;
		}
		_host->Restore(*savedRoom, turn, std::move(replayed), blocks, std::move(editorFiles));
		KeepJournal();
		while (_host->Replaying())
		{
			const auto next = _serverWorld.turns;
			ASSERT_TRUE(_host->HasTurn(next));
			const auto commands = _host->TakeTurn(next);
			_host->TurnChecksum(next + 1, _serverWorld.Play(next, commands));
		}
	}

	std::map<uint32_t, std::vector<uint8_t>> _journal;
	std::map<uint32_t, server::snapshots::BlockSequences> _journalBlocks;
	std::shared_ptr<const server::SessionHasherInterface> _hasher = std::make_shared<FakeSessionHasher>();
	int64_t _now {1'000'000};
	int _secrets {0};
	/// The turns that the room of MakeHost keeps to send again
	uint32_t _keptTurns {3200};
	LoopbackSockets _network;
	std::unique_ptr<LockstepHost> _host;
	FakeWorld _serverWorld;
};

} // namespace

TEST_F(LockstepTest, TwoGodsJoinAndTheGameStartsWhenBothAreReady)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
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
	EXPECT_FALSE(_host->Started());
	// No turn before the start
	EXPECT_EQ(_serverWorld.turns, 0u);

	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 6);
	EXPECT_TRUE(_host->Started());
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
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 7);
	ASSERT_TRUE(_host->Started());
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
	EXPECT_GE(_serverWorld.turns, 590u);
	EXPECT_GE(alice.world.turns + 3, _serverWorld.turns);
	EXPECT_EQ(alice.played,
	          std::vector(bob.played.begin(),
	                      bob.played.begin() + static_cast<std::ptrdiff_t>(std::min(alice.played.size(), bob.played.size()))));
	// The worlds stay in step: every compared checksum is the same on the clients and on the server
	EXPECT_GT(alice.session.GetCompares().compared, 580u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
	const auto& seats = _host->Seats();
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

TEST_F(LockstepTest, AfterTheLandOfAScenarioLoadsEveryComputerFollowsTheTurnsOfTheNewLand)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 7);
	ASSERT_TRUE(_host->Started());
	// Each world loads the new land before the same turn, as the command of a testbed scenario does
	constexpr uint32_t k_LandBefore = 30;
	_serverWorld.landBefore = k_LandBefore;
	alice.world.landBefore = k_LandBefore;
	bob.world.landBefore = k_LandBefore;
	const auto play = [&](int frames, bool bobReads) {
		for (int i = 0; i < frames; ++i)
		{
			if (i % 5 == 0)
			{
				alice.session.Send(HandTap {.object = Entity(static_cast<uint32_t>(i))});
				bob.session.Send(HandTake {.object = Entity(static_cast<uint32_t>(i))});
			}
			Frame(bobReads ? std::vector<Computer*> {&alice, &bob} : std::vector<Computer*> {&alice});
		}
	};
	for (int i = 0; i < 100 && _serverWorld.turns < k_LandBefore - 5; ++i)
	{
		play(1, true);
	}
	// Bob is late: the server and alice load the new land and do its first turns before bob does the last turns of the
	// old land
	for (int i = 0; i < 100 && (_serverWorld.lands == 0 || _serverWorld.turns < 5); ++i)
	{
		play(1, false);
	}
	EXPECT_EQ(_serverWorld.lands, 1u);
	EXPECT_EQ(alice.world.lands, 1u);
	EXPECT_EQ(bob.world.lands, 0u);
	play(200, true);
	EXPECT_EQ(bob.world.lands, 1u);
	// The worlds stay in step after the land: every computer does the turns of the new land
	EXPECT_GE(_serverWorld.turns, 190u);
	EXPECT_GE(alice.world.turns + 3, _serverWorld.turns);
	EXPECT_GE(bob.world.turns + 3, _serverWorld.turns);
	// Each turn that both computers did has the same commands
	const auto both = static_cast<std::ptrdiff_t>(std::min(alice.played.size(), bob.played.size()));
	EXPECT_TRUE(std::equal(alice.played.begin(), alice.played.begin() + both, bob.played.begin()));
	EXPECT_GT(alice.session.GetCompares().compared, 200u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_GT(bob.session.GetCompares().compared, 200u);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
	// The server compares no checksum of the old land with a checksum of the new land
	const auto& seats = _host->Seats();
	EXPECT_GT(seats[1].compares.compared, 180u);
	EXPECT_EQ(seats[0].compares.compared, seats[0].compares.same);
	EXPECT_EQ(seats[1].compares.compared, seats[1].compares.same);
	EXPECT_EQ(_host->TurnRestarts(), 1u);
}

TEST_F(LockstepTest, TheFirstTurnOfALandThatAScenarioLoadsGivesTheJoinsOfTheGodsAgain)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 7);
	ASSERT_TRUE(_host->Started());
	// Each world loads the new land before the same turn, after the humans joined on the first land
	constexpr uint32_t k_LandBefore = 30;
	_serverWorld.landBefore = k_LandBefore;
	alice.world.landBefore = k_LandBefore;
	bob.world.landBefore = k_LandBefore;
	for (int i = 0; i < 200 && (_serverWorld.lands == 0 || _serverWorld.turns < 10); ++i)
	{
		Frame({&alice, &bob});
	}
	ASSERT_EQ(_serverWorld.lands, 1u);
	// Each god joined on the first land and again in the first turn of the new land, on the server and on each client
	const std::vector<std::pair<uint32_t, PlayerNames>> expected {{0u, PlayerNames::PLAYER_ONE},
	                                                              {0u, PlayerNames::PLAYER_TWO},
	                                                              {1u, PlayerNames::PLAYER_ONE},
	                                                              {1u, PlayerNames::PLAYER_TWO}};
	EXPECT_EQ(_serverWorld.joins, expected);
	EXPECT_EQ(alice.world.joins, expected);
	EXPECT_EQ(bob.world.joins, expected);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, AWorldThatLoadsALandThatTheServerDidNotLoadDoesNoMoreTurns)
{
	MakeHost(1);
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 7);
	ASSERT_TRUE(_host->Started());
	alice.world.landBefore = 20;
	Frames({&alice}, 60);
	EXPECT_EQ(alice.world.lands, 1u);
	// The session has no turn of a new land: the world waits, and no checksum of the other world differs
	EXPECT_LE(alice.world.turns, 1u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
}

TEST_F(LockstepTest, TheServerRefusesBadTicketsLateJoinersAndTheCommandsOfObservers)
{
	MakeHost(1);
	Computer bad(_network, _now, "nothing");
	Computer watcher(_network, _now, "observer:carol");
	Frames({&bad, &watcher}, 3);
	EXPECT_EQ(bad.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(bad.session.RefusedReason(), "ticket");
	EXPECT_EQ(watcher.session.GetJoined()->role, game_socket::Role::Observer);
	EXPECT_EQ(_host->Observers(), 1u);
	// An observer alone does not start the game
	EXPECT_FALSE(_host->Started());

	Computer alice(_network, _now, "god:alice");
	Frames({&watcher, &alice}, 6);
	ASSERT_TRUE(_host->Started());
	// The observer gets the turns too, and sends no command
	watcher.session.Send(HandTap {.object = Entity(1)});
	Frames({&watcher, &alice}, 20);
	EXPECT_GT(watcher.world.turns, 10u);
	EXPECT_EQ(watcher.session.GetCompares().compared, watcher.session.GetCompares().same);

	// A player who comes after the start needs the live world. This server has no world state to send.
	Computer late(_network, _now, "god:dave");
	Frames({&watcher, &alice, &late}, 3);
	EXPECT_EQ(late.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(late.session.RefusedReason(), "started");
}

TEST_F(LockstepTest, AClientCannotSendTheLeaveOfTheServer)
{
	MakeHost(1);
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 6);
	alice.session.Send(PlayerLeft {});
	alice.session.Send(PlayerJoined {});
	alice.session.Send(HandTap {.object = Entity(2)});
	Frames({&alice}, 3);
	EXPECT_EQ(_host->RefusedBlocks(), 2u);
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Taken);
}

TEST_F(LockstepTest, AServerLeavesOutTheFightCheatWithoutItsTestCommands)
{
	MakeHost(1);
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 6);
	alice.session.Send(CreatureFightCheat {});
	Frames({&alice}, 3);
	EXPECT_EQ(_host->RefusedBlocks(), 1u);
	EXPECT_EQ(_host->Seats()[0].commands.count(static_cast<uint8_t>(turn_codec::CommandTag::CreatureFightCheat)), 0u);
}

TEST_F(LockstepTest, AServerWithItsTestCommandsOnTakesTheFightCheat)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 7, true);
	Computer bob(_network, _now, "god:bob");
	Frames({&bob}, 6);
	bob.session.Send(CreatureFightCheat {});
	Frames({&bob}, 3);
	EXPECT_EQ(_host->RefusedBlocks(), 0u);
	EXPECT_EQ(_host->Seats()[0].commands.at(static_cast<uint8_t>(turn_codec::CommandTag::CreatureFightCheat)), 1u);
}

// A changed client can send the commands of a single player game in a network game. A real client never sends them, so
// the server leaves them out of its turns, and the other commands of the block stay.
TEST_F(LockstepTest, TheServerLeavesOutTheCommandsOfASinglePlayerGame)
{
	MakeHost(1);
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 6);
	alice.session.Send(QuickSave {});
	alice.session.Send(QuickLoad {});
	alice.session.Send(MusicMood {.creature = Entity(3), .track = 9, .mood = 0.5f});
	alice.session.Send(CreatureTattoo {.creature = Entity(3)});
	alice.session.Send(HandTap {.object = Entity(2)});
	Frames({&alice}, 3);
	EXPECT_EQ(_host->RefusedBlocks(), 4u);
	const auto& commands = _host->Seats()[0].commands;
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

TEST(LockstepRules, OnlyAPlayerWithTheEditorClaimSendsTheEditsAndTheCheats)
{
	EXPECT_TRUE(OnlyEditorSends(EditorEdit {}));
	EXPECT_TRUE(OnlyEditorSends(DebugCheat {}));
	EXPECT_FALSE(OnlyEditorSends(HandTap {}));
	EXPECT_FALSE(OnlyEditorSends(CreatureFightCheat {}));
	// An observer sends the edits of the editor only with the editor claim, and never a debug cheat, as it has no god
	EXPECT_TRUE(ServerTakesFromObserver(EditorEdit {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(EditorEdit {}, false));
	EXPECT_FALSE(ServerTakesFromObserver(DebugCheat {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(HandTap {}, true));
	EXPECT_FALSE(server::LeavesOut(EditorEdit {}, false));
	EXPECT_FALSE(server::LeavesOut(DebugCheat {}, false));
	// An edit of a creature is an edit of the editor
	EXPECT_TRUE(OnlyEditorSends(EditorCreatureEdit {}));
	EXPECT_TRUE(ServerTakesFromObserver(EditorCreatureEdit {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(EditorCreatureEdit {}, false));
	EXPECT_FALSE(server::LeavesOut(EditorCreatureEdit {}, false));
	// An act of the creature tools is an edit of the editor too. An observer with the editor claim also sends the tattoos.
	EXPECT_TRUE(OnlyEditorSends(EditorCreatureAct {}));
	EXPECT_TRUE(ServerTakesFromObserver(EditorCreatureAct {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(EditorCreatureAct {}, false));
	EXPECT_FALSE(server::LeavesOut(EditorCreatureAct {}, false));
	// A new creature of the creature tools is an edit of the editor too
	EXPECT_TRUE(OnlyEditorSends(EditorCreatureSpawn {}));
	EXPECT_TRUE(ServerTakesFromObserver(EditorCreatureSpawn {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(EditorCreatureSpawn {}, false));
	EXPECT_FALSE(server::LeavesOut(EditorCreatureSpawn {}, false));
	EXPECT_TRUE(OnlyEditorSends(EditorVillagerEdit {}));
	EXPECT_TRUE(ServerTakesFromObserver(EditorVillagerEdit {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(EditorVillagerEdit {}, false));
	EXPECT_FALSE(server::LeavesOut(EditorVillagerEdit {}, false));
	// The strokes and the paths of the land tools are edits of the editor too
	EXPECT_TRUE(OnlyEditorSends(EditorLandStroke {}));
	EXPECT_TRUE(ServerTakesFromObserver(EditorLandStroke {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(EditorLandStroke {}, false));
	EXPECT_FALSE(server::LeavesOut(EditorLandStroke {}, false));
	// The mind loads of the creature tools are edits of the editor too
	EXPECT_TRUE(OnlyEditorSends(EditorMindLoad {}));
	EXPECT_TRUE(ServerTakesFromObserver(EditorMindLoad {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(EditorMindLoad {}, false));
	EXPECT_FALSE(server::LeavesOut(EditorMindLoad {}, false));
	EXPECT_TRUE(OnlyEditorSends(EditorPath {}));
	EXPECT_TRUE(ServerTakesFromObserver(EditorPath {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(EditorPath {}, false));
	EXPECT_FALSE(server::LeavesOut(EditorPath {}, false));
	// The acts of the script tabs are edits of the editor too
	EXPECT_TRUE(OnlyEditorSends(EditorScript {}));
	EXPECT_TRUE(ServerTakesFromObserver(EditorScript {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(EditorScript {}, false));
	EXPECT_FALSE(server::LeavesOut(EditorScript {}, false));
	EXPECT_TRUE(OnlyEditorSends(EditorLandScript {}));
	EXPECT_TRUE(ServerTakesFromObserver(EditorLandScript {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(EditorLandScript {}, false));
	EXPECT_TRUE(OnlyEditorSends(EditorScriptProgram {}));
	EXPECT_TRUE(ServerTakesFromObserver(EditorScriptProgram {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(EditorScriptProgram {}, false));
	// The make again of an undo or a redo is an edit of the editor too
	EXPECT_TRUE(OnlyEditorSends(EditorRemake {}));
	EXPECT_TRUE(ServerTakesFromObserver(EditorRemake {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(EditorRemake {}, false));
	EXPECT_FALSE(server::LeavesOut(EditorRemake {}, false));
	EXPECT_FALSE(OnlyEditorSends(CreatureTattoo {}));
	EXPECT_TRUE(ServerTakesFromObserver(CreatureTattoo {}, true));
	EXPECT_FALSE(ServerTakesFromObserver(CreatureTattoo {}, false));
	// A skip to a land of the story is a debug cheat, and the server refuses it from every player
	EXPECT_TRUE(OnlyEditorSends(DebugLandSkip {}));
	EXPECT_TRUE(ServerAlwaysRefuses(DebugLandSkip {}));
	EXPECT_FALSE(ServerAlwaysRefuses(DebugCheat {}));
	EXPECT_FALSE(ServerAlwaysRefuses(EditorEdit {}));
	EXPECT_FALSE(ServerTakesFromObserver(DebugLandSkip {}, true));
}

TEST_F(LockstepTest, TheServerRefusesASkipToALandOfTheStoryAlsoFromAGodWithTheEditorClaim)
{
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 8);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.GetJoined()->editor);
	alice.session.Send(DebugLandSkip {.land = 3});
	alice.session.Send(DebugCheat {.cheat = 0, .on = true});
	bob.session.Send(DebugLandSkip {.land = 2});
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(_host->RefusedEditorCommands(), 2u);
	const auto count = [this](size_t seat, turn_codec::CommandTag tag) {
		const auto& commands = _host->Seats()[seat].commands;
		const auto found = commands.find(static_cast<uint8_t>(tag));
		return found != commands.end() ? found->second : 0u;
	};
	EXPECT_EQ(count(0, turn_codec::CommandTag::DebugLandSkip), 0u);
	EXPECT_EQ(count(1, turn_codec::CommandTag::DebugLandSkip), 0u);
	// The other cheat of the player with the editor claim stays
	EXPECT_EQ(count(0, turn_codec::CommandTag::DebugCheat), 1u);
	EXPECT_GE(alice.session.TakeEditorRefusals(), 1u);
	EXPECT_GE(bob.session.TakeEditorRefusals(), 1u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, TheServerTakesTheEditsAndTheCheatsOnlyFromAGodWithTheEditorClaim)
{
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 8);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.GetJoined().has_value());
	EXPECT_TRUE(alice.session.GetJoined()->editor);
	EXPECT_FALSE(bob.session.GetJoined()->editor);
	const EditorEdit move {.kind = EditorEdit::Kind::Move, .object = Entity(5), .position = {1.0f, 0.0f, 2.0f}};
	alice.session.Send(move);
	alice.session.Send(DebugCheat {.cheat = 0, .on = true});
	bob.session.Send(move);
	bob.session.Send(DebugCheat {.cheat = 1, .on = true});
	bob.session.Send(HandTap {.object = Entity(2)});
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(_host->RefusedEditorCommands(), 2u);
	const auto count = [this](size_t seat, turn_codec::CommandTag tag) {
		const auto& commands = _host->Seats()[seat].commands;
		const auto found = commands.find(static_cast<uint8_t>(tag));
		return found != commands.end() ? found->second : 0u;
	};
	EXPECT_EQ(count(0, turn_codec::CommandTag::EditorEdit), 1u);
	EXPECT_EQ(count(0, turn_codec::CommandTag::DebugCheat), 1u);
	EXPECT_EQ(count(1, turn_codec::CommandTag::EditorEdit), 0u);
	EXPECT_EQ(count(1, turn_codec::CommandTag::DebugCheat), 0u);
	// The other commands of the block of the player with no editor claim stay
	EXPECT_EQ(count(1, turn_codec::CommandTag::HandTap), 1u);
	// Only the player with no editor claim gets the refusal, and the world of the other player does not get the edit
	// The server sends one refusal for each block of commands that it refused commands from
	EXPECT_GE(bob.session.TakeEditorRefusals(), 1u);
	EXPECT_EQ(alice.session.TakeEditorRefusals(), 0u);
	EXPECT_EQ(bob.session.TakeEditorRefusals(), 0u);
	// The worlds stay in step
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, AChangeOfTheEditorRightDuringTheGameTakesEffectAtOnce)
{
	MakeHost(2);
	Computer watcher(_network, _now, "observer:13");
	Frames({&watcher}, 3);
	Computer alice(_network, _now, "god+editor:11");
	Computer bob(_network, _now, "god:12");
	Frames({&alice, &bob, &watcher}, 8);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.GetJoined()->editor);
	ASSERT_FALSE(bob.session.GetJoined()->editor);
	// Staff remove the right of Alice and give it to Bob and to the observer. The next status answer of the lobby tells
	// the server.
	const std::vector<int64_t> editors {12, 13};
	_host->SetEditors(editors);
	EXPECT_FALSE(_host->Seats()[0].editor);
	EXPECT_TRUE(_host->Seats()[1].editor);
	Frames({&alice, &bob, &watcher}, 2);
	EXPECT_FALSE(alice.session.GetJoined()->editor);
	EXPECT_TRUE(bob.session.GetJoined()->editor);
	EXPECT_TRUE(watcher.session.GetJoined()->editor);
	// The server takes the edits and the cheats by the new rights
	const EditorEdit move {.kind = EditorEdit::Kind::Move, .object = Entity(5), .position = {1.0f, 0.0f, 2.0f}};
	alice.session.Send(move);
	alice.session.Send(DebugCheat {.cheat = 0, .on = true});
	bob.session.Send(move);
	Frames({&alice, &bob, &watcher}, 4);
	EXPECT_EQ(_host->RefusedEditorCommands(), 2u);
	const auto& bobCommands = _host->Seats()[1].commands;
	const auto edits = bobCommands.find(static_cast<uint8_t>(turn_codec::CommandTag::EditorEdit));
	EXPECT_TRUE(edits != bobCommands.end() && edits->second == 1u);
	EXPECT_GE(alice.session.TakeEditorRefusals(), 1u);
	EXPECT_EQ(bob.session.TakeEditorRefusals(), 0u);
	// The same rights again change nothing
	_host->SetEditors(editors);
	EXPECT_TRUE(_host->Seats()[1].editor);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, APlayerWithNoAccountKeepsTheEditorClaimOfTheJoin)
{
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:12");
	Frames({&alice, &bob}, 8);
	ASSERT_TRUE(_host->Started());
	_host->SetEditors({});
	EXPECT_TRUE(_host->Seats()[0].editor);
	EXPECT_FALSE(_host->Seats()[1].editor);
	Frames({&alice, &bob}, 2);
	EXPECT_TRUE(alice.session.GetJoined()->editor);
}

TEST_F(LockstepTest, APlayerWhoIsAwayGetsTheChangedEditorRightWhenThePlayerComesBack)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:11");
	Computer bob(_network, _now, "god:12");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());
	_network.Cut(bob.sockets->Ids().back());
	bob.Frame();
	Frames({&alice}, 10);
	ASSERT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Away);
	const std::vector<int64_t> editors {12};
	_host->SetEditors(editors);
	EXPECT_TRUE(_host->Seats()[1].editor);
	EXPECT_FALSE(bob.session.GetJoined()->editor);
	Frames({&alice, &bob}, 10);
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_TRUE(bob.session.GetJoined()->editor);
	EXPECT_FALSE(alice.session.GetJoined()->editor);
}

TEST_F(LockstepTest, ThePauseOfTheEditorEndsWhenTheLobbyRemovesTheLastEditorRight)
{
	using game_socket::EditorTime;
	using game_socket::TimeAct;
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:11");
	Computer bob(_network, _now, "god:12");
	Frames({&alice, &bob}, 20);
	ASSERT_TRUE(_host->Started());
	alice.session.RequestTime(EditorTime {.act = TimeAct::Pause});
	Frames({&alice, &bob}, 5);
	ASSERT_TRUE(_host->RoomClock().paused);
	const auto pausedAt = _serverWorld.turns;
	_host->SetEditors({});
	Frames({&alice, &bob}, 10);
	EXPECT_FALSE(_host->RoomClock().paused);
	EXPECT_GT(bob.world.turns, pausedAt);
	// Without the right, a new pause of Alice is refused
	alice.session.RequestTime(EditorTime {.act = TimeAct::Pause});
	Frames({&alice, &bob}, 5);
	EXPECT_FALSE(_host->RoomClock().paused);
}

TEST_F(LockstepTest, TheJoinAnswerGivesTheDebuggerClaimOfTheTicketApartFromTheEditorClaim)
{
	MakeHost(2);
	Computer watcher(_network, _now, "observer+debugger:carol");
	Frames({&watcher}, 3);
	Computer alice(_network, _now, "god+editor+debugger:alice");
	Computer bob(_network, _now, "god+editor:bob");
	Frames({&alice, &bob, &watcher}, 8);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.GetJoined().has_value());
	ASSERT_TRUE(bob.session.GetJoined().has_value());
	ASSERT_TRUE(watcher.session.GetJoined().has_value());
	EXPECT_TRUE(alice.session.GetJoined()->debugger);
	EXPECT_TRUE(alice.session.GetJoined()->editor);
	EXPECT_FALSE(bob.session.GetJoined()->debugger);
	EXPECT_TRUE(bob.session.GetJoined()->editor);
	EXPECT_TRUE(watcher.session.GetJoined()->debugger);
	EXPECT_FALSE(watcher.session.GetJoined()->editor);
}

TEST_F(LockstepTest, TheServerTakesTheEditsOfTheEditorOnlyWithValuesInTheirRange)
{
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 8);
	ASSERT_TRUE(_host->Started());
	alice.session.Send(EditorEdit {.kind = EditorEdit::Kind::Scale, .object = Entity(5), .scale = 2.0f});
	// A changed client can send a scale that the editor does not give
	alice.session.Send(EditorEdit {.kind = EditorEdit::Kind::Scale, .object = Entity(5), .scale = 500.0f});
	alice.session.Send(EditorEdit {.kind = EditorEdit::Kind::Scale, .object = entt::null, .scale = 2.0f});
	bob.session.Send(EditorEdit {.kind = EditorEdit::Kind::Scale, .object = Entity(5), .scale = 2.0f});
	// A new creature with a size that the creature tools do not give
	alice.session.Send(EditorCreatureSpawn {.species = 1, .scale = 40.0f});
	// A life of a villager above full
	alice.session.Send(EditorVillagerEdit {.villager = Entity(6), .field = EditorVillagerEdit::Field::Life, .amount = 2.0f});
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(_host->RefusedEditorValues(), 4u);
	EXPECT_EQ(_host->RefusedEditorCommands(), 1u);
	const auto count = [this](size_t seat) {
		const auto& commands = _host->Seats()[seat].commands;
		const auto found = commands.find(static_cast<uint8_t>(turn_codec::CommandTag::EditorEdit));
		return found != commands.end() ? found->second : 0u;
	};
	EXPECT_EQ(count(0), 1u);
	EXPECT_EQ(count(1), 0u);
	// A refused value gives no line to the player with the editor claim
	EXPECT_EQ(alice.session.TakeEditorRefusals(), 0u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, TheServerTakesTheLandToolsOnlyFromTheEditorAndInTheirLimits)
{
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 8);
	ASSERT_TRUE(_host->Started());
	const EditorLandStroke raise {.mode = 0, .innerRadius = 10.0f, .outerRadius = 30.0f, .step = 4, .centre = {800.0f, 900.0f}};
	const EditorPath footpath {.kind = 0, .points = {{100.0f, 100.0f}, {140.0f, 120.0f}}};
	alice.session.Send(raise);
	alice.session.Send(footpath);
	// A player with no editor claim sends a stroke and a path
	bob.session.Send(raise);
	bob.session.Send(footpath);
	// A changed client can send values that the land tools do not give
	auto wide = raise;
	wide.outerRadius = openblack::editor::land_brush::k_MostRadius * 2.0f;
	alice.session.Send(wide);
	auto steep = raise;
	steep.step = openblack::editor::land_brush::k_MostStep + 1;
	alice.session.Send(steep);
	auto country = raise;
	country.mode = 2;
	country.country = openblack::editor::land_brush::k_MostCountries;
	alice.session.Send(country);
	auto away = raise;
	away.centre = {-50.0f, 900.0f};
	alice.session.Send(away);
	auto longPath = footpath;
	longPath.points.push_back({9000.0f, 100.0f});
	alice.session.Send(longPath);
	Frames({&alice, &bob}, 6);
	EXPECT_EQ(_host->RefusedEditorValues(), 5u);
	EXPECT_EQ(_host->RefusedEditorCommands(), 2u);
	const auto count = [this](size_t seat, turn_codec::CommandTag tag) {
		const auto& commands = _host->Seats()[seat].commands;
		const auto found = commands.find(static_cast<uint8_t>(tag));
		return found != commands.end() ? found->second : 0u;
	};
	EXPECT_EQ(count(0, turn_codec::CommandTag::EditorLandStroke), 1u);
	EXPECT_EQ(count(0, turn_codec::CommandTag::EditorPath), 1u);
	EXPECT_EQ(count(1, turn_codec::CommandTag::EditorLandStroke), 0u);
	EXPECT_EQ(count(1, turn_codec::CommandTag::EditorPath), 0u);
	EXPECT_GE(bob.session.TakeEditorRefusals(), 1u);
	EXPECT_EQ(alice.session.TakeEditorRefusals(), 0u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, TheServerTakesTheUndoAndRedoOfTheEditorOnlyFromTheEditorAndInTheirLimits)
{
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 8);
	ASSERT_TRUE(_host->Started());
	const EditorEdit pose {.kind = EditorEdit::Kind::Pose, .object = Entity(5), .request = 1, .poseScale = {2.0f, 2.0f, 2.0f}};
	const EditorRemake remake {.request = 2, .placeKind = 3, .placeType = 1};
	alice.session.Send(pose);
	alice.session.Send(remake);
	// A player with no editor claim sends a pose and a make again
	bob.session.Send(pose);
	bob.session.Send(remake);
	// A changed client can send values that the editor does not give
	auto huge = pose;
	huge.poseScale.x = 500.0f;
	alice.session.Send(huge);
	auto noThing = pose;
	noThing.object = entt::null;
	alice.session.Send(noThing);
	auto treeWithBody = remake;
	treeWithBody.creature = true;
	alice.session.Send(treeWithBody);
	auto bigCreature = EditorRemake {.placeKind = 0, .placeType = 1, .creature = true, .size = 40.0f};
	alice.session.Send(bigCreature);
	Frames({&alice, &bob}, 6);
	EXPECT_EQ(_host->RefusedEditorValues(), 4u);
	EXPECT_EQ(_host->RefusedEditorCommands(), 2u);
	const auto count = [this](size_t seat, turn_codec::CommandTag tag) {
		const auto& commands = _host->Seats()[seat].commands;
		const auto found = commands.find(static_cast<uint8_t>(tag));
		return found != commands.end() ? found->second : 0u;
	};
	EXPECT_EQ(count(0, turn_codec::CommandTag::EditorEdit), 1u);
	EXPECT_EQ(count(0, turn_codec::CommandTag::EditorRemake), 1u);
	EXPECT_EQ(count(1, turn_codec::CommandTag::EditorEdit), 0u);
	EXPECT_EQ(count(1, turn_codec::CommandTag::EditorRemake), 0u);
	EXPECT_GE(bob.session.TakeEditorRefusals(), 1u);
	EXPECT_EQ(alice.session.TakeEditorRefusals(), 0u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, EveryComputerHasTheMindFileOfTheEditorAtTheTurnOfItsLoad)
{
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 8);
	ASSERT_TRUE(_host->Started());
	const std::vector<uint8_t> file {'M', 'I', 'N', 'D', 1, 2, 3};
	const auto hash = openblack::editor::mind_loads::HashOf(file);
	// The file goes on the game socket first, then the command in a block of commands
	ASSERT_TRUE(alice.session.SendEditorMindFile(file));
	alice.session.Send(EditorMindLoad {.use = EditorMindLoad::Use::LoadInto, .file = hash, .creature = Entity(5)});
	alice.session.Send(EditorMindLoad {.use = EditorMindLoad::Use::Spawn, .file = hash, .spawn = {.species = 2}});
	Frames({&alice, &bob}, 6);
	const auto& commands = _host->Seats()[0].commands;
	const auto found = commands.find(static_cast<uint8_t>(turn_codec::CommandTag::EditorMindLoad));
	ASSERT_NE(found, commands.end());
	EXPECT_EQ(found->second, 2u);
	// The server, the sender and the other player had the file at the turn of each load
	EXPECT_EQ(_serverWorld.mindFilesAtTurn, (std::vector<bool> {true, true}));
	EXPECT_EQ(alice.world.mindFilesAtTurn, (std::vector<bool> {true, true}));
	EXPECT_EQ(bob.world.mindFilesAtTurn, (std::vector<bool> {true, true}));
	const auto kept = bob.session.EditorMindFile(hash);
	EXPECT_EQ(std::vector<uint8_t>(kept.begin(), kept.end()), file);
	EXPECT_EQ(_host->RefusedEditorCommands(), 0u);
	EXPECT_EQ(_host->RefusedEditorValues(), 0u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, TheServerTakesAMindFileAndItsLoadOnlyFromTheEditorAndOnlyForAKeptFile)
{
	MakeHost(2, 5LL * 60 * 1'000'000, 7, false, false,
	         [](std::span<const uint8_t> data) { return !data.empty() && data.front() == 'M'; });
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 8);
	ASSERT_TRUE(_host->Started());
	const std::vector<uint8_t> file {'M', 9};
	const std::vector<uint8_t> bad {'X', 9};
	const auto hash = openblack::editor::mind_loads::HashOf(file);
	const auto badHash = openblack::editor::mind_loads::HashOf(bad);
	// A player with no editor claim sends a file and a load
	ASSERT_TRUE(bob.session.SendEditorMindFile(file));
	bob.session.Send(EditorMindLoad {.use = EditorMindLoad::Use::LoadInto, .file = hash, .creature = Entity(5)});
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(_host->RefusedEditorCommands(), 2u);
	EXPECT_TRUE(_host->EditorMindFile(hash).empty());
	EXPECT_TRUE(alice.session.EditorMindFile(hash).empty());
	EXPECT_GE(bob.session.TakeEditorRefusals(), 1u);
	// The editor sends a file that does not read, and a load of that file, which the server does not keep
	ASSERT_TRUE(alice.session.SendEditorMindFile(bad));
	alice.session.Send(EditorMindLoad {.use = EditorMindLoad::Use::LoadInto, .file = badHash, .creature = Entity(5)});
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(_host->RefusedEditorValues(), 2u);
	EXPECT_TRUE(bob.session.EditorMindFile(badHash).empty());
	EXPECT_EQ(_host->Seats()[0].commands.count(static_cast<uint8_t>(turn_codec::CommandTag::EditorMindLoad)), 0u);
	EXPECT_TRUE(_serverWorld.mindFilesAtTurn.empty());
	EXPECT_EQ(alice.session.TakeEditorRefusals(), 0u);
}

TEST_F(LockstepTest, APlayerWhoComesBackGetsTheMindFilesBeforeTheTurnsThatItMissed)
{
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());
	_network.Cut(bob.sockets->Ids().back());
	Frames({&alice}, 1);
	bob.Frame();
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Reconnecting);
	// The editor loads a mind while the other player is away
	const std::vector<uint8_t> file {'M', 7, 7};
	const auto hash = openblack::editor::mind_loads::HashOf(file);
	ASSERT_TRUE(alice.session.SendEditorMindFile(file));
	alice.session.Send(EditorMindLoad {.use = EditorMindLoad::Use::LoadInto, .file = hash, .creature = Entity(5)});
	Frames({&alice}, 10);
	ASSERT_EQ(alice.world.mindFilesAtTurn, (std::vector<bool> {true}));
	EXPECT_TRUE(bob.session.EditorMindFile(hash).empty());
	Frames({&alice, &bob}, 10);
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	// The kept turn with the load came after the file
	EXPECT_EQ(bob.world.mindFilesAtTurn, (std::vector<bool> {true}));
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

/// A file of the script tabs that is larger than one part, with bytes that differ from part to part
std::vector<uint8_t> ScriptFileOfParts(size_t parts, uint8_t first)
{
	std::vector<uint8_t> file(((parts - 1) * game_socket::k_MostEditorScriptPartBytes) + 100);
	for (size_t i = 0; i < file.size(); ++i)
	{
		file[i] = static_cast<uint8_t>(first + (i * 7));
	}
	file.front() = first;
	return file;
}

TEST_F(LockstepTest, EveryComputerHasTheScriptFilesOfTheEditorAtTheTurnOfTheirUse)
{
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 8);
	ASSERT_TRUE(_host->Started());
	using Kind = game_socket::EditorScriptFile::Kind;
	// A program of three parts, the source of a script and a land script
	const auto program = ScriptFileOfParts(3, 'P');
	const std::string source = "begin script X\nend script X\n";
	const std::string land = "VERSION(2.0)\n";
	const auto bytesOf = [](const std::string& text) { return std::vector<uint8_t>(text.begin(), text.end()); };
	ASSERT_TRUE(alice.session.SendEditorScriptFile(Kind::Program, program));
	ASSERT_TRUE(alice.session.SendEditorScriptFile(Kind::Source, bytesOf(source)));
	ASSERT_TRUE(alice.session.SendEditorScriptFile(Kind::LandScript, bytesOf(land)));
	alice.session.Send(
	    EditorScriptProgram {.use = EditorScriptProgram::Use::Open, .file = openblack::editor::mind_loads::HashOf(program)});
	alice.session.Send(EditorScriptProgram {
	    .use = EditorScriptProgram::Use::Compile, .file = openblack::editor::mind_loads::HashOf(bytesOf(source)), .script = 4});
	alice.session.Send(
	    EditorLandScript {.act = EditorLandScript::Act::Open, .file = openblack::editor::mind_loads::HashOf(bytesOf(land))});
	alice.session.Send(EditorScript {.act = EditorScript::Act::HoldAll});
	Frames({&alice, &bob}, 6);
	const auto& commands = _host->Seats()[0].commands;
	EXPECT_EQ(commands.count(static_cast<uint8_t>(turn_codec::CommandTag::EditorScriptProgram)), 1u);
	EXPECT_EQ(commands.at(static_cast<uint8_t>(turn_codec::CommandTag::EditorScriptProgram)), 2u);
	EXPECT_EQ(commands.at(static_cast<uint8_t>(turn_codec::CommandTag::EditorLandScript)), 1u);
	EXPECT_EQ(commands.at(static_cast<uint8_t>(turn_codec::CommandTag::EditorScript)), 1u);
	// The server, the sender and the other player had each file at the turn of its use
	const std::vector<bool> all {true, true, true};
	EXPECT_EQ(_serverWorld.scriptFilesAtTurn, all);
	EXPECT_EQ(alice.world.scriptFilesAtTurn, all);
	EXPECT_EQ(bob.world.scriptFilesAtTurn, all);
	const auto kept =
	    bob.session.EditorScriptFile(static_cast<uint8_t>(Kind::Program), openblack::editor::mind_loads::HashOf(program));
	EXPECT_EQ(std::vector<uint8_t>(kept.begin(), kept.end()), program);
	EXPECT_EQ(_host->RefusedEditorCommands(), 0u);
	EXPECT_EQ(_host->RefusedEditorValues(), 0u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, TheServerTakesTheScriptFilesAndActsOnlyFromTheEditorAndOnlyForAKeptFile)
{
	MakeHost(2, 5LL * 60 * 1'000'000, 7, false, false, {},
	         [](std::span<const uint8_t> data) { return !data.empty() && data.front() == 'P'; });
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 8);
	ASSERT_TRUE(_host->Started());
	using Kind = game_socket::EditorScriptFile::Kind;
	const auto program = ScriptFileOfParts(2, 'P');
	const auto bad = ScriptFileOfParts(2, 'X');
	const auto hash = openblack::editor::mind_loads::HashOf(program);
	const auto badHash = openblack::editor::mind_loads::HashOf(bad);
	// A player with no editor claim sends a file of two parts, a change of the program and an act
	ASSERT_TRUE(bob.session.SendEditorScriptFile(Kind::Program, program));
	bob.session.Send(EditorScriptProgram {.use = EditorScriptProgram::Use::Open, .file = hash});
	bob.session.Send(EditorScript {.act = EditorScript::Act::HoldAll});
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(_host->RefusedEditorCommands(), 3u);
	EXPECT_TRUE(_host->EditorScriptFile(static_cast<uint8_t>(Kind::Program), hash).empty());
	EXPECT_GE(bob.session.TakeEditorRefusals(), 1u);
	// The editor sends a program that does not read, a change that uses it, and an act with a bad value
	ASSERT_TRUE(alice.session.SendEditorScriptFile(Kind::Program, bad));
	alice.session.Send(EditorScriptProgram {.use = EditorScriptProgram::Use::Open, .file = badHash});
	alice.session.Send(EditorScript {.act = EditorScript::Act::Breakpoint, .index = 2});
	alice.session.Send(EditorLandScript {.act = EditorLandScript::Act::Open, .file = badHash});
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(_host->RefusedEditorValues(), 4u);
	EXPECT_TRUE(alice.session.EditorScriptFile(static_cast<uint8_t>(Kind::Program), badHash).empty());
	const auto& commands = _host->Seats()[0].commands;
	EXPECT_EQ(commands.count(static_cast<uint8_t>(turn_codec::CommandTag::EditorScriptProgram)), 0u);
	EXPECT_EQ(commands.count(static_cast<uint8_t>(turn_codec::CommandTag::EditorScript)), 0u);
	EXPECT_EQ(commands.count(static_cast<uint8_t>(turn_codec::CommandTag::EditorLandScript)), 0u);
	EXPECT_TRUE(_serverWorld.scriptFilesAtTurn.empty());
	// The program of the game uses no file, so the server takes it
	alice.session.Send(EditorScriptProgram {.use = EditorScriptProgram::Use::GamesProgram});
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(commands.count(static_cast<uint8_t>(turn_codec::CommandTag::EditorScriptProgram)), 1u);
	EXPECT_EQ(alice.session.TakeEditorRefusals(), 0u);
}

TEST_F(LockstepTest, APlayerWhoComesBackGetsTheScriptFilesBeforeTheTurnsThatItMissed)
{
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());
	_network.Cut(bob.sockets->Ids().back());
	Frames({&alice}, 1);
	bob.Frame();
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Reconnecting);
	// The editor opens a program of two parts while the other player is away
	const auto program = ScriptFileOfParts(2, 'P');
	const auto hash = openblack::editor::mind_loads::HashOf(program);
	ASSERT_TRUE(alice.session.SendEditorScriptFile(game_socket::EditorScriptFile::Kind::Program, program));
	alice.session.Send(EditorScriptProgram {.use = EditorScriptProgram::Use::Open, .file = hash});
	Frames({&alice}, 10);
	ASSERT_EQ(alice.world.scriptFilesAtTurn, (std::vector<bool> {true}));
	Frames({&alice, &bob}, 10);
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	// The kept turn with the change came after the file
	EXPECT_EQ(bob.world.scriptFilesAtTurn, (std::vector<bool> {true}));
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, AnObserverWithTheEditorClaimSendsTheEditsButNoCheat)
{
	MakeHost(1);
	// The observers join before the start, as this server has no world state for a later join
	Computer watcher(_network, _now, "observer+editor:carol");
	Computer other(_network, _now, "observer:dave");
	Frames({&watcher, &other}, 3);
	Computer alice(_network, _now, "god:alice");
	Frames({&alice, &watcher, &other}, 8);
	ASSERT_TRUE(_host->Started());
	EXPECT_TRUE(watcher.session.GetJoined()->editor);
	const EditorEdit remove {.kind = EditorEdit::Kind::Remove, .object = Entity(7)};
	watcher.session.Send(remove);
	watcher.session.Send(DebugCheat {.cheat = 0, .on = true});
	other.session.Send(remove);
	Frames({&alice, &watcher, &other}, 4);
	// The client of an observer with no editor claim does not send the edit, and no observer client sends a cheat
	EXPECT_EQ(_host->RefusedEditorCommands(), 0u);
	const auto edits = std::ranges::count_if(alice.world.observerCommands, [](const Command& command) {
		return std::holds_alternative<EditorEdit>(command.action);
	});
	const auto cheats = std::ranges::count_if(alice.world.observerCommands, [](const Command& command) {
		return std::holds_alternative<DebugCheat>(command.action);
	});
	EXPECT_EQ(edits, 1);
	EXPECT_EQ(cheats, 0);
}

// A changed client can send bytes that are not a creature file. The server sends empty data in their place, so no
// computer reads them, and each computer brings no creature for that god, as it does for a file that does not read.
TEST_F(LockstepTest, TheServerSendsNoCreatureFileForAFileThatDoesNotRead)
{
	_host = std::make_unique<LockstepHost>(
	    _network, CheckFakeTicket, [this] { return _now; }, [this] { return "secret-" + std::to_string(++_secrets); },
	    server::HostSettings {
	        .start = {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow", .godsRingMap = false},
	        .landChecksum = online::start_rules::LandChecksum(k_LandFile),
	        .minGods = 2,
	        .godSlots = 7,
	        .comeBackMicroseconds = 5LL * 60 * 1'000'000,
	        .keptTurns = 3200,
	        .testCommands = false,
	        .sessionHasher = _hasher,
	        .lanNames = false,
	        .relaysCreature = [](std::span<const uint8_t> data) { return data.empty() || data.front() == 'C'; }});
	Computer alice(_network, _now, "god:alice", k_LandFile, {'C', 'R'}, 0.0f);
	Computer bob(_network, _now, "god:bob", k_LandFile, {'X', 'X'}, 0.0f);
	Frames({&alice, &bob}, 8);
	ASSERT_TRUE(_host->Started());
	for (const player_commands::TurnCommandSourceInterface* source :
	     std::initializer_list<const player_commands::TurnCommandSourceInterface*> {_host.get(), &alice.session, &bob.session})
	{
		const auto first = source->StartCreatureFile(PlayerNames::PLAYER_ONE);
		EXPECT_EQ(std::vector<uint8_t>(first.begin(), first.end()), (std::vector<uint8_t> {'C', 'R'}));
		EXPECT_TRUE(source->StartCreatureFile(PlayerNames::PLAYER_TWO).empty());
	}
}

TEST_F(LockstepTest, AClientThatLosesTheSocketComesBackAndGetsTheTurnsThatItMissed)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());

	// Bob's network stops; his commands wait and are not lost
	_network.Cut(bob.sockets->Ids().back());
	bob.session.Send(HandTap {.object = Entity(5)});
	Frames({&alice}, 1);
	bob.Frame();
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Reconnecting);
	Frames({&alice}, 10);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Away);
	// Alice still plays: the world turns while a god's player is connected
	const auto turnsWhileAway = _serverWorld.turns;
	EXPECT_GT(turnsWhileAway, 35u);

	Frames({&alice, &bob}, 10);
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.session.Resumes(), 1u);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Taken);
	EXPECT_EQ(_host->Seats()[1].resumes, 1u);
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
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 10);
	ASSERT_TRUE(_host->Started());
	alice.session.Leave();
	alice.Frame();
	Frames({}, 3);
	// The leave closed the slot; the defeat goes in the next turn, and then the world waits with no god
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Closed);
	EXPECT_TRUE(_host->Paused());
	const auto turns = _serverWorld.turns;
	Frames({}, 20);
	EXPECT_EQ(_serverWorld.turns, turns);
	EXPECT_FALSE(_host->HasTurn(_serverWorld.turns));
}

TEST_F(LockstepTest, AnAwayGodIsDefeatedAfterTheTimeToComeBack)
{
	MakeHost(2, 3 * 1'000'000);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 10);
	_network.Cut(bob.sockets->Ids().back());
	const std::vector<Command> leaves;
	// Bob's computer stops: no frame of his any more
	for (int i = 0; i < 50; ++i)
	{
		Frame({&alice});
	}
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Closed);
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

TEST_F(LockstepTest, WhenThePlayersOfAllTheGodsLoseTheConnectionTheGameEndsAfterTheShortTime)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(_host->Started());
	_network.Cut(alice.sockets->Ids().back());
	_network.Cut(bob.sockets->Ids().back());
	// Both computers stop. The gods wait 10 seconds, not the time to come back of 5 minutes.
	for (int i = 0; i < 90; ++i)
	{
		Frame({});
	}
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Away);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Away);
	EXPECT_FALSE(_host->GameOver());
	for (int i = 0; i < 30; ++i)
	{
		Frame({});
	}
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Closed);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Closed);
	EXPECT_TRUE(_host->GameOver());
}

TEST_F(LockstepTest, TheShortTimeStartsWhenThePlayerOfTheLastGodLosesTheConnection)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 10);
	_network.Cut(alice.sockets->Ids().back());
	for (int i = 0; i < 50; ++i)
	{
		Frame({&bob});
	}
	_network.Cut(bob.sockets->Ids().back());
	// Alice is away for 13 seconds, but Bob for 8 seconds only: both gods still wait
	for (int i = 0; i < 80; ++i)
	{
		Frame({});
	}
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Away);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Away);
	for (int i = 0; i < 30; ++i)
	{
		Frame({});
	}
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Closed);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Closed);
}

TEST_F(LockstepTest, WhenTheLastConnectedGodLeavesAnAwayGodWaitsOnlyTheShortTime)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 10);
	_network.Cut(bob.sockets->Ids().back());
	Frames({&alice}, 5);
	alice.session.Leave();
	alice.Frame();
	for (int i = 0; i < 120; ++i)
	{
		Frame({});
	}
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Closed);
	EXPECT_TRUE(_host->GameOver());
}

TEST_F(LockstepTest, AnAwayGodKeepsTheTimeToComeBackWhileAnotherGodsPlayerIsConnected)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 10);
	_network.Cut(bob.sockets->Ids().back());
	for (int i = 0; i < 200; ++i)
	{
		Frame({&alice});
	}
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Away);
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Taken);
}

TEST_F(LockstepTest, AClientThatPartsTriesThreeRejoinsAndThenLeaves)
{
	MakeHost(1);
	Computer alice(_network, _now, "god:alice");
	alice.world.partsAfter = 20;
	for (int i = 0; i < 200 && alice.session.GetState() != NetworkSession::State::Ended; ++i)
	{
		Frame({&alice});
	}
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::OutOfSync);
	EXPECT_EQ(alice.session.Resync().Tries(), online::ResyncRule::k_MostTries);
	EXPECT_EQ(alice.session.Resumes(), online::ResyncRule::k_MostTries);
	EXPECT_EQ(alice.session.GetCompares().firstDifferent, 21u);
	ASSERT_TRUE(_host->Seats()[0].compares.firstDifferent.has_value());
	EXPECT_EQ(*_host->Seats()[0].compares.firstDifferent, 21u);
}

TEST_F(LockstepTest, TheHostTellsTheFirstTurnOfEachPart)
{
	MakeHost(1);
	std::vector<std::pair<uint8_t, uint32_t>> parts;
	_host->SetPartListener([&parts](uint8_t seat, uint32_t turn) { parts.emplace_back(seat, turn); });
	Computer alice(_network, _now, "god:alice");
	alice.world.partsAfter = 20;
	for (int i = 0; i < 200 && alice.session.GetState() != NetworkSession::State::Ended; ++i)
	{
		Frame({&alice});
	}
	// The world stays parted for some turns after 21, but only the first turn of the part comes
	ASSERT_FALSE(parts.empty());
	EXPECT_EQ(parts.front(), std::make_pair(uint8_t {0}, 21u));
	EXPECT_LT(parts.size(), _host->Seats()[0].compares.compared - _host->Seats()[0].compares.same);
}

TEST_F(LockstepTest, AClientThatPartsLoadsTheWorldOfTheServerAndStaysInStep)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	alice.world.partsAfter = 20;
	Frames({&alice}, 120);
	EXPECT_EQ(alice.loads, 1u);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(alice.session.Resumes(), 1u);
	EXPECT_EQ(alice.session.Resync().GetState(), online::ResyncRule::State::InStep);
	EXPECT_FALSE(alice.session.ShowsResync());
	// After the load every turn is the same as the server's again
	EXPECT_EQ(alice.world.hash, _serverWorld.hash);
	EXPECT_EQ(alice.world.turns, _serverWorld.turns);
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
	_host->SetPartsOfTurn([](uint32_t turn) {
		return online::PartHashes {{"entities", 1}, {"Transform", 10 + turn}, {"synced seed", 7}};
	});
	Computer alice(_network, _now, "god:alice");
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
	EXPECT_NE(serverPart->find("(the player of god 0)"), std::string::npos) << *serverPart;
	// No line of the server's log names the player
	EXPECT_TRUE(
	    std::ranges::none_of(serverLines, [](const std::string& line) { return line.find("alice") != std::string::npos; }));
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

TEST_F(LockstepTest, TheRoomRefusesAGodWhoseGameDataIsNotTheServers)
{
	MakeHost(2);
	_host->SetDataPrint({{.path = "Data/info.dat", .present = true, .size = 4, .hash = 1},
	                     {.path = "Data/Text/english.txt", .present = true, .size = 4, .hash = 2}});
	const auto print = [](uint64_t infoHash, uint64_t textHash) {
		return [infoHash, textHash](std::span<const std::string> files) {
			EXPECT_EQ(files.size(), 2U);
			return game_socket::DataPrint {
			    .files = {{.path = "Data/info.dat", .present = true, .size = 4, .hash = infoHash},
			              {.path = "Data/Text/english.txt", .present = true, .size = 4, .hash = textHash}},
			    .patch = "1.20",
			    .language = {}};
		};
	};
	// A file of the table of objects refuses the join, another file only goes in the log
	Computer alice(_network, _now, "god:alice");
	alice.printData = print(1, 3);
	Computer bob(_network, _now, "god:bob");
	bob.printData = print(9, 2);
	Frames({&alice, &bob}, 10);
	EXPECT_EQ(bob.session.RefusedReason(), "data");
	EXPECT_EQ(bob.session.RefusedPath(), "Data/info.dat");
	EXPECT_TRUE(alice.session.RefusedReason().empty());
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Waiting);
	EXPECT_TRUE(std::ranges::any_of(bob.session.Log().Lines(),
	                                [](const std::string& line) { return line.find("Data/info.dat") != std::string::npos; }));
	// A god with the same data takes the free seat, and the game starts
	Computer carol(_network, _now, "god:carol");
	carol.printData = print(1, 2);
	Frames({&alice, &carol}, 10);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(carol.session.GetState(), NetworkSession::State::Playing);
}

TEST_F(LockstepTest, TheServerCapturesTheFirstDesyncOfAGodEntityByEntity)
{
	MakeHost(1);
	GiveWorldState();
	_host->SetPartsOfTurn([](uint32_t turn) {
		return online::PartHashes {{"entities", 1}, {"Transform", 10 + turn}, {"synced seed", 7}};
	});
	const auto entities = [](uint64_t change) {
		return [change](std::span<const std::string> names) {
			std::vector<synced_state::PartEntities> parts;
			for (const auto& name : names)
			{
				synced_state::PartEntities part {.part = name, .entities = {}};
				for (uint32_t i = 0; i < 300; ++i)
				{
					part.entities.push_back(
					    {.entity = i, .hash = i == 270 ? change : i + 1, .kind = "Villager", .position = {}});
				}
				parts.push_back(std::move(part));
			}
			return parts;
		};
	};
	std::vector<server::CaptureRecord> records;
	_host->SetCapture({.turn = 0,
	                   .build = "b",
	                   .land = "Land1.txt",
	                   .landSha256 = {},
	                   .map = {},
	                   .seed = 1,
	                   .data = 5,
	                   .patch = {},
	                   .language = {}},
	                  entities(5), [&records](server::CaptureRecord record) { records.push_back(std::move(record)); });
	Computer alice(_network, _now, "god:alice");
	constexpr uint32_t k_PartsAfter = 20;
	alice.parts = [](uint32_t turn) {
		return online::PartHashes {
		    {"entities", 1}, {"Transform", 10 + turn + (turn > k_PartsAfter ? 1 : 0)}, {"synced seed", 7}};
	};
	alice.entities = entities(6);
	alice.saveWorld = [] { return std::vector<uint8_t> {4, 3, 2, 1}; };
	alice.world.partsAfter = k_PartsAfter;
	Frames({&alice}, 120);
	std::string clientLines;
	for (const auto& line : alice.session.Log().Lines())
	{
		clientLines += line + '\n';
	}
	ASSERT_EQ(records.size(), 1U) << clientLines;
	const auto& record = records[0];
	EXPECT_TRUE(record.missing.empty());
	EXPECT_EQ(record.playerWorld, (std::vector<uint8_t> {4, 3, 2, 1}));
	EXPECT_FALSE(record.serverWorld.empty());
	ASSERT_TRUE(record.player.has_value());
	ASSERT_FALSE(record.parts.empty());
	EXPECT_EQ(record.parts[0].part, "Transform");
	ASSERT_EQ(record.parts[0].differences.size(), 1U);
	EXPECT_EQ(record.parts[0].differences[0].entity, 270U);
	// After the capture the client loads the server's world
	EXPECT_EQ(alice.loads, 1u);
	EXPECT_TRUE(std::ranges::any_of(alice.session.Log().Lines(), [](const std::string& line) {
		return line.find("The resync starts") != std::string::npos;
	}));
}

TEST_F(LockstepTest, BothSidesTellTheDesyncWithItsChecksumsAndThePartsThatDiffer)
{
	MakeHost(1);
	GiveWorldState();
	_host->SetPartsOfTurn([](uint32_t turn) {
		return online::PartHashes {{"entities", 1}, {"Transform", 10 + turn}, {"synced seed", 7}};
	});
	struct HostDesync
	{
		uint8_t seat {0};
		uint32_t turn {0};
		uint64_t server {0};
		uint64_t client {0};
	};
	std::vector<HostDesync> hostDesyncs;
	_host->SetDesyncListener([&hostDesyncs](uint8_t seat, uint32_t turn, uint64_t server, uint64_t client) {
		hostDesyncs.push_back({.seat = seat, .turn = turn, .server = server, .client = client});
	});
	std::vector<std::pair<uint8_t, std::vector<std::string>>> hostParts;
	_host->SetPartedListener([&hostParts](uint8_t seat, std::span<const std::string> names) {
		hostParts.emplace_back(seat, std::vector<std::string>(names.begin(), names.end()));
	});
	Computer alice(_network, _now, "god:alice");
	constexpr uint32_t k_PartsAfter = 20;
	alice.parts = [](uint32_t turn) {
		const bool parted = turn > k_PartsAfter;
		return online::PartHashes {{"entities", 1}, {"Transform", 10 + turn + (parted ? 1 : 0)}, {"synced seed", 7}};
	};
	alice.world.partsAfter = k_PartsAfter;
	Frames({&alice}, 60);

	// One desync on each side: the turns that differ until the resync do not count again
	ASSERT_EQ(alice.desyncs.size(), 1u);
	EXPECT_EQ(alice.desyncs[0].turn, k_PartsAfter + 1);
	EXPECT_NE(alice.desyncs[0].local, alice.desyncs[0].server);
	ASSERT_EQ(hostDesyncs.size(), 1u);
	EXPECT_EQ(hostDesyncs[0].seat, 0u);
	EXPECT_EQ(hostDesyncs[0].turn, k_PartsAfter + 1);
	// The two sides see the same two checksums
	EXPECT_EQ(hostDesyncs[0].server, alice.desyncs[0].server);
	EXPECT_EQ(hostDesyncs[0].client, alice.desyncs[0].local);
	EXPECT_EQ(alice.partedNames, (std::vector<std::string> {"Transform"}));
	ASSERT_EQ(hostParts.size(), 1u);
	EXPECT_EQ(hostParts[0].first, 0u);
	EXPECT_EQ(hostParts[0].second, (std::vector<std::string> {"Transform"}));
}

TEST_F(LockstepTest, TheServerLogTellsWhyItRefusesAResume)
{
	auto sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(64);
	auto logger = std::make_shared<spdlog::logger>("game", sink);
	spdlog::drop("game");
	spdlog::register_logger(logger);
	MakeHost(1);
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 5);
	auto client = _network.MakeClient();
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

namespace
{

/// The lines of the "game" log while a test runs
struct GameLogLines
{
	GameLogLines()
	{
		spdlog::drop("game");
		spdlog::register_logger(std::make_shared<spdlog::logger>("game", sink));
	}
	~GameLogLines() { spdlog::drop("game"); }
	GameLogLines(const GameLogLines&) = delete;
	GameLogLines& operator=(const GameLogLines&) = delete;

	/// True when a line has each of the texts
	[[nodiscard]] bool Has(std::initializer_list<std::string_view> texts) const
	{
		return std::ranges::any_of(sink->last_formatted(), [texts](const std::string& line) {
			return std::ranges::all_of(texts, [&line](std::string_view text) { return line.find(text) != std::string::npos; });
		});
	}

	std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(256);
};

} // namespace

// A client with another version of the format of the turns cannot play in the game. The server closes it with "version",
// and the logs of the server and of the client name the two builds. The other god stays.
TEST_F(LockstepTest, TheServerClosesAClientThatSendsAFrameOfAnotherVersionOfTheTurns)
{
	const GameLogLines log;
	MakeHost(2);
	Computer alice(_network, _now, "god:alice", k_LandFile, {'C'}, 0.0f, {}, "0.0.1+old/codec1");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 7);
	ASSERT_TRUE(_host->Started());
	const auto refused = _host->RefusedBlocks();
	const std::vector<uint8_t> block = {static_cast<uint8_t>(turn_codec::k_Version + 1), 0, 0, 0};
	ASSERT_TRUE(alice.sockets->SendBinary(alice.sockets->Ids().back(), block));
	Frames({&alice, &bob}, 3);
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(alice.session.RefusedReason(), "version");
	EXPECT_EQ(_host->RefusedBlocks(), refused + 1);
	const auto server = online::build_id::Own();
	const auto version = std::to_string(turn_codec::k_Version);
	const auto other = std::to_string(turn_codec::k_Version + 1);
	EXPECT_TRUE(log.Has({"refuses connection", ": version: a binary frame of version " + other, "not version " + version,
	                     "the client has the build \"0.0.1+old/codec1\"", "the server has the build \"" + server + "\""}));
	EXPECT_TRUE(log.Has({"refused the client: version", "the client has the build \"0.0.1+old/codec1\"",
	                     "the server has the build \"" + server + "\""}));
	EXPECT_EQ(bob.session.GetEnd(), NetworkSession::End::None);
}

// A frame of the turns that does not read for another cause is only left out: the client stays
TEST_F(LockstepTest, ABinaryFrameThatDoesNotReadForAnotherCauseIsOnlyLeftOut)
{
	MakeHost(1);
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 7);
	ASSERT_TRUE(_host->Started());
	const std::vector<uint8_t> frame = {turn_codec::k_Version, 0xFF};
	ASSERT_TRUE(alice.sockets->SendBinary(alice.sockets->Ids().back(), frame));
	Frames({&alice}, 3);
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::None);
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Taken);
}

// A client of another build can send a join that the server cannot read. The refusal for the protocol names the build of
// the client, when the text gives one, and the build of the server.
TEST_F(LockstepTest, ARefusalForTheProtocolNamesTheBuildsOfTheClientAndOfTheServer)
{
	const GameLogLines log;
	MakeHost(1);
	const auto server = online::build_id::Own();
	auto client = _network.MakeClient();
	const auto id =
	    client->Connect({.url = "wss://127.0.0.1/game/1", .headers = {}, .subprotocols = {}, .serverKey = std::nullopt}, {});
	Frames({}, 1);
	client->Poll();
	client->SendText(id, R"({"type":"room.join","ticket":5,"build":"0.0.1+old/codec1"})");
	Frames({}, 2);
	EXPECT_TRUE(log.Has({"refuses connection", ": protocol: a text frame that the server does not know",
	                     "the client has the build \"0.0.1+old/codec1\"", "the server has the build \"" + server + "\""}));

	auto older = _network.MakeClient();
	const auto olderId =
	    older->Connect({.url = "wss://127.0.0.1/game/1", .headers = {}, .subprotocols = {}, .serverKey = std::nullopt}, {});
	Frames({}, 1);
	older->Poll();
	older->SendText(olderId, R"({"type":"room.join","ticket":5})");
	Frames({}, 2);
	EXPECT_TRUE(log.Has({"refuses connection", ": protocol: a text frame that the server does not know",
	                     "the client gave no build id", "the server has the build \"" + server + "\""}));
}

// A refusal for another cause does not name the builds
TEST_F(LockstepTest, ARefusalForAnotherCauseDoesNotNameTheBuilds)
{
	const GameLogLines log;
	MakeHost(1);
	Computer alice(_network, _now, "not-a-ticket", k_LandFile, {'C'}, 0.0f, {}, "0.1.0+abc/codec34");
	Frames({&alice}, 3);
	ASSERT_EQ(alice.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(alice.session.RefusedReason(), "ticket");
	EXPECT_TRUE(log.Has({"refuses connection", "ticket"}));
	EXPECT_FALSE(log.Has({"refuses connection", "the server has the build"}));
	EXPECT_FALSE(log.Has({"refused the client", "the server has the build"}));
}

TEST_F(LockstepTest, TheGodsSendTheirCreaturesToAllPlayersBeforeTheStart)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice", k_LandFile, {'A', 'L'}, -0.5f);
	Computer bob(_network, _now, "god:bob", k_LandFile, {}, 0.7f);
	Computer watcher(_network, _now, "observer:watcher", std::nullopt, {'W'}, 0.25f);
	Frames({&alice, &bob, &watcher}, 8);
	ASSERT_TRUE(_host->Started());
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
	     std::initializer_list<const player_commands::TurnCommandSourceInterface*> {_host.get(), &alice.session, &bob.session,
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
	EXPECT_FALSE(_host->OwnGod().has_value());
}

// The server sends the symbol of each god's player to all players. It is for the view only, so the server keeps none.
TEST_F(LockstepTest, TheGodsSendTheSymbolsOfTheirPlayersToAllPlayers)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice", k_LandFile, {'A'});
	Computer bob(_network, _now, "god:bob", k_LandFile, {});
	Computer watcher(_network, _now, "observer:watcher", std::nullopt, {});
	const std::vector<uint8_t> picture(game_socket::k_SymbolPictureBytes, 0x3C);
	alice.symbol = {.design = 4, .picture = picture};
	bob.symbol = {.design = 11, .picture = {}};
	Frames({&alice, &bob, &watcher}, 8);
	ASSERT_TRUE(_host->Started());
	for (const auto* computer : {&alice, &bob, &watcher})
	{
		EXPECT_EQ(computer->session.StartSymbol(PlayerNames::PLAYER_ONE), alice.symbol);
		EXPECT_EQ(computer->session.StartSymbol(PlayerNames::PLAYER_TWO), bob.symbol);
		EXPECT_FALSE(computer->session.StartSymbol(PlayerNames::PLAYER_THREE).has_value());
	}
}

TEST_F(LockstepTest, AGodWithoutTheLandStopsTheStartAndIsNotReady)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob", std::nullopt);
	Frames({&alice, &bob}, 8);
	EXPECT_FALSE(_host->Started());
	EXPECT_FALSE(_host->Starting());
	EXPECT_FALSE(_host->Seats()[1].ready);
	EXPECT_TRUE(_host->Seats()[0].ready);
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
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob", std::vector<uint8_t> {'X'});
	Frames({&alice, &bob}, 8);
	EXPECT_FALSE(_host->Started());
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Ended);
	EXPECT_EQ(bob.session.RefusedReason(), "kicked");
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Open);
	EXPECT_EQ(alice.session.TakeStartEvent(), online::StartEvent::HasLand);
	EXPECT_EQ(alice.session.TakeStartEvent(), online::StartEvent::Cancelled);
}

TEST_F(LockstepTest, AGodWhoLeavesDuringTheStartStopsIt)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	auto bob = std::make_unique<Computer>(_network, _now, "god:bob");
	// The start begins, then Bob leaves before the game starts
	for (int i = 0; i < 8 && !_host->Starting(); ++i)
	{
		Frame({&alice, bob.get()});
	}
	ASSERT_TRUE(_host->Starting());
	bob->session.Leave();
	bob->Frame();
	Frames({&alice}, 6);
	EXPECT_FALSE(_host->Started());
	EXPECT_FALSE(_host->Starting());
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
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 7);
	ASSERT_TRUE(_host->Started());
	// Every computer names each god by the name of its player
	ASSERT_TRUE(bob.session.GetStart().has_value());
	ASSERT_GE(bob.session.GetStart()->names.size(), 2u);
	EXPECT_EQ(bob.session.GetStart()->names[0], "alice");
	EXPECT_EQ(bob.session.GetStart()->names[1], "bob");
	Frames({&alice, &bob}, 50);
	bob.session.StopGame();
	bob.session.StopGame();
	Frames({&alice, &bob}, 50);
	ASSERT_TRUE(_host->StoppedAfter().has_value());
	const auto last = *_host->StoppedAfter();
	EXPECT_EQ(_serverWorld.turns, last + 1);
	EXPECT_EQ(alice.world.turns, last + 1);
	EXPECT_EQ(bob.world.turns, last + 1);
	EXPECT_EQ(alice.session.StoppedAfter(), last);
	EXPECT_EQ(bob.session.StoppedAfter(), last);
	EXPECT_EQ(alice.world.hash, _serverWorld.hash);
	EXPECT_EQ(bob.world.hash, _serverWorld.hash);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	// The game ends when the gods leave, with no turn more
	alice.session.Leave();
	bob.session.Leave();
	Frames({&alice, &bob}, 5);
	EXPECT_TRUE(_host->GameOver());
	EXPECT_EQ(_serverWorld.turns, last + 1);
}

TEST_F(LockstepTest, ANewProcessOfAnAwayGodsPlayerComesBackIntoTheGodWithTheWorldOfTheServer)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	auto bob = std::make_unique<Computer>(_network, _now, "god:bob");
	Frames({&alice, bob.get()}, 30);
	ASSERT_TRUE(_host->Started());
	const auto oldSession = bob->session.GetJoined()->session;

	// Bob's program stops: the god waits for him
	_network.Cut(bob->sockets->Ids().back());
	bob.reset();
	Frames({&alice}, 20);
	ASSERT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Away);

	// Bob starts the program again with a new ticket of the same account
	Computer again(_network, _now, "god:bob");
	Frames({&alice, &again}, 40);
	ASSERT_TRUE(again.session.GetJoined().has_value());
	EXPECT_EQ(again.session.GetJoined()->role, game_socket::Role::God);
	EXPECT_EQ(again.session.GetJoined()->slot, std::optional<uint8_t>(1));
	EXPECT_NE(again.session.GetJoined()->session, oldSession);
	EXPECT_EQ(again.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(again.loads, 1u);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Taken);
	EXPECT_EQ(_host->Seats()[1].resumes, 1u);
	// The god plays on: its commands reach the server, and every turn is the same as the server's
	again.session.Send(HandTap {.object = Entity(5)});
	Frames({&alice, &again}, 20);
	EXPECT_EQ(again.world.hash, _serverWorld.hash);
	EXPECT_EQ(again.world.turns, _serverWorld.turns);
	EXPECT_GT(again.session.GetCompares().same, 40u);
	EXPECT_EQ(again.session.GetCompares().compared, again.session.GetCompares().same);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(_host->Seats()[1].commands.size(), 1u);
}

TEST_F(LockstepTest, ANewProcessReplacesTheOldSocketOfTheSamePlayer)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());

	// The old program of Bob still has its socket when the new one joins, as after a reload of the page
	Computer again(_network, _now, "god:bob");
	Frames({&alice, &bob, &again}, 40);
	EXPECT_EQ(again.session.GetJoined()->slot, std::optional<uint8_t>(1));
	EXPECT_EQ(again.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Taken);
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
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 30);
	ASSERT_TRUE(_host->Started());

	// A new god's account and an observer come after the start (MP-Q12)
	Computer dave(_network, _now, "god:dave");
	Computer carol(_network, _now, "observer:carol");
	Frames({&alice, &dave, &carol}, 30);
	for (const auto* late : {&dave, &carol})
	{
		ASSERT_TRUE(late->session.GetJoined().has_value());
		EXPECT_EQ(late->session.GetJoined()->role, game_socket::Role::Observer);
		EXPECT_FALSE(late->session.GetJoined()->slot.has_value());
		EXPECT_EQ(late->loads, 1u);
		EXPECT_EQ(late->world.turns, _serverWorld.turns);
		EXPECT_EQ(late->world.hash, _serverWorld.hash);
		EXPECT_EQ(late->session.GetCompares().compared, late->session.GetCompares().same);
	}
	EXPECT_EQ(_host->Observers(), 2u);
	EXPECT_EQ(std::ranges::count(_host->Seats(), LockstepHost::SeatState::Taken, &LockstepHost::Seat::state), 1);
	// An observer's commands do not reach the server
	dave.session.Send(HandTap {.object = Entity(3)});
	Frames({&alice, &dave, &carol}, 5);
	EXPECT_TRUE(_host->Seats()[1].commands.empty());
}

// The state of a testbed scenario is not in the world: every computer plays the scenario from the first turn. An observer
// in the room before the start gets the scenario with the start, and nobody joins the game as it turns.
TEST_F(LockstepTest, TheStartGivesTheScenarioAndNobodyJoinsTheRunningGameOfAScenario)
{
	MakeScenarioHost(1);
	GiveWorldState();
	Computer watcher(_network, _now, "observer:carol");
	Computer alice(_network, _now, "god:alice");
	Frames({&watcher, &alice}, 30);
	ASSERT_TRUE(_host->Started());
	for (const auto* computer : {&watcher, &alice})
	{
		ASSERT_TRUE(computer->session.GetStart().has_value());
		EXPECT_EQ(computer->session.GetStart()->scenario, "mind.computer_god_rivals");
		EXPECT_GT(computer->world.turns, 10u);
		EXPECT_EQ(computer->session.GetCompares().compared, computer->session.GetCompares().same);
	}

	Computer late(_network, _now, "observer:dave");
	Frames({&watcher, &alice, &late}, 3);
	EXPECT_EQ(late.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(late.session.RefusedReason(), "started");
	EXPECT_EQ(late.loads, 0u);
}

TEST_F(LockstepTest, ThePlayerOfADefeatedGodComesBackAsAnObserver)
{
	MakeHost(2, 2 * 1'000'000);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	auto bob = std::make_unique<Computer>(_network, _now, "god:bob");
	Frames({&alice, bob.get()}, 10);
	_network.Cut(bob->sockets->Ids().back());
	bob.reset();
	Frames({&alice}, 40);
	ASSERT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Closed);

	// The slot of a defeated god stays closed (MP-Q5): its player can only watch
	Computer again(_network, _now, "god:bob");
	Frames({&alice, &again}, 20);
	ASSERT_TRUE(again.session.GetJoined().has_value());
	EXPECT_EQ(again.session.GetJoined()->role, game_socket::Role::Observer);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Closed);
	EXPECT_EQ(again.world.hash, _serverWorld.hash);
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
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 6);
	ASSERT_TRUE(_host->Started());
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(_host->Seats().size(), 2u);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Open);
}

TEST_F(LockstepTest, TwoHumansPlayInARoomOfTwoSlotsThatStartsWithOne)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2);
	Computer alice(_network, _now, "god:alice");
	alice.session.SetReady(false);
	Frames({&alice}, 3);
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 6);
	// Bob is ready, but Alice in the room is not: the start waits for her
	EXPECT_FALSE(_host->Started());
	EXPECT_FALSE(_host->Starting());
	alice.session.SetReady(true);
	Frames({&alice, &bob}, 6);
	ASSERT_TRUE(_host->Started());
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.session.GetJoined()->slot, 1);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Taken);
}

TEST_F(LockstepTest, AThirdGodIsRefusedFromARoomOfTwoSlotsAndAnObserverIsLetIn)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2);
	Computer alice(_network, _now, "god:alice");
	alice.session.SetReady(false);
	Computer bob(_network, _now, "god:bob");
	Computer carol(_network, _now, "god:carol");
	Computer watcher(_network, _now, "observer:dave");
	Frames({&alice, &bob, &carol, &watcher}, 3);
	EXPECT_EQ(carol.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(carol.session.RefusedReason(), "full");
	EXPECT_EQ(watcher.session.GetJoined()->role, game_socket::Role::Observer);
	alice.session.SetReady(true);
	Frames({&alice, &bob, &watcher}, 6);
	EXPECT_TRUE(_host->Started());
}

// The lobby gave slot 1 to a computer god and closed slot 3: the players take only the open slots
TEST_F(LockstepTest, NoPlayerTakesAComputerSlotOrAClosedSlot)
{
	using lobby_api::SlotKind;
	MakeKindsHost({SlotKind::Open, SlotKind::Computer, SlotKind::Open, SlotKind::Closed}, {3});
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Computer);
	EXPECT_EQ(_host->Seats()[3].state, LockstepHost::SeatState::Closed);
	Computer alice(_network, _now, "god:alice");
	alice.session.SetReady(false);
	Computer bob(_network, _now, "god:bob");
	Computer carol(_network, _now, "god:carol");
	Frames({&alice, &bob, &carol}, 3);
	EXPECT_EQ(alice.session.GetJoined()->slot, 0);
	EXPECT_EQ(bob.session.GetJoined()->slot, 2);
	EXPECT_EQ(carol.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(carol.session.RefusedReason(), "full");
	alice.session.SetReady(true);
	Frames({&alice, &bob}, 6);
	ASSERT_TRUE(_host->Started());
	// The computer slot and the closed slot have no player. The start tells every computer which slot is closed.
	ASSERT_TRUE(alice.session.GetStart().has_value());
	const auto& start = *alice.session.GetStart();
	EXPECT_EQ(start.names, (std::vector<std::string> {"alice", "", "bob", ""}));
	EXPECT_EQ(start.closedSlots, (std::vector<uint8_t> {3}));
	ASSERT_TRUE(bob.session.GetStart().has_value());
	EXPECT_EQ(bob.session.GetStart()->closedSlots, (std::vector<uint8_t> {3}));
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Computer);
	EXPECT_EQ(_host->Seats()[3].state, LockstepHost::SeatState::Closed);
}

// The lobby gave slot 1 to a computer god of random alignment: no player takes it, and the start names it
TEST_F(LockstepTest, AComputerSlotOfRandomAlignmentGoesInTheStart)
{
	using lobby_api::SlotKind;
	MakeKindsHost({SlotKind::Open, SlotKind::ComputerRandom}, {});
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Computer);
	EXPECT_EQ(_host->RandomAlignmentSlots(), (std::vector<uint8_t> {1}));
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 6);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.GetStart().has_value());
	EXPECT_EQ(alice.session.GetStart()->randomAlignmentSlots, (std::vector<uint8_t> {1}));
	EXPECT_TRUE(alice.session.GetStart()->closedSlots.empty());
}

// A list of kinds that is shorter than the room keeps the last slots open
TEST_F(LockstepTest, TheSlotsAfterTheLastKindAreOpen)
{
	using lobby_api::SlotKind;
	MakeKindsHost({SlotKind::Open, SlotKind::Computer}, {});
	Computer alice(_network, _now, "god:alice");
	alice.session.SetReady(false);
	Frames({&alice}, 3);
	EXPECT_EQ(alice.session.GetJoined()->slot, 0);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Computer);
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 3);
	EXPECT_EQ(bob.session.GetJoined()->slot, 2);
}

// A fixed server has no member list: the first ticket of the lobby's game gives the kinds of the slots
TEST_F(LockstepTest, AFixedServerTakesTheSlotKindsOfTheLobbyFromTheTicket)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 4);
	Computer alice(_network, _now, "god:alice@lobby1#2%closed,open,computer");
	alice.session.SetReady(false);
	Frames({&alice}, 3);
	EXPECT_EQ(alice.session.GetJoined()->slot, 1);
	ASSERT_TRUE(alice.session.GetJoined()->start.has_value());
	EXPECT_EQ(alice.session.GetJoined()->start->closedSlots, (std::vector<uint8_t> {0}));
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Closed);
	EXPECT_EQ(_host->Seats()[2].state, LockstepHost::SeatState::Computer);
	// A later ticket with other kinds changes nothing
	Computer bob(_network, _now, "god:bob@lobby1#2%open,open,open,open");
	Frames({&alice, &bob}, 3);
	EXPECT_EQ(bob.session.GetJoined()->slot, 3);
	alice.session.SetReady(true);
	Frames({&alice, &bob}, 6);
	ASSERT_TRUE(_host->Started());
	EXPECT_EQ(_host->ClosedSlots(), (std::vector<uint8_t> {0}));
	ASSERT_TRUE(bob.session.GetStart().has_value());
	EXPECT_EQ(bob.session.GetStart()->closedSlots, (std::vector<uint8_t> {0}));
}

// A fixed server has no member list: each god takes the slot that its ticket gives, in any order of the joins
TEST_F(LockstepTest, AFixedServerSeatsEachGodInTheSlotOfItsTicket)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 4);
	GiveWorldState();
	Computer bob(_network, _now, "god:bob@lobby1#3!2");
	Computer carol(_network, _now, "god:carol@lobby1#3!0");
	Computer alice(_network, _now, "god:alice@lobby1#3!3");
	Frames({&bob}, 3);
	Frames({&bob, &carol}, 3);
	Frames({&bob, &carol, &alice}, 30);
	ASSERT_TRUE(_host->Started());
	EXPECT_EQ(bob.session.GetJoined()->slot, std::optional<uint8_t>(2));
	EXPECT_EQ(carol.session.GetJoined()->slot, std::optional<uint8_t>(0));
	EXPECT_EQ(alice.session.GetJoined()->slot, std::optional<uint8_t>(3));
	EXPECT_EQ(_host->Seats()[0].account, "carol");
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Open);
	EXPECT_EQ(_host->Seats()[2].account, "bob");
	EXPECT_EQ(_host->Seats()[3].account, "alice");
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

// The room refuses a ticket for a slot that another player holds, a slot that the lobby shuts or a slot out of the room
TEST_F(LockstepTest, AFixedServerRefusesATicketForASlotThatIsNotFree)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 4);
	Computer alice(_network, _now, "god:alice@lobby1#2!1%open,open,closed");
	alice.session.SetReady(false);
	Frames({&alice}, 3);
	EXPECT_EQ(alice.session.GetJoined()->slot, std::optional<uint8_t>(1));
	Computer taken(_network, _now, "god:mallory@lobby1#2!1");
	Computer closed(_network, _now, "god:trent@lobby1#2!2");
	Computer outside(_network, _now, "god:oscar@lobby1#2!4");
	Frames({&alice, &taken, &closed, &outside}, 3);
	for (const auto* refused : {&taken, &closed, &outside})
	{
		EXPECT_EQ(refused->session.GetEnd(), NetworkSession::End::Refused);
		EXPECT_EQ(refused->session.RefusedReason(), "full");
	}
	EXPECT_EQ(_host->Seats()[1].account, "alice");
	EXPECT_EQ(_host->Seats()[2].state, LockstepHost::SeatState::Closed);
	// A god of the lobby without a slot in the ticket, as from an older lobby, takes the first free slot
	Computer dave(_network, _now, "god:dave@lobby1#2");
	Frames({&alice, &dave}, 3);
	EXPECT_EQ(dave.session.GetJoined()->slot, std::optional<uint8_t>(0));
}

// A slot claim outside a lobby's game changes nothing: the god takes the first free slot
TEST_F(LockstepTest, ASlotClaimWithNoLobbyGivesTheFirstFreeSlot)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 4);
	Computer alice(_network, _now, "god:alice!3");
	alice.session.SetReady(false);
	Frames({&alice}, 3);
	EXPECT_EQ(alice.session.GetJoined()->slot, std::optional<uint8_t>(0));
}

// The player of a god of a fixed server comes back into the slot of the ticket after the start
TEST_F(LockstepTest, APlayerOfAFixedServerComesBackIntoTheSlotOfTheTicket)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 4);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice@lobby1#2!0");
	auto bob = std::make_unique<Computer>(_network, _now, "god:bob@lobby1#2!3");
	Frames({&alice, bob.get()}, 30);
	ASSERT_TRUE(_host->Started());
	ASSERT_EQ(bob->session.GetJoined()->slot, std::optional<uint8_t>(3));
	_network.Cut(bob->sockets->Ids().back());
	bob.reset();
	Frames({&alice}, 20);
	ASSERT_EQ(_host->Seats()[3].state, LockstepHost::SeatState::Away);
	Computer again(_network, _now, "god:bob@lobby1#2!3");
	Frames({&alice, &again}, 40);
	ASSERT_TRUE(again.session.GetJoined().has_value());
	EXPECT_EQ(again.session.GetJoined()->role, game_socket::Role::God);
	EXPECT_EQ(again.session.GetJoined()->slot, std::optional<uint8_t>(3));
	EXPECT_EQ(again.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(_host->Seats()[3].state, LockstepHost::SeatState::Taken);
}

TEST_F(LockstepTest, AFixedServerWaitsForEachGodOfTheLobbyGameThatTheTicketsGive)
{
	// The fixed room has 2 slots and starts with 1 god, but the tickets of the lobby's game give 2 gods
	MakeHost(1, 5LL * 60 * 1'000'000, 2);
	Computer alice(_network, _now, "god:alice@lobby1#2");
	Frames({&alice}, 6);
	// Alice is ready, but the second god of the lobby's game is not in the room: the room does not start
	EXPECT_FALSE(_host->Started());
	EXPECT_FALSE(_host->Starting());
	EXPECT_EQ(_host->GodsToWaitFor(), 2u);
	Computer bob(_network, _now, "god:bob@lobby1#2");
	bob.session.SetReady(false);
	Frames({&alice, &bob}, 6);
	// Bob is in the room, but he is not ready: the room does not start
	EXPECT_FALSE(_host->Started());
	bob.session.SetReady(true);
	Frames({&alice, &bob}, 6);
	ASSERT_TRUE(_host->Started());
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
}

TEST_F(LockstepTest, AFixedServerWaitsForItsOwnFewestGodsAgainAfterTheLobbyGodsLeave)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2);
	Computer alice(_network, _now, "god:alice@lobby1#2");
	Frames({&alice}, 6);
	EXPECT_EQ(_host->GodsToWaitFor(), 2u);
	alice.session.Leave();
	alice.Frame();
	Frames({}, 3);
	EXPECT_FALSE(_host->Started());
	EXPECT_EQ(_host->GodsToWaitFor(), 1u);
	// A player of the server's own room with no lobby starts it alone, as before
	Computer carol(_network, _now, "god:carol");
	Frames({&carol}, 6);
	EXPECT_TRUE(_host->Started());
}

TEST_F(LockstepTest, AFixedServerStartsAOneGodLobbyGameWithOneGod)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2);
	Computer alice(_network, _now, "god:alice@lobby1#1");
	Frames({&alice}, 6);
	ASSERT_TRUE(_host->Started());
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
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
	Computer alice(_network, _now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	alice.session.SetReady(false);
	Computer bob(_network, _now, "", k_LandFile, {'C'}, 0.0f, "Bob");
	Computer again(_network, _now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	Computer carol(_network, _now, "", k_LandFile, {'C'}, 0.0f, "Carol");
	Computer nameless(_network, _now, "", k_LandFile, {'C'}, 0.0f, "");
	Frames({&alice, &bob, &again, &carol, &nameless}, 3);
	ASSERT_TRUE(alice.session.GetJoined().has_value());
	EXPECT_EQ(alice.session.GetJoined()->name, "Alice");
	EXPECT_EQ(alice.session.GetJoined()->slot, 0);
	EXPECT_EQ(bob.session.GetJoined()->slot, 1);
	EXPECT_EQ(_host->Seats()[0].name, "Alice");
	// A second player with the name of a god in the room is refused, as a second player of an account
	EXPECT_EQ(again.session.RefusedReason(), "already");
	// With no free slot, a player on the local network watches the game
	ASSERT_TRUE(carol.session.GetJoined().has_value());
	EXPECT_EQ(carol.session.GetJoined()->role, game_socket::Role::Observer);
	EXPECT_EQ(nameless.session.RefusedReason(), "ticket");
	alice.session.SetReady(true);
	Frames({&alice, &bob, &carol}, 6);
	EXPECT_TRUE(_host->Started());
}

TEST_F(LockstepTest, ARoomOfTheLobbyRefusesAJoinWithANameAndNoTicket)
{
	MakeHost(1);
	Computer alice(_network, _now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	Frames({&alice}, 3);
	EXPECT_EQ(alice.session.RefusedReason(), "ticket");
}

TEST_F(LockstepTest, ARoomWithNoHumanGodStartsAtOnceAndTurnsWhenTheObserversJoined)
{
	MakeAllComputerHost(2);
	GiveWorldState();
	Frames({}, 3);
	EXPECT_TRUE(_host->Started());
	EXPECT_FALSE(_host->Paused());
	EXPECT_FALSE(_host->GameOver());
	// No observer yet: the world waits
	EXPECT_FALSE(_host->HasTurn(_serverWorld.turns));
	EXPECT_EQ(_serverWorld.turns, 0u);

	Computer first(_network, _now, "observer:one");
	Frames({&first}, 20);
	ASSERT_TRUE(first.session.GetJoined().has_value());
	EXPECT_EQ(first.session.GetJoined()->role, game_socket::Role::Observer);
	// The start that the observer gets names the tool modes
	ASSERT_TRUE(first.session.GetJoined()->start.has_value());
	EXPECT_TRUE(first.session.GetJoined()->start->allComputerGods);
	EXPECT_EQ(first.session.GetJoined()->start->aiSeed, std::optional<uint32_t>(3u));
	EXPECT_EQ(_serverWorld.turns, 0u);

	Computer second(_network, _now, "observer:two");
	Frames({&first, &second}, 40);
	EXPECT_EQ(_host->Observers(), 2u);
	EXPECT_GT(_serverWorld.turns, 0u);
	for (const auto* observer : {&first, &second})
	{
		EXPECT_EQ(observer->world.turns, _serverWorld.turns);
		EXPECT_EQ(observer->world.hash, _serverWorld.hash);
		EXPECT_EQ(observer->session.GetCompares().compared, observer->session.GetCompares().same);
	}
	EXPECT_EQ(std::ranges::count(_host->Seats(), LockstepHost::SeatState::Taken, &LockstepHost::Seat::state), 0);
}

TEST_F(LockstepTest, ARoomWithNoHumanGodAndNoObserverToWaitForTurnsAtOnce)
{
	MakeAllComputerHost(0);
	GiveWorldState();
	Frames({}, 10);
	EXPECT_GT(_serverWorld.turns, 0u);
	// With no observer seen yet the room is not over
	EXPECT_FALSE(_host->GameOver());
}

TEST_F(LockstepTest, ARoomWithNoHumanGodIsOverWhenTheObserversThatJoinedAllLeft)
{
	MakeAllComputerHost(1);
	GiveWorldState();
	Computer watcher(_network, _now, "observer:one");
	Frames({&watcher}, 20);
	ASSERT_EQ(_host->Observers(), 1u);
	EXPECT_FALSE(_host->GameOver());
	// An observer that leaves cannot come back, so the room ends at once
	watcher.session.Leave();
	Frames({&watcher}, 2);
	EXPECT_EQ(_host->Observers(), 0u);
	EXPECT_TRUE(_host->Watchers().empty());
	EXPECT_TRUE(_host->GameOver());
}

TEST_F(LockstepTest, ARoomWithNoHumanGodWaitsForItsLastObserverToComeBack)
{
	MakeAllComputerHost(1);
	GiveWorldState();
	Computer watcher(_network, _now, "observer:one");
	Frames({&watcher}, 20);
	ASSERT_GT(_serverWorld.turns, 0u);

	// The socket of the only observer closes: the room stays open, and the observer gets the turns that it missed
	_network.Cut(watcher.sockets->Ids().back());
	Frames({}, 10);
	EXPECT_EQ(_host->Observers(), 0u);
	EXPECT_FALSE(_host->GameOver());
	Frames({&watcher}, 10);
	EXPECT_EQ(_host->Observers(), 1u);
	EXPECT_EQ(watcher.session.GetState(), NetworkSession::State::Playing);

	// The world of the only observer parts: it comes back with the world of the server and stays in step
	watcher.world.partsAfter = watcher.world.turns + 3;
	Frames({&watcher}, 60);
	EXPECT_FALSE(_host->GameOver());
	EXPECT_EQ(watcher.loads, 2u);
	EXPECT_EQ(watcher.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(watcher.world.turns, _serverWorld.turns);
	EXPECT_EQ(watcher.world.hash, _serverWorld.hash);
	ASSERT_EQ(_host->Watchers().size(), 1u);
	EXPECT_EQ(_host->Watchers()[0].resumes, 2u);
}

TEST_F(LockstepTest, ARoomWithNoHumanGodEndsWhenItsLastObserverDoesNotComeBackInTime)
{
	MakeAllComputerHost(1, 3 * 1'000'000);
	GiveWorldState();
	auto watcher = std::make_unique<Computer>(_network, _now, "observer:one");
	Frames({watcher.get()}, 20);
	ASSERT_EQ(_host->Observers(), 1u);
	_network.Cut(watcher->sockets->Ids().back());
	watcher.reset();
	Frames({}, 20);
	EXPECT_FALSE(_host->GameOver());
	Frames({}, 20);
	EXPECT_TRUE(_host->Watchers().empty());
	EXPECT_TRUE(_host->GameOver());
}

TEST_F(LockstepTest, TheRoomRefusesABannedGodBeforeTheStartWithAKick)
{
	MakeHost(3);
	_host->SetBans({101});
	Computer alice(_network, _now, "god:100");
	Computer banned(_network, _now, "god:101");
	Frames({&alice, &banned}, 3);
	EXPECT_EQ(banned.session.GetState(), NetworkSession::State::Ended);
	EXPECT_EQ(banned.session.RefusedReason(), "kicked");
	EXPECT_EQ(alice.session.GetJoined()->slot, 0);
	EXPECT_TRUE(std::ranges::none_of(_host->Seats(), [](const LockstepHost::Seat& seat) { return seat.account == "101"; }));
	// The lobby lifted the ban: the account joins again
	_host->SetBans({});
	Computer again(_network, _now, "god:101");
	Frames({&alice, &again}, 3);
	ASSERT_TRUE(again.session.GetJoined().has_value());
	EXPECT_EQ(again.session.GetJoined()->slot, 1);
}

TEST_F(LockstepTest, ABanTakesAGodOutOfTheRoomBeforeTheStart)
{
	MakeHost(3);
	Computer alice(_network, _now, "god:100");
	Computer bob(_network, _now, "god:101");
	Frames({&alice, &bob}, 3);
	ASSERT_EQ(bob.session.GetJoined()->slot, 1);
	_host->SetBans({101, 555});
	Frames({&alice, &bob}, 2);
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Ended);
	EXPECT_EQ(bob.session.RefusedReason(), "kicked");
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Open);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Waiting);
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Taken);
}

TEST_F(LockstepTest, ABanDuringTheStartStopsTheStart)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:100");
	Computer bob(_network, _now, "god:101");
	for (int i = 0; i < 8 && !_host->Starting(); ++i)
	{
		Frame({&alice, &bob});
	}
	ASSERT_TRUE(_host->Starting());
	_host->SetBans({101});
	Frames({&alice, &bob}, 6);
	EXPECT_FALSE(_host->Started());
	EXPECT_FALSE(_host->Starting());
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
	Computer alice(_network, _now, "god:100");
	Computer bob(_network, _now, "god:101");
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(_host->Started());
	// Bob's network stops, then the lobby's host kicks him while his god waits
	_network.Cut(bob.sockets->Ids().back());
	Frames({&alice}, 1);
	bob.Frame();
	Frames({&alice}, 3);
	ASSERT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Away);
	_host->SetBans({101});
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Closed);
	// Bob's session tries to come back, and the server refuses it
	Frames({&alice, &bob}, 10);
	EXPECT_NE(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Closed);
	EXPECT_EQ(_host->Seats()[1].resumes, 0u);
}

TEST_F(LockstepTest, AnAwayObserverThatTheLobbyKicksCannotComeBack)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(_network, _now, "god:100");
	Computer bob(_network, _now, "god:101");
	Computer carol(_network, _now, "observer:102");
	Frames({&alice, &bob, &carol}, 10);
	ASSERT_TRUE(_host->Started());
	ASSERT_EQ(_host->Observers(), 1u);
	// Carol's network stops, then the lobby's host kicks her while her session waits
	_network.Cut(carol.sockets->Ids().back());
	Frames({&alice, &bob}, 1);
	carol.Frame();
	Frames({&alice, &bob}, 3);
	ASSERT_EQ(_host->Watchers().size(), 1u);
	ASSERT_FALSE(_host->Watchers()[0].connection.has_value());
	EXPECT_EQ(_host->Watchers()[0].account, "102");
	_host->SetBans({102});
	EXPECT_TRUE(_host->Watchers().empty());
	// Carol's session tries to come back, and the server refuses it
	Frames({&alice, &bob, &carol}, 10);
	EXPECT_NE(carol.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(carol.session.RefusedReason(), "resume");
	EXPECT_EQ(_host->Observers(), 0u);
	// The gods play on
	Frames({&alice, &bob}, 10);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
}

TEST_F(LockstepTest, AnObserverThatCameBackKeepsItsAccountForAKick)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(_network, _now, "god:100");
	Computer bob(_network, _now, "god:101");
	Computer carol(_network, _now, "observer:102");
	Frames({&alice, &bob, &carol}, 10);
	ASSERT_TRUE(_host->Started());
	// Carol's network stops, and her session comes back
	_network.Cut(carol.sockets->Ids().back());
	Frames({&alice, &bob}, 1);
	carol.Frame();
	Frames({&alice, &bob, &carol}, 20);
	ASSERT_EQ(carol.session.GetState(), NetworkSession::State::Playing);
	ASSERT_EQ(_host->Watchers().size(), 1u);
	ASSERT_EQ(_host->Watchers()[0].resumes, 1u);
	// The kick of her account takes the observer that came back out of the game
	_host->SetBans({102});
	Frames({&alice, &bob, &carol}, 3);
	EXPECT_EQ(carol.session.RefusedReason(), "kicked");
	EXPECT_TRUE(_host->Watchers().empty());
	EXPECT_EQ(_host->Observers(), 0u);
}

TEST_F(LockstepTest, ABanAfterTheStartTakesThePlayerOutOfTheGame)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(_network, _now, "god:100");
	Computer bob(_network, _now, "god:101");
	Computer carol(_network, _now, "observer:102");
	Frames({&alice, &bob, &carol}, 7);
	ASSERT_TRUE(_host->Started());
	Frames({&alice, &bob, &carol}, 10);
	// The lobby's host kicks the god of bob and the observer carol
	_host->SetBans({101, 102});
	Frames({&alice, &bob, &carol}, 3);
	EXPECT_EQ(_host->Bans(), (std::set<int64_t> {101, 102}));
	// The server closes the sockets at once with "kicked", and the god leaves as a player who leaves
	EXPECT_EQ(bob.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(bob.session.RefusedReason(), "kicked");
	EXPECT_EQ(carol.session.RefusedReason(), "kicked");
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Closed);
	EXPECT_EQ(_host->Observers(), 0u);
	// The other god plays on
	Frames({&alice}, 10);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_FALSE(_host->GameOver());

	// A new join of the kicked account gets "kicked" in every role, and the old session cannot come back
	Computer again(_network, _now, "god:101");
	Computer watch(_network, _now, "observer:101");
	Frames({&alice, &again, &watch}, 3);
	EXPECT_EQ(again.session.RefusedReason(), "kicked");
	EXPECT_EQ(watch.session.RefusedReason(), "kicked");

	// When the other god leaves too, the game is over
	alice.session.Leave();
	Frames({&alice}, 5);
	EXPECT_TRUE(_host->GameOver());
}

TEST_F(LockstepTest, AnObserverThatPartsLoadsTheWorldOfTheServerAndStaysInStep)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	Computer watcher(_network, _now, "observer:carol");
	watcher.world.partsAfter = 20;
	Frames({&alice, &watcher}, 120);
	ASSERT_TRUE(watcher.session.GetJoined().has_value());
	EXPECT_FALSE(watcher.session.GetJoined()->session.empty());
	EXPECT_EQ(watcher.loads, 1u);
	EXPECT_EQ(watcher.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(watcher.session.Resumes(), 1u);
	EXPECT_EQ(watcher.session.Resync().GetState(), online::ResyncRule::State::InStep);
	// After the load every turn is the same as the server's again
	EXPECT_EQ(watcher.world.hash, _serverWorld.hash);
	EXPECT_EQ(watcher.world.turns, _serverWorld.turns);
	EXPECT_EQ(watcher.session.GetCompares().compared, watcher.session.GetCompares().same + 1);
	ASSERT_EQ(_host->Watchers().size(), 1u);
	EXPECT_EQ(_host->Watchers()[0].resumes, 1u);
	EXPECT_EQ(_host->Observers(), 1u);
	// The observer did not take a god, and the god plays on in step
	EXPECT_EQ(std::ranges::count(_host->Seats(), LockstepHost::SeatState::Taken, &LockstepHost::Seat::state), 1);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
}

TEST_F(LockstepTest, AnObserverThatJoinedTheRunningGameComesBackAfterItsWorldParts)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 30);
	ASSERT_TRUE(_host->Started());
	Computer first(_network, _now, "observer:one");
	Computer second(_network, _now, "observer:two");
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
		EXPECT_EQ(observer->world.turns, _serverWorld.turns);
		EXPECT_EQ(observer->world.hash, _serverWorld.hash);
	}
	EXPECT_EQ(first.session.Resumes(), 1u);
	EXPECT_EQ(first.session.GetCompares().compared, first.session.GetCompares().same);
	EXPECT_EQ(_host->Observers(), 2u);
	ASSERT_EQ(_host->Watchers().size(), 2u);
	EXPECT_EQ(_host->Watchers()[0].resumes, 0u);
	EXPECT_EQ(_host->Watchers()[1].resumes, 1u);
}

TEST_F(LockstepTest, AnObserverThatJoinsTheRunningGameGetsTheNamesOfTheGods)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 30);
	ASSERT_TRUE(_host->Started());
	Computer carol(_network, _now, "observer:carol");
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
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 30);
	ASSERT_TRUE(_host->Started());
	EXPECT_TRUE(_serverWorld.observersGod.empty());
	// The room of the test has 7 god slots, so the god of the observers is the god of the eighth slot
	const auto god = static_cast<PlayerNames>(8);
	EXPECT_EQ(_host->ObserversGod(), god);
	Computer carol(_network, _now, "observer:carol");
	Computer dave(_network, _now, "observer:dave");
	Frames({&alice, &carol, &dave}, 20);
	ASSERT_EQ(_serverWorld.observersGod.size(), 1u);
	EXPECT_EQ(_serverWorld.observersGod[0].player, god);
	EXPECT_TRUE(std::holds_alternative<ObserversJoined>(_serverWorld.observersGod[0].action));
	EXPECT_TRUE(_host->ObserversIn());
	dave.session.Leave();
	Frames({&alice, &carol, &dave}, 10);
	EXPECT_EQ(_serverWorld.observersGod.size(), 1u);
	carol.session.Leave();
	Frames({&alice, &carol, &dave}, 10);
	ASSERT_EQ(_serverWorld.observersGod.size(), 2u);
	EXPECT_EQ(_serverWorld.observersGod[1].player, god);
	EXPECT_TRUE(std::holds_alternative<ObserversLeft>(_serverWorld.observersGod[1].action));
	EXPECT_FALSE(_host->ObserversIn());
	EXPECT_EQ(alice.world.observersGod.size(), 2u);
	EXPECT_EQ(alice.world.hash, _serverWorld.hash);
}

// Each observer gets its own number. The turns tell every world of each observer that comes in, with its name, and
// stamp the report of its hand and camera and its lines of the chat with its number.
TEST_F(LockstepTest, EachObserverHasANumberAndSendsItsReportAndItsChat)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 30);
	ASSERT_TRUE(_host->Started());
	Computer carol(_network, _now, "observer:carol");
	Computer dave(_network, _now, "observer:dave");
	Frames({&alice, &carol, &dave}, 20);
	EXPECT_EQ(carol.session.OwnObserver(), uint8_t {1});
	EXPECT_EQ(dave.session.OwnObserver(), uint8_t {2});
	EXPECT_FALSE(alice.session.OwnObserver().has_value());
	EXPECT_EQ(_host->ObserverNumbers(), (std::vector<uint8_t> {1, 2}));
	const auto god = static_cast<PlayerNames>(8);
	ASSERT_EQ(_serverWorld.observerCommands.size(), 2u);
	EXPECT_EQ(_serverWorld.observerCommands[0].player, god);
	EXPECT_EQ(_serverWorld.observerCommands[0].observer, 1u);
	EXPECT_EQ(std::get<ObserverIn>(_serverWorld.observerCommands[0].action).name, u"carol");
	EXPECT_EQ(_serverWorld.observerCommands[1].observer, 2u);
	EXPECT_EQ(std::get<ObserverIn>(_serverWorld.observerCommands[1].action).name, u"dave");

	// The hand of an observer does not act on things, so its computer sends only its report and its chat
	const HandReport report {.collidePoint = {1.0f, 2.0f, 3.0f},
	                         .handPoint = {4.0f, 5.0f, 6.0f},
	                         .camera = {7.0f, 8.0f, 9.0f},
	                         .focus = {10.0f, 11.0f, 12.0f}};
	dave.session.Send(report);
	dave.session.Send(HandTap {.object = Entity(1)});
	dave.session.Send(ChatSpeech {.recipients = {}, .text = u"Hi"});
	Frames({&alice, &carol, &dave}, 10);
	ASSERT_EQ(_serverWorld.observerCommands.size(), 4u);
	EXPECT_EQ(_serverWorld.observerCommands[2].observer, 2u);
	EXPECT_EQ(_serverWorld.observerCommands[2].player, god);
	EXPECT_EQ(std::get<HandReport>(_serverWorld.observerCommands[2].action), report);
	EXPECT_EQ(_serverWorld.observerCommands[3].observer, 2u);
	EXPECT_EQ(std::get<ChatSpeech>(_serverWorld.observerCommands[3].action).text, u"Hi");
	EXPECT_TRUE(_serverWorld.taps.empty());
	EXPECT_EQ(alice.world.hash, _serverWorld.hash);
	EXPECT_EQ(carol.world.hash, _serverWorld.hash);
	EXPECT_EQ(dave.world.hash, _serverWorld.hash);
	EXPECT_EQ(dave.session.GetCompares().compared, dave.session.GetCompares().same);
}

// A changed client of an observer can send any command. The server leaves out each command that an observer does not
// send, and keeps the rest of the block.
TEST_F(LockstepTest, TheServerLeavesOutTheCommandsThatAnObserverDoesNotSend)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 30);
	Computer carol(_network, _now, "observer:carol");
	Frames({&alice, &carol}, 20);
	const auto refused = _host->RefusedBlocks();
	const auto block = turn_codec::Encode(turn_codec::CommandBlock {
	    .sequence = 0, .turn = 0, .actions = {HandTap {.object = Entity(1)}, ChatTaunt {.recipients = {}, .taunt = 3}}});
	ASSERT_TRUE(carol.sockets->SendBinary(carol.sockets->Ids().back(), block));
	Frames({&alice, &carol}, 10);
	EXPECT_EQ(_host->RefusedBlocks(), refused + 1);
	EXPECT_TRUE(_serverWorld.taps.empty());
	ASSERT_FALSE(_serverWorld.observerCommands.empty());
	EXPECT_EQ(std::get<ChatTaunt>(_serverWorld.observerCommands.back().action).taunt, 3u);
	EXPECT_EQ(_serverWorld.observerCommands.back().observer, 1u);
}

// The leave of an observer goes in the next turn. Its number is free again after that turn, so no other observer has
// the number while a world still knows the observer that left.
TEST_F(LockstepTest, TheLeaveOfAnObserverGoesInTheNextTurnAndThenItsNumberIsFree)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 30);
	Computer carol(_network, _now, "observer:carol");
	Computer dave(_network, _now, "observer:dave");
	Frames({&alice, &carol, &dave}, 20);
	carol.session.Leave();
	Frames({&alice, &carol, &dave}, 10);
	ASSERT_EQ(_serverWorld.observerCommands.size(), 3u);
	EXPECT_TRUE(std::holds_alternative<ObserverOut>(_serverWorld.observerCommands[2].action));
	EXPECT_EQ(_serverWorld.observerCommands[2].observer, 1u);
	EXPECT_EQ(_host->ObserverNumbers(), (std::vector<uint8_t> {2}));
	// The god of the observers stays while an observer is in the game
	EXPECT_EQ(_serverWorld.observersGod.size(), 1u);
	Computer erin(_network, _now, "observer:erin");
	Frames({&alice, &dave, &erin}, 20);
	EXPECT_EQ(erin.session.OwnObserver(), uint8_t {1});
	EXPECT_EQ(std::get<ObserverIn>(_serverWorld.observerCommands.back().action).name, u"erin");
	EXPECT_EQ(alice.world.hash, _serverWorld.hash);
	EXPECT_EQ(erin.world.hash, _serverWorld.hash);
}

// An observer that joins before the start comes into every world in the first turn, after the god of the observers
TEST_F(LockstepTest, AnObserverOfTheRoomComesInAfterItsGodInTheFirstTurn)
{
	MakeHost(1);
	Computer carol(_network, _now, "observer:carol");
	Computer alice(_network, _now, "god:alice");
	Frames({&carol, &alice}, 30);
	ASSERT_TRUE(_host->Started());
	EXPECT_EQ(carol.session.OwnObserver(), uint8_t {1});
	ASSERT_EQ(_serverWorld.observersGod.size(), 1u);
	ASSERT_EQ(_serverWorld.observerCommands.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<ObserverIn>(_serverWorld.observerCommands[0].action));
	EXPECT_EQ(carol.world.observerCommands.size(), 1u);
	EXPECT_EQ(carol.world.hash, _serverWorld.hash);
}

// A restored server gives each observer its old number back, and keeps the blocks that the sent turns hold
TEST_F(LockstepTest, ARestoredServerKeepsTheNumbersOfItsObservers)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Computer carol(_network, _now, "observer:carol");
	Computer dave(_network, _now, "observer:dave");
	Frames({&alice, &bob, &carol, &dave}, 30);
	ASSERT_TRUE(_host->Started());
	dave.session.Send(ChatTaunt {.recipients = {}, .taunt = 4});
	Frames({&alice, &bob, &carol, &dave}, 10);
	const auto room = _host->RoomRecord();
	EXPECT_EQ(room.watcherNumbers, (std::vector<uint8_t> {1, 2}));
	const auto blocks = _host->BlocksInTurns();
	EXPECT_TRUE(blocks.contains(static_cast<uint8_t>(server::snapshots::k_ObserverBlocksKey + 2)));
	RestartFrom(room, _host->TurnsDone(), _serverWorld, {&alice, &bob, &carol, &dave}, 0, blocks);
	EXPECT_EQ(_host->ObserverNumbers(), (std::vector<uint8_t> {1, 2}));
	Frames({&alice, &bob, &carol, &dave}, 40);
	EXPECT_EQ(dave.session.Resumes(), 1u);
	const auto taunts = std::ranges::count_if(_serverWorld.observerCommands, [](const Command& command) {
		return std::holds_alternative<ChatTaunt>(command.action);
	});
	EXPECT_EQ(taunts, 1);
	dave.session.Send(ChatTaunt {.recipients = {}, .taunt = 5});
	Frames({&alice, &bob, &carol, &dave}, 10);
	ASSERT_FALSE(_serverWorld.observerCommands.empty());
	EXPECT_EQ(_serverWorld.observerCommands.back().observer, 2u);
	EXPECT_EQ(std::get<ChatTaunt>(_serverWorld.observerCommands.back().action).taunt, 5u);
	EXPECT_EQ(alice.world.hash, _serverWorld.hash);
}

// An observer that loses its socket can come back, so its god stays in the game until its session ends
TEST_F(LockstepTest, TheGodOfTheObserversWaitsForAnObserverThatCanComeBack)
{
	MakeHost(1, 3 * 1'000'000);
	Computer alice(_network, _now, "god:alice");
	auto watcher = std::make_unique<Computer>(_network, _now, "observer:carol");
	Frames({&alice, watcher.get()}, 30);
	ASSERT_EQ(_serverWorld.observersGod.size(), 1u);
	_network.Cut(watcher->sockets->Ids().back());
	watcher.reset();
	Frames({&alice}, 20);
	EXPECT_EQ(_serverWorld.observersGod.size(), 1u);
	Frames({&alice}, 30);
	ASSERT_EQ(_serverWorld.observersGod.size(), 2u);
	EXPECT_TRUE(std::holds_alternative<ObserversLeft>(_serverWorld.observersGod[1].action));
}

// The god of the observers is the god of the slot after the last god slot of the room, and at least player seven
TEST_F(LockstepTest, TheGodOfTheObserversFollowsTheGodSlotsOfTheRoom)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2);
	EXPECT_EQ(_host->ObserversGod(), PlayerNames::PLAYER_SEVEN);
	MakeHost(1, 5LL * 60 * 1'000'000, 32);
	EXPECT_EQ(_host->ObserversGod(), static_cast<PlayerNames>(33));
}

// A god and an observer with the editor claim keep the claim when they come back to a server that a snapshot restored.
// The server still refuses the edits of the god with no editor claim.
TEST_F(LockstepTest, ARestoredServerKeepsTheEditorClaimsOfTheGodsAndTheObservers)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Computer carol(_network, _now, "observer+editor:carol");
	Frames({&alice, &bob, &carol}, 30);
	ASSERT_TRUE(_host->Started());
	const auto room = _host->RoomRecord();
	ASSERT_GE(room.seats.size(), 2u);
	EXPECT_TRUE(room.seats[0].editor);
	EXPECT_FALSE(room.seats[1].editor);
	EXPECT_EQ(room.watcherEditors, (std::vector<bool> {true}));
	RestartFrom(room, _host->TurnsDone(), _serverWorld, {&alice, &bob, &carol});
	EXPECT_TRUE(_host->Seats()[0].editor);
	EXPECT_FALSE(_host->Seats()[1].editor);
	ASSERT_EQ(_host->Watchers().size(), 1u);
	EXPECT_TRUE(_host->Watchers()[0].editor);
	Frames({&alice, &bob, &carol}, 40);
	ASSERT_EQ(alice.session.Resumes(), 1u);
	ASSERT_EQ(bob.session.Resumes(), 1u);
	ASSERT_EQ(carol.session.Resumes(), 1u);
	const EditorEdit move {.kind = EditorEdit::Kind::Move, .object = Entity(5), .position = {1.0f, 0.0f, 2.0f}};
	alice.session.Send(move);
	bob.session.Send(move);
	carol.session.Send(EditorEdit {.kind = EditorEdit::Kind::Remove, .object = Entity(7)});
	Frames({&alice, &bob, &carol}, 4);
	EXPECT_EQ(_host->RefusedEditorCommands(), 1u);
	const auto count = [this](size_t seat) {
		const auto& commands = _host->Seats()[seat].commands;
		const auto found = commands.find(static_cast<uint8_t>(turn_codec::CommandTag::EditorEdit));
		return found != commands.end() ? found->second : 0u;
	};
	EXPECT_EQ(count(0), 1u);
	EXPECT_EQ(count(1), 0u);
	EXPECT_GE(bob.session.TakeEditorRefusals(), 1u);
	EXPECT_EQ(alice.session.TakeEditorRefusals(), 0u);
	EXPECT_EQ(carol.session.TakeEditorRefusals(), 0u);
	const auto edits = std::ranges::count_if(alice.world.observerCommands, [](const Command& command) {
		return std::holds_alternative<EditorEdit>(command.action);
	});
	EXPECT_EQ(edits, 1);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

// A server that a snapshot restores knows the names of its observers and whether their god is in the game. So the
// players still see the name of an observer that leaves after the restart.
TEST_F(LockstepTest, ARestoredServerTellsTheNameOfAnObserverThatLeaves)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Computer carol(_network, _now, "observer:carol");
	Frames({&alice, &bob, &carol}, 30);
	ASSERT_TRUE(_host->Started());
	const auto room = _host->RoomRecord();
	EXPECT_TRUE(room.observersIn);
	EXPECT_EQ(room.watcherNames, (std::vector<std::string> {"carol"}));
	RestartFrom(room, _host->TurnsDone(), _serverWorld, {&alice, &bob, &carol});
	EXPECT_TRUE(_host->ObserversIn());
	ASSERT_EQ(_host->Watchers().size(), 1u);
	EXPECT_EQ(_host->Watchers()[0].name, "carol");
	Frames({&alice, &bob, &carol}, 40);
	EXPECT_EQ(carol.session.Resumes(), 1u);
	carol.session.Leave();
	Frames({&alice, &bob, &carol}, 10);
	EXPECT_EQ(alice.session.TakeObserversLeft(), (std::vector<std::string> {"carol"}));
}

TEST_F(LockstepTest, AnObserverThatLosesTheSocketComesBackAndGetsTheTurnsThatItMissed)
{
	MakeHost(1);
	Computer alice(_network, _now, "god:alice");
	Computer watcher(_network, _now, "observer:carol");
	Frames({&alice, &watcher}, 30);
	ASSERT_TRUE(_host->Started());

	_network.Cut(watcher.sockets->Ids().back());
	Frames({&alice}, 1);
	watcher.Frame();
	EXPECT_EQ(watcher.session.GetState(), NetworkSession::State::Reconnecting);
	Frames({&alice}, 10);
	EXPECT_EQ(_host->Observers(), 0u);
	ASSERT_EQ(_host->Watchers().size(), 1u);
	EXPECT_FALSE(_host->Watchers()[0].connection.has_value());
	const auto turnsWhileAway = _serverWorld.turns;

	Frames({&alice, &watcher}, 10);
	EXPECT_EQ(watcher.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(watcher.session.Resumes(), 1u);
	EXPECT_EQ(watcher.loads, 0u);
	EXPECT_EQ(_host->Observers(), 1u);
	EXPECT_EQ(_host->Watchers()[0].resumes, 1u);
	// The observer did the turns that it missed, and each of its checksums is the same as the server's
	EXPECT_GE(watcher.world.turns, turnsWhileAway);
	EXPECT_GT(watcher.session.GetCompares().same, 35u);
	EXPECT_EQ(watcher.session.GetCompares().compared, watcher.session.GetCompares().same);
	// The observer is not a god: the server takes no command from it
	watcher.session.Send(HandTap {.object = Entity(4)});
	Frames({&alice, &watcher}, 5);
	EXPECT_TRUE(_host->Seats()[1].commands.empty());
}

TEST_F(LockstepTest, AnObserverThatDoesNotComeBackInTimeLosesItsSession)
{
	MakeHost(1, 3 * 1'000'000);
	Computer alice(_network, _now, "god:alice");
	auto watcher = std::make_unique<Computer>(_network, _now, "observer:carol");
	Frames({&alice, watcher.get()}, 10);
	ASSERT_EQ(_host->Watchers().size(), 1u);
	_network.Cut(watcher->sockets->Ids().back());
	watcher.reset();
	Frames({&alice}, 20);
	ASSERT_EQ(_host->Watchers().size(), 1u);
	Frames({&alice}, 30);
	EXPECT_TRUE(_host->Watchers().empty());
}

// The other players see that an observer left the game, by its name, as they see any player who leaves
TEST_F(LockstepTest, ThePlayersSeeTheNameOfAnObserverThatLeavesTheGame)
{
	MakeHost(1);
	Computer alice(_network, _now, "god:alice");
	Computer carol(_network, _now, "observer:carol");
	Frames({&alice, &carol}, 30);
	ASSERT_TRUE(_host->Started());
	carol.session.Leave();
	Frames({&alice, &carol}, 10);
	EXPECT_EQ(alice.session.TakeObserversLeft(), (std::vector<std::string> {"carol"}));
	EXPECT_TRUE(alice.session.TakeObserversLeft().empty());
}

// A line from the server reaches the chat of every player and observer in a game that runs
TEST_F(LockstepTest, TheServerAnnouncesALineToEveryPlayerOfTheGame)
{
	MakeHost(1);
	Computer alice(_network, _now, "god:alice");
	Computer carol(_network, _now, "observer:carol");
	Frames({&alice, &carol}, 30);
	ASSERT_TRUE(_host->Started());
	_host->AnnounceToPlayers("The game server ends in 10 minutes.");
	Frames({&alice, &carol}, 10);
	EXPECT_EQ(alice.session.TakeTimeNotices(), std::vector<std::string> {"The game server ends in 10 minutes."});
	EXPECT_EQ(carol.session.TakeTimeNotices(), std::vector<std::string> {"The game server ends in 10 minutes."});
	EXPECT_TRUE(alice.session.TakeTimeNotices().empty());
}

// The room has its own chat, so the server says nothing to it before the start
TEST_F(LockstepTest, TheServerAnnouncesNothingBeforeTheStart)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 5);
	ASSERT_FALSE(_host->Started());
	_host->AnnounceToPlayers("The game server ends in 10 minutes.");
	Frames({&alice}, 10);
	EXPECT_TRUE(alice.session.TakeTimeNotices().empty());
	EXPECT_TRUE(alice.session.TakeRoomChat().empty());
}

// An observer that loses its socket can come back, so the others see its leave only when its session ends
TEST_F(LockstepTest, ThePlayersSeeTheLeaveOfAnObserverThatDoesNotComeBackWhenItsSessionEnds)
{
	MakeHost(1, 3 * 1'000'000);
	Computer alice(_network, _now, "god:alice");
	auto watcher = std::make_unique<Computer>(_network, _now, "observer:carol");
	Frames({&alice, watcher.get()}, 10);
	ASSERT_TRUE(_host->Started());
	_network.Cut(watcher->sockets->Ids().back());
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
	Computer alice(_network, _now, "god:alice");
	Computer leaver(_network, _now, "observer:leaver");
	Frames({&alice, &leaver}, 5);
	ASSERT_FALSE(_host->Started());
	leaver.session.Leave();
	Frames({&alice, &leaver}, 10);
	EXPECT_TRUE(alice.session.TakeObserversLeft().empty());
}

TEST_F(LockstepTest, AnObserverThatLeavesOrPartsBeforeTheStartCannotComeBack)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer early(_network, _now, "observer:early");
	Computer leaver(_network, _now, "observer:leaver");
	Frames({&alice, &early, &leaver}, 5);
	ASSERT_FALSE(_host->Started());
	ASSERT_EQ(_host->Watchers().size(), 2u);
	// Before the start, an observer that loses its socket has no session to come back to
	_network.Cut(early.sockets->Ids().back());
	leaver.session.Leave();
	Frames({&alice, &early, &leaver}, 10);
	EXPECT_TRUE(_host->Watchers().empty());
	EXPECT_EQ(early.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(early.session.RefusedReason(), "resume");
}

TEST_F(LockstepTest, TheSessionOfAnObserverDoesNotTakeAGod)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	auto bob = std::make_unique<Computer>(_network, _now, "god:bob");
	Computer watcher(_network, _now, "observer:carol");
	Frames({&alice, bob.get(), &watcher}, 30);
	ASSERT_TRUE(_host->Started());
	// Bob is away, and the observer comes back while the god of slot 1 waits
	_network.Cut(bob->sockets->Ids().back());
	bob.reset();
	_network.Cut(watcher.sockets->Ids().back());
	Frames({&alice, &watcher}, 20);
	EXPECT_EQ(watcher.session.Resumes(), 1u);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Away);
	EXPECT_EQ(_host->Seats()[1].resumes, 0u);
	EXPECT_FALSE(watcher.session.GetJoined()->slot.has_value());
	EXPECT_EQ(watcher.session.GetJoined()->role, game_socket::Role::Observer);
}

TEST_F(LockstepTest, AGodThatComesBackWithTheWorldAfterTheStopGetsTheStop)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());
	const auto last = StopWhileAClientComesBackWithTheWorld(alice, bob);
	ASSERT_EQ(_host->StoppedAfter(), std::optional<uint32_t>(last));
	EXPECT_FALSE(bob.session.StoppedAfter().has_value());
	Frames({&alice, &bob}, 20);
	EXPECT_EQ(bob.loads, 1u);
	EXPECT_EQ(bob.session.StoppedAfter(), std::optional<uint32_t>(last));
	EXPECT_EQ(bob.world.turns, last + 1);
	EXPECT_EQ(bob.world.hash, _serverWorld.hash);
	EXPECT_EQ(_serverWorld.turns, last + 1);
}

TEST_F(LockstepTest, AnObserverThatComesBackWithTheWorldAfterTheStopGetsTheStop)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	Computer watcher(_network, _now, "observer:carol");
	Frames({&alice, &watcher}, 30);
	ASSERT_TRUE(_host->Started());
	const auto last = StopWhileAClientComesBackWithTheWorld(alice, watcher);
	ASSERT_EQ(_host->StoppedAfter(), std::optional<uint32_t>(last));
	EXPECT_FALSE(watcher.session.StoppedAfter().has_value());
	Frames({&alice, &watcher}, 20);
	EXPECT_EQ(watcher.loads, 1u);
	EXPECT_EQ(watcher.session.StoppedAfter(), std::optional<uint32_t>(last));
	EXPECT_EQ(watcher.world.turns, last + 1);
	EXPECT_EQ(watcher.world.hash, _serverWorld.hash);
}

TEST_F(LockstepTest, AnObserverThatJoinsAfterTheStopGetsTheStopWithTheWorld)
{
	MakeHost(1);
	GiveWorldState();
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 30);
	alice.session.StopGame();
	Frames({&alice}, 5);
	ASSERT_TRUE(_host->StoppedAfter().has_value());
	Computer late(_network, _now, "observer:late");
	Frames({&alice, &late}, 10);
	EXPECT_EQ(late.loads, 1u);
	EXPECT_EQ(late.session.StoppedAfter(), _host->StoppedAfter());
	EXPECT_EQ(late.world.turns, _serverWorld.turns);
}

TEST_F(LockstepTest, TheGodsAfterTheSeventhTakeTheNumbersAfterTheNeutralPlayer)
{
	constexpr uint8_t k_Gods = 9;
	MakeHost(k_Gods, 5LL * 60 * 1'000'000, k_Gods);
	std::vector<std::unique_ptr<Computer>> gods;
	std::vector<Computer*> computers;
	for (uint8_t i = 0; i < k_Gods; ++i)
	{
		gods.push_back(std::make_unique<Computer>(_network, _now, "god:god" + std::to_string(i), k_LandFile,
		                                          std::vector<uint8_t> {static_cast<uint8_t>('a' + i)}));
		computers.push_back(gods.back().get());
	}
	Frames(computers, 12);
	ASSERT_TRUE(_host->Started());
	// The first 7 gods have the numbers of the gods of a land; the eighth and the ninth come after the neutral player
	EXPECT_EQ(gods[0]->session.OwnGod(), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(gods[6]->session.OwnGod(), PlayerNames::PLAYER_SEVEN);
	EXPECT_EQ(gods[7]->session.OwnGod(), static_cast<PlayerNames>(8));
	EXPECT_EQ(gods[8]->session.OwnGod(), static_cast<PlayerNames>(9));
	for (const player_commands::TurnCommandSourceInterface* source :
	     std::initializer_list<const player_commands::TurnCommandSourceInterface*> {_host.get(), &gods[0]->session})
	{
		const auto eighth = source->StartCreatureFile(static_cast<PlayerNames>(8));
		EXPECT_EQ(std::vector<uint8_t>(eighth.begin(), eighth.end()), (std::vector<uint8_t> {'h'}));
		EXPECT_TRUE(source->StartCreatureFile(PlayerNames::NEUTRAL).empty());
	}
	// The joins of the first turn come from each god's own number, never from the neutral player
	EXPECT_TRUE(_serverWorld.players.contains(static_cast<PlayerNames>(8)));
	EXPECT_TRUE(_serverWorld.players.contains(static_cast<PlayerNames>(9)));
	EXPECT_FALSE(_serverWorld.players.contains(PlayerNames::NEUTRAL));
	EXPECT_EQ(gods[8]->world.players, _serverWorld.players);
}

TEST_F(LockstepTest, AServerThatRestartsFromItsSnapshotGetsItsPlayersBackWithNoStepBack)
{
	_hasher = std::make_shared<server::Sha256SessionHasher>();
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());
	// The snapshot of the server, then more turns with a command of a player
	const auto room = _host->RoomRecord();
	const auto snapshotTurn = _host->TurnsDone();
	const auto snapshotWorld = _serverWorld;
	// The snapshot file keeps the hashes of the secrets, not the secrets
	const auto file = server::snapshots::EncodeSnapshot({.turn = snapshotTurn, .room = room, .world = {}, .blocks = {}});
	EXPECT_EQ(_secrets, 2);
	EXPECT_FALSE(HasText(file, "secret-"));
	for (const auto& seat : _host->Seats())
	{
		EXPECT_TRUE(seat.session.empty() || !HasText(file, seat.session));
	}
	alice.session.Send(HandTap {.object = Entity(5)});
	Frames({&alice, &bob}, 20);
	const auto lastTurn = _host->TurnsDone();
	const auto lastHash = _serverWorld.hash;

	RestartFrom(room, snapshotTurn, snapshotWorld, {&alice, &bob});
	// The new server did again every turn that the old server sent, with the same commands
	EXPECT_EQ(_host->TurnsDone(), lastTurn);
	EXPECT_EQ(_serverWorld.hash, lastHash);
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Away);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Away);
	EXPECT_FALSE(_host->HasTurn(lastTurn));

	// The players come back with the secrets of their sessions and get only the turns that they missed
	Frames({&alice, &bob}, 40);
	for (const auto* computer : {&alice, &bob})
	{
		EXPECT_EQ(computer->session.GetState(), NetworkSession::State::Playing);
		EXPECT_EQ(computer->session.Resumes(), 1u);
		EXPECT_EQ(computer->loads, 0u);
		EXPECT_EQ(computer->world.hash, _serverWorld.hash);
		EXPECT_EQ(computer->world.turns, _serverWorld.turns);
		EXPECT_EQ(computer->session.GetCompares().compared, computer->session.GetCompares().same);
	}
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Taken);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Taken);
	EXPECT_GT(_serverWorld.turns, lastTurn + 20);
}

TEST_F(LockstepTest, AfterARestartAPlayerWithTheWrongSecretIsRefused)
{
	_hasher = std::make_shared<server::Sha256SessionHasher>();
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());
	auto room = _host->RoomRecord();
	const auto snapshotTurn = _host->TurnsDone();
	const auto snapshotWorld = _serverWorld;
	Frames({&alice, &bob}, 10);

	// The slot of Bob keeps the hash of another secret, so the secret of Bob does not match it
	room.seats[1].session = _hasher->Hash("another-secret");
	RestartFrom(room, snapshotTurn, snapshotWorld, {&alice, &bob});
	Frames({&alice, &bob}, 40);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Taken);
	EXPECT_EQ(bob.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(bob.session.RefusedReason(), "resume");
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Away);
}

TEST_F(LockstepTest, AClientAheadOfTheRestoredServerGetsTheWorldOfTheServer)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());
	const auto room = _host->RoomRecord();
	const auto snapshotTurn = _host->TurnsDone();
	const auto snapshotWorld = _serverWorld;
	Frames({&alice, &bob}, 20);
	const auto lastTurn = _host->TurnsDone();

	// The journal lost its last turns, so the clients are at turns after the turn of the new server
	RestartFrom(room, snapshotTurn, snapshotWorld, {&alice, &bob}, 5);
	EXPECT_EQ(_host->TurnsDone(), lastTurn - 5);
	Frames({&alice, &bob}, 40);
	for (const auto* computer : {&alice, &bob})
	{
		EXPECT_EQ(computer->session.GetState(), NetworkSession::State::Playing);
		EXPECT_EQ(computer->loads, 1u);
		EXPECT_EQ(computer->world.hash, _serverWorld.hash);
		EXPECT_EQ(computer->world.turns, _serverWorld.turns);
	}
}

TEST_F(LockstepTest, ACommandThatTheServerHadInNoTurnAtItsStopComesToTheNewServerOnce)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());
	const auto room = _host->RoomRecord();
	const auto snapshotTurn = _host->TurnsDone();
	const auto snapshotWorld = _serverWorld;
	const auto snapshotBlocks = _host->BlocksInTurns();
	alice.session.Send(HandTap {.object = Entity(5)});
	Frames({&alice, &bob}, 20);
	EXPECT_EQ(_serverWorld.taps[5], 1u);
	EXPECT_TRUE(alice.session.Sent().Kept().empty());

	// The server gets a second tap, then stops before a turn holds it
	alice.session.Send(HandTap {.object = Entity(6)});
	ServerReads();
	ASSERT_EQ(alice.session.Sent().Kept().size(), 1u);
	RestartFrom(room, snapshotTurn, snapshotWorld, {&alice, &bob}, 0, snapshotBlocks);
	EXPECT_EQ(_serverWorld.taps[6], 0u);

	// Alice comes back and sends the tap again. Each world does each tap one time.
	Frames({&alice, &bob}, 40);
	EXPECT_EQ(alice.session.Resumes(), 1u);
	EXPECT_TRUE(alice.session.Sent().Kept().empty());
	EXPECT_EQ(_host->Seats()[0].blocksAgain, 0u);
	for (const auto* world : {&_serverWorld, &alice.world, &bob.world})
	{
		EXPECT_EQ(world->taps.at(5), 1u);
		EXPECT_EQ(world->taps.at(6), 1u);
	}
	EXPECT_EQ(alice.world.hash, _serverWorld.hash);
	EXPECT_EQ(bob.world.hash, _serverWorld.hash);
}

TEST_F(LockstepTest, ACommandInASentTurnThatTheClientDidNotGetIsNotDoneTwiceAfterARestart)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());
	const auto room = _host->RoomRecord();
	const auto snapshotTurn = _host->TurnsDone();
	const auto snapshotWorld = _serverWorld;
	const auto snapshotBlocks = _host->BlocksInTurns();
	Frames({&alice, &bob}, 5);

	// The server puts the tap in a turn and writes the turn in its journal. It stops before Alice gets the turn.
	alice.session.Send(HandTap {.object = Entity(8)});
	ServerTurn();
	EXPECT_EQ(_serverWorld.taps[8], 1u);
	RestartFrom(room, snapshotTurn, snapshotWorld, {&alice, &bob}, 0, snapshotBlocks);
	EXPECT_EQ(_serverWorld.taps[8], 1u);

	// Alice sends the tap again, because no turn that she got holds it. The new server had it in its sent turns.
	Frames({&alice, &bob}, 40);
	EXPECT_EQ(_host->Seats()[0].blocksAgain, 1u);
	EXPECT_TRUE(alice.session.Sent().Kept().empty());
	for (const auto* world : {&_serverWorld, &alice.world, &bob.world})
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
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());

	// The server gets the tap, then the socket of Alice stops before the turn comes to her
	alice.session.Send(HandTap {.object = Entity(4)});
	ServerReads();
	_network.Cut(alice.sockets->Ids().back());
	Frames({&alice, &bob}, 40);
	EXPECT_EQ(alice.session.Resumes(), 1u);
	EXPECT_EQ(_host->Seats()[0].blocksAgain, 1u);
	EXPECT_TRUE(alice.session.Sent().Kept().empty());
	for (const auto* world : {&_serverWorld, &alice.world, &bob.world})
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
	Computer alice(_network, _now, "god:alice");
	auto bob = std::make_unique<Computer>(_network, _now, "god:bob");
	Frames({&alice, bob.get()}, 30);
	ASSERT_TRUE(_host->Started());
	for (uint32_t object = 1; object <= 3; ++object)
	{
		bob->session.Send(HandTap {.object = Entity(object)});
		Frames({&alice, bob.get()}, 2);
	}
	EXPECT_EQ(_host->Seats()[1].lastBlock, std::optional<uint16_t>(2));

	// Bob's program stops. He starts it again with a new ticket, and its first block has the number 0.
	_network.Cut(bob->sockets->Ids().back());
	bob.reset();
	Frames({&alice}, 5);
	Computer again(_network, _now, "god:bob");
	Frames({&alice, &again}, 40);
	ASSERT_EQ(again.session.GetState(), NetworkSession::State::Playing);
	again.session.Send(HandTap {.object = Entity(9)});
	Frames({&alice, &again}, 10);
	EXPECT_EQ(_host->Seats()[1].blocksAgain, 0u);
	EXPECT_EQ(_host->Seats()[1].lastBlock, std::optional<uint16_t>(0));
	EXPECT_EQ(_serverWorld.taps[9], 1u);
	EXPECT_EQ(alice.world.taps[9], 1u);
}

TEST_F(LockstepTest, AGodWhoseProgramStoppedWithTheServerComesBackWithANewTicket)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(_network, _now, "god:alice");
	auto bob = std::make_unique<Computer>(_network, _now, "god:bob");
	Frames({&alice, bob.get()}, 30);
	ASSERT_TRUE(_host->Started());
	const auto room = _host->RoomRecord();
	const auto snapshotTurn = _host->TurnsDone();
	const auto snapshotWorld = _serverWorld;
	Frames({&alice, bob.get()}, 10);

	// Bob's program stopped with the server. He starts it again with a new ticket of the same account.
	_network.Cut(bob->sockets->Ids().back());
	bob.reset();
	RestartFrom(room, snapshotTurn, snapshotWorld, {&alice});
	Computer again(_network, _now, "god:bob");
	Frames({&alice, &again}, 40);
	ASSERT_TRUE(again.session.GetJoined().has_value());
	EXPECT_EQ(again.session.GetJoined()->slot, std::optional<uint8_t>(1));
	EXPECT_EQ(again.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(again.world.hash, _serverWorld.hash);
	// Alice came back with the secret of her session and stays in step with the server
	EXPECT_GT(alice.session.GetCompares().same, 60u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(alice.loads, 0u);
	EXPECT_EQ(alice.session.Resumes(), 1u);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Taken);
}

TEST_F(LockstepTest, TheReportOfTheRoomGivesTheGodsAndTheTurnsForTheOperator)
{
	MakeHost(2);
	_host->SetBans({7, 3});
	auto report = server::admin::RoomReportOf(*_host);
	EXPECT_FALSE(report.started);
	EXPECT_EQ(report.turnsDone, 0u);
	EXPECT_EQ(report.bans, (std::vector<int64_t> {3, 7}));
	ASSERT_EQ(report.seats.size(), 7u);
	EXPECT_TRUE(std::ranges::all_of(report.seats, [](const auto& seat) { return seat.state == "open"; }));

	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 12);
	ASSERT_TRUE(_host->Started());
	report = server::admin::RoomReportOf(*_host);
	EXPECT_TRUE(report.started);
	EXPECT_FALSE(report.paused);
	EXPECT_EQ(report.turnsDone, _host->TurnsDone());
	EXPECT_GT(report.turnsDone, 0u);
	const auto& first = report.seats.at(0);
	EXPECT_EQ(first.state, "taken");
	EXPECT_EQ(first.account, "alice");
	EXPECT_TRUE(first.connected);
	EXPECT_TRUE(first.ready);
	EXPECT_EQ(first.checksumsCompared, _host->Seats().at(0).compares.compared);
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
	Computer alice(_network, _now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	alice.session.SetReady(false);
	Computer bob(_network, _now, "", k_LandFile, {'C'}, 0.0f, "Bob");
	bob.session.SetReady(false);
	Computer carol(_network, _now, "", k_LandFile, {'C'}, 0.0f, "Carol");
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
	ASSERT_TRUE(_host->Started());
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
	Computer alice(_network, _now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	alice.session.SetReady(false);
	Computer bob(_network, _now, "", k_LandFile, {'C'}, 0.0f, "Bob");
	bob.session.SetReady(false);
	Computer carol(_network, _now, "", k_LandFile, {'C'}, 0.0f, "Carol");
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
	Computer alice(_network, _now, "god:alice");
	alice.session.SetReady(false);
	Computer bob(_network, _now, "god:bob");
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
	Computer alice(_network, _now, "god:alice");
	Frames({&alice}, 7);
	ASSERT_TRUE(_host->Started());
	Computer watcher(_network, _now, "observer:carol");
	Frames({&alice, &watcher}, 10);
	ASSERT_EQ(watcher.session.GetJoined()->role, game_socket::Role::Observer);
	watcher.sockets->SendText(watcher.sockets->Ids().back(), game_socket::ToJson(game_socket::StopRequest {}));
	Frames({&alice, &watcher}, 5);
	EXPECT_FALSE(_host->StoppedAfter().has_value());
	EXPECT_FALSE(watcher.session.StoppedAfter().has_value());
	// The god's player can still stop the turns of the decided game
	alice.session.StopGame();
	Frames({&alice, &watcher}, 5);
	EXPECT_TRUE(_host->StoppedAfter().has_value());
}

// A room of computer gods has no human god. The end box of its observer stops the turns of the decided game.
TEST_F(LockstepTest, AnObserverStopsTheTurnsOfARoomWithNoHumanGod)
{
	MakeAllComputerHost(1);
	GiveWorldState();
	Computer watcher(_network, _now, "observer:one");
	Frames({&watcher}, 20);
	ASSERT_GT(_serverWorld.turns, 0u);
	watcher.session.StopGame();
	Frames({&watcher}, 5);
	EXPECT_TRUE(_host->StoppedAfter().has_value());
	EXPECT_TRUE(watcher.session.StoppedAfter().has_value());
}

// On the local network a name is only a label. A computer that gives the name of a god of the running game does not take
// the god: it watches the game, and the player of the god keeps playing.
TEST_F(LockstepTest, OnTheLocalNetworkANameDoesNotTakeTheGodOfTheRunningGame)
{
	MakeHost(1, 5LL * 60 * 1'000'000, 2, false, true);
	GiveWorldState();
	Computer alice(_network, _now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	Frames({&alice}, 8);
	ASSERT_TRUE(_host->Started());
	const auto aliceConnection = _host->Seats()[0].connection;
	Computer impostor(_network, _now, "", k_LandFile, {'C'}, 0.0f, "Alice");
	Frames({&alice, &impostor}, 10);
	ASSERT_TRUE(impostor.session.GetJoined().has_value());
	EXPECT_EQ(impostor.session.GetJoined()->role, game_socket::Role::Observer);
	EXPECT_EQ(_host->Seats()[0].connection, aliceConnection);
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Taken);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	alice.session.Send(HandTap {.object = Entity(4)});
	Frames({&alice, &impostor}, 5);
	EXPECT_EQ(_serverWorld.taps[4], 1u);
}

// One god sends more commands than a turn can hold. The server refuses the commands of that god over the limit, and the
// commands of the other god still come in the turn on every computer.
TEST_F(LockstepTest, AFloodOfOneGodDoesNotPushTheCommandsOfAnotherGodOutOfTheTurn)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 7);
	ASSERT_TRUE(_host->Started());
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
	EXPECT_GT(_host->RefusedBlocks(), 0u);
	EXPECT_EQ(_serverWorld.taps[5], 1u);
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
		watchers.push_back(std::make_unique<Computer>(_network, _now, "observer:w" + std::to_string(i)));
	}
	std::vector<Computer*> computers;
	std::ranges::transform(watchers, std::back_inserter(computers), [](const auto& watcher) { return watcher.get(); });
	Frames(computers, 3);
	EXPECT_EQ(_host->Observers(), server::k_MostObservers);
	EXPECT_EQ(watchers.back()->session.RefusedReason(), "full");
}

// A socket that never joins does not stay open
TEST_F(LockstepTest, AConnectionThatDoesNotJoinInItsTimeCloses)
{
	MakeHost(1);
	auto client = _network.MakeClient();
	const auto id =
	    client->Connect({.url = "wss://127.0.0.1/game/1", .headers = {}, .subprotocols = {}, .serverKey = std::nullopt}, {});
	Frames({}, 2);
	EXPECT_EQ(_network.Count(), 1u);
	Frames({}, 200);
	EXPECT_EQ(_network.Count(), 0u);
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
	for (const std::string land : {"//host/share/x.txt", R"x(\\host\share\x.txt)x", "C:/Windows/win.ini", "../../x.txt",
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
	server.Text(game_socket::StartQuery {.land = R"x(\\host\share\land.txt)x", .testbed = false, .checksum = 1});
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

// The game socket is TCP, so the turns come in order and once. After a resume the server sends again turns that the
// client can have, and a fault of a server could send a turn twice or early: the client does each turn once and in order.
TEST(HostileGameServer, TurnsThatComeTwiceOrOutOfOrderTurnOnceAndInOrder)
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
	server.Binary(turn_codec::TurnBundle {.turn = 2, .blocks = {}});
	server.Binary(turn_codec::TurnBundle {.turn = 0, .blocks = {}});
	Exchange(network, alice);
	// Turn 1 did not come, so the world waits after turn 0
	EXPECT_EQ(alice.world.turns, 1u);
	server.Binary(turn_codec::TurnBundle {.turn = 0, .blocks = {}});
	server.Binary(turn_codec::TurnBundle {.turn = 2, .blocks = {}});
	server.Binary(turn_codec::TurnBundle {.turn = 1, .blocks = {}});
	server.Binary(turn_codec::TurnBundle {.turn = 1, .blocks = {}});
	for (int frame = 0; frame < 10; ++frame)
	{
		now += 100'000;
		Exchange(network, alice, 1);
	}
	EXPECT_EQ(alice.world.turns, 3u);
	EXPECT_EQ(alice.session.TurnsDone(), 3u);
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::None);
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
	Computer alice(_network, _now, "god:100");
	Computer bob(_network, _now, "god:bob");
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
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());
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
	const auto lostAt = _now;
	for (int frame = 0; frame < 3100; ++frame)
	{
		_now += k_Turn;
		_network.Cut(alice.sockets->Ids().back());
		_network.Poll();
		alice.Frame();
		if (notice.Update(facts()))
		{
			++told;
			toldAt = _now;
		}
		// The box waits while the client tries to come back
		if (_now - lostAt < 5LL * 60 * 1'000'000)
		{
			ASSERT_EQ(told, 0);
			ASSERT_NE(alice.session.GetState(), NetworkSession::State::Ended);
		}
	}
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::Lost);
	EXPECT_EQ(told, 1);
	EXPECT_GT(toldAt - lostAt, 5LL * 60 * 1'000'000);
}

TEST_F(LockstepTest, AServerThatStopsShowsTheSilenceAfterFourTurnsAndTheNextMessageEndsIt)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 60);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.WaitsForTurns());
	EXPECT_FALSE(alice.session.ServerSilence().has_value());

	// The server stops: the clients go on, but no message comes
	const auto pause = [&](int frames) {
		for (int frame = 0; frame < frames; ++frame)
		{
			_now += k_Turn;
			alice.Frame();
			bob.Frame();
		}
	};
	pause(4);
	EXPECT_FALSE(alice.session.ServerSilence().has_value());
	pause(1);
	ASSERT_TRUE(alice.session.ServerSilence().has_value());
	EXPECT_EQ(*alice.session.ServerSilence(), 5 * k_Turn);
	pause(295);
	ASSERT_TRUE(bob.session.ServerSilence().has_value());
	EXPECT_EQ(*bob.session.ServerSilence(), 300 * k_Turn);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);

	// The server goes on: the first message ends the silence, and the worlds stay in step
	Frame({&alice, &bob});
	EXPECT_FALSE(alice.session.ServerSilence().has_value());
	EXPECT_FALSE(bob.session.ServerSilence().has_value());
	Frames({&alice, &bob}, 100);
	EXPECT_FALSE(alice.session.ServerSilence().has_value());
	EXPECT_EQ(alice.session.Resumes(), 0u);
	EXPECT_GT(alice.session.GetCompares().compared, 100u);
	EXPECT_FALSE(alice.session.GetCompares().firstDifferent.has_value());
	EXPECT_FALSE(bob.session.GetCompares().firstDifferent.has_value());
	EXPECT_EQ(alice.world.hash, bob.world.hash);
}

TEST_F(LockstepTest, AnOpenSocketWithNoMessageForFiftySecondsMakesTheClientComeBack)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 60);
	ASSERT_TRUE(_host->Started());

	// Only Alice's socket gives nothing, and it stays open, as when the network of the server goes with no close
	for (int frame = 0; frame < 500; ++frame)
	{
		_now += k_Turn;
		alice.Frame();
	}
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	_now += k_Turn;
	alice.Frame();
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Reconnecting);
	ASSERT_TRUE(alice.session.ServerSilence().has_value());

	// The network comes back: the client comes back into its god, and the silence ends
	Frames({&alice, &bob}, 200);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(alice.session.Resumes(), 1u);
	EXPECT_FALSE(alice.session.ServerSilence().has_value());
	EXPECT_FALSE(alice.session.GetCompares().firstDifferent.has_value());
	EXPECT_EQ(alice.world.turns, bob.world.turns);
	EXPECT_EQ(alice.world.hash, bob.world.hash);
}

TEST_F(LockstepTest, NoSilenceShowsBeforeTheStartOrAfterTheTurnsStop)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	// Alone in the room, the client waits for the start and not for turns
	for (int frame = 0; frame < 50; ++frame)
	{
		_now += k_Turn;
		alice.Frame();
	}
	EXPECT_FALSE(alice.session.WaitsForTurns());
	EXPECT_FALSE(alice.session.ServerSilence().has_value());
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
	Computer alice(_network, _now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer carol(_network, _now, "observer:carol@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer dave(_network, _now, "god:dave@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer other(_network, _now, "god:bob@lobby2", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer noLobby(_network, _now, "god:bob", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer wrongRole(_network, _now, "observer:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
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
	Computer other(_network, _now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, "0.1.0+b/codec1");
	Computer none(_network, _now, "god:bob@lobby1");
	Frames({&other, &none}, 3);
	EXPECT_EQ(other.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(other.session.RefusedReason(), "version");
	EXPECT_EQ(none.session.RefusedReason(), "version");
	EXPECT_EQ(_host->GodsInRoom(), 0u);
}

TEST_F(LockstepTest, ARoomWithNoLobbyRefusesAnotherBuildAndTakesAClientWithNoBuild)
{
	MakeHost(2);
	// A room with no build id of its own takes every build
	Computer any(_network, _now, "god:alice", k_LandFile, {'C'}, 0.0f, {}, "0.1.0+b/codec1");
	Frames({&any}, 3);
	EXPECT_TRUE(any.session.GetJoined().has_value());

	_host.reset();
	_host = std::make_unique<LockstepHost>(
	    _network, CheckFakeTicket, [this] { return _now; }, [this] { return "secret-" + std::to_string(++_secrets); },
	    server::HostSettings {.start = {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow"},
	                          .landChecksum = online::start_rules::LandChecksum(k_LandFile),
	                          .minGods = 2,
	                          .godSlots = 4,
	                          .comeBackMicroseconds = 5LL * 60 * 1'000'000,
	                          .keptTurns = 3200,
	                          .sessionHasher = _hasher,
	                          .buildId = std::string(k_ServerBuild)});
	Computer older(_network, _now, "god:bob");
	Computer other(_network, _now, "god:carol", k_LandFile, {'C'}, 0.0f, {}, "0.1.0+b/codec1");
	Frames({&older, &other}, 3);
	EXPECT_TRUE(older.session.GetJoined().has_value());
	EXPECT_EQ(other.session.RefusedReason(), "version");
}

TEST_F(LockstepTest, TheRoomOfALobbyRefusesAClientWithAnotherFileOfTheLand)
{
	const std::string map(64, 'a');
	MakeLobbyHost(TwoGodsAndAnObserver(), 1, std::string(k_ServerBuild), map);
	const std::string build(k_ServerBuild);
	Computer same(_network, _now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build, map);
	Computer other(_network, _now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build, std::string(64, 'b'));
	Frames({&same, &other}, 3);
	EXPECT_TRUE(same.session.GetJoined().has_value());
	EXPECT_EQ(other.session.RefusedReason(), "map");
}

TEST_F(LockstepTest, TheRoomOfALobbyWaitsForEachGodUntilTheTimeToJoinEnds)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 1);
	EXPECT_EQ(_host->GodsToWaitFor(), 2u);
	const std::string build(k_ServerBuild);
	Computer alice(_network, _now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice}, 10);
	// One god of two is ready: the room waits for the other god of the lobby
	EXPECT_EQ(_host->GodsInRoom(), 1u);
	EXPECT_FALSE(_host->Started());
	EXPECT_FALSE(_host->Starting());
	_host->EndJoinWindow();
	EXPECT_EQ(_host->GodsToWaitFor(), 1u);
	Frames({&alice}, 10);
	EXPECT_TRUE(_host->Started());
}

TEST_F(LockstepTest, TheRoomOfALobbyStartsWhenEachGodOfTheLobbyJoined)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 1);
	const std::string build(k_ServerBuild);
	Computer alice(_network, _now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer bob(_network, _now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice, &bob}, 10);
	EXPECT_TRUE(_host->Started());
	EXPECT_EQ(bob.session.GetJoined()->slot, 0);
}

TEST_F(LockstepTest, TheTurnsOfALobbyGameWaitUntilEachGodsPlayerLoadedTheWorld)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 2);
	const std::string build(k_ServerBuild);
	Computer alice(_network, _now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer bob(_network, _now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(_host->Started());
	// No turn runs: the game is paused for every computer, and each sees the players who load
	EXPECT_TRUE(_host->WaitsForLoads());
	EXPECT_EQ(_serverWorld.turns, 0u);
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
	EXPECT_EQ(_serverWorld.turns, 0u);
	ASSERT_TRUE(bob.session.LoadWait().has_value());
	EXPECT_EQ(bob.session.LoadWait()->loading, (std::vector<std::string> {"bob"}));
	EXPECT_EQ(alice.session.LoadWait()->loading, (std::vector<std::string> {"bob"}));

	// Bob loaded: the pause ends for all, and the turns run
	bob.session.ReportLoaded();
	Frames({&alice, &bob}, 10);
	EXPECT_FALSE(_host->WaitsForLoads());
	EXPECT_FALSE(alice.session.LoadWait().has_value());
	EXPECT_FALSE(bob.session.LoadWait().has_value());
	EXPECT_GT(_serverWorld.turns, 5u);
	EXPECT_GT(alice.world.turns, 5u);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
	// The tap of the pause did not go into a turn
	EXPECT_TRUE(_host->Seats()[2].commands.empty());
}

TEST_F(LockstepTest, AReportOfTheLoadThatTheTestsDelayGoesLateAndTheTurnsWaitForIt)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 2);
	const std::string build(k_ServerBuild);
	Computer alice(_network, _now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer bob(_network, _now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build, {}, 3LL * 1'000'000);
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(_host->Started());
	alice.session.ReportLoaded();
	bob.session.ReportLoaded();
	// Bob's computer loaded, but it tells the server 3 seconds late: alice sees the pause with bob's name
	Frames({&alice, &bob}, 20);
	EXPECT_TRUE(_host->WaitsForLoads());
	EXPECT_EQ(_serverWorld.turns, 0u);
	ASSERT_TRUE(alice.session.LoadWait().has_value());
	EXPECT_EQ(alice.session.LoadWait()->loading, (std::vector<std::string> {"bob"}));
	// After the delay the report goes, and the turns run
	Frames({&alice, &bob}, 20);
	EXPECT_FALSE(_host->WaitsForLoads());
	EXPECT_FALSE(alice.session.LoadWait().has_value());
	EXPECT_GT(_serverWorld.turns, 5u);
}

TEST_F(LockstepTest, AClientOfARoomThatTheServerEndsBeforeTheStartDoesNotTryToComeBack)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 2);
	const std::string build(k_ServerBuild);
	Computer alice(_network, _now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice}, 10);
	ASSERT_FALSE(_host->Started());
	ASSERT_TRUE(alice.session.GetJoined().has_value());
	_host->EndRoom("ended");
	Frames({&alice}, 10);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Ended);
	EXPECT_EQ(alice.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(alice.session.RefusedReason(), "ended");
}

TEST_F(LockstepTest, AGodWhosePlayerLeavesDuringTheLoadsLeavesAndTheTurnsStartWithTheOthers)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 2);
	const std::string build(k_ServerBuild);
	Computer alice(_network, _now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer bob(_network, _now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(_host->Started());
	alice.session.ReportLoaded();
	Frames({&alice, &bob}, 5);
	EXPECT_EQ(_serverWorld.turns, 0u);
	// Bob leaves before his world loaded: his god leaves the game, and the game goes on with alice
	bob.session.Leave();
	Frames({&alice, &bob}, 10);
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Closed);
	EXPECT_FALSE(_host->WaitsForLoads());
	EXPECT_FALSE(alice.session.LoadWait().has_value());
	EXPECT_GT(_serverWorld.turns, 5u);
	EXPECT_FALSE(_host->GameOver());
}

TEST_F(LockstepTest, AGodWhosePlayerDoesNotLoadInTimeLeavesTheGame)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 2, std::string(k_ServerBuild), {}, 5LL * 1'000'000);
	const std::string build(k_ServerBuild);
	Computer alice(_network, _now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer bob(_network, _now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(_host->Started());
	alice.session.ReportLoaded();
	// The time to load counts 5 seconds from the time when the room was made
	Frames({&alice, &bob}, 30);
	EXPECT_TRUE(_host->WaitsForLoads());
	EXPECT_EQ(_serverWorld.turns, 0u);
	Frames({&alice, &bob}, 30);
	// Bob did not load in time: the server closes his socket with "load", and his god leaves as a player who leaves
	EXPECT_EQ(bob.session.GetEnd(), NetworkSession::End::Refused);
	EXPECT_EQ(bob.session.RefusedReason(), "load");
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Closed);
	EXPECT_FALSE(_host->WaitsForLoads());
	EXPECT_GT(_serverWorld.turns, 5u);
	EXPECT_EQ(alice.session.GetState(), NetworkSession::State::Playing);
}

TEST_F(LockstepTest, ARoomWithNoLobbyDoesNotWaitForTheLoads)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 20);
	ASSERT_TRUE(_host->Started());
	EXPECT_FALSE(_host->WaitsForLoads());
	EXPECT_FALSE(alice.session.LoadWait().has_value());
	EXPECT_GT(_serverWorld.turns, 5u);
}

TEST_F(LockstepTest, AGameThatStartsAfterTheTimeToLoadGetsTheWholeTimeFromItsStart)
{
	MakeLobbyHost(TwoGodsAndAnObserver(), 2, std::string(k_ServerBuild), {}, 5LL * 1'000'000);
	// 6 seconds go by before the players join
	Frames({}, 60);
	const std::string build(k_ServerBuild);
	Computer alice(_network, _now, "god:alice@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Computer bob(_network, _now, "god:bob@lobby1", k_LandFile, {'C'}, 0.0f, {}, build);
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.LoadWait().has_value());
	// The wait gives the whole 5 seconds from the start
	EXPECT_GE(alice.session.LoadWait()->secondsLeft, 4u);
	EXPECT_LE(alice.session.LoadWait()->secondsLeft, 5u);
	Frames({&alice, &bob}, 30);
	EXPECT_TRUE(_host->WaitsForLoads());
	Frames({&alice, &bob}, 30);
	EXPECT_FALSE(_host->WaitsForLoads());
	EXPECT_EQ(alice.session.RefusedReason(), "load");
	EXPECT_EQ(bob.session.RefusedReason(), "load");
}

/// The server's world as a world file of the editor gives it: the hash and the turn of the fake world
std::vector<uint8_t> FakeWorldFile(uint64_t hash, uint32_t turn)
{
	std::vector<uint8_t> bytes(sizeof(uint64_t) + sizeof(uint32_t));
	std::memcpy(bytes.data(), &hash, sizeof(uint64_t));
	std::memcpy(bytes.data() + sizeof(uint64_t), &turn, sizeof(uint32_t));
	return bytes;
}

TEST_F(LockstepTest, AWorldFileOfTheEditorLoadsOnTheServerAndEveryComputerGoesOnFromItsTurn)
{
	MakeHost(2);
	GiveWorldState();
	std::vector<server::LockstepHost::WorldChange> changes;
	_host->SetChangeWorld([this, &changes](const server::LockstepHost::WorldChange& change) {
		changes.push_back(change);
		// The fake world file is the hash and the turn of a world
		std::memcpy(&_serverWorld.hash, change.file.data(), sizeof(uint64_t));
		std::memcpy(&_serverWorld.turns, change.file.data() + sizeof(uint64_t), sizeof(uint32_t));
		return server::LockstepHost::WorldChanged {.turn = _serverWorld.turns, .refusal = {}, .mapSha256 = {}};
	});
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 40);
	ASSERT_TRUE(_host->Started());
	ASSERT_GT(alice.world.turns, 20u);
	alice.session.ReportLoaded();
	bob.session.ReportLoaded();
	Frames({&alice, &bob}, 2);
	// A world file of an earlier turn, larger than one part, so that it goes in several parts
	auto file = FakeWorldFile(0x1234'5678'9abc'def0ull, 7);
	file.resize(game_socket::k_MostEditorWorldChunkBytes * 2 + 5, 0);
	ASSERT_TRUE(alice.session.SendEditorWorld(file));
	Frames({&alice, &bob}, 20);
	ASSERT_EQ(changes.size(), 1u);
	EXPECT_EQ(changes.front().kind, server::LockstepHost::WorldChange::Kind::WorldFile);
	EXPECT_EQ(changes.front().file, file);
	EXPECT_EQ(changes.front().by, "alice");
	EXPECT_EQ(_host->WorldChanges(), 1u);
	// Each computer loaded the server's new world and does its turns from turn 7, in step with the server
	EXPECT_EQ(alice.loads, 1u);
	EXPECT_EQ(bob.loads, 1u);
	EXPECT_GT(alice.world.turns, 7u);
	EXPECT_LT(alice.world.turns, 40u);
	EXPECT_EQ(alice.world.turns, _serverWorld.turns);
	EXPECT_EQ(bob.world.hash, _serverWorld.hash);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
	EXPECT_TRUE(alice.desyncs.empty());
	EXPECT_TRUE(bob.desyncs.empty());
	EXPECT_EQ(_host->RefusedEditorValues(), 0u);
	// The server compares the checksums of the new world again, and each is the same
	for (const auto& seat : {_host->Seats()[0], _host->Seats()[1]})
	{
		EXPECT_FALSE(seat.oldWorldChecksums);
		EXPECT_GT(seat.compares.compared, 40u);
		EXPECT_EQ(seat.compares.compared, seat.compares.same);
	}
}

TEST_F(LockstepTest, TheServerTakesAWorldFileOrAScenarioOnlyFromTheEditor)
{
	MakeHost(2);
	GiveWorldState();
	uint32_t changes = 0;
	_host->SetChangeWorld([&changes](const server::LockstepHost::WorldChange& /*change*/) {
		++changes;
		return server::LockstepHost::WorldChanged {
		    .turn = std::nullopt, .refusal = "the fake world does not change", .mapSha256 = {}};
	});
	_host->SetScenarioRefusal([](std::string_view id) -> std::optional<std::string> {
		return id == "known" ? std::nullopt : std::optional<std::string>("no such scenario");
	});
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 10);
	ASSERT_TRUE(_host->Started());
	// A player with no editor claim
	ASSERT_TRUE(bob.session.SendEditorWorld(FakeWorldFile(1, 2)));
	ASSERT_TRUE(bob.session.SendEditorScenario("known"));
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(_host->RefusedEditorCommands(), 2u);
	EXPECT_GE(bob.session.TakeEditorRefusals(), 1u);
	EXPECT_EQ(changes, 0u);
	// The editor asks for a scenario that no computer has, and a world file that the world of the server refuses
	ASSERT_TRUE(alice.session.SendEditorScenario("unknown"));
	Frames({&alice, &bob}, 2);
	EXPECT_EQ(changes, 0u);
	ASSERT_TRUE(alice.session.SendEditorWorld(FakeWorldFile(1, 2)));
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(changes, 1u);
	EXPECT_EQ(_host->RefusedEditorValues(), 2u);
	EXPECT_EQ(_host->WorldChanges(), 0u);
	EXPECT_EQ(alice.loads, 0u);
	EXPECT_EQ(alice.restarts, 0u);
	EXPECT_GT(alice.world.turns, 15u);
}

TEST_F(LockstepTest, AScenarioOfTheEditorStartsTheRoomAgainOnTheTestbedForEveryComputer)
{
	MakeHost(2);
	std::vector<std::string> scenarios;
	_host->SetChangeWorld([this, &scenarios](const server::LockstepHost::WorldChange& change) {
		scenarios.push_back(change.scenario);
		EXPECT_EQ(change.seed, 1u);
		_serverWorld = FakeWorld {};
		_serverWorld.mindFileHere = [this](uint64_t hash) { return !_host->EditorMindFile(hash).empty(); };
		return server::LockstepHost::WorldChanged {.turn = 0, .refusal = {}, .mapSha256 = {}};
	});
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 40);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.SendEditorScenario("creature.walk"));
	Frames({&alice, &bob}, 20);
	ASSERT_EQ(scenarios, (std::vector<std::string> {"creature.walk"}));
	EXPECT_EQ(_host->WorldChanges(), 1u);
	// Each computer started the game again on the testbed with the scenario, and does the turns from turn 0
	for (auto* computer : {&alice, &bob})
	{
		EXPECT_EQ(computer->restarts, 1u);
		const auto& start = computer->session.GetStart();
		ASSERT_TRUE(start.has_value());
		EXPECT_EQ(start->scenario, std::optional<std::string>("creature.walk"));
		EXPECT_TRUE(start->testbed);
		EXPECT_TRUE(start->land.empty());
		EXPECT_EQ(start->turn, 0u);
		EXPECT_EQ(computer->world.turns, _serverWorld.turns);
		EXPECT_LT(computer->world.turns, 40u);
		EXPECT_EQ(computer->world.hash, _serverWorld.hash);
		EXPECT_EQ(computer->session.GetCompares().compared, computer->session.GetCompares().same);
		EXPECT_TRUE(computer->desyncs.empty());
	}
	// The first turn of the new game gave each god its hands again
	EXPECT_TRUE(alice.world.players.contains(PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(alice.world.players.contains(PlayerNames::PLAYER_TWO));
	EXPECT_EQ(_host->RefusedEditorValues(), 0u);
}

TEST_F(LockstepTest, APlayerWhoWasAwayDuringAWorldFileComesBackWithTheNewWorld)
{
	MakeHost(2);
	GiveWorldState();
	std::vector<PlayerNames> players;
	_host->SetChangeWorld([this, &players](const server::LockstepHost::WorldChange& change) {
		players = change.players;
		std::memcpy(&_serverWorld.hash, change.file.data(), sizeof(uint64_t));
		std::memcpy(&_serverWorld.turns, change.file.data() + sizeof(uint64_t), sizeof(uint32_t));
		return server::LockstepHost::WorldChanged {.turn = _serverWorld.turns, .refusal = {}, .mapSha256 = {}};
	});
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 40);
	ASSERT_TRUE(_host->Started());
	_network.Cut(bob.sockets->Ids().back());
	Frames({&alice}, 1);
	bob.Frame();
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Reconnecting);
	// The world goes back to turn 30 while the other player is away: the turn of the away client is a turn of the new
	// world too, but its world is the old one
	ASSERT_TRUE(alice.session.SendEditorWorld(FakeWorldFile(0xabcdull, 30)));
	Frames({&alice}, 5);
	ASSERT_EQ(alice.loads, 1u);
	Frames({&alice, &bob}, 15);
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.loads, 1u);
	EXPECT_EQ(bob.world.turns, _serverWorld.turns);
	EXPECT_EQ(bob.world.hash, _serverWorld.hash);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
	EXPECT_TRUE(bob.desyncs.empty());
	// The world of the server gives a god to each player of the room, also to the player who was away
	EXPECT_EQ(players, (std::vector<PlayerNames> {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO}));
}

TEST_F(LockstepTest, APlayerWhoWasAwayDuringAScenarioStartComesBackIntoTheScenario)
{
	MakeHost(2);
	_host->SetChangeWorld([this](const server::LockstepHost::WorldChange& /*change*/) {
		_serverWorld = FakeWorld {};
		_serverWorld.mindFileHere = [this](uint64_t hash) { return !_host->EditorMindFile(hash).empty(); };
		return server::LockstepHost::WorldChanged {.turn = 0, .refusal = {}, .mapSha256 = {}};
	});
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 40);
	ASSERT_TRUE(_host->Started());
	_network.Cut(bob.sockets->Ids().back());
	Frames({&alice}, 1);
	bob.Frame();
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Reconnecting);
	ASSERT_TRUE(alice.session.SendEditorScenario("creature.walk"));
	Frames({&alice}, 20);
	ASSERT_EQ(alice.restarts, 1u);
	// The god of the away player gets its hands in the first turn of the scenario too
	EXPECT_TRUE(alice.world.players.contains(PlayerNames::PLAYER_TWO));
	Frames({&alice, &bob}, 30);
	// The client of bob got the start of the scenario again, and did each turn of it from its first turn
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.restarts, 1u);
	EXPECT_EQ(bob.loads, 0u);
	const auto& start = bob.session.GetStart();
	ASSERT_TRUE(start.has_value());
	EXPECT_EQ(start->scenario, std::optional<std::string>("creature.walk"));
	EXPECT_TRUE(start->testbed);
	EXPECT_EQ(start->turn, 0u);
	// The playout of a client that came back keeps a turn or two in hand
	EXPECT_LE(bob.world.turns, _serverWorld.turns);
	EXPECT_GE(bob.world.turns + 2, _serverWorld.turns);
	EXPECT_TRUE(bob.world.players.contains(PlayerNames::PLAYER_TWO));
	// The checksum of each turn of the scenario is the same as the server's
	EXPECT_GE(bob.session.GetCompares().compared, 40u);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
	EXPECT_TRUE(bob.desyncs.empty());
	EXPECT_TRUE(alice.desyncs.empty());
	// The server compares the checksums of the client of bob again after the report of its load, and each is the same
	const auto& seat = _host->Seats()[1];
	EXPECT_FALSE(seat.oldWorldChecksums);
	EXPECT_FALSE(seat.missedWorldChange);
	EXPECT_EQ(seat.compares.compared, seat.compares.same);
}

TEST_F(LockstepTest, ALandOfTheEditorStartsTheRoomAgainOnTheLandForEveryComputer)
{
	MakeHost(2);
	std::vector<server::LockstepHost::WorldChange> changes;
	_host->SetChangeWorld([this, &changes](const server::LockstepHost::WorldChange& change) {
		changes.push_back(change);
		_serverWorld = FakeWorld {};
		return server::LockstepHost::WorldChanged {.turn = 0, .refusal = {}, .mapSha256 = {}};
	});
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 40);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.SendEditorLand({.land = "Land2.txt", .testbed = false, .godsRingMap = false}));
	Frames({&alice, &bob}, 20);
	ASSERT_EQ(changes.size(), 1u);
	EXPECT_EQ(changes.front().kind, server::LockstepHost::WorldChange::Kind::Land);
	EXPECT_EQ(changes.front().land, "Land2.txt");
	EXPECT_FALSE(changes.front().testbed);
	EXPECT_EQ(changes.front().seed, 1u);
	EXPECT_EQ(changes.front().by, "alice");
	EXPECT_EQ(_host->WorldChanges(), 1u);
	// Each computer started the game again on the land at the same turn, and does the turns in step with the server
	for (auto* computer : {&alice, &bob})
	{
		EXPECT_EQ(computer->restarts, 1u);
		const auto& start = computer->session.GetStart();
		ASSERT_TRUE(start.has_value());
		EXPECT_EQ(start->land, "Land2.txt");
		EXPECT_FALSE(start->testbed);
		EXPECT_FALSE(start->scenario.has_value());
		EXPECT_EQ(start->turn, 0u);
		EXPECT_EQ(computer->world.turns, _serverWorld.turns);
		EXPECT_EQ(computer->world.hash, _serverWorld.hash);
		EXPECT_EQ(computer->session.GetCompares().compared, computer->session.GetCompares().same);
		EXPECT_TRUE(computer->desyncs.empty());
	}
	// The first turn of the new game gave each god its hands again
	EXPECT_TRUE(alice.world.players.contains(PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(alice.world.players.contains(PlayerNames::PLAYER_TWO));
	// The room keeps the new land, so a player who joins later loads it too
	EXPECT_NE(_host->RoomRecord().start.find("Land2.txt"), std::string::npos);
	// The editor then loads the testbed for the room
	ASSERT_TRUE(alice.session.SendEditorLand({.land = {}, .testbed = true, .godsRingMap = false}));
	Frames({&alice, &bob}, 20);
	ASSERT_EQ(changes.size(), 2u);
	EXPECT_TRUE(changes.back().testbed);
	EXPECT_TRUE(changes.back().land.empty());
	for (auto* computer : {&alice, &bob})
	{
		EXPECT_EQ(computer->restarts, 2u);
		EXPECT_TRUE(computer->session.GetStart()->testbed);
		EXPECT_TRUE(computer->session.GetStart()->land.empty());
		EXPECT_EQ(computer->world.hash, _serverWorld.hash);
	}
	EXPECT_EQ(_host->RefusedEditorValues(), 0u);
}

TEST_F(LockstepTest, TheMapFor32GodsOfTheEditorStartsTheRoomAgainOnTheMapWithTheFileOfTheServer)
{
	MakeHost(2);
	const std::string sha256(64, 'a');
	std::vector<server::LockstepHost::WorldChange> changes;
	_host->SetChangeWorld([this, &changes, &sha256](const server::LockstepHost::WorldChange& change) {
		changes.push_back(change);
		_serverWorld = FakeWorld {};
		return server::LockstepHost::WorldChanged {.turn = 0, .refusal = {}, .mapSha256 = sha256};
	});
	// The map has no script of the game, so the server does not check a land name
	uint32_t landChecks = 0;
	_host->SetLandRefusal([&landChecks](std::string_view /*land*/) -> std::optional<std::string> {
		++landChecks;
		return "no land passes";
	});
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 40);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.SendEditorLand({.land = {}, .testbed = false, .godsRingMap = true}));
	Frames({&alice, &bob}, 20);
	EXPECT_EQ(landChecks, 0u);
	ASSERT_EQ(changes.size(), 1u);
	EXPECT_EQ(changes.front().kind, server::LockstepHost::WorldChange::Kind::Land);
	EXPECT_TRUE(changes.front().godsRingMap);
	EXPECT_FALSE(changes.front().testbed);
	EXPECT_TRUE(changes.front().land.empty());
	EXPECT_TRUE(changes.front().opening.empty());
	// Each computer gets the start of the map with the SHA-256 of the server's file, and plays in step with the server
	for (auto* computer : {&alice, &bob})
	{
		EXPECT_EQ(computer->restarts, 1u);
		const auto& start = computer->session.GetStart();
		ASSERT_TRUE(start.has_value());
		EXPECT_TRUE(start->godsRingMap);
		EXPECT_FALSE(start->testbed);
		EXPECT_EQ(start->land, gods_ring_map_file::WireName());
		EXPECT_EQ(start->mapSha256, sha256);
		EXPECT_EQ(computer->world.hash, _serverWorld.hash);
		EXPECT_TRUE(computer->desyncs.empty());
	}
	// A later land of the editor is a script of the game again, with no file of a map
	_host->SetLandRefusal([](std::string_view /*land*/) -> std::optional<std::string> { return std::nullopt; });
	ASSERT_TRUE(alice.session.SendEditorLand({.land = "Land2.txt", .testbed = false, .godsRingMap = false}));
	Frames({&alice, &bob}, 20);
	ASSERT_EQ(changes.size(), 2u);
	for (auto* computer : {&alice, &bob})
	{
		EXPECT_EQ(computer->restarts, 2u);
		EXPECT_FALSE(computer->session.GetStart()->godsRingMap);
		EXPECT_EQ(computer->session.GetStart()->land, "Land2.txt");
		EXPECT_TRUE(computer->session.GetStart()->mapSha256.empty());
	}
	// The session sends no request that names the map and a land or the testbed
	EXPECT_FALSE(alice.session.SendEditorLand({.land = "Land2.txt", .testbed = false, .godsRingMap = true}));
	EXPECT_FALSE(alice.session.SendEditorLand({.land = {}, .testbed = true, .godsRingMap = true}));
}

TEST_F(LockstepTest, TheServerTakesALandOnlyFromAPlayerWithTheEditorClaim)
{
	MakeHost(3);
	uint32_t changes = 0;
	_host->SetChangeWorld([&changes](const server::LockstepHost::WorldChange& /*change*/) {
		++changes;
		return server::LockstepHost::WorldChanged {
		    .turn = std::nullopt, .refusal = "the fake world does not change", .mapSha256 = {}};
	});
	_host->SetLandRefusal([](std::string_view land) -> std::optional<std::string> {
		return land == "Land2.txt" ? std::nullopt : std::optional<std::string>("the game has no such land");
	});
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Computer carol(_network, _now, "god+debugger:carol");
	Frames({&alice, &bob, &carol}, 10);
	ASSERT_TRUE(_host->Started());
	// A player with no claim, and a player with only the debugger claim
	ASSERT_TRUE(bob.session.SendEditorLand({.land = "Land2.txt", .testbed = false, .godsRingMap = false}));
	ASSERT_TRUE(carol.session.SendEditorLand({.land = "Land2.txt", .testbed = false, .godsRingMap = false}));
	Frames({&alice, &bob, &carol}, 4);
	EXPECT_EQ(_host->RefusedEditorCommands(), 2u);
	EXPECT_GE(bob.session.TakeEditorRefusals(), 1u);
	EXPECT_GE(carol.session.TakeEditorRefusals(), 1u);
	EXPECT_EQ(changes, 0u);
	// The editor asks for a land that the game does not have, then for a land that the world of the server refuses
	ASSERT_TRUE(alice.session.SendEditorLand({.land = "NoSuchLand.txt", .testbed = false, .godsRingMap = false}));
	Frames({&alice, &bob, &carol}, 2);
	EXPECT_EQ(changes, 0u);
	ASSERT_TRUE(alice.session.SendEditorLand({.land = "Land2.txt", .testbed = false, .godsRingMap = false}));
	Frames({&alice, &bob, &carol}, 4);
	EXPECT_EQ(changes, 1u);
	EXPECT_EQ(_host->RefusedEditorValues(), 2u);
	EXPECT_EQ(_host->WorldChanges(), 0u);
	EXPECT_EQ(alice.restarts, 0u);
	EXPECT_EQ(bob.restarts, 0u);
	// The session sends no request that names no land and no testbed, or both
	EXPECT_FALSE(alice.session.SendEditorLand({.land = {}, .testbed = false, .godsRingMap = false}));
	EXPECT_FALSE(alice.session.SendEditorLand({.land = "Land2.txt", .testbed = true, .godsRingMap = false}));
}

TEST_F(LockstepTest, APlayerWhoWasAwayDuringALandChangeLoadsTheNewLandAndThenTheWorldOfTheServer)
{
	MakeHost(2);
	GiveWorldState();
	_host->SetChangeWorld([this](const server::LockstepHost::WorldChange& /*change*/) {
		_serverWorld = FakeWorld {};
		return server::LockstepHost::WorldChanged {.turn = 0, .refusal = {}, .mapSha256 = {}};
	});
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 40);
	ASSERT_TRUE(_host->Started());
	_network.Cut(bob.sockets->Ids().back());
	Frames({&alice}, 1);
	bob.Frame();
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Reconnecting);
	ASSERT_TRUE(alice.session.SendEditorLand({.land = "Land2.txt", .testbed = false, .godsRingMap = false}));
	Frames({&alice}, 20);
	ASSERT_EQ(alice.restarts, 1u);
	EXPECT_TRUE(_host->Seats()[1].missedWorldChange);
	// The god of the away player gets its hands in the first turn of the new land too
	EXPECT_TRUE(alice.world.players.contains(PlayerNames::PLAYER_TWO));
	Frames({&alice, &bob}, 30);
	// The client of bob got the start of the land, and then the world of the server
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.restarts, 1u);
	EXPECT_EQ(bob.loads, 1u);
	const auto& start = bob.session.GetStart();
	ASSERT_TRUE(start.has_value());
	EXPECT_EQ(start->land, "Land2.txt");
	EXPECT_FALSE(start->scenario.has_value());
	EXPECT_LE(bob.world.turns, _serverWorld.turns);
	EXPECT_GE(bob.world.turns + 2, _serverWorld.turns);
	EXPECT_EQ(bob.world.hash, _serverWorld.hash);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
	EXPECT_TRUE(bob.desyncs.empty());
	EXPECT_TRUE(alice.desyncs.empty());
	const auto& seat = _host->Seats()[1];
	EXPECT_FALSE(seat.missedWorldChange);
	EXPECT_EQ(seat.compares.compared, seat.compares.same);
}

// A long load of the new land on the server can outlast the patience of a client: the client closes its socket while the
// server loads, and asks to resume from a turn of the old land
TEST_F(LockstepTest, APlayerWhoLostTheConnectionDuringTheLoadOfALandComesBackOnTheNewLand)
{
	MakeHost(2);
	GiveWorldState();
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	_host->SetChangeWorld([this, &bob](const server::LockstepHost::WorldChange& /*change*/) {
		// The socket of bob closes while the server loads the land
		_network.Cut(bob.sockets->Ids().back());
		_serverWorld = FakeWorld {};
		return server::LockstepHost::WorldChanged {.turn = 0, .refusal = {}, .mapSha256 = {}};
	});
	Frames({&alice, &bob}, 40);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.SendEditorLand({.land = "Land2.txt", .testbed = false, .godsRingMap = false}));
	Frames({&alice}, 20);
	ASSERT_EQ(alice.restarts, 1u);
	EXPECT_TRUE(_host->Seats()[1].missedWorldChange);
	EXPECT_FALSE(_host->Seats()[1].oldWorldChecksums);
	Frames({&alice, &bob}, 40);
	// The client of bob got the start of the land and the world of the server, and the server did not refuse it
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.restarts, 1u);
	EXPECT_EQ(bob.loads, 1u);
	ASSERT_TRUE(bob.session.GetStart().has_value());
	EXPECT_EQ(bob.session.GetStart()->land, "Land2.txt");
	EXPECT_EQ(bob.world.hash, _serverWorld.hash);
	EXPECT_TRUE(bob.desyncs.empty());
	EXPECT_FALSE(_host->Seats()[1].missedWorldChange);
}

// The snapshots of a room that changed its land keep the start of the command line. A server that continues the game
// from such a snapshot sends the land of the room to a player who was away, before the world of the server.
TEST_F(LockstepTest, ARestoredServerSendsTheLandOfTheRoomToAPlayerWhoWasAwayAtTheLandChange)
{
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	_host->SetChangeWorld([this](const server::LockstepHost::WorldChange& /*change*/) {
		_serverWorld = FakeWorld {};
		return server::LockstepHost::WorldChanged {.turn = 0, .refusal = {}, .mapSha256 = {}};
	});
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 40);
	ASSERT_TRUE(_host->Started());
	EXPECT_TRUE(_host->RoomRecord().firstStart.empty());
	_network.Cut(bob.sockets->Ids().back());
	Frames({&alice}, 1);
	bob.Frame();
	ASSERT_TRUE(alice.session.SendEditorLand({.land = "Land2.txt", .testbed = false, .godsRingMap = false}));
	Frames({&alice}, 20);
	ASSERT_EQ(alice.restarts, 1u);
	ASSERT_EQ(bob.restarts, 0u);
	const auto room = _host->RoomRecord();
	EXPECT_NE(room.start.find("Land2.txt"), std::string::npos);
	EXPECT_NE(room.firstStart.find("Land1.txt"), std::string::npos);
	EXPECT_EQ(room.firstStart.find("Land2.txt"), std::string::npos);
	// The server stops before the player of bob comes back
	RestartFrom(room, _host->TurnsDone(), _serverWorld, {&alice, &bob});
	EXPECT_FALSE(_host->Seats()[0].missedWorldChange);
	EXPECT_TRUE(_host->Seats()[1].missedWorldChange);
	// The restored room keeps the start of the command line for the next snapshots
	EXPECT_EQ(_host->RoomRecord().firstStart, room.firstStart);
	Frames({&alice, &bob}, 40);
	ASSERT_EQ(alice.session.Resumes(), 1u);
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	// The player of alice was in the game at the change: the client did not load the land again
	EXPECT_EQ(alice.restarts, 1u);
	// The client of bob got the start of the land, and then the world of the server
	EXPECT_EQ(bob.restarts, 1u);
	EXPECT_EQ(bob.loads, 1u);
	EXPECT_EQ(bob.world.hash, _serverWorld.hash);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_TRUE(bob.desyncs.empty());
	EXPECT_TRUE(alice.desyncs.empty());
	EXPECT_FALSE(_host->Seats()[1].missedWorldChange);
}

TEST(RoomClock, ThePlayerWithTheDebuggerClaimChangesOnlyTheSpeed)
{
	using game_socket::EditorTime;
	using game_socket::TimeAct;
	namespace room_clock = online::room_clock;
	const room_clock::State paused {.paused = true, .speed = 1.0f, .steps = 0};
	const auto& rule = room_clock::k_Rule;
	const room_clock::Claims debugger {.editor = false, .debugger = true};
	// The Game Speed menu of the debug menu bar changes the speed
	EXPECT_FALSE(room_clock::RefusalOf(rule, debugger, paused, EditorTime {.act = TimeAct::Speed, .speed = 2.0f}));
	EXPECT_EQ(room_clock::RefusalOf(rule, debugger, paused, EditorTime {.act = TimeAct::Speed, .speed = 20.0f}),
	          room_clock::Refusal::SpeedOutOfRange);
	// The pause, the play and the step stay with the editor claim
	for (const auto act : {TimeAct::Pause, TimeAct::Play, TimeAct::Step})
	{
		EXPECT_EQ(room_clock::RefusalOf(rule, debugger, paused, EditorTime {.act = act}), room_clock::Refusal::NoRight);
		EXPECT_FALSE(room_clock::RefusalOf(rule, room_clock::Claims {.editor = true}, paused, EditorTime {.act = act}));
	}
	EXPECT_EQ(room_clock::RefusalOf(rule, room_clock::Claims {}, paused, EditorTime {.act = TimeAct::Speed, .speed = 2.0f}),
	          room_clock::Refusal::NoRight);
}

// A fast room clock can do more turns of the scenario than the server usually keeps before the away player comes back.
// The server keeps each turn of the scenario until that player is back, and then keeps only the usual turns again.
TEST_F(LockstepTest, APlayerWhoWasAwayAtAScenarioStartComesBackAfterMoreTurnsThanTheServerUsuallyKeeps)
{
	_keptTurns = 10;
	MakeHost(2);
	_host->SetChangeWorld([this](const server::LockstepHost::WorldChange& /*change*/) {
		_serverWorld = FakeWorld {};
		_serverWorld.mindFileHere = [this](uint64_t hash) { return !_host->EditorMindFile(hash).empty(); };
		return server::LockstepHost::WorldChanged {.turn = 0, .refusal = {}, .mapSha256 = {}};
	});
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 40);
	ASSERT_TRUE(_host->Started());
	_network.Cut(bob.sockets->Ids().back());
	Frames({&alice}, 1);
	bob.Frame();
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Reconnecting);
	ASSERT_TRUE(alice.session.SendEditorScenario("creature.walk"));
	// The scenario does four times the turns that the server usually keeps while bob is away
	Frames({&alice}, 40);
	ASSERT_EQ(alice.restarts, 1u);
	ASSERT_GT(_serverWorld.turns, 4 * _keptTurns - 5);
	EXPECT_TRUE(_host->KeepsTurn(0));
	Frames({&alice, &bob}, 30);
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.restarts, 1u);
	EXPECT_LE(bob.world.turns, _serverWorld.turns);
	EXPECT_GE(bob.world.turns + 2, _serverWorld.turns);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
	EXPECT_TRUE(bob.desyncs.empty());
	EXPECT_TRUE(alice.desyncs.empty());
	// Bob is back, so the server keeps only the usual turns again
	EXPECT_FALSE(_host->KeepsTurn(0));
	EXPECT_FALSE(_host->KeepsTurn(_serverWorld.turns - _keptTurns - 2));
}

// A player who never comes back loses the seat at the end of the time to come back. Then the server keeps only the usual
// turns again.
TEST_F(LockstepTest, TheServerKeepsTheTurnsOfAScenarioOnlyWhileTheAwayPlayerCanComeBack)
{
	_keptTurns = 10;
	MakeHost(2, 2'000'000);
	_host->SetChangeWorld([this](const server::LockstepHost::WorldChange& /*change*/) {
		_serverWorld = FakeWorld {};
		return server::LockstepHost::WorldChanged {.turn = 0, .refusal = {}, .mapSha256 = {}};
	});
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 40);
	ASSERT_TRUE(_host->Started());
	_network.Cut(bob.sockets->Ids().back());
	Frames({&alice}, 1);
	ASSERT_TRUE(alice.session.SendEditorScenario("creature.walk"));
	// The scenario does more turns than the server usually keeps
	Frames({&alice}, 15);
	ASSERT_EQ(alice.restarts, 1u);
	EXPECT_TRUE(_host->KeepsTurn(0));
	// The time to come back ends, and the seat of bob closes
	for (int i = 0; i < 200 && _host->Seats()[1].state == server::LockstepHost::SeatState::Away; ++i)
	{
		Frames({&alice}, 1);
	}
	ASSERT_NE(_host->Seats()[1].state, server::LockstepHost::SeatState::Away);
	Frames({&alice}, 2);
	EXPECT_FALSE(_host->KeepsTurn(0));
}

TEST(LockstepRules, TheFirstKeptTurnFollowsTheKeptTurnsAndTheHeldTurn)
{
	EXPECT_EQ(server::FirstKeptTurn(5, 10, std::nullopt), 0u);
	EXPECT_EQ(server::FirstKeptTurn(10, 10, std::nullopt), 0u);
	EXPECT_EQ(server::FirstKeptTurn(25, 10, std::nullopt), 15u);
	EXPECT_EQ(server::FirstKeptTurn(25, 10, 3u), 3u);
	EXPECT_EQ(server::FirstKeptTurn(25, 10, 20u), 15u);
}

TEST(RoomClock, OnlyAPlayerWithTheEditorClaimChangesTheClockAndTheValuesStayInTheirLimits)
{
	using game_socket::EditorTime;
	using game_socket::TimeAct;
	namespace room_clock = online::room_clock;
	const room_clock::State playing {};
	const room_clock::State paused {.paused = true, .speed = 1.0f, .steps = 0};
	const auto& rule = room_clock::k_Rule;
	EXPECT_EQ(room_clock::RefusalOf(rule, false, playing, EditorTime {.act = TimeAct::Pause}), room_clock::Refusal::NoRight);
	EXPECT_FALSE(room_clock::RefusalOf(rule, true, playing, EditorTime {.act = TimeAct::Pause}).has_value());
	EXPECT_FALSE(room_clock::RefusalOf(rule, true, paused, EditorTime {.act = TimeAct::Play}).has_value());
	// A step comes only while the clock is paused, and only a few steps wait
	EXPECT_EQ(room_clock::RefusalOf(rule, true, playing, EditorTime {.act = TimeAct::Step}),
	          room_clock::Refusal::StepWhilePlaying);
	EXPECT_FALSE(room_clock::RefusalOf(rule, true, paused, EditorTime {.act = TimeAct::Step}).has_value());
	auto full = paused;
	full.steps = room_clock::k_MostWaitingSteps;
	EXPECT_EQ(room_clock::RefusalOf(rule, true, full, EditorTime {.act = TimeAct::Step}), room_clock::Refusal::TooManySteps);
	// The speed is from 0.1 to 10, as the slider of the editor
	for (const float speed : {0.1f, 1.0f, 10.0f})
	{
		EXPECT_FALSE(room_clock::RefusalOf(rule, true, playing, EditorTime {.act = TimeAct::Speed, .speed = speed})) << speed;
	}
	for (const float speed :
	     {0.09f, 10.5f, 0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
	{
		EXPECT_EQ(room_clock::RefusalOf(rule, true, playing, EditorTime {.act = TimeAct::Speed, .speed = speed}),
		          room_clock::Refusal::SpeedOutOfRange)
		    << speed;
	}
	// The rule of nobody refuses each player
	EXPECT_EQ(room_clock::RefusalOf(room_clock::Rule {.right = room_clock::Right::Nobody, .tellsEveryPlayer = false}, true,
	                                playing, EditorTime {.act = TimeAct::Pause}),
	          room_clock::Refusal::NoRight);
}

TEST(RoomClock, APauseStopsTheTurnsAndEachStepLetsOneTurnGo)
{
	using game_socket::EditorTime;
	using game_socket::TimeAct;
	namespace room_clock = online::room_clock;
	auto state = room_clock::State {};
	EXPECT_TRUE(room_clock::SendsTurn(state));
	state = room_clock::Apply(state, EditorTime {.act = TimeAct::Pause});
	EXPECT_FALSE(room_clock::SendsTurn(state));
	state = room_clock::Apply(state, EditorTime {.act = TimeAct::Step});
	state = room_clock::Apply(state, EditorTime {.act = TimeAct::Step});
	EXPECT_EQ(state.steps, 2u);
	EXPECT_TRUE(room_clock::SendsTurn(state));
	state = room_clock::TurnSent(state);
	EXPECT_TRUE(room_clock::SendsTurn(state));
	state = room_clock::TurnSent(state);
	EXPECT_FALSE(room_clock::SendsTurn(state));
	EXPECT_EQ(room_clock::TurnSent(state), state);
	state = room_clock::Apply(state, EditorTime {.act = TimeAct::Speed, .speed = 2.0f});
	EXPECT_EQ(state.speed, 2.0f);
	EXPECT_TRUE(state.paused);
	state = room_clock::Apply(state, EditorTime {.act = TimeAct::Play});
	EXPECT_TRUE(room_clock::SendsTurn(state));
	// A turn while the clock plays uses no step
	EXPECT_EQ(room_clock::TurnSent(state), state);
}

TEST(RoomClock, TheClockOfAComputerFollowsTheSpeedOfTheRoomAndStartsFromTheTurnAfterAPause)
{
	using game_socket::GameTime;
	using game_socket::TimeAct;
	namespace room_clock = online::room_clock;
	// A turn of twice the time is a clock of half the speed
	EXPECT_EQ(room_clock::FollowOf(GameTime {.paused = false, .speed = 2.0f, .act = TimeAct::Speed}, 1.0f),
	          (room_clock::Follow {.clockSpeed = 0.5f, .fromTurn = false}));
	EXPECT_EQ(room_clock::FollowOf(GameTime {.paused = false, .speed = 0.1f, .act = TimeAct::Speed}, 1.0f).clockSpeed,
	          1.0f / 0.1f);
	// The same speed does not set the clock again
	EXPECT_EQ(room_clock::FollowOf(GameTime {.paused = true, .speed = 2.0f, .act = TimeAct::Pause}, 2.0f),
	          (room_clock::Follow {.clockSpeed = std::nullopt, .fromTurn = false}));
	// After a pause, the next turn of a play or of a step starts from its turn
	EXPECT_TRUE(room_clock::FollowOf(GameTime {.paused = false, .speed = 1.0f, .act = TimeAct::Play}, 1.0f).fromTurn);
	EXPECT_TRUE(room_clock::FollowOf(GameTime {.paused = true, .speed = 1.0f, .act = TimeAct::Step}, 1.0f).fromTurn);
}

TEST(RoomClock, TheSpeedOfTheGameGivesTheTimeBetweenTwoTurnsAsTheGameCalculatesIt)
{
	namespace room_clock = online::room_clock;
	// 100 divided by the speed, with the fraction cut off
	EXPECT_EQ(room_clock::PeriodOf(1.0f), 100u);
	EXPECT_EQ(room_clock::PeriodOf(2.0f), 50u);
	EXPECT_EQ(room_clock::PeriodOf(0.5f), 200u);
	EXPECT_EQ(room_clock::PeriodOf(3.0f), 33u);
	EXPECT_EQ(room_clock::PeriodOf(1.5f), 66u);
	EXPECT_EQ(room_clock::PeriodOf(0.3f), 333u);
	EXPECT_EQ(room_clock::PeriodOf(100.0f), 1u);
	EXPECT_EQ(room_clock::PeriodOf(0.001f), 99'999u);
	// A time out of the limits, or a speed that is not a positive number, asks for no change
	EXPECT_FALSE(room_clock::PeriodOf(101.0f).has_value());
	EXPECT_FALSE(room_clock::PeriodOf(0.0009f).has_value());
	EXPECT_FALSE(room_clock::PeriodOf(0.0f).has_value());
	EXPECT_FALSE(room_clock::PeriodOf(-1.0f).has_value());
	EXPECT_FALSE(room_clock::PeriodOf(std::nanf("")).has_value());
}

TEST(RoomClock, TheTimeBetweenTwoTurnsSetsTheClockOfTheRoomAndOfEachComputer)
{
	using game_socket::GameTime;
	using game_socket::TimeAct;
	namespace room_clock = online::room_clock;
	const auto state = room_clock::ApplyPeriod(room_clock::State {.paused = true, .speed = 1.0f, .steps = 2}, 33);
	EXPECT_EQ(state, (room_clock::State {.paused = true, .speed = 0.33f, .steps = 2, .period = 33}));
	// The clock speed is 100 divided by the time, not one divided by the time of a turn
	EXPECT_EQ(room_clock::ClockSpeedOf(state.speed, state.period), 100.0f / 33.0f);
	EXPECT_EQ(room_clock::ClockSpeedOfPeriod(50), 2.0f);
	EXPECT_EQ(room_clock::ClockSpeedOf(2.0f, std::nullopt), 0.5f);
	// A speed of the editor removes the time between two turns
	const auto edited = room_clock::Apply(state, game_socket::EditorTime {.act = TimeAct::Speed, .speed = 2.0f});
	EXPECT_FALSE(edited.period.has_value());
	// A computer follows a new time between two turns, also when the time of a turn is the same as a float
	const GameTime paced {.paused = false, .speed = 0.33f, .act = TimeAct::Speed, .period = 33};
	EXPECT_EQ(room_clock::FollowOf(paced, 0.33f).clockSpeed, 100.0f / 33.0f);
	EXPECT_FALSE(room_clock::FollowOf(paced, 0.33f, 33u).clockSpeed.has_value());
	// The time that this computer has from the room: the normal time before a change, none after a speed of the editor
	EXPECT_EQ(room_clock::PeriodOfRoom(std::nullopt), 100u);
	EXPECT_EQ(room_clock::PeriodOfRoom(GameTime {.speed = 1.0f}), 100u);
	EXPECT_EQ(room_clock::PeriodOfRoom(paced), 33u);
	EXPECT_FALSE(room_clock::PeriodOfRoom(GameTime {.speed = 2.0f}).has_value());
	EXPECT_FALSE(room_clock::SendsPeriod(100u, 100));
	EXPECT_TRUE(room_clock::SendsPeriod(100u, 50));
	EXPECT_TRUE(room_clock::SendsPeriod(std::nullopt, 100));
}

TEST(RoomClock, TheGameServerPlaysInAFrameTheTurnsThatTheClockOfTheRoomGives)
{
	namespace room_clock = online::room_clock;
	using std::chrono::milliseconds;
	// The default frame of the server is 100 ms: one turn at the normal speed, ten at the fastest speed
	EXPECT_EQ(room_clock::ServerTurnsInFrame(1.0f, milliseconds(100)), 1u);
	EXPECT_EQ(room_clock::ServerTurnsInFrame(0.1f, milliseconds(100)), 10u);
	EXPECT_EQ(room_clock::ServerTurnsInFrame(0.5f, milliseconds(100)), 2u);
	// A part of a turn goes up to a full turn
	EXPECT_EQ(room_clock::ServerTurnsInFrame(0.3f, milliseconds(100)), 4u);
	// A slow clock and a short frame give one turn
	EXPECT_EQ(room_clock::ServerTurnsInFrame(10.0f, milliseconds(100)), 1u);
	EXPECT_EQ(room_clock::ServerTurnsInFrame(0.1f, milliseconds(10)), 1u);
	EXPECT_EQ(room_clock::ServerTurnsInFrame(1.0f, milliseconds(10)), 1u);
	// The number stays at the limit, also for a long frame
	EXPECT_EQ(room_clock::ServerTurnsInFrame(0.1f, milliseconds(1000)), room_clock::k_MostServerTurnsInFrame);
	// No fixed frame time: the frame takes the time of a normal turn
	EXPECT_EQ(room_clock::ServerTurnsInFrame(0.1f, std::nullopt), 10u);
	EXPECT_EQ(room_clock::ServerTurnsInFrame(1.0f, std::nullopt), 1u);
	// The shortest time between two turns of the speed of the game gives the most turns
	EXPECT_EQ(room_clock::ServerTurnsInFrame(room_clock::SpeedOfPeriod(1), milliseconds(100)),
	          room_clock::k_MostServerTurnsInFrame);
	// A speed out of the limits gives one turn
	EXPECT_EQ(room_clock::ServerTurnsInFrame(0.0f, milliseconds(100)), 1u);
	EXPECT_EQ(room_clock::ServerTurnsInFrame(std::nanf(""), milliseconds(100)), 1u);
}

TEST(RoomClock, EveryPlayerSeesALineForAPauseAPlayAndASpeedOfAPlayer)
{
	using game_socket::GameTime;
	using game_socket::TimeAct;
	namespace room_clock = online::room_clock;
	EXPECT_EQ(room_clock::NoticeOf(GameTime {.paused = true, .act = TimeAct::Pause, .by = "alice"}), "alice paused the game");
	EXPECT_EQ(room_clock::NoticeOf(GameTime {.paused = false, .act = TimeAct::Play, .by = "alice"}),
	          "alice started the game again");
	EXPECT_EQ(room_clock::NoticeOf(GameTime {.paused = false, .speed = 2.5f, .act = TimeAct::Speed, .by = "alice"}),
	          "alice set the speed of the game to 2.50");
	EXPECT_FALSE(room_clock::NoticeOf(GameTime {.paused = true, .act = TimeAct::Step, .by = "alice"}).has_value());
	// A frame to a player who comes back, or a change of the server, names no player and shows no line
	EXPECT_FALSE(room_clock::NoticeOf(GameTime {.paused = true, .act = TimeAct::Play, .by = {}}).has_value());
}

TEST_F(LockstepTest, TheServerTakesTheActsOfTheDebugWindowsOnlyFromAGodWithTheDebuggerClaim)
{
	using game_socket::EditorTime;
	using game_socket::TimeAct;
	MakeHost(2);
	Computer alice(_network, _now, "god+debugger:alice");
	Computer bob(_network, _now, "god+editor:bob");
	Frames({&alice, &bob}, 20);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.GetJoined()->debugger);
	ASSERT_FALSE(bob.session.GetJoined()->debugger);
	const DebugAdvisor out {.act = DebugAdvisor::Act::Out, .advisor = 1};
	const DebugWorld noon {.act = DebugWorld::Act::TimeOfDay, .value = 12.0f};
	const DebugConsoleLine line {.text = R"x(CREATE_TOWN(1, "1,1", "Celtic"))x"};
	alice.session.Send(out);
	alice.session.Send(noon);
	alice.session.Send(line);
	// No computer takes a time of the day after 24 hours, thus the server refuses it also from a player with the claim
	alice.session.Send(DebugWorld {.act = DebugWorld::Act::TimeOfDay, .value = 30.0f});
	// The editor claim does not give the acts of the debug windows
	bob.session.Send(out);
	bob.session.Send(noon);
	bob.session.Send(line);
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(_host->RefusedDebuggerCommands(), 4u);
	EXPECT_EQ(_host->RefusedEditorCommands(), 0u);
	const auto count = [this](size_t seat, turn_codec::CommandTag tag) {
		const auto& commands = _host->Seats()[seat].commands;
		const auto found = commands.find(static_cast<uint8_t>(tag));
		return found != commands.end() ? found->second : 0u;
	};
	EXPECT_EQ(count(0, turn_codec::CommandTag::DebugAdvisor), 1u);
	EXPECT_EQ(count(0, turn_codec::CommandTag::DebugWorld), 1u);
	EXPECT_EQ(count(0, turn_codec::CommandTag::DebugConsoleLine), 1u);
	EXPECT_EQ(count(1, turn_codec::CommandTag::DebugAdvisor), 0u);
	EXPECT_EQ(count(1, turn_codec::CommandTag::DebugWorld), 0u);
	EXPECT_EQ(count(1, turn_codec::CommandTag::DebugConsoleLine), 0u);
	EXPECT_GE(bob.session.TakeEditorRefusals(), 1u);

	// The Game Speed menu of a player with the debugger claim changes the speed of the room, but not the pause
	alice.session.RequestTime(EditorTime {.act = TimeAct::Speed, .speed = 2.0f});
	alice.session.RequestTime(EditorTime {.act = TimeAct::Pause});
	Frames({&alice, &bob}, 4);
	EXPECT_FLOAT_EQ(_host->RoomClock().speed, 2.0f);
	EXPECT_FALSE(_host->RoomClock().paused);
	EXPECT_EQ(_host->RefusedTimeRequests(), 1u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, TheServerTakesTheActsOfTheMagicWeatherAndMoonWindowsOnlyFromAGodWithTheDebuggerClaim)
{
	MakeHost(2);
	Computer alice(_network, _now, "god+debugger:alice");
	Computer bob(_network, _now, "god+editor:bob");
	Frames({&alice, &bob}, 20);
	ASSERT_TRUE(_host->Started());
	const DebugMagic dispenser {.act = DebugMagic::Act::PlaceDispenser, .magic = 1, .position = {10.0f, 0.0f, 10.0f}};
	const DebugTownMiracle town {.town = 1, .magic = 2};
	const DebugWeather storm {.act = DebugWeather::Act::ForceStorm, .temperature = 12, .rain = 80, .overcast = 90};
	const DebugSky noon {.act = DebugSky::Act::ScriptHour, .hour = 12.0f};
	for (auto* computer : {&alice, &bob})
	{
		computer->session.Send(dispenser);
		computer->session.Send(town);
		computer->session.Send(storm);
		computer->session.Send(noon);
	}
	// No computer takes a miracle out of the table, a storm hotter than the window gives, or an hour after 24
	alice.session.Send(DebugMagic {.act = DebugMagic::Act::PlaceBubble, .magic = 200});
	alice.session.Send(DebugWeather {.act = DebugWeather::Act::Preset, .temperature = 90, .rain = 10});
	alice.session.Send(DebugSky {.act = DebugSky::Act::ScriptHour, .hour = 25.0f});
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(_host->RefusedDebuggerCommands(), 7u);
	EXPECT_EQ(_host->RefusedEditorCommands(), 0u);
	const auto count = [this](size_t seat, turn_codec::CommandTag tag) {
		const auto& commands = _host->Seats()[seat].commands;
		const auto found = commands.find(static_cast<uint8_t>(tag));
		return found != commands.end() ? found->second : 0u;
	};
	for (const auto tag : {turn_codec::CommandTag::DebugMagic, turn_codec::CommandTag::DebugTownMiracle,
	                       turn_codec::CommandTag::DebugWeather, turn_codec::CommandTag::DebugSky})
	{
		EXPECT_EQ(count(0, tag), 1u) << static_cast<int>(tag);
		EXPECT_EQ(count(1, tag), 0u) << static_cast<int>(tag);
	}
	EXPECT_GE(bob.session.TakeEditorRefusals(), 1u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, TheServerTakesTheActsOnTheThingsOfTheWorldOnlyFromAGodWithTheDebuggerClaim)
{
	MakeHost(2);
	Computer alice(_network, _now, "god+debugger:alice");
	Computer bob(_network, _now, "god+editor:bob");
	Frames({&alice, &bob}, 20);
	ASSERT_TRUE(_host->Started());
	const DebugPhysics throwAct {.type = 36, .height = 20.0f, .scale = 1.0f, .place = {100.0f, 100.0f}};
	const DebugParticle pause {.act = DebugParticle::Act::Pause, .paused = true};
	const DebugTempleVillager share {.act = DebugTempleVillager::Act::Share, .value = 0.5f};
	const DebugVillagerPath speed {.act = DebugVillagerPath::Act::Speed, .speed = 1.0f};
	alice.session.Send(throwAct);
	alice.session.Send(pause);
	alice.session.Send(share);
	alice.session.Send(speed);
	// No computer takes these acts, thus the server refuses them also from a player with the claim: a type that is not a
	// type of a static thing, a share above 1, an effect of one computer only and too many new villagers
	alice.session.Send(DebugPhysics {.type = 999, .scale = 1.0f});
	alice.session.Send(DebugTempleVillager {.act = DebugTempleVillager::Act::Share, .value = 1.5f});
	alice.session.Send(DebugParticle {.act = DebugParticle::Act::Close, .effect = 0x4000'0001u});
	alice.session.Send(DebugTempleVillager {.act = DebugTempleVillager::Act::Make, .age = 30, .count = 33});
	// The editor claim does not give the acts of the debug windows
	bob.session.Send(throwAct);
	bob.session.Send(pause);
	bob.session.Send(share);
	bob.session.Send(speed);
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(_host->RefusedDebuggerCommands(), 8u);
	EXPECT_EQ(_host->RefusedEditorCommands(), 0u);
	const auto count = [this](size_t seat, turn_codec::CommandTag tag) {
		const auto& commands = _host->Seats()[seat].commands;
		const auto found = commands.find(static_cast<uint8_t>(tag));
		return found != commands.end() ? found->second : 0u;
	};
	for (const auto tag : {turn_codec::CommandTag::DebugPhysics, turn_codec::CommandTag::DebugParticle,
	                       turn_codec::CommandTag::DebugTempleVillager, turn_codec::CommandTag::DebugVillagerPath})
	{
		EXPECT_EQ(count(0, tag), 1u) << static_cast<int>(tag);
		EXPECT_EQ(count(1, tag), 0u) << static_cast<int>(tag);
	}
	EXPECT_GE(bob.session.TakeEditorRefusals(), 1u);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, TheSpeedOfTheGameOfAPlayerWithNoClaimSetsTheTimeBetweenTwoTurnsOfEveryComputer)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 20);
	ASSERT_TRUE(_host->Started());
	// The faster key of a god with no claim: the speed 2 asks for 50 ms between two turns
	alice.session.RequestPace(online::room_clock::PeriodOf(2.0f).value());
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(_host->RoomClock().period, 50u);
	EXPECT_FLOAT_EQ(_host->RoomClock().speed, 0.5f);
	EXPECT_EQ(_host->RefusedTimeRequests(), 0u);
	for (const auto* computer : {&alice, &bob})
	{
		ASSERT_TRUE(computer->session.RoomClock().has_value());
		EXPECT_EQ(computer->session.RoomClock()->period, 50u);
		EXPECT_EQ(computer->session.RoomClock()->act, game_socket::TimeAct::Speed);
		EXPECT_TRUE(computer->session.RoomClock()->by.empty());
	}
	// No player sees a line for the change
	EXPECT_TRUE(alice.session.TakeTimeNotices().empty());
	EXPECT_TRUE(bob.session.TakeTimeNotices().empty());
	// The slower key of the other god sets the time for every player again
	bob.session.RequestPace(*online::room_clock::PeriodOf(0.5f));
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(_host->RoomClock().period, 200u);
	EXPECT_EQ(alice.session.RoomClock()->period, 200u);
	// The worlds stay the same as the world of the server
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, TheSpeedOfTheGameSendsNoRequestWhenTheTimeBetweenTwoTurnsDoesNotChange)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 20);
	ASSERT_TRUE(_host->Started());
	const auto changes = alice.session.RoomClockChanges();
	// The normal speed asks for the normal time, which the room has
	alice.session.RequestPace(online::room_clock::PeriodOf(1.0f).value());
	Frames({&alice, &bob}, 4);
	EXPECT_EQ(alice.session.RoomClockChanges(), changes);
	EXPECT_FALSE(_host->RoomClock().period.has_value());
}

TEST_F(LockstepTest, TheEditorPausesStepsAndChangesTheSpeedOfTheTurnsOfEveryComputer)
{
	using game_socket::EditorTime;
	using game_socket::TimeAct;
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 20);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.RoomClock().has_value());
	EXPECT_FALSE(alice.session.RoomClock()->paused);

	alice.session.RequestTime(EditorTime {.act = TimeAct::Pause});
	Frames({&alice, &bob}, 3);
	EXPECT_TRUE(_host->RoomClock().paused);
	const auto pausedAt = _serverWorld.turns;
	Frames({&alice, &bob}, 20);
	// The server sends no turn, and each computer waits at the same turn
	EXPECT_EQ(_serverWorld.turns, pausedAt);
	EXPECT_EQ(alice.world.turns, pausedAt);
	EXPECT_EQ(bob.world.turns, pausedAt);
	ASSERT_TRUE(bob.session.RoomClock().has_value());
	EXPECT_TRUE(bob.session.RoomClock()->paused);
	EXPECT_EQ(bob.session.RoomClock()->by, "alice");
	EXPECT_EQ(bob.session.TakeTimeNotices(), std::vector<std::string> {"alice paused the game"});
	EXPECT_EQ(alice.session.TakeTimeNotices(), std::vector<std::string> {"alice paused the game"});

	// Each step lets one turn go on every computer
	alice.session.RequestTime(EditorTime {.act = TimeAct::Step});
	Frames({&alice, &bob}, 5);
	EXPECT_EQ(_serverWorld.turns, pausedAt + 1);
	EXPECT_EQ(alice.world.turns, pausedAt + 1);
	EXPECT_EQ(bob.world.turns, pausedAt + 1);
	alice.session.RequestTime(EditorTime {.act = TimeAct::Step});
	alice.session.RequestTime(EditorTime {.act = TimeAct::Step});
	Frames({&alice, &bob}, 5);
	EXPECT_EQ(bob.world.turns, pausedAt + 3);
	EXPECT_TRUE(bob.session.TakeTimeNotices().empty());

	// The speed goes to every computer; a speed out of the range and the requests of a player with no claim do not
	alice.session.RequestTime(EditorTime {.act = TimeAct::Speed, .speed = 2.0f});
	alice.session.RequestTime(EditorTime {.act = TimeAct::Speed, .speed = 20.0f});
	bob.session.RequestTime(EditorTime {.act = TimeAct::Play});
	bob.session.RequestTime(EditorTime {.act = TimeAct::Step});
	Frames({&alice, &bob}, 5);
	EXPECT_EQ(_host->RefusedTimeRequests(), 3u);
	EXPECT_EQ(_host->RoomClock().speed, 2.0f);
	EXPECT_TRUE(_host->RoomClock().paused);
	EXPECT_EQ(bob.session.RoomClock()->speed, 2.0f);
	EXPECT_EQ(bob.world.turns, pausedAt + 3);
	EXPECT_GE(bob.session.TakeEditorRefusals(), 1u);
	EXPECT_EQ(bob.session.TakeTimeNotices(), std::vector<std::string> {"alice set the speed of the game to 2.00"});

	// A step while the game plays is refused
	alice.session.RequestTime(EditorTime {.act = TimeAct::Play});
	Frames({&alice, &bob}, 10);
	EXPECT_FALSE(bob.session.RoomClock()->paused);
	EXPECT_GT(bob.world.turns, pausedAt + 3);
	alice.session.RequestTime(EditorTime {.act = TimeAct::Step});
	Frames({&alice, &bob}, 2);
	EXPECT_EQ(_host->RefusedTimeRequests(), 4u);
	EXPECT_EQ(_host->RoomClock().steps, 0u);
	// The worlds stay in step
	Frames({&alice, &bob}, 5);
	EXPECT_EQ(alice.world.hash, bob.world.hash);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
	EXPECT_GT(bob.session.GetCompares().compared, 0u);
}

TEST_F(LockstepTest, APlayerWhoComesBackGetsThePauseOfTheEditor)
{
	using game_socket::EditorTime;
	using game_socket::TimeAct;
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 20);
	ASSERT_TRUE(_host->Started());
	alice.session.RequestTime(EditorTime {.act = TimeAct::Speed, .speed = 3.0f});
	Frames({&alice, &bob}, 2);
	// Bob's network stops before the pause, so he does not get it
	_network.Cut(bob.sockets->Ids().back());
	bob.Frame();
	alice.session.RequestTime(EditorTime {.act = TimeAct::Pause});
	Frames({&alice}, 5);
	const auto pausedAt = _serverWorld.turns;
	Frames({&alice, &bob}, 15);
	EXPECT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_EQ(bob.session.Resumes(), 1u);
	// Bob comes back to the paused game: he gets the turns before the pause, and the clock of the room
	ASSERT_TRUE(bob.session.RoomClock().has_value());
	EXPECT_TRUE(bob.session.RoomClock()->paused);
	EXPECT_EQ(bob.session.RoomClock()->speed, 3.0f);
	EXPECT_EQ(bob.world.turns, pausedAt);
	EXPECT_EQ(_serverWorld.turns, pausedAt);
	alice.session.RequestTime(EditorTime {.act = TimeAct::Play});
	Frames({&alice, &bob}, 10);
	EXPECT_GT(bob.world.turns, pausedAt);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, ThePauseOfTheEditorEndsWhenNoPlayerWithTheEditorClaimIsInTheGame)
{
	using game_socket::EditorTime;
	using game_socket::TimeAct;
	MakeHost(2);
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 20);
	ASSERT_TRUE(_host->Started());
	alice.session.RequestTime(EditorTime {.act = TimeAct::Pause});
	Frames({&alice, &bob}, 5);
	const auto pausedAt = _serverWorld.turns;
	// The editor's player loses the connection: the pause stays while the player can come back
	_network.Cut(alice.sockets->Ids().back());
	alice.Frame();
	Frames({&bob}, 5);
	EXPECT_EQ(_host->Seats()[0].state, LockstepHost::SeatState::Away);
	EXPECT_TRUE(_host->RoomClock().paused);
	EXPECT_EQ(bob.world.turns, pausedAt);
	// The editor's player leaves: the game of the other players plays again
	alice.Frame();
	Frames({&alice, &bob}, 10);
	alice.session.Leave();
	Frames({&alice, &bob}, 10);
	EXPECT_FALSE(_host->RoomClock().paused);
	EXPECT_FALSE(bob.session.RoomClock()->paused);
	EXPECT_GT(bob.world.turns, pausedAt);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, TheWorldStillWaitsWithNoPlayersAfterAPauseOfTheEditor)
{
	using game_socket::EditorTime;
	using game_socket::TimeAct;
	MakeHost(1, 2 * 1'000'000);
	Computer alice(_network, _now, "god+editor:alice");
	Frames({&alice}, 10);
	ASSERT_TRUE(_host->Started());
	alice.session.RequestTime(EditorTime {.act = TimeAct::Pause});
	Frames({&alice}, 3);
	alice.session.RequestTime(EditorTime {.act = TimeAct::Play});
	Frames({&alice}, 3);
	EXPECT_FALSE(_host->RoomClock().paused);
	alice.session.Leave();
	alice.Frame();
	Frames({}, 3);
	// The server's own pause with no god connected is as before
	EXPECT_TRUE(_host->Paused());
	const auto turns = _serverWorld.turns;
	Frames({}, 20);
	EXPECT_EQ(_serverWorld.turns, turns);
}

TEST(ServerLogNames, APlayerIsNamedByItsGodOrObserverNumber)
{
	using server::log_names::Capitalised;
	using server::log_names::PlayerOf;
	EXPECT_EQ(PlayerOf(2, 0, true), "the player of god 2");
	EXPECT_EQ(PlayerOf(std::nullopt, 3, true), "observer 3");
	EXPECT_EQ(PlayerOf(std::nullopt, 0, true), "an observer");
	EXPECT_EQ(PlayerOf(std::nullopt, 0, false), "a player");
	EXPECT_EQ(Capitalised("the player of god 0"), "The player of god 0");
	EXPECT_EQ(Capitalised(""), "");
}

// The snapshot keeps the files of the editor and the journal keeps the files that came after it. The restored server
// does the replayed turns with the files, and a player who was away gets the files before the turns that use them.
TEST_F(LockstepTest, ARestoredServerKeepsTheFilesOfTheEditorAndAPlayerWhoComesBackGetsThem)
{
	using Kind = game_socket::EditorScriptFile::Kind;
	using openblack::editor::mind_loads::HashOf;
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());
	// The editor sends a mind file and a program before the snapshot
	const std::vector<uint8_t> mind {'M', 1, 2};
	const auto program = ScriptFileOfParts(2, 'P');
	const std::string landText = "VERSION(2.0)\n";
	const std::vector<uint8_t> land(landText.begin(), landText.end());
	ASSERT_TRUE(alice.session.SendEditorMindFile(mind));
	ASSERT_TRUE(alice.session.SendEditorScriptFile(Kind::Program, program));
	Frames({&alice, &bob}, 4);
	const auto room = _host->RoomRecord();
	const auto snapshotTurn = _host->TurnsDone();
	const auto snapshotWorld = _serverWorld;
	auto files = _host->EditorFilesAfter({});
	const auto counts = _host->KeptEditorFiles();
	ASSERT_EQ(files.size(), 2u);
	EXPECT_EQ(files[0].kind, server::snapshots::EditorFileKind::Mind);
	EXPECT_EQ(files[1].kind, server::snapshots::EditorFileKind::ScriptProgram);
	EXPECT_EQ(files[1].bytes, program);

	// Bob goes away. The editor sends a land script after the snapshot, then uses the three files.
	_network.Cut(bob.sockets->Ids().back());
	Frames({&alice}, 1);
	bob.Frame();
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Reconnecting);
	ASSERT_TRUE(alice.session.SendEditorScriptFile(Kind::LandScript, land));
	alice.session.Send(EditorMindLoad {.use = EditorMindLoad::Use::LoadInto, .file = HashOf(mind), .creature = Entity(5)});
	alice.session.Send(EditorScriptProgram {.use = EditorScriptProgram::Use::Open, .file = HashOf(program)});
	alice.session.Send(EditorLandScript {.act = EditorLandScript::Act::Open, .file = HashOf(land)});
	Frames({&alice}, 10);
	ASSERT_EQ(alice.world.mindFilesAtTurn, (std::vector<bool> {true}));
	ASSERT_EQ(alice.world.scriptFilesAtTurn, (std::vector<bool> {true, true}));
	const auto lastTurn = _host->TurnsDone();
	const auto lastHash = _serverWorld.hash;
	// The journal keeps the file that came after the snapshot
	const auto journalFiles = _host->EditorFilesAfter(counts);
	ASSERT_EQ(journalFiles.size(), 1u);
	EXPECT_EQ(journalFiles[0].kind, server::snapshots::EditorFileKind::LandScript);
	files.insert(files.end(), journalFiles.begin(), journalFiles.end());

	RestartFrom(room, snapshotTurn, snapshotWorld, {&alice}, 0, {}, std::move(files));
	// The new server did the replayed turns with the files of the editor
	EXPECT_EQ(_host->TurnsDone(), lastTurn);
	EXPECT_EQ(_serverWorld.hash, lastHash);
	EXPECT_EQ(_serverWorld.mindFilesAtTurn, (std::vector<bool> {true}));
	EXPECT_EQ(_serverWorld.scriptFilesAtTurn, (std::vector<bool> {true, true}));
	EXPECT_EQ(_host->KeptEditorFiles(), (LockstepHost::EditorFileCounts {.minds = 1, .scripts = 2}));

	// Both players come back. Bob gets the files before the kept turns with the loads.
	Frames({&alice, &bob}, 40);
	for (const auto* computer : {&alice, &bob})
	{
		EXPECT_EQ(computer->session.GetState(), NetworkSession::State::Playing);
		EXPECT_EQ(computer->session.Resumes(), 1u);
		// The client can be one turn after the server, and each checksum that it compared is the same
		EXPECT_GE(computer->world.turns + 1, _serverWorld.turns);
		EXPECT_GT(computer->session.GetCompares().compared, 0u);
		EXPECT_EQ(computer->session.GetCompares().compared, computer->session.GetCompares().same);
	}
	EXPECT_EQ(bob.world.mindFilesAtTurn, (std::vector<bool> {true}));
	EXPECT_EQ(bob.world.scriptFilesAtTurn, (std::vector<bool> {true, true}));
	EXPECT_FALSE(bob.session.EditorScriptFile(static_cast<uint8_t>(Kind::LandScript), HashOf(land)).empty());

	// The editor uses a kept file again after the restart, with no new send of the file
	alice.session.Send(EditorMindLoad {.use = EditorMindLoad::Use::LoadInto, .file = HashOf(mind), .creature = Entity(5)});
	Frames({&alice, &bob}, 6);
	EXPECT_EQ(_host->RefusedEditorCommands(), 0u);
	EXPECT_EQ(_host->RefusedEditorValues(), 0u);
	EXPECT_EQ(_serverWorld.mindFilesAtTurn, (std::vector<bool> {true, true}));
	EXPECT_EQ(bob.world.mindFilesAtTurn, (std::vector<bool> {true, true}));
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

// The room of the snapshot keeps the clock of the room. A restart does not end an editor pause or change the speed.
TEST_F(LockstepTest, ARestoredServerKeepsThePauseAndTheSpeedOfTheEditor)
{
	using game_socket::EditorTime;
	using game_socket::TimeAct;
	MakeHost(2);
	GiveWorldState();
	KeepJournal();
	Computer alice(_network, _now, "god+editor:alice");
	Computer bob(_network, _now, "god:bob");
	Frames({&alice, &bob}, 20);
	ASSERT_TRUE(_host->Started());
	alice.session.RequestTime(EditorTime {.act = TimeAct::Speed, .speed = 2.0f});
	alice.session.RequestTime(EditorTime {.act = TimeAct::Pause});
	Frames({&alice, &bob}, 5);
	const auto pausedAt = _serverWorld.turns;
	const auto room = _host->RoomRecord();
	EXPECT_TRUE(room.clock.paused);
	EXPECT_EQ(room.clock.speed, 2.0f);

	RestartFrom(room, _host->TurnsDone(), _serverWorld, {&alice, &bob});
	EXPECT_TRUE(_host->RoomClock().paused);
	EXPECT_EQ(_host->RoomClock().speed, 2.0f);
	Frames({&alice, &bob}, 30);
	for (const auto* computer : {&alice, &bob})
	{
		EXPECT_EQ(computer->session.GetState(), NetworkSession::State::Playing);
		EXPECT_EQ(computer->session.Resumes(), 1u);
		ASSERT_TRUE(computer->session.RoomClock().has_value());
		EXPECT_TRUE(computer->session.RoomClock()->paused);
		EXPECT_EQ(computer->session.RoomClock()->speed, 2.0f);
		EXPECT_EQ(computer->world.turns, pausedAt);
	}
	// The pause continues after the restart: no turn comes until a step or a play
	EXPECT_EQ(_serverWorld.turns, pausedAt);
	alice.session.RequestTime(EditorTime {.act = TimeAct::Step});
	Frames({&alice, &bob}, 5);
	EXPECT_EQ(_serverWorld.turns, pausedAt + 1);
	EXPECT_EQ(bob.world.turns, pausedAt + 1);
	alice.session.RequestTime(EditorTime {.act = TimeAct::Play});
	Frames({&alice, &bob}, 10);
	EXPECT_GT(bob.world.turns, pausedAt + 1);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, AChangeOfTheDebuggerRightDuringTheGameTakesEffectAtOnce)
{
	using game_socket::EditorTime;
	using game_socket::TimeAct;
	MakeHost(2);
	Computer watcher(_network, _now, "observer:13");
	Frames({&watcher}, 3);
	Computer alice(_network, _now, "god+debugger:11");
	Computer bob(_network, _now, "god:12");
	Frames({&alice, &bob, &watcher}, 8);
	ASSERT_TRUE(_host->Started());
	ASSERT_TRUE(alice.session.GetJoined()->debugger);
	ASSERT_FALSE(bob.session.GetJoined()->debugger);
	// Staff remove the mark of Alice and give it to Bob and to the observer. The next status answer of the lobby tells
	// the server.
	const std::vector<int64_t> debuggers {12, 13};
	_host->SetDebuggers(debuggers);
	EXPECT_FALSE(_host->Seats()[0].debugger);
	EXPECT_TRUE(_host->Seats()[1].debugger);
	EXPECT_TRUE(_host->Seats()[1].debuggerChanged);
	Frames({&alice, &bob, &watcher}, 2);
	EXPECT_FALSE(alice.session.GetJoined()->debugger);
	EXPECT_TRUE(bob.session.GetJoined()->debugger);
	EXPECT_TRUE(watcher.session.GetJoined()->debugger);
	// The editor right does not change
	EXPECT_FALSE(bob.session.GetJoined()->editor);
	// The server takes the acts of the debug windows by the new rights
	const DebugAdvisor out {.act = DebugAdvisor::Act::Out, .advisor = 1};
	alice.session.Send(out);
	bob.session.Send(out);
	Frames({&alice, &bob, &watcher}, 4);
	EXPECT_EQ(_host->RefusedDebuggerCommands(), 1u);
	EXPECT_EQ(_host->RefusedEditorCommands(), 0u);
	const auto count = [this](size_t seat) {
		const auto& commands = _host->Seats()[seat].commands;
		const auto found = commands.find(static_cast<uint8_t>(turn_codec::CommandTag::DebugAdvisor));
		return found != commands.end() ? found->second : 0u;
	};
	EXPECT_EQ(count(0), 0u);
	EXPECT_EQ(count(1), 1u);
	EXPECT_GE(alice.session.TakeEditorRefusals(), 1u);
	// The speed of the room follows the new right too
	alice.session.RequestTime(EditorTime {.act = TimeAct::Speed, .speed = 2.0f});
	Frames({&alice, &bob, &watcher}, 4);
	EXPECT_EQ(_host->RefusedTimeRequests(), 1u);
	bob.session.RequestTime(EditorTime {.act = TimeAct::Speed, .speed = 2.0f});
	Frames({&alice, &bob, &watcher}, 4);
	EXPECT_FLOAT_EQ(_host->RoomClock().speed, 2.0f);
	// The same rights again change nothing
	_host->SetDebuggers(debuggers);
	EXPECT_TRUE(_host->Seats()[1].debugger);
	EXPECT_EQ(alice.session.GetCompares().compared, alice.session.GetCompares().same);
	EXPECT_EQ(bob.session.GetCompares().compared, bob.session.GetCompares().same);
}

TEST_F(LockstepTest, APlayerWithNoAccountKeepsTheDebuggerClaimOfTheJoin)
{
	MakeHost(2);
	Computer alice(_network, _now, "god+debugger:alice");
	Computer bob(_network, _now, "god:12");
	Frames({&alice, &bob}, 8);
	ASSERT_TRUE(_host->Started());
	_host->SetDebuggers({});
	EXPECT_TRUE(_host->Seats()[0].debugger);
	EXPECT_FALSE(_host->Seats()[1].debugger);
	Frames({&alice, &bob}, 2);
	EXPECT_TRUE(alice.session.GetJoined()->debugger);
}

TEST_F(LockstepTest, APlayerWhoIsAwayGetsTheChangedDebuggerRightWhenThePlayerComesBack)
{
	MakeHost(2);
	Computer alice(_network, _now, "god:11");
	Computer bob(_network, _now, "god:12");
	Frames({&alice, &bob}, 30);
	ASSERT_TRUE(_host->Started());
	_network.Cut(bob.sockets->Ids().back());
	bob.Frame();
	Frames({&alice}, 10);
	ASSERT_EQ(_host->Seats()[1].state, LockstepHost::SeatState::Away);
	const std::vector<int64_t> debuggers {12};
	_host->SetDebuggers(debuggers);
	EXPECT_TRUE(_host->Seats()[1].debugger);
	EXPECT_FALSE(bob.session.GetJoined()->debugger);
	Frames({&alice, &bob}, 10);
	ASSERT_EQ(bob.session.GetState(), NetworkSession::State::Playing);
	EXPECT_TRUE(bob.session.GetJoined()->debugger);
	EXPECT_FALSE(alice.session.GetJoined()->debugger);
	// The editor right stays as the join gave it
	EXPECT_FALSE(bob.session.GetJoined()->editor);
}
