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
#include <mutex>
#include <string>
#include <thread>
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
/// The longest time that the thread of the sockets waits. A socket, a command or the stop of the client wakes it before.
/// The thread then also examines the times of the handshakes and of the silence of the servers.
constexpr int k_MostWaitMs = 100;

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

/// The part of a connection that the thread of the sockets keeps. Only that thread uses it.
struct Link
{
	struct Outgoing
	{
		std::vector<uint8_t> bytes;
		unsigned int flags {0};
		size_t sent {0};
	};

	WebSocketId id {0};
	std::unique_ptr<CURL, EasyDeleter> easy;
	std::unique_ptr<curl_slist, ListDeleter> headers;
	WebSocketState state {WebSocketState::Connecting};
	/// The easy handle is in the multi handle. After the handshake it stays there, because libcurl keeps the connection
	/// of a connect-only transfer only while the transfer is in its multi handle.
	bool inMulti {false};
	std::deque<Outgoing> outgoing;
	/// The parts of the data message that the thread receives now
	std::vector<uint8_t> message;
	bool messageIsText {false};
	/// The parts of the control frame that the thread receives now
	std::vector<uint8_t> control;
	Clock::time_point started {Clock::now()};
	Clock::time_point lastReceived {Clock::now()};
	Clock::time_point lastPing {Clock::now()};
	/// The events that the thread did not give to the game yet
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
				// The socket takes no more now. The thread sends the rest when the socket can take it.
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

/// The game gives a new connection to the thread of the sockets
struct StartCommand
{
	std::unique_ptr<Link> link;
};

struct SendCommand
{
	WebSocketId id {0};
	std::vector<uint8_t> bytes;
	unsigned int flags {0};
};

/// The game ends a connection. The thread sends a close frame with the code when the connection is open.
struct CloseCommand
{
	WebSocketId id {0};
	uint16_t code {websocket_close::k_Normal};
};

using Command = std::variant<StartCommand, SendCommand, CloseCommand>;

struct TakenEvent
{
	WebSocketId id {0};
	Event event;
};

} // namespace

/// The part of a connection that the game keeps. Only the thread of the game uses it.
struct CurlWebSocketClient::Connection
{
	WebSocketId id {0};
	WebSocketHandlers handlers;
	WebSocketState state {WebSocketState::Connecting};
	/// The connection ended. The client removes it after the handlers ran.
	bool finished {false};
	/// The events that Poll took from the thread of the sockets, and that the handlers did not get yet
	std::deque<Event> events;

	void End(WebSocketClosed closed)
	{
		if (state == WebSocketState::None)
		{
			return;
		}
		state = WebSocketState::None;
		events.emplace_back(closed);
	}
};

/// The thread of the sockets with its multi handle and its two queues. The mutex keeps the queues and the stop request.
/// All the other data is for the thread of the sockets only.
struct CurlWebSocketClient::Worker
{
	CURLM* multi {curl_multi_init()};
	std::mutex mutex;
	std::vector<Command> commands;
	std::vector<TakenEvent> events;
	bool stop {false};
	std::vector<std::unique_ptr<Link>> links;
	std::vector<uint8_t> receiveBuffer = std::vector<uint8_t>(k_ReceiveChunk);
	std::thread thread;

	Worker()
	{
		if (multi != nullptr)
		{
			thread = std::thread([this]() { Run(); });
		}
	}

	~Worker()
	{
		if (thread.joinable())
		{
			{
				const std::scoped_lock lock(mutex);
				stop = true;
			}
			curl_multi_wakeup(multi);
			thread.join();
		}
		if (multi != nullptr)
		{
			curl_multi_cleanup(multi);
		}
	}

	Worker(const Worker&) = delete;
	Worker& operator=(const Worker&) = delete;
	Worker(Worker&&) = delete;
	Worker& operator=(Worker&&) = delete;

	[[nodiscard]] bool Running() const { return thread.joinable(); }

	/// The thread of the game puts a command in the queue and wakes the thread of the sockets
	void Post(Command command)
	{
		{
			const std::scoped_lock lock(mutex);
			commands.push_back(std::move(command));
		}
		curl_multi_wakeup(multi);
	}

	/// The thread of the game takes all the events that the thread of the sockets gave, in their order
	std::vector<TakenEvent> Take()
	{
		std::vector<TakenEvent> taken;
		const std::scoped_lock lock(mutex);
		taken.swap(events);
		return taken;
	}

private:
	[[nodiscard]] Link* Find(WebSocketId id) const
	{
		const auto found = std::ranges::find_if(links, [id](const auto& link) { return link->id == id; });
		return found != links.end() ? found->get() : nullptr;
	}

	void Release(Link& link) const
	{
		if (link.inMulti)
		{
			curl_multi_remove_handle(multi, link.easy.get());
		}
		link.inMulti = false;
		link.easy.reset();
		link.headers.reset();
	}

	void Apply(Command& command)
	{
		if (auto* start = std::get_if<StartCommand>(&command))
		{
			auto& link = *start->link;
			if (curl_multi_add_handle(multi, link.easy.get()) != CURLM_OK)
			{
				link.End({.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0});
			}
			else
			{
				link.inMulti = true;
			}
			links.push_back(std::move(start->link));
		}
		else if (auto* send = std::get_if<SendCommand>(&command))
		{
			auto* link = Find(send->id);
			if (link == nullptr || link->state != WebSocketState::Open)
			{
				return;
			}
			link->outgoing.push_back({.bytes = std::move(send->bytes), .flags = send->flags, .sent = 0});
			// The message goes at once when the socket takes it
			if (!link->Flush())
			{
				link->End({.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0});
			}
		}
		else
		{
			const auto& close = std::get<CloseCommand>(command);
			auto* link = Find(close.id);
			if (link == nullptr)
			{
				return;
			}
			if (link->state == WebSocketState::Open)
			{
				link->Flush();
				link->SendClose(close.code);
			}
			Release(*link);
			std::erase_if(links, [id = close.id](const auto& item) { return item->id == id; });
		}
	}

	void MoveHandshakes()
	{
		if (std::ranges::none_of(links, [](const auto& link) { return link->state == WebSocketState::Connecting; }))
		{
			return;
		}
		int running = 0;
		curl_multi_perform(multi, &running);
		int queued = 0;
		while (CURLMsg* done = curl_multi_info_read(multi, &queued))
		{
			if (done->msg != CURLMSG_DONE)
			{
				continue;
			}
			Link* link = nullptr;
			curl_easy_getinfo(done->easy_handle, CURLINFO_PRIVATE, &link);
			if (link == nullptr || link->state != WebSocketState::Connecting)
			{
				continue;
			}
			const auto code = done->data.result;
			long status = 0;
			curl_easy_getinfo(done->easy_handle, CURLINFO_RESPONSE_CODE, &status);
			if (code == CURLE_OK && status == k_SwitchingProtocols)
			{
				link->state = WebSocketState::Open;
				link->lastReceived = Clock::now();
				link->lastPing = link->lastReceived;
				link->events.emplace_back(Opened {});
				continue;
			}
			if (status != 0 && status != k_SwitchingProtocols)
			{
				link->End({.end = WebSocketEnd::HandshakeRefused, .code = 0, .httpStatus = static_cast<int32_t>(status)});
			}
			else
			{
				link->End({.end = EndOf(tls_settings::ErrorOf(code)), .code = 0, .httpStatus = 0});
			}
			if (const auto logger = spdlog::get("game"))
			{
				SPDLOG_LOGGER_DEBUG(logger, "A WSS connection failed: {} (HTTP {})", curl_easy_strerror(code), status);
			}
			Release(*link);
		}
	}

	void MoveLinks()
	{
		const auto now = Clock::now();
		for (auto& link : links)
		{
			if (link->state == WebSocketState::Connecting && now - link->started > websocket_settings::k_HandshakeTimeout)
			{
				link->End({.end = WebSocketEnd::Timeout, .code = 0, .httpStatus = 0});
			}
			if (link->state == WebSocketState::Open)
			{
				link->Receive(receiveBuffer);
			}
			if (link->state == WebSocketState::Open)
			{
				link->KeepAlive();
			}
			if (link->state == WebSocketState::Open && !link->Flush())
			{
				link->End({.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0});
			}
			if (link->state == WebSocketState::None)
			{
				Release(*link);
			}
		}
	}

	/// Gives the new events to the thread of the game, and removes the connections that ended
	void Publish()
	{
		{
			const std::scoped_lock lock(mutex);
			for (auto& link : links)
			{
				for (auto& event : link->events)
				{
					events.push_back({.id = link->id, .event = std::move(event)});
				}
				link->events.clear();
			}
		}
		std::erase_if(links, [](const auto& link) { return link->state == WebSocketState::None; });
	}

	/// Waits until a socket of an open connection can read, or can write when data waits, or until a command comes
	void Wait()
	{
		std::vector<curl_waitfd> sockets;
		sockets.reserve(links.size());
		for (const auto& link : links)
		{
			if (link->state != WebSocketState::Open)
			{
				continue;
			}
			curl_socket_t socket = CURL_SOCKET_BAD;
			if (curl_easy_getinfo(link->easy.get(), CURLINFO_ACTIVESOCKET, &socket) != CURLE_OK || socket == CURL_SOCKET_BAD)
			{
				continue;
			}
			const auto wanted =
			    static_cast<short>(link->outgoing.empty() ? CURL_WAIT_POLLIN : CURL_WAIT_POLLIN | CURL_WAIT_POLLOUT);
			sockets.push_back({.fd = socket, .events = wanted, .revents = 0});
		}
		curl_multi_poll(multi, sockets.data(), static_cast<unsigned int>(sockets.size()), k_MostWaitMs, nullptr);
	}

	void Run()
	{
		std::vector<Command> taken;
		while (true)
		{
			{
				const std::scoped_lock lock(mutex);
				if (stop)
				{
					break;
				}
				taken.swap(commands);
			}
			for (auto& command : taken)
			{
				Apply(command);
			}
			taken.clear();
			MoveHandshakes();
			MoveLinks();
			Publish();
			Wait();
		}
		// The game stops: the client tells each server that it goes away
		for (auto& link : links)
		{
			if (link->state == WebSocketState::Open)
			{
				link->Flush();
				link->SendClose(websocket_close::k_GoingAway);
			}
			Release(*link);
		}
		links.clear();
	}
};

CurlWebSocketClient::CurlWebSocketClient(std::optional<std::string> extraAuthorities)
    : _extraAuthorities(std::move(extraAuthorities))
{
	tls_settings::StartLibrary();
}

CurlWebSocketClient::~CurlWebSocketClient() = default;

CurlWebSocketClient::Connection* CurlWebSocketClient::Find(WebSocketId id) const
{
	const auto found = std::ranges::find_if(
	    _connections, [id](const auto& connection) { return connection->id == id && !connection->finished; });
	return found != _connections.end() ? found->get() : nullptr;
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
	if (_worker == nullptr)
	{
		_worker = std::make_unique<Worker>();
	}
	auto link = std::make_unique<Link>();
	link->id = id;
	link->easy.reset(curl_easy_init());
	if (!link->easy || !_worker->Running())
	{
		connection->End({.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0});
		_connections.push_back(std::move(connection));
		return id;
	}

	auto* easy = link->easy.get();
	curl_easy_setopt(easy, CURLOPT_URL, request.url.c_str());
	// WSS only, and no redirection to another address
	curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "wss");
	curl_easy_setopt(easy, CURLOPT_REDIR_PROTOCOLS_STR, "wss");
	curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 0L);
	// The transfer stops after the upgrade, and the client then sends and receives the messages itself
	curl_easy_setopt(easy, CURLOPT_CONNECT_ONLY, 2L);
	tls_settings::Apply(easy, _extraAuthorities.has_value() ? _extraAuthorities->c_str() : nullptr);
	if (request.serverKey.has_value())
	{
		// A game server on the local network with a certificate of its own: no authority signs it and it names no host,
		// so the client accepts only the public key that the answer of the server gave (deviation 113)
		tls_settings::PinKey(easy, *request.serverKey);
	}
	curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, tls_settings::k_ConnectTimeoutMs);
	curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(easy, CURLOPT_USERAGENT, "openblack");
	curl_easy_setopt(easy, CURLOPT_PRIVATE, link.get());

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
	link->headers.reset(list);
	curl_easy_setopt(easy, CURLOPT_HTTPHEADER, list);

	_connections.push_back(std::move(connection));
	_worker->Post(StartCommand {.link = std::move(link)});
	return id;
}

bool CurlWebSocketClient::Queue(WebSocketId id, std::span<const uint8_t> bytes, unsigned int flags)
{
	const auto* connection = Find(id);
	if (connection == nullptr || connection->state != WebSocketState::Open)
	{
		return false;
	}
	// The thread of the sockets sends the message at once when the socket takes it, so a command does not wait for the
	// next frame
	_worker->Post(SendCommand {.id = id, .bytes = {bytes.begin(), bytes.end()}, .flags = flags});
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
	if (_worker != nullptr)
	{
		// The thread of the sockets sends the messages that wait, then the close frame when the connection is open
		_worker->Post(CloseCommand {.id = id, .code = code});
	}
	// The handlers stay until the client removes the connection, because this call can come from one of them
	connection->state = WebSocketState::None;
	connection->finished = true;
	connection->events.clear();
}

void CurlWebSocketClient::TakeEvents()
{
	if (_worker == nullptr)
	{
		return;
	}
	for (auto& [id, event] : _worker->Take())
	{
		auto* connection = Find(id);
		if (connection == nullptr)
		{
			// The game closed the connection before the event came
			continue;
		}
		if (std::holds_alternative<Opened>(event))
		{
			connection->state = WebSocketState::Open;
		}
		else if (std::holds_alternative<WebSocketClosed>(event))
		{
			connection->state = WebSocketState::None;
		}
		connection->events.push_back(std::move(event));
	}
}

void CurlWebSocketClient::Poll()
{
	TakeEvents();
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
