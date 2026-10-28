/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SocketWebSocketServer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <spdlog/spdlog.h>

#include "Server/WebSocketFrames.h"

namespace openblack::server
{
namespace
{

namespace frames = websocket_frames;

#if defined(_WIN32)
using Socket = SOCKET;
const Socket k_NoSocket = INVALID_SOCKET;
void CloseSocket(Socket socket)
{
	closesocket(socket);
}
bool WouldBlock()
{
	return WSAGetLastError() == WSAEWOULDBLOCK;
}
bool MakeNonBlocking(Socket socket)
{
	u_long on = 1;
	return ioctlsocket(socket, FIONBIO, &on) == 0;
}
#else
using Socket = int;
constexpr Socket k_NoSocket = -1;
void CloseSocket(Socket socket)
{
	close(socket);
}
bool WouldBlock()
{
	return errno == EAGAIN || errno == EWOULDBLOCK;
}
bool MakeNonBlocking(Socket socket)
{
#if defined(SO_NOSIGPIPE)
	// A write to a client that closed its socket is an error that the server handles. It must not stop the server.
	int on = 1;
	setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
	const auto flags = fcntl(socket, F_GETFL, 0);
	return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
}
#endif

#if defined(MSG_NOSIGNAL)
constexpr int k_SendFlags = MSG_NOSIGNAL;
#else
constexpr int k_SendFlags = 0;
#endif

Socket ToSocket(intptr_t value)
{
	return static_cast<Socket>(value);
}

/// The largest message that a client can send. The largest binary frame of the game is 64 KB.
constexpr size_t k_MostMessageBytes = 1024 * 1024;
/// A connection that sends nothing for this time ends. A client pings after 20 s of quiet.
constexpr auto k_IdleLimit = std::chrono::seconds(60);
/// A connection must finish the TLS and WebSocket handshakes in this time
constexpr auto k_HandshakeLimit = std::chrono::seconds(15);
constexpr size_t k_ReadChunk = 16 * 1024;
/// The TLS 1.2 cipher suites: ECDHE with AES-GCM or ChaCha20-Poly1305 only, as the game's clients
constexpr const char* k_Tls12Suites = "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:"
                                      "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:"
                                      "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305";
constexpr const char* k_Tls13Suites = "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256";

std::string Base64(std::span<const uint8_t> bytes)
{
	constexpr std::string_view k_Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	size_t i = 0;
	for (; i + 2 < bytes.size(); i += 3)
	{
		const uint32_t value = (bytes[i] << 16) | (bytes[i + 1] << 8) | bytes[i + 2];
		out += k_Alphabet[(value >> 18) & 0x3F];
		out += k_Alphabet[(value >> 12) & 0x3F];
		out += k_Alphabet[(value >> 6) & 0x3F];
		out += k_Alphabet[value & 0x3F];
	}
	if (i + 1 == bytes.size())
	{
		const uint32_t value = bytes[i] << 16;
		out += k_Alphabet[(value >> 18) & 0x3F];
		out += k_Alphabet[(value >> 12) & 0x3F];
		out += "==";
	}
	else if (i + 2 == bytes.size())
	{
		const uint32_t value = (bytes[i] << 16) | (bytes[i + 1] << 8);
		out += k_Alphabet[(value >> 18) & 0x3F];
		out += k_Alphabet[(value >> 12) & 0x3F];
		out += k_Alphabet[(value >> 6) & 0x3F];
		out += '=';
	}
	return out;
}

std::string AcceptValue(std::string_view key)
{
	const auto source = frames::AcceptSource(key);
	std::array<uint8_t, SHA_DIGEST_LENGTH> digest {};
	SHA1(reinterpret_cast<const unsigned char*>(source.data()), source.size(), digest.data());
	return Base64(digest);
}

struct SslDeleter
{
	void operator()(SSL* ssl) const { SSL_free(ssl); }
};
struct ContextDeleter
{
	void operator()(SSL_CTX* context) const { SSL_CTX_free(context); }
};

} // namespace

bool IsLoopbackAddress(std::string_view address)
{
	return address.starts_with("127.") || address == "localhost";
}

struct SocketWebSocketServer::Tls
{
	std::unique_ptr<SSL_CTX, ContextDeleter> context;
};

struct SocketWebSocketServer::Connection
{
	enum class Stage : uint8_t
	{
		Tls,
		Handshake,
		Open,
		/// The server closed it and sends what is left
		Closing,
		Ended,
	};

	Socket socket {k_NoSocket};
	std::unique_ptr<SSL, SslDeleter> ssl;
	Stage stage {Stage::Handshake};
	std::string handshake;
	frames::FrameReader reader {k_MostMessageBytes};
	std::vector<uint8_t> out;
	std::chrono::steady_clock::time_point started;
	std::chrono::steady_clock::time_point lastHeard;

	~Connection()
	{
		ssl.reset();
		if (socket != k_NoSocket)
		{
			CloseSocket(socket);
		}
	}

	/// Reads what the socket has. Gives false when the connection ended.
	bool Read(std::vector<uint8_t>& into)
	{
		std::array<uint8_t, k_ReadChunk> chunk {};
		while (true)
		{
			int got = 0;
			if (ssl)
			{
				got = SSL_read(ssl.get(), chunk.data(), static_cast<int>(chunk.size()));
				if (got <= 0)
				{
					const auto error = SSL_get_error(ssl.get(), got);
					return error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE;
				}
			}
			else
			{
				got = static_cast<int>(recv(socket, reinterpret_cast<char*>(chunk.data()), static_cast<int>(chunk.size()), 0));
				if (got == 0)
				{
					return false;
				}
				if (got < 0)
				{
					return WouldBlock();
				}
			}
			into.insert(into.end(), chunk.begin(), chunk.begin() + got);
		}
	}

	/// Writes what waits. Gives false when the connection ended.
	bool Flush()
	{
		while (!out.empty())
		{
			int sent = 0;
			if (ssl)
			{
				sent = SSL_write(ssl.get(), out.data(), static_cast<int>(out.size()));
				if (sent <= 0)
				{
					const auto error = SSL_get_error(ssl.get(), sent);
					return error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE;
				}
			}
			else
			{
				sent = static_cast<int>(
				    send(socket, reinterpret_cast<const char*>(out.data()), static_cast<int>(out.size()), k_SendFlags));
				if (sent < 0)
				{
					return WouldBlock();
				}
			}
			out.erase(out.begin(), out.begin() + sent);
		}
		return true;
	}
};

SocketWebSocketServer::SocketWebSocketServer()
{
#if defined(_WIN32)
	WSADATA data {};
	WSAStartup(MAKEWORD(2, 2), &data);
#endif
}

SocketWebSocketServer::~SocketWebSocketServer()
{
	_connections.clear();
	if (_listener != -1)
	{
		CloseSocket(ToSocket(_listener));
	}
#if defined(_WIN32)
	WSACleanup();
#endif
}

std::optional<std::string> SocketWebSocketServer::Listen(const ListenSettings& settings)
{
	if (settings.certificate.has_value() != settings.key.has_value())
	{
		return "TLS needs both a certificate and a key";
	}
	if (!settings.certificate.has_value() && !IsLoopbackAddress(settings.address))
	{
		return "plain WebSocket is only for a loopback address; give a certificate and a key for TLS";
	}
	if (settings.certificate.has_value())
	{
		auto tls = std::make_unique<Tls>();
		tls->context.reset(SSL_CTX_new(TLS_server_method()));
		auto* context = tls->context.get();
		if (context == nullptr || SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) != 1 ||
		    SSL_CTX_set_cipher_list(context, k_Tls12Suites) != 1 || SSL_CTX_set_ciphersuites(context, k_Tls13Suites) != 1)
		{
			return "the TLS settings do not apply";
		}
		SSL_CTX_set_mode(context, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
		SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);
		if (SSL_CTX_use_certificate_chain_file(context, settings.certificate->string().c_str()) != 1 ||
		    SSL_CTX_use_PrivateKey_file(context, settings.key->string().c_str(), SSL_FILETYPE_PEM) != 1 ||
		    SSL_CTX_check_private_key(context) != 1)
		{
			return "the certificate or the key of TLS does not load";
		}
		_tls = std::move(tls);
	}

	const auto listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == k_NoSocket)
	{
		return "no socket";
	}
	sockaddr_in address {};
	address.sin_family = AF_INET;
	address.sin_port = htons(settings.port);
	const auto host = settings.address == "localhost" ? std::string("127.0.0.1") : settings.address;
	if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1)
	{
		CloseSocket(listener);
		return "the address is not an IPv4 address";
	}
	if (bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 || listen(listener, 64) != 0 ||
	    !MakeNonBlocking(listener))
	{
		CloseSocket(listener);
		return "the server cannot listen on that address and port";
	}
	socklen_t length = sizeof(address);
	getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length);
	_port = ntohs(address.sin_port);
	_listener = static_cast<intptr_t>(listener);
	return std::nullopt;
}

void SocketWebSocketServer::Accept()
{
	while (true)
	{
		const auto client = accept(ToSocket(_listener), nullptr, nullptr);
		if (client == k_NoSocket)
		{
			return;
		}
		if (!MakeNonBlocking(client))
		{
			CloseSocket(client);
			continue;
		}
		// The turns are small messages that must go at once
		int on = 1;
		setsockopt(client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof(on));
		auto connection = std::make_unique<Connection>();
		connection->socket = client;
		connection->started = std::chrono::steady_clock::now();
		connection->lastHeard = connection->started;
		if (_tls)
		{
			connection->ssl.reset(SSL_new(_tls->context.get()));
			if (!connection->ssl || SSL_set_fd(connection->ssl.get(), static_cast<int>(client)) != 1)
			{
				continue;
			}
			connection->stage = Connection::Stage::Tls;
		}
		_connections.emplace(_nextId++, std::move(connection));
	}
}

bool SocketWebSocketServer::Service(ConnectionId id, Connection& connection)
{
	using Stage = Connection::Stage;
	const auto now = std::chrono::steady_clock::now();
	if (connection.stage == Stage::Ended)
	{
		return false;
	}
	if (connection.stage == Stage::Tls)
	{
		const auto result = SSL_accept(connection.ssl.get());
		if (result != 1)
		{
			const auto error = SSL_get_error(connection.ssl.get(), result);
			ERR_clear_error();
			return (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) &&
			       now - connection.started < k_HandshakeLimit;
		}
		connection.stage = Stage::Handshake;
	}

	std::vector<uint8_t> bytes;
	const bool alive = connection.Read(bytes);
	ERR_clear_error();
	if (!bytes.empty())
	{
		connection.lastHeard = now;
	}
	if (connection.stage == Stage::Handshake)
	{
		connection.handshake.append(bytes.begin(), bytes.end());
		bytes.clear();
		frames::Handshake handshake;
		size_t used = 0;
		const auto result = frames::ReadHandshake(connection.handshake, handshake, used);
		if (result == frames::HandshakeResult::Bad)
		{
			const auto answer = frames::RefuseAnswer();
			connection.out.assign(answer.begin(), answer.end());
			connection.Flush();
			return false;
		}
		if (result == frames::HandshakeResult::Incomplete)
		{
			return alive && now - connection.started < k_HandshakeLimit;
		}
		const auto answer = frames::AcceptAnswer(AcceptValue(handshake.key));
		connection.out.insert(connection.out.end(), answer.begin(), answer.end());
		// Bytes after the request are the first frames
		bytes.assign(connection.handshake.begin() + static_cast<std::ptrdiff_t>(used), connection.handshake.end());
		connection.handshake.clear();
		connection.stage = Stage::Open;
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Game socket {} opened on {}", id, handshake.path);
		if (_handlers.opened)
		{
			_handlers.opened(id);
		}
		if (connection.stage != Stage::Open)
		{
			return connection.Flush() && !connection.out.empty();
		}
	}

	if (connection.stage == Stage::Open && !bytes.empty())
	{
		for (auto& message : connection.reader.Feed(bytes))
		{
			if (connection.stage != Stage::Open)
			{
				break;
			}
			switch (message.opcode)
			{
			case frames::Opcode::Text:
				if (_handlers.text)
				{
					_handlers.text(
					    id, std::string_view(reinterpret_cast<const char*>(message.payload.data()), message.payload.size()));
				}
				break;
			case frames::Opcode::Binary:
				if (_handlers.binary)
				{
					_handlers.binary(id, message.payload);
				}
				break;
			case frames::Opcode::Ping:
			{
				const auto pong = frames::EncodeFrame(frames::Opcode::Pong, message.payload);
				connection.out.insert(connection.out.end(), pong.begin(), pong.end());
				break;
			}
			case frames::Opcode::Close:
			{
				const uint16_t code = message.payload.size() >= 2
				                          ? static_cast<uint16_t>((message.payload[0] << 8) | message.payload[1])
				                          : server_close::k_Normal;
				const auto reply = frames::EncodeClose(code);
				connection.out.insert(connection.out.end(), reply.begin(), reply.end());
				connection.Flush();
				connection.stage = Stage::Ended;
				if (_handlers.closed)
				{
					_handlers.closed(id, code);
				}
				return false;
			}
			default:
				break;
			}
		}
		if (connection.stage == Stage::Open && connection.reader.GetError() != frames::FrameReader::Error::None)
		{
			const auto code = connection.reader.GetError() == frames::FrameReader::Error::TooLarge ? server_close::k_TooLarge
			                                                                                       : server_close::k_Protocol;
			const auto reply = frames::EncodeClose(code);
			connection.out.insert(connection.out.end(), reply.begin(), reply.end());
			connection.Flush();
			connection.stage = Stage::Ended;
			if (_handlers.closed)
			{
				_handlers.closed(id, code);
			}
			return false;
		}
	}

	const bool flushed = connection.Flush();
	if (connection.stage == Stage::Closing)
	{
		return flushed && !connection.out.empty() && now - connection.lastHeard < k_HandshakeLimit;
	}
	if (!alive || !flushed || now - connection.lastHeard > k_IdleLimit)
	{
		if (connection.stage == Stage::Open && _handlers.closed)
		{
			connection.stage = Stage::Ended;
			_handlers.closed(id, server_close::k_NoCloseFrame);
		}
		return false;
	}
	return true;
}

void SocketWebSocketServer::Poll()
{
	if (_listener == -1)
	{
		return;
	}
	Accept();
	std::vector<ConnectionId> ids;
	ids.reserve(_connections.size());
	for (const auto& [id, connection] : _connections)
	{
		ids.push_back(id);
	}
	for (const auto id : ids)
	{
		const auto found = _connections.find(id);
		if (found == _connections.end())
		{
			continue;
		}
		if (!Service(id, *found->second))
		{
			_connections.erase(id);
		}
	}
}

bool SocketWebSocketServer::Send(ConnectionId id, uint8_t opcode, std::span<const uint8_t> bytes)
{
	const auto found = _connections.find(id);
	if (found == _connections.end() || found->second->stage != Connection::Stage::Open)
	{
		return false;
	}
	auto& connection = *found->second;
	const auto frame = frames::EncodeFrame(static_cast<frames::Opcode>(opcode), bytes);
	connection.out.insert(connection.out.end(), frame.begin(), frame.end());
	// The frame goes at once when the socket takes it
	connection.Flush();
	ERR_clear_error();
	return true;
}

bool SocketWebSocketServer::SendText(ConnectionId connection, std::string_view text)
{
	return Send(connection, static_cast<uint8_t>(frames::Opcode::Text),
	            {reinterpret_cast<const uint8_t*>(text.data()), text.size()});
}

bool SocketWebSocketServer::SendBinary(ConnectionId connection, std::span<const uint8_t> bytes)
{
	return Send(connection, static_cast<uint8_t>(frames::Opcode::Binary), bytes);
}

void SocketWebSocketServer::Close(ConnectionId id, uint16_t code)
{
	const auto found = _connections.find(id);
	if (found == _connections.end())
	{
		return;
	}
	auto& connection = *found->second;
	if (connection.stage == Connection::Stage::Open)
	{
		const auto frame = frames::EncodeClose(code);
		connection.out.insert(connection.out.end(), frame.begin(), frame.end());
		connection.stage = Connection::Stage::Closing;
		connection.lastHeard = std::chrono::steady_clock::now();
		connection.Flush();
		ERR_clear_error();
	}
	else
	{
		connection.stage = Connection::Stage::Ended;
	}
}

size_t SocketWebSocketServer::Count() const
{
	return static_cast<size_t>(
	    std::ranges::count_if(_connections, [](const auto& entry) { return entry.second->stage == Connection::Stage::Open; }));
}

} // namespace openblack::server
