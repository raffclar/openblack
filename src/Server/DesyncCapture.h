/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

#include <GameSocket.h>

#include "ECS/EntityHashes.h"
#include "Online/DesyncFacts.h"
#include "Server/WebSocketServerInterface.h"

/// The server's capture of a desync: when the world of a god's client parts from the server's world, the server asks the
/// client to hash its world entity by entity at a turn a short time later, and does the same with its own world. The
/// server then names the facts and the first entities that differ, and keeps both worlds of that turn. A capture has no
/// effect on the turns of the game.
namespace openblack::server
{

/// The turns from the server's last turn to the turn of a capture. The client is behind the server, so it gets the
/// capture before it does that turn.
inline constexpr uint32_t k_CaptureLeadTurns = 20;
/// The most captures of one game, for all gods. A god has one capture in a game.
inline constexpr size_t k_MostCapturesInGame = 4;
/// The longest time in microseconds that a capture waits for the client after the server asked for it
inline constexpr int64_t k_CaptureWaitMicroseconds = 60LL * 1'000'000;

/// What a capture found
struct CaptureRecord
{
	struct Part
	{
		std::string part;
		uint32_t serverEntities {0};
		/// None when the client did not send the blocks of the part
		std::optional<uint32_t> playerEntities;
		/// The first entities that differ, from the blocks that differ
		std::vector<synced_state::EntityDifference> differences;
		/// True when the server compared the entities of the blocks that differ, or found no block that differs
		bool compared {false};
	};

	uint8_t seat {0};
	/// The turn at which the worlds parted, and the turn of the capture
	uint32_t partTurn {0};
	uint32_t captureTurn {0};
	game_socket::SyncFacts server;
	std::optional<game_socket::SyncFacts> player;
	/// The first fact that differs, when the client sent its facts
	std::optional<online::desync_facts::FactDifference> fact;
	std::vector<Part> parts;
	/// The saved worlds of the turn of the capture. The player's world is empty when not all of it came.
	std::vector<uint8_t> serverWorld;
	std::vector<uint8_t> playerWorld;
	/// What did not come before the capture ended, such as "blocks" or "world"
	std::vector<std::string> missing;
};

class DesyncCapture
{
public:
	struct Hooks
	{
		/// Sends a frame to a connection
		std::function<void(ConnectionId connection, const game_socket::Frame& frame)> send;
		/// The hash of each entity of the parts in the server's world now
		std::function<std::vector<synced_state::PartEntities>(std::span<const std::string> parts)> entities;
		/// The server's saved world now
		std::function<std::vector<uint8_t>()> world;
		/// Gets each capture when it ends
		std::function<void(CaptureRecord record)> finished;
	};

	/// The facts of the server's world. The turn of the facts is the turn of each desync.
	DesyncCapture(game_socket::SyncFacts facts, Hooks hooks);

	/// The facts that the client of a god sent after its world parted
	void Facts(uint8_t seat, const game_socket::SyncFacts& facts);
	/// The client of a god sent its parts of the turn at which its world parted. The first time for the god, and while
	/// the game has captures left, the server asks for a capture.
	void Parted(uint8_t seat, ConnectionId connection, uint32_t partTurn, uint32_t turnsDone,
	            std::span<const std::string> parts, int64_t now);
	/// The server's world did this turn: the captures of the turn hash the server's world and keep it
	void TurnDone(uint32_t turn);
	void Blocks(uint8_t seat, const game_socket::SyncBlocks& blocks);
	void Entities(uint8_t seat, const game_socket::SyncEntities& entities);
	void World(uint8_t seat, const game_socket::SyncWorld& world);
	/// Once a frame: a capture that waits too long ends with what it has
	void Update(int64_t now);
	/// The connection of the god closed: its capture ends with what it has
	void Gone(uint8_t seat);
	/// The game ends: each capture ends with what it has
	void FinishAll();

	[[nodiscard]] bool Capturing(uint8_t seat) const { return _captures.contains(seat); }
	[[nodiscard]] size_t Captures() const { return _started; }

private:
	struct Capture
	{
		ConnectionId connection {0};
		CaptureRecord record;
		int64_t askedAt {0};
		bool serverDone {false};
		std::vector<synced_state::PartEntities> serverEntities;
		std::optional<game_socket::SyncBlocks> blocks;
		std::optional<game_socket::SyncAsk> asked;
		bool entitiesDone {false};
		/// The parts of the client's world that came, in order
		uint32_t worldParts {0};
		uint32_t worldCount {0};
		bool worldDone {false};
		bool worldFailed {false};
		/// The connection of the player closed
		bool gone {false};
	};

	void CompareBlocks(Capture& capture);
	void FinishIfDone(uint8_t seat);
	void Finish(uint8_t seat);

	game_socket::SyncFacts _facts;
	Hooks _hooks;
	std::map<uint8_t, Capture> _captures;
	std::map<uint8_t, game_socket::SyncFacts> _playerFacts;
	/// The gods that had a capture in this game
	std::set<uint8_t> _captured;
	size_t _started {0};
};

/// The entities of a frame as the hashes of the entities
[[nodiscard]] std::vector<synced_state::EntityHash> EntitiesOfFrame(std::span<const game_socket::SyncEntity> entities);

} // namespace openblack::server
