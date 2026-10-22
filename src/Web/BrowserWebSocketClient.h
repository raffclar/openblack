/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include "Online/WebSocketInterface.h"

namespace openblack::web
{

/// The WSS client of the web build, with the WebSocket of the browser. The sockets are on the worker thread of the game,
/// so their events wait in the worker until Poll takes them, and no other thread touches the client. The client opens
/// only paths on the origin of the page, and only when the page is an HTTPS page, so the browser checks the certificate.
/// The browser cannot send headers in the handshake, so the access token goes in a subprotocol. The browser answers the
/// pings of the server.
class BrowserWebSocketClient final: public online::WebSocketClientInterface
{
public:
	BrowserWebSocketClient();
	~BrowserWebSocketClient() override;
	BrowserWebSocketClient(const BrowserWebSocketClient&) = delete;
	BrowserWebSocketClient& operator=(const BrowserWebSocketClient&) = delete;
	BrowserWebSocketClient(BrowserWebSocketClient&&) = delete;
	BrowserWebSocketClient& operator=(BrowserWebSocketClient&&) = delete;

	online::WebSocketId Connect(online::WebSocketRequest request, online::WebSocketHandlers handlers) override;
	bool SendText(online::WebSocketId id, std::string_view text) override;
	bool SendBinary(online::WebSocketId id, std::span<const uint8_t> bytes) override;
	void Close(online::WebSocketId id, uint16_t code) override;
	void Poll() override;
	[[nodiscard]] online::WebSocketState State(online::WebSocketId id) const override;
	[[nodiscard]] size_t Count() const override;

	struct Connection;

private:
	[[nodiscard]] Connection* Find(online::WebSocketId id) const;
	void Take(Connection& connection);
	void Release(Connection& connection);
	void Dispatch();

	std::vector<std::unique_ptr<Connection>> _connections;
	online::WebSocketId _nextId {1};
};

} // namespace openblack::web
