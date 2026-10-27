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
#include <span>
#include <string_view>

/// The WebSocket end of the game server. The clients open the game socket to it. It moves on only in Poll, with no
/// thread and no wait, and its handlers run only from Poll.
namespace openblack::server
{

using ConnectionId = uint32_t;

struct ServerSocketHandlers
{
	/// A client finished the WebSocket handshake
	std::function<void(ConnectionId connection)> opened;
	/// A whole text message from a client
	std::function<void(ConnectionId connection, std::string_view text)> text;
	/// A whole binary message from a client
	std::function<void(ConnectionId connection, std::span<const uint8_t> bytes)> binary;
	/// The connection ended from the side of the client or of the network. The close code of the client's close frame,
	/// or 1006 when there was none. It does not run for a connection that the server closed.
	std::function<void(ConnectionId connection, uint16_t code)> closed;
};

class WebSocketServerInterface
{
public:
	virtual ~WebSocketServerInterface() = default;

	virtual void SetHandlers(ServerSocketHandlers handlers) = 0;
	/// Sends a text message. Returns false when the connection is not open.
	virtual bool SendText(ConnectionId connection, std::string_view text) = 0;
	/// Sends a binary message. Returns false when the connection is not open.
	virtual bool SendBinary(ConnectionId connection, std::span<const uint8_t> bytes) = 0;
	/// Sends a close frame with the code and ends the connection. No handler of the connection runs after this call.
	virtual void Close(ConnectionId connection, uint16_t code) = 0;
	/// Once a frame: accepts the new connections, reads and writes without a wait, and runs the handlers
	virtual void Poll() = 0;
	/// The number of open connections
	[[nodiscard]] virtual size_t Count() const = 0;
};

/// The close codes of RFC 6455 and of the game server
namespace server_close
{
constexpr uint16_t k_Normal = 1000;
constexpr uint16_t k_GoingAway = 1001;
constexpr uint16_t k_Protocol = 1002;
/// No close frame came: the network or the client stopped
constexpr uint16_t k_NoCloseFrame = 1006;
constexpr uint16_t k_TooLarge = 1009;
/// The ticket or the join was refused
constexpr uint16_t k_Refused = 4003;
/// The world of the client parted from the server's world and the resyncs did not work
constexpr uint16_t k_OutOfSync = 4010;
} // namespace server_close

} // namespace openblack::server
