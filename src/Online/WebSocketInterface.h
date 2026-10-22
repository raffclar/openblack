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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Online/HttpClientInterface.h"

/// The secure WebSocket (WSS) client of the game. The push channel of the lobby and the socket of a game server use it.
/// The desktop client accepts only wss:// addresses and uses the TLS rules of the HTTPS client. The browser client
/// accepts only paths on the origin of the page, which is an HTTPS origin, and opens them over WSS.
namespace openblack::online
{

using WebSocketId = uint32_t;

enum class WebSocketState : uint8_t
{
	/// The client has no connection with this id, or the connection ended
	None,
	/// The connection or its handshake is in progress
	Connecting,
	Open,
};

/// Why a connection ended
enum class WebSocketEnd : uint8_t
{
	/// The server sent a close frame. The close code tells why.
	ClosedByServer,
	/// The server did not accept the handshake. The HTTP status tells why, when the client can read it.
	HandshakeRefused,
	/// The address is not one that the client can open: not wss://, or in the browser not a path of the origin
	AddressRefused,
	/// The client could not connect, or the connection stopped with no close frame
	Network,
	/// The TLS connection failed: a bad certificate, a wrong host name, or no version or cipher suite that both sides have
	Tls,
	/// The connection did not open in time, or the server sent nothing for too long
	Timeout,
	/// A message from the server was larger than the client accepts
	TooLarge,
};

struct WebSocketClosed
{
	WebSocketEnd end {WebSocketEnd::Network};
	/// The close code of the close frame of the server, 0 when there was no close frame
	uint16_t code {0};
	/// The HTTP status of a refused handshake, 0 when the client does not know it
	int32_t httpStatus {0};
};

struct WebSocketRequest
{
	/// The full wss:// address on the desktop, or a path that starts with "/" in the browser
	std::string url;
	/// More headers of the handshake. The browser cannot send headers, so its client ignores them.
	std::vector<HttpHeader> headers;
	/// The subprotocols that the handshake asks for, in the order of preference
	std::vector<std::string> subprotocols;
};

/// The handlers of a connection. They run from Poll, never in Connect, SendText, SendBinary or Close.
struct WebSocketHandlers
{
	std::function<void()> opened;
	/// A whole text message
	std::function<void(std::string_view text)> text;
	/// A whole binary message
	std::function<void(std::span<const uint8_t> bytes)> binary;
	/// The connection ended. No handler of the connection runs after this one.
	std::function<void(const WebSocketClosed& closed)> closed;
};

/// The close codes of RFC 6455 that the game uses
namespace websocket_close
{
/// The usual end of a connection
constexpr uint16_t k_Normal = 1000;
/// The client goes away, for example because the game stops
constexpr uint16_t k_GoingAway = 1001;
} // namespace websocket_close

class WebSocketClientInterface
{
public:
	virtual ~WebSocketClientInterface() = default;

	/// Starts a connection. The opened handler, or the closed handler, runs later from Poll.
	virtual WebSocketId Connect(WebSocketRequest request, WebSocketHandlers handlers) = 0;
	/// Sends a text message on an open connection. Returns false, and sends nothing, when the connection is not open.
	virtual bool SendText(WebSocketId id, std::string_view text) = 0;
	/// Sends a binary message on an open connection. Returns false, and sends nothing, when the connection is not open.
	virtual bool SendBinary(WebSocketId id, std::span<const uint8_t> bytes) = 0;
	/// Ends a connection with a close code. No handler of the connection runs after this call.
	virtual void Close(WebSocketId id, uint16_t code) = 0;
	/// Once a frame: moves the connections on without a wait, and runs the handlers
	virtual void Poll() = 0;
	[[nodiscard]] virtual WebSocketState State(WebSocketId id) const = 0;
	/// The number of connections that did not end
	[[nodiscard]] virtual size_t Count() const = 0;
};

} // namespace openblack::online
