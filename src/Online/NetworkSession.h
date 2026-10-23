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
#include <string>
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
	/// How long the client tries to come back after it lost the socket. The server keeps the god for 5 minutes.
	int64_t comeBackMicroseconds {5LL * 60 * 1'000'000};
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
	/// The client loaded the land and is ready for the start
	void SetReady();
	/// The player leaves the game. The server defeats the god.
	void Leave();
	/// Once a frame, after the poll of the sockets: the attempts to come back and the pings
	void Update();

	/// The checksum of this computer's world after a turn
	void LocalChecksum(uint32_t turn, uint64_t checksum);

	void Send(const player_commands::Action& action) override;
	[[nodiscard]] bool HasTurn(uint32_t turn) const override;
	[[nodiscard]] std::vector<player_commands::Command> TakeTurn(uint32_t turn) override;

	[[nodiscard]] State GetState() const { return _state; }
	[[nodiscard]] End GetEnd() const { return _end; }
	[[nodiscard]] const std::optional<game_socket::Joined>& GetJoined() const { return _joined; }
	[[nodiscard]] const std::optional<game_socket::Start>& GetStart() const { return _start; }
	/// The reason that the server gave for a refusal
	[[nodiscard]] const std::string& RefusedReason() const { return _refusedReason; }
	[[nodiscard]] const ResyncRule& Resync() const { return _resync; }
	/// True while the game shows "Resynchronising..." (MI-4)
	[[nodiscard]] bool ShowsResync() const { return _resync.GetState() == ResyncRule::State::Resyncing; }
	[[nodiscard]] const Compares& GetCompares() const { return _compares; }
	[[nodiscard]] const TurnPlayout& Playout() const { return _playout; }
	[[nodiscard]] std::optional<uint64_t> RoundTrip() const;
	/// The number of turns that the world did
	[[nodiscard]] uint32_t TurnsDone() const { return _turnsDone; }
	/// The last turn that the server sent
	[[nodiscard]] std::optional<uint32_t> LastTurnReceived() const;
	/// The number of times that the client came back after a loss or a resync
	[[nodiscard]] uint32_t Resumes() const { return _resumes; }

	/// The time between pings
	static constexpr int64_t k_PingMicroseconds = 1'000'000;

private:
	void Open(bool resume);
	void Text(std::string_view text);
	void Closed(const WebSocketClosed& closed);
	void Turn(const turn_codec::TurnBundle& turn);
	void ServerChecksum(const turn_codec::TurnChecksum& checksum);
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
};

[[nodiscard]] std::string_view StateName(NetworkSession::State state) noexcept;
[[nodiscard]] std::string_view EndName(NetworkSession::End end) noexcept;

} // namespace openblack::online
