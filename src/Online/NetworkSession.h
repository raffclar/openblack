/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <GameSocket.h>

#include "Commands/TurnCodec.h"
#include "Commands/TurnCommandSource.h"
#include "Online/Backoff.h"
#include "Online/ResyncRule.h"
#include "Online/TurnPlayout.h"
#include "Online/TurnStream.h"
#include "Online/WebSocketInterface.h"

namespace openblack::online
{

struct NetworkSessionSettings
{
	/// The game socket of the server: a wss:// address on the desktop, a path in the browser
	WebSocketRequest request;
	/// The lobby's join ticket
	std::string ticket;
	std::string gamePassword;
	/// The name of the player's profile, on the local network only
	std::string name;
	/// How long the client tries to come back after it lost the socket. The server keeps the god for 5 minutes.
	int64_t comeBackMicroseconds {5LL * 60 * 1'000'000};
	/// Reads the file of a land in the Scripts folder of the game, for the check of the land before the start. None when
	/// the land is not on this computer. With no reader, the client has no land but the testbed.
	std::function<std::optional<std::vector<uint8_t>>(std::string_view land)> readLand;
	/// Reads the creature file of the player, which goes to the other players before the start. Empty when the player
	/// has no creature, and with no reader.
	std::function<std::vector<uint8_t>()> readCreature;
	/// Tools: gets each turn at which the world of this computer differs from the server's world
	std::function<void(uint32_t turn)> parts;
};

/// A step of the start of the game that the front end shows to the player
enum class StartEvent : uint8_t
{
	/// The server asked for the land, and this computer has it
	HasLand,
	/// The server asked for the land, and this computer does not have it. The server takes the player's Ready.
	LacksLand,
	/// Each player has the land: the creature files go to all players
	Transfer,
	/// A player does not have the land, so the game does not start
	Cancelled,
	/// A player left the room during the start, so the game does not start
	Stopped,
};

/// The client of a network game: it joins the room of a game server with a ticket, sends the commands of its player at
/// once, and gives the world the turns that the server ordered (authoritative lockstep). It compares the checksum of
/// each turn with the server's checksum, and starts the resync when they differ. After a loss of the socket it comes
/// back with the secret of its session and gets the turns that it did not get.
class NetworkSession final: public player_commands::TurnCommandSourceInterface
{
public:
	/// Microseconds of a steady clock of this computer
	using Clock = std::function<int64_t()>;

	enum class State : uint8_t
	{
		Idle,
		/// The socket opens
		Connecting,
		/// The client sent its join and waits for the answer
		Joining,
		/// The server accepted the client. The client waits for the start of the game.
		Waiting,
		Playing,
		/// The socket closed during the game. The client tries to come back.
		Reconnecting,
		Ended,
	};

	enum class End : uint8_t
	{
		None,
		/// The server refused the join or the resume
		Refused,
		/// The player left the game
		Left,
		/// The resyncs did not put the world back in step
		OutOfSync,
		/// The client did not come back in time
		Lost,
		/// The socket closed before the server accepted the client
		NoConnection,
	};

	/// The counts of the checksums that the client compared with the server's
	struct Compares
	{
		uint32_t compared {0};
		uint32_t same {0};
		std::optional<uint32_t> firstDifferent;
	};

	NetworkSession(WebSocketClientInterface& sockets, Clock clock);
	~NetworkSession() override;
	NetworkSession(const NetworkSession&) = delete;
	NetworkSession& operator=(const NetworkSession&) = delete;
	NetworkSession(NetworkSession&&) = delete;
	NetworkSession& operator=(NetworkSession&&) = delete;

	void Join(NetworkSessionSettings settings);
	/// The player is ready for the start, or cancels the ready before the start
	void SetReady(bool ready);
	/// The player leaves the game. The server defeats the god.
	void Leave();
	/// The end box of a decided game opened: the client asks the server to stop the turns of the game, one time
	void StopGame();
	/// The last turn of the game, after the server stopped the turns. None before.
	[[nodiscard]] std::optional<uint32_t> StoppedAfter() const { return _stoppedAfter; }
	/// Once a frame, after the poll of the sockets: the attempts to come back and the pings
	void Update();

	/// The checksum of this computer's world after a turn
	void LocalChecksum(uint32_t turn, uint64_t checksum);

	void Send(const player_commands::Action& action) override;
	[[nodiscard]] bool HasTurn(uint32_t turn) const override;
	[[nodiscard]] std::vector<player_commands::Command> TakeTurn(uint32_t turn) override;
	[[nodiscard]] std::span<const uint8_t> StartCreatureFile(PlayerNames god) const override;
	/// The god of the slot that the server gave to this computer. None for an observer.
	[[nodiscard]] std::optional<PlayerNames> OwnGod() const override;

	[[nodiscard]] State GetState() const { return _state; }
	[[nodiscard]] End GetEnd() const { return _end; }
	[[nodiscard]] const std::optional<game_socket::Joined>& GetJoined() const { return _joined; }
	[[nodiscard]] const std::optional<game_socket::Start>& GetStart() const { return _start; }
	/// The next step of the start since the last call, one time each
	[[nodiscard]] std::optional<StartEvent> TakeStartEvent();
	/// The creature files of the gods that the server sent before the start, by god slot. An empty file is a god with no
	/// creature.
	[[nodiscard]] const std::map<uint8_t, std::vector<uint8_t>>& StartCreatures() const { return _startCreatures; }
	/// The reason that the server gave for a refusal
	[[nodiscard]] const std::string& RefusedReason() const { return _refusedReason; }
	[[nodiscard]] const ResyncRule& Resync() const { return _resync; }
	/// True while the game shows "Resynchronising..." (MI-4)
	[[nodiscard]] bool ShowsResync() const
	{
		return _resync.GetState() == ResyncRule::State::Resyncing || _worldTurn.has_value();
	}
	[[nodiscard]] const Compares& GetCompares() const { return _compares; }
	[[nodiscard]] const TurnPlayout& Playout() const { return _playout; }
	[[nodiscard]] std::optional<uint64_t> RoundTrip() const;
	/// The number of turns that the world did
	[[nodiscard]] uint32_t TurnsDone() const { return _turnsDone; }
	/// The last turn that the server sent
	[[nodiscard]] std::optional<uint32_t> LastTurnReceived() const;
	/// The number of times that the client came back after a loss or a resync
	[[nodiscard]] uint32_t Resumes() const { return _resumes; }

	/// The server's world state at the start of a turn, once all its parts came
	struct WorldState
	{
		uint32_t turn {0};
		std::vector<uint8_t> bytes;
	};
	/// The world state that came after a resync, once. The world loads it, then tells the session with WorldLoaded.
	[[nodiscard]] std::optional<WorldState> TakeWorld();
	/// The world of this computer loaded the server's world state of the turn, and goes on from there
	void WorldLoaded(uint32_t turn);
	/// True from a resume with the server's world until the world loaded it
	[[nodiscard]] bool WaitsForWorld() const { return _worldTurn.has_value(); }

	/// The time between pings
	static constexpr int64_t k_PingMicroseconds = 1'000'000;

private:
	void Open(bool resume);
	void Text(std::string_view text);
	/// The frames of the steps of the start
	void StartFrame(const game_socket::Frame& frame);
	void Closed(const WebSocketClosed& closed);
	void Turn(const turn_codec::TurnBundle& turn);
	void ServerChecksum(const turn_codec::TurnChecksum& checksum);
	void WorldPart(const turn_codec::WorldPart& part);
	void Compare(uint32_t turn);
	void FinishWith(End end);
	void DropStream();
	void SendWaiting();
	/// Closes the socket and starts the attempts to come back
	void ComeBack(bool resync);

	WebSocketClientInterface& _sockets;
	Clock _clock;
	std::unique_ptr<TurnStream> _stream;
	NetworkSessionSettings _settings;
	State _state {State::Idle};
	End _end {End::None};
	bool _ready {false};
	std::optional<game_socket::Joined> _joined;
	std::optional<game_socket::Start> _start;
	std::vector<StartEvent> _startEvents;
	std::map<uint8_t, std::vector<uint8_t>> _startCreatures;
	std::string _refusedReason;

	std::map<uint32_t, turn_codec::TurnBundle> _turns;
	TurnPlayout _playout;
	uint32_t _turnsDone {0};
	/// The commands of the player that did not go yet, because the socket was not open
	std::vector<player_commands::Action> _waiting;

	std::map<uint32_t, uint64_t> _localChecksums;
	std::map<uint32_t, uint64_t> _serverChecksums;
	Compares _compares;
	ResyncRule _resync;

	ReconnectBackoff _backoff {std::chrono::milliseconds(250), std::chrono::milliseconds(5000)};
	int64_t _lostAt {0};
	int64_t _nextAttempt {0};
	int64_t _nextPing {0};
	uint32_t _resumes {0};
	/// The generation of the socket, so that the handlers of an old socket do nothing
	uint32_t _generation {0};
	/// The socket closes at the next update
	bool _dropStream {false};
	/// The next resume asks for the server's world, because this computer's world parted
	bool _wantsWorld {false};
	/// The turn of the world state that comes, and its parts so far
	std::optional<uint32_t> _worldTurn;
	std::vector<std::vector<uint8_t>> _worldParts;
	std::optional<WorldState> _world;
	/// The client asked the server to stop the turns of the game
	bool _stopAsked {false};
	std::optional<uint32_t> _stoppedAfter;
};

[[nodiscard]] std::string_view StateName(NetworkSession::State state) noexcept;
[[nodiscard]] std::string_view EndName(NetworkSession::End end) noexcept;

} // namespace openblack::online
