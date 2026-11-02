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

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "Server/ConnectionLimits.h"
#include "Server/WebSocketServerInterface.h"

namespace openblack::server
{

struct ListenSettings
{
	/// The IPv4 address that the server listens on
	std::string address {"127.0.0.1"};
	/// The TCP port. 0 lets the system choose a free port.
	uint16_t port {0};
	/// The certificate chain and the key of TLS, as PEM files. With none, the server speaks plain WebSocket, which it
	/// allows only on a loopback address, behind a reverse proxy that does the TLS.
	std::optional<std::filesystem::path> certificate;
	std::optional<std::filesystem::path> key;
};

/// The WebSocket server of the desktop: non-blocking TCP sockets, with TLS 1.2 or 1.3 from OpenSSL (AEAD suites only).
class SocketWebSocketServer final: public WebSocketServerInterface
{
public:
	SocketWebSocketServer();
	~SocketWebSocketServer() override;
	SocketWebSocketServer(const SocketWebSocketServer&) = delete;
	SocketWebSocketServer& operator=(const SocketWebSocketServer&) = delete;
	SocketWebSocketServer(SocketWebSocketServer&&) = delete;
	SocketWebSocketServer& operator=(SocketWebSocketServer&&) = delete;

	/// Starts to listen. Gives the error, or none when the server listens.
	[[nodiscard]] std::optional<std::string> Listen(const ListenSettings& settings);
	/// The port that the server listens on
	[[nodiscard]] uint16_t Port() const { return _port; }

	void SetHandlers(ServerSocketHandlers handlers) override { _handlers = std::move(handlers); }
	bool SendText(ConnectionId connection, std::string_view text) override;
	bool SendBinary(ConnectionId connection, std::span<const uint8_t> bytes) override;
	void Close(ConnectionId connection, uint16_t code) override;
	void Poll() override;
	[[nodiscard]] size_t Count() const override;

	struct Connection;
	struct Tls;

private:
	void Accept();
	/// Moves one connection on. Gives false when the connection ended.
	bool Service(ConnectionId id, Connection& connection);
	bool Send(ConnectionId id, uint8_t opcode, std::span<const uint8_t> bytes);

	ServerSocketHandlers _handlers;
	std::unique_ptr<Tls> _tls;
	intptr_t _listener {-1};
	uint16_t _port {0};
	ConnectionId _nextId {1};
	std::map<ConnectionId, std::unique_ptr<Connection>> _connections;
	/// The connections of each client address
	connection_limits::AddressCounts _addresses;
};

/// True for an address of 127.0.0.0/8
[[nodiscard]] bool IsLoopbackAddress(std::string_view address);

} // namespace openblack::server
