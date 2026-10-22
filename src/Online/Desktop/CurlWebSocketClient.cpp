/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CurlWebSocketClient.h"

#include <algorithm>
#include <array>
#include <deque>
#include <string>
#include <utility>
#include <variant>

#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include "Online/Desktop/CurlTls.h"

namespace openblack::online
{
namespace
{

using Clock = std::chrono::steady_clock;

struct EasyDeleter
{
	void operator()(CURL* easy) const { curl_easy_cleanup(easy); }
};

struct ListDeleter
{
	void operator()(curl_slist* list) const { curl_slist_free_all(list); }
};

/// The size of the buffer that each receive fills
constexpr size_t k_ReceiveChunk = 16 * 1024;
/// The HTTP status of an accepted upgrade to WebSocket
constexpr long k_SwitchingProtocols = 101;
/// The close code of a close frame with no code
constexpr uint16_t k_NoCode = 1005;
/// The close code when a message is too large
constexpr uint16_t k_MessageTooBig = 1009;

struct Opened
{
};

struct Message
{
	bool text {false};
	std::vector<uint8_t> bytes;
};

using Event = std::variant<Opened, Message, WebSocketClosed>;

WebSocketEnd EndOf(HttpError error)
{
	switch (error)
	{
	case HttpError::AddressRefused:
		return WebSocketEnd::AddressRefused;
	case HttpError::Tls:
		return WebSocketEnd::Tls;
	case HttpError::Timeout:
		return WebSocketEnd::Timeout;
	case HttpError::None:
	case HttpError::Network:
	case HttpError::Cancelled:
		return WebSocketEnd::Network;
	}
	return WebSocketEnd::Network;
}

} // namespace

struct CurlWebSocketClient::Multi
{
	CURLM* handle {nullptr};

	Multi()
	    : handle(curl_multi_init())
	{
	}
	~Multi()
	{
		if (handle != nullptr)
		{
			curl_multi_cleanup(handle);
		}
	}
	Multi(const Multi&) = delete;
	Multi& operator=(const Multi&) = delete;
	Multi(Multi&&) = delete;
	Multi& operator=(Multi&&) = delete;
};

struct CurlWebSocketClient::Connection
{
	struct Outgoing
	{
		std::vector<uint8_t> bytes;
		unsigned int flags {0};
		size_t sent {0};
	};

	WebSocketId id {0};
	WebSocketHandlers handlers;
	std::unique_ptr<CURL, EasyDeleter> easy;
	std::unique_ptr<curl_slist, ListDeleter> headers;
	WebSocketState state {WebSocketState::Connecting};
	/// The easy handle is in the multi handle. After the handshake it stays there, because libcurl keeps the connection
	/// of a connect-only transfer only while the transfer is in its multi handle.
	bool inMulti {false};
	/// The connection ended. The client removes it after the handlers ran.
	bool finished {false};
	std::deque<Outgoing> outgoing;
	/// The parts of the data message that the client receives now
	std::vector<uint8_t> message;
	bool messageIsText {false};
	/// The parts of the control frame that the client receives now
	std::vector<uint8_t> control;
	Clock::time_point started {Clock::now()};
	Clock::time_point lastReceived {Clock::now()};
	Clock::time_point lastPing {Clock::now()};
	std::deque<Event> events;

	void End(WebSocketClosed closed)
	{
		if (state == WebSocketState::None)
		{
			return;
		}
		state = WebSocketState::None;
		outgoing.clear();
		events.emplace_back(closed);
	}

	/// Sends what waits, as far as the socket takes it. Returns false when the connection failed.
	bool Flush()
	{
		while (!outgoing.empty())
		{
			auto& next = outgoing.front();
			size_t sent = 0;
			const auto* data = next.bytes.data() + next.sent;
			const auto result = curl_ws_send(easy.get(), data, next.bytes.size() - next.sent, &sent, 0, next.flags);
			if (result == CURLE_AGAIN)
			{
				return true;
			}
			if (result != CURLE_OK)
			{
				return false;
			}
			next.sent += sent;
			if (next.sent < next.bytes.size())
			{
				// The socket takes no more now. The rest goes in the next Poll.
				return true;
			}
			outgoing.pop_front();
		}
		return true;
	}

	/// Sends a close frame and does not wait for the answer
	void SendClose(uint16_t code)
	{
		const std::array<uint8_t, 2> payload {static_cast<uint8_t>(code >> 8), static_cast<uint8_t>(code & 0xFF)};
		size_t sent = 0;
		curl_ws_send(easy.get(), payload.data(), payload.size(), &sent, 0, CURLWS_CLOSE);
	}

	void Receive(std::vector<uint8_t>& buffer)
	{
		while (state == WebSocketState::Open)
		{
			size_t got = 0;
			const curl_ws_frame* meta = nullptr;
			const auto result = curl_ws_recv(easy.get(), buffer.data(), buffer.size(), &got, &meta);
			if (result == CURLE_AGAIN)
			{
				return;
			}
			if (result != CURLE_OK || meta == nullptr)
			{
				End({.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0});
				return;
			}
			lastReceived = Clock::now();
			const auto flags = static_cast<unsigned int>(meta->flags);
			const bool last = meta->bytesleft == 0;
			const std::span<const uint8_t> part(buffer.data(), got);
			if ((flags & (CURLWS_TEXT | CURLWS_BINARY)) != 0)
			{
				if (message.size() + part.size() > websocket_settings::k_MostMessageBytes)
				{
					SendClose(k_MessageTooBig);
					End({.end = WebSocketEnd::TooLarge, .code = 0, .httpStatus = 0});
					return;
				}
				if (message.empty())
				{
					messageIsText = (flags & CURLWS_TEXT) != 0;
				}
				message.insert(message.end(), part.begin(), part.end());
				if (last && (flags & CURLWS_CONT) == 0)
				{
					events.emplace_back(Message {.text = messageIsText, .bytes = std::move(message)});
					message.clear();
				}
				continue;
			}
			// A control frame: a close, a ping that libcurl answered, or a pong
			control.insert(control.end(), part.begin(), part.end());
			if (!last)
			{
				continue;
			}
			if ((flags & CURLWS_CLOSE) != 0)
			{
				const uint16_t code = control.size() >= 2 ? static_cast<uint16_t>((control[0] << 8) | control[1]) : k_NoCode;
				// The client answers the close frame with the same code, as RFC 6455 tells
				SendClose(code == k_NoCode ? websocket_close::k_Normal : code);
				End({.end = WebSocketEnd::ClosedByServer, .code = code, .httpStatus = 0});
				return;
			}
			control.clear();
		}
	}

	/// Sends a ping when the server was silent, and ends the connection when the server was silent for too long
	void KeepAlive()
	{
		const auto now = Clock::now();
		if (now - lastReceived > websocket_settings::k_SilenceTimeout)
		{
			End({.end = WebSocketEnd::Timeout, .code = 0, .httpStatus = 0});
			return;
		}
		if (now - lastReceived > websocket_settings::k_PingAfter && now - lastPing > websocket_settings::k_PingAfter)
		{
			lastPing = now;
			outgoing.push_back({.bytes = {}, .flags = CURLWS_PING, .sent = 0});
		}
	}
};

CurlWebSocketClient::CurlWebSocketClient(std::optional<std::string> extraAuthorities)
    : _extraAuthorities(std::move(extraAuthorities))
    , _receiveBuffer(k_ReceiveChunk)
{
	tls_settings::StartLibrary();
}

CurlWebSocketClient::~CurlWebSocketClient()
{
	for (auto& connection : _connections)
	{
		if (connection->state == WebSocketState::Open)
		{
			connection->SendClose(websocket_close::k_GoingAway);
		}
		Release(*connection);
	}
	_connections.clear();
}

CurlWebSocketClient::Connection* CurlWebSocketClient::Find(WebSocketId id) const
{
	const auto found = std::ranges::find_if(
	    _connections, [id](const auto& connection) { return connection->id == id && !connection->finished; });
	return found != _connections.end() ? found->get() : nullptr;
}

void CurlWebSocketClient::Release(Connection& connection)
{
	if (connection.inMulti && _multi != nullptr)
	{
		curl_multi_remove_handle(_multi->handle, connection.easy.get());
	}
	connection.inMulti = false;
	connection.easy.reset();
	connection.headers.reset();
}

WebSocketId CurlWebSocketClient::Connect(WebSocketRequest request, WebSocketHandlers handlers)
{
	auto connection = std::make_unique<Connection>();
	connection->id = _nextId++;
	connection->handlers = std::move(handlers);
	const auto id = connection->id;

	if (!http_policy::IsWssAddress(request.url))
	{
		connection->End({.end = WebSocketEnd::AddressRefused, .code = 0, .httpStatus = 0});
		_connections.push_back(std::move(connection));
		return id;
	}
	if (_multi == nullptr)
	{
		// The client makes the multi handle at the first connection, so a game with no lobby makes none
		_multi = std::make_unique<Multi>();
	}
	connection->easy.reset(curl_easy_init());
	if (!connection->easy || _multi->handle == nullptr)
	{
		connection->End({.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0});
		_connections.push_back(std::move(connection));
		return id;
	}

	auto* easy = connection->easy.get();
	curl_easy_setopt(easy, CURLOPT_URL, request.url.c_str());
	// WSS only, and no redirection to another address
	curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "wss");
	curl_easy_setopt(easy, CURLOPT_REDIR_PROTOCOLS_STR, "wss");
	curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 0L);
	// The transfer stops after the upgrade, and the client then sends and receives the messages itself
	curl_easy_setopt(easy, CURLOPT_CONNECT_ONLY, 2L);
	tls_settings::Apply(easy, _extraAuthorities.has_value() ? _extraAuthorities->c_str() : nullptr);
	curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, tls_settings::k_ConnectTimeoutMs);
	curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(easy, CURLOPT_USERAGENT, "openblack");
	curl_easy_setopt(easy, CURLOPT_PRIVATE, connection.get());

	curl_slist* list = nullptr;
	for (const auto& [name, value] : request.headers)
	{
		list = curl_slist_append(list, (name + ": " + value).c_str());
	}
	if (!request.subprotocols.empty())
	{
		std::string protocols;
		for (const auto& protocol : request.subprotocols)
		{
			protocols += protocols.empty() ? protocol : ", " + protocol;
		}
		list = curl_slist_append(list, ("Sec-WebSocket-Protocol: " + protocols).c_str());
	}
	connection->headers.reset(list);
	curl_easy_setopt(easy, CURLOPT_HTTPHEADER, list);

	if (curl_multi_add_handle(_multi->handle, easy) != CURLM_OK)
	{
		connection->End({.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0});
	}
	else
	{
		connection->inMulti = true;
	}
	_connections.push_back(std::move(connection));
	return id;
}

bool CurlWebSocketClient::Queue(WebSocketId id, std::span<const uint8_t> bytes, unsigned int flags)
{
	auto* connection = Find(id);
	if (connection == nullptr || connection->state != WebSocketState::Open)
	{
		return false;
	}
	connection->outgoing.push_back({.bytes = {bytes.begin(), bytes.end()}, .flags = flags, .sent = 0});
	// The message goes at once when the socket takes it, so that a command does not wait for the next frame
	if (!connection->Flush())
	{
		connection->End({.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0});
	}
	return true;
}

bool CurlWebSocketClient::SendText(WebSocketId id, std::string_view text)
{
	return Queue(id, {reinterpret_cast<const uint8_t*>(text.data()), text.size()}, CURLWS_TEXT);
}

bool CurlWebSocketClient::SendBinary(WebSocketId id, std::span<const uint8_t> bytes)
{
	return Queue(id, bytes, CURLWS_BINARY);
}

void CurlWebSocketClient::Close(WebSocketId id, uint16_t code)
{
	auto* connection = Find(id);
	if (connection == nullptr)
	{
		return;
	}
	if (connection->state == WebSocketState::Open)
	{
		connection->Flush();
		connection->SendClose(code);
	}
	// The handlers stay until the client removes the connection, because this call can come from one of them
	connection->state = WebSocketState::None;
	connection->finished = true;
	connection->events.clear();
	Release(*connection);
}

void CurlWebSocketClient::MoveHandshakes()
{
	if (_multi == nullptr || std::ranges::none_of(_connections, [](const auto& connection) {
		    return connection->state == WebSocketState::Connecting;
	    }))
	{
		return;
	}
	int running = 0;
	curl_multi_perform(_multi->handle, &running);
	int queued = 0;
	while (CURLMsg* done = curl_multi_info_read(_multi->handle, &queued))
	{
		if (done->msg != CURLMSG_DONE)
		{
			continue;
		}
		Connection* connection = nullptr;
		curl_easy_getinfo(done->easy_handle, CURLINFO_PRIVATE, &connection);
		if (connection == nullptr || connection->state != WebSocketState::Connecting)
		{
			continue;
		}
		const auto code = done->data.result;
		long status = 0;
		curl_easy_getinfo(done->easy_handle, CURLINFO_RESPONSE_CODE, &status);
		if (code == CURLE_OK && status == k_SwitchingProtocols)
		{
			connection->state = WebSocketState::Open;
			connection->lastReceived = Clock::now();
			connection->lastPing = connection->lastReceived;
			connection->events.emplace_back(Opened {});
			continue;
		}
		if (status != 0 && status != k_SwitchingProtocols)
		{
			connection->End({.end = WebSocketEnd::HandshakeRefused, .code = 0, .httpStatus = static_cast<int32_t>(status)});
		}
		else
		{
			connection->End({.end = EndOf(tls_settings::ErrorOf(code)), .code = 0, .httpStatus = 0});
		}
		if (const auto logger = spdlog::get("game"))
		{
			SPDLOG_LOGGER_DEBUG(logger, "A WSS connection failed: {} (HTTP {})", curl_easy_strerror(code), status);
		}
		Release(*connection);
	}
}

void CurlWebSocketClient::Poll()
{
	MoveHandshakes();
	const auto now = Clock::now();
	for (auto& connection : _connections)
	{
		if (connection->finished)
		{
			continue;
		}
		if (connection->state == WebSocketState::Connecting &&
		    now - connection->started > websocket_settings::k_HandshakeTimeout)
		{
			connection->End({.end = WebSocketEnd::Timeout, .code = 0, .httpStatus = 0});
		}
		if (connection->state != WebSocketState::Open)
		{
			continue;
		}
		connection->Receive(_receiveBuffer);
		if (connection->state == WebSocketState::Open)
		{
			connection->KeepAlive();
		}
		if (connection->state == WebSocketState::Open && !connection->Flush())
		{
			connection->End({.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0});
		}
		if (connection->state == WebSocketState::None)
		{
			Release(*connection);
		}
	}
	Dispatch();
}

void CurlWebSocketClient::Dispatch()
{
	// A handler can connect, send or close, so the loop finds each connection again by its id before each event
	std::vector<WebSocketId> ids;
	ids.reserve(_connections.size());
	for (const auto& connection : _connections)
	{
		ids.push_back(connection->id);
	}
	for (const auto id : ids)
	{
		while (auto* connection = Find(id))
		{
			if (connection->events.empty())
			{
				break;
			}
			auto event = std::move(connection->events.front());
			connection->events.pop_front();
			if (std::holds_alternative<Opened>(event))
			{
				if (connection->handlers.opened)
				{
					connection->handlers.opened();
				}
			}
			else if (auto* message = std::get_if<Message>(&event))
			{
				if (message->text && connection->handlers.text)
				{
					connection->handlers.text({reinterpret_cast<const char*>(message->bytes.data()), message->bytes.size()});
				}
				else if (!message->text && connection->handlers.binary)
				{
					connection->handlers.binary(message->bytes);
				}
			}
			else
			{
				// The last handler of the connection. It runs from a copy, because the connection goes after it.
				auto closed = std::move(connection->handlers.closed);
				connection->finished = true;
				connection->handlers = {};
				connection->events.clear();
				Release(*connection);
				if (closed)
				{
					closed(std::get<WebSocketClosed>(event));
				}
				break;
			}
		}
	}
	std::erase_if(_connections, [](const auto& connection) { return connection->finished; });
}

WebSocketState CurlWebSocketClient::State(WebSocketId id) const
{
	const auto* connection = Find(id);
	return connection != nullptr ? connection->state : WebSocketState::None;
}

size_t CurlWebSocketClient::Count() const
{
	return static_cast<size_t>(
	    std::ranges::count_if(_connections, [](const auto& connection) { return connection->state != WebSocketState::None; }));
}

} // namespace openblack::online
