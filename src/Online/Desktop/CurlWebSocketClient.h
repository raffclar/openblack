/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Online/WebSocketInterface.h"

namespace openblack::online
{

/// The times and the limits of the desktop WSS client
namespace websocket_settings
{
/// The most time for the connection, the TLS handshake and the upgrade of the connection
constexpr std::chrono::milliseconds k_HandshakeTimeout {15'000};
/// The client sends a ping when it got nothing from the server for this time
constexpr std::chrono::milliseconds k_PingAfter {20'000};
/// The client ends the connection when it got nothing from the server for this time, also no answer to a ping
constexpr std::chrono::milliseconds k_SilenceTimeout {50'000};
/// The largest message that the client accepts. A full world state for a player who joins a game can be large.
constexpr size_t k_MostMessageBytes = 16 * 1024 * 1024;
} // namespace websocket_settings

/// The WSS client of the desktop, with the WebSocket support of libcurl and OpenSSL. It uses the TLS rules of the HTTPS
/// client. The connections move on in Poll, without a wait, so a connection does not stop a frame. libcurl answers the
/// pings of the server.
class CurlWebSocketClient final: public WebSocketClientInterface
{
public:
	/// The file of more certificate authorities is for the tests on this computer only. The client always checks the
	/// certificate and the host name.
	explicit CurlWebSocketClient(std::optional<std::string> extraAuthorities = std::nullopt);
	~CurlWebSocketClient() override;
	CurlWebSocketClient(const CurlWebSocketClient&) = delete;
	CurlWebSocketClient& operator=(const CurlWebSocketClient&) = delete;
	CurlWebSocketClient(CurlWebSocketClient&&) = delete;
	CurlWebSocketClient& operator=(CurlWebSocketClient&&) = delete;

	WebSocketId Connect(WebSocketRequest request, WebSocketHandlers handlers) override;
	bool SendText(WebSocketId id, std::string_view text) override;
	bool SendBinary(WebSocketId id, std::span<const uint8_t> bytes) override;
	void Close(WebSocketId id, uint16_t code) override;
	void Poll() override;
	[[nodiscard]] WebSocketState State(WebSocketId id) const override;
	[[nodiscard]] size_t Count() const override;

	struct Connection;

private:
	struct Multi;

	[[nodiscard]] Connection* Find(WebSocketId id) const;
	bool Queue(WebSocketId id, std::span<const uint8_t> bytes, unsigned int flags);
	void MoveHandshakes();
	void Release(Connection& connection);
	void Dispatch();

	std::optional<std::string> _extraAuthorities;
	std::unique_ptr<Multi> _multi;
	std::vector<std::unique_ptr<Connection>> _connections;
	std::vector<uint8_t> _receiveBuffer;
	WebSocketId _nextId {1};
};

} // namespace openblack::online
