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
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Commands/TurnCodec.h"
#include "Online/WebSocketInterface.h"

namespace openblack::online
{

struct TurnStreamHandlers
{
	std::function<void()> opened;
	/// A turn from the server, with the commands of all the players
	std::function<void(const turn_codec::TurnBundle& turn)> turn;
	/// The checksum of the server's world after a turn
	std::function<void(const turn_codec::TurnChecksum& checksum)> checksum;
	/// A JSON text frame of the game socket: the room before the start, joins, leaves and the end of the game
	std::function<void(std::string_view text)> text;
	std::function<void(const WebSocketClosed& closed)> closed;
};

/// The client end of the game socket to a game server. It sends the commands of the player at once, and the checksums
/// and the pings, as binary frames. It gives the turns of the server to the game, and answers the pings of the server.
/// The handlers run from the Poll of the WSS client.
class TurnStream
{
public:
	/// The clock gives microseconds. Only this client reads its values, so any start works.
	using MicrosecondClock = std::function<uint64_t()>;

	TurnStream(WebSocketClientInterface& sockets, MicrosecondClock clock);
	~TurnStream();
	TurnStream(const TurnStream&) = delete;
	TurnStream& operator=(const TurnStream&) = delete;
	TurnStream(TurnStream&&) = delete;
	TurnStream& operator=(TurnStream&&) = delete;

	void Open(WebSocketRequest request, TurnStreamHandlers handlers);
	/// Closes the socket. No handler runs after this call.
	void Close();
	[[nodiscard]] bool IsOpen() const;

	/// Sends a block of commands that the player made after a turn. Returns false when the socket is not open or the block
	/// breaks a limit of the format.
	bool SendCommands(uint32_t turn, std::span<const player_commands::Action> actions);
	/// Sends the checksum of the synced world after a turn
	bool SendChecksum(uint32_t turn, uint64_t checksum);
	/// Sends a ping. Its answer gives the time of the round trip.
	bool SendPing();
	/// Sends a JSON text frame
	bool SendText(std::string_view text);

	/// The time of the round trip of the last ping that the server answered, in microseconds
	[[nodiscard]] std::optional<uint64_t> RoundTrip() const { return _roundTrip; }
	/// The number of binary frames from the server that the client did not accept
	[[nodiscard]] uint32_t RejectedFrames() const { return _rejected; }

private:
	bool SendFrame(const turn_codec::Frame& frame);
	void Received(std::span<const uint8_t> bytes);

	WebSocketClientInterface& _sockets;
	MicrosecondClock _clock;
	std::optional<WebSocketId> _connection;
	TurnStreamHandlers _handlers;
	/// The buffer that each frame uses, so a send makes no new memory after the first frames
	std::vector<uint8_t> _buffer;
	uint16_t _nextSequence {0};
	uint32_t _nextPing {0};
	std::optional<uint64_t> _roundTrip;
	uint32_t _rejected {0};
};

} // namespace openblack::online
