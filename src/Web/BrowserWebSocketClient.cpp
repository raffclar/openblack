/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BrowserWebSocketClient.h"

#include <algorithm>
#include <deque>
#include <string>
#include <utility>
#include <variant>

#include <emscripten/em_js.h>

// The sockets live in the worker thread of the game. Each socket keeps its events in a list until the game takes them.
// The kinds of the events: 1 open, 2 text message, 3 binary message, 4 error, 5 close.
// clang-format off
EM_JS(int, openblack_ws_open, (const char* path, const char* protocols), {
	if (location.protocol !== 'https:' || typeof WebSocket === 'undefined') {
		return 0;
	}
	const sockets = globalThis.openblackSockets || (globalThis.openblackSockets = {next: 1, open: {}});
	let socket;
	try {
		const list = UTF8ToString(protocols);
		socket = new WebSocket('wss://' + location.host + UTF8ToString(path), list ? list.split(',') : []);
	} catch (error) {
		return 0;
	}
	socket.binaryType = 'arraybuffer';
	const entry = {socket: socket, events: []};
	socket.onopen = () => entry.events.push({kind: 1});
	socket.onmessage = (event) => {
		if (typeof event.data === 'string') {
			entry.events.push({kind: 2, bytes: new TextEncoder().encode(event.data)});
		} else {
			entry.events.push({kind: 3, bytes: new Uint8Array(event.data)});
		}
	};
	socket.onerror = () => entry.events.push({kind: 4});
	socket.onclose = (event) => entry.events.push({kind: 5, code: event.code});
	const id = sockets.next++;
	sockets.open[id] = entry;
	return id;
});

EM_JS(int, openblack_ws_peek, (int id, int* value), {
	const entry = globalThis.openblackSockets && globalThis.openblackSockets.open[id];
	if (!entry || entry.events.length === 0) {
		return 0;
	}
	const event = entry.events[0];
	HEAP32[value >> 2] = event.kind === 5 ? event.code : (event.bytes ? event.bytes.length : 0);
	return event.kind;
});

EM_JS(void, openblack_ws_pop, (int id, uint8_t* buffer), {
	const entry = globalThis.openblackSockets && globalThis.openblackSockets.open[id];
	if (!entry || entry.events.length === 0) {
		return;
	}
	const event = entry.events.shift();
	if (event.bytes && buffer) {
		HEAPU8.set(event.bytes, buffer);
	}
});

EM_JS(int, openblack_ws_send, (int id, const uint8_t* data, double length, int text), {
	const entry = globalThis.openblackSockets && globalThis.openblackSockets.open[id];
	if (!entry || entry.socket.readyState !== 1) {
		return 0;
	}
	// A copy, because the socket cannot send a view of the shared memory of the threads
	const bytes = HEAPU8.slice(data, data + length);
	entry.socket.send(text ? new TextDecoder().decode(bytes) : bytes);
	return 1;
});

EM_JS(void, openblack_ws_close, (int id, int code), {
	const entry = globalThis.openblackSockets && globalThis.openblackSockets.open[id];
	if (!entry) {
		return;
	}
	delete globalThis.openblackSockets.open[id];
	entry.socket.onopen = entry.socket.onmessage = entry.socket.onerror = entry.socket.onclose = null;
	// A browser sends only the code 1000 or a code from 3000 to 4999
	const sent = code === 1000 || (code >= 3000 && code <= 4999) ? code : 1000;
	try {
		entry.socket.close(sent);
	} catch (error) {
	}
});
EM_JS_DEPS(openblack_websocket, "$UTF8ToString");
// clang-format on

namespace openblack::web
{

using online::WebSocketClosed;
using online::WebSocketEnd;
using online::WebSocketState;

namespace
{

/// The largest message that the client accepts, as on the desktop
constexpr size_t k_MostMessageBytes = 16 * 1024 * 1024;
/// The close code of a connection that stopped with no close frame
constexpr uint16_t k_Abnormal = 1006;

enum class EventKind : int
{
	None = 0,
	Open = 1,
	Text = 2,
	Binary = 3,
	Error = 4,
	Close = 5,
};

struct Opened
{
};

struct Message
{
	bool text {false};
	std::vector<uint8_t> bytes;
};

using Event = std::variant<Opened, Message, WebSocketClosed>;

} // namespace

struct BrowserWebSocketClient::Connection
{
	online::WebSocketId id {0};
	/// The number of the socket in the worker, 0 when there is none
	int socket {0};
	online::WebSocketHandlers handlers;
	WebSocketState state {WebSocketState::Connecting};
	bool finished {false};
	/// The browser told of an error before the close
	bool failed {false};
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

BrowserWebSocketClient::BrowserWebSocketClient() = default;

BrowserWebSocketClient::~BrowserWebSocketClient()
{
	for (auto& connection : _connections)
	{
		Release(*connection);
	}
}

BrowserWebSocketClient::Connection* BrowserWebSocketClient::Find(online::WebSocketId id) const
{
	const auto found = std::ranges::find_if(
	    _connections, [id](const auto& connection) { return connection->id == id && !connection->finished; });
	return found != _connections.end() ? found->get() : nullptr;
}

void BrowserWebSocketClient::Release(Connection& connection)
{
	if (connection.socket != 0)
	{
		openblack_ws_close(connection.socket, online::websocket_close::k_Normal);
		connection.socket = 0;
	}
}

online::WebSocketId BrowserWebSocketClient::Connect(online::WebSocketRequest request, online::WebSocketHandlers handlers)
{
	auto connection = std::make_unique<Connection>();
	connection->id = _nextId++;
	connection->handlers = std::move(handlers);
	const auto id = connection->id;

	std::string protocols;
	for (const auto& protocol : request.subprotocols)
	{
		protocols += protocols.empty() ? protocol : "," + protocol;
	}
	if (online::http_policy::IsSameOriginPath(request.url))
	{
		connection->socket = openblack_ws_open(request.url.c_str(), protocols.c_str());
	}
	if (connection->socket == 0)
	{
		// Not a path of the origin, a page that is not an HTTPS page, or a browser with no WebSocket
		connection->End({.end = WebSocketEnd::AddressRefused, .code = 0, .httpStatus = 0});
	}
	_connections.push_back(std::move(connection));
	return id;
}

bool BrowserWebSocketClient::SendText(online::WebSocketId id, std::string_view text)
{
	auto* connection = Find(id);
	if (connection == nullptr || connection->state != WebSocketState::Open)
	{
		return false;
	}
	return openblack_ws_send(connection->socket, reinterpret_cast<const uint8_t*>(text.data()),
	                         static_cast<double>(text.size()), 1) != 0;
}

bool BrowserWebSocketClient::SendBinary(online::WebSocketId id, std::span<const uint8_t> bytes)
{
	auto* connection = Find(id);
	if (connection == nullptr || connection->state != WebSocketState::Open)
	{
		return false;
	}
	return openblack_ws_send(connection->socket, bytes.data(), static_cast<double>(bytes.size()), 0) != 0;
}

void BrowserWebSocketClient::Close(online::WebSocketId id, uint16_t code)
{
	auto* connection = Find(id);
	if (connection == nullptr)
	{
		return;
	}
	if (connection->socket != 0)
	{
		openblack_ws_close(connection->socket, code);
		connection->socket = 0;
	}
	// The handlers stay until the client removes the connection, because this call can come from one of them
	connection->state = WebSocketState::None;
	connection->finished = true;
	connection->events.clear();
}

void BrowserWebSocketClient::Take(Connection& connection)
{
	while (connection.socket != 0 && connection.state != WebSocketState::None)
	{
		int value = 0;
		const auto kind = static_cast<EventKind>(openblack_ws_peek(connection.socket, &value));
		if (kind == EventKind::None)
		{
			return;
		}
		switch (kind)
		{
		case EventKind::Open:
			openblack_ws_pop(connection.socket, nullptr);
			connection.state = WebSocketState::Open;
			connection.events.emplace_back(Opened {});
			break;
		case EventKind::Text:
		case EventKind::Binary:
		{
			const auto size = static_cast<size_t>(value);
			if (size > k_MostMessageBytes)
			{
				openblack_ws_pop(connection.socket, nullptr);
				connection.End({.end = WebSocketEnd::TooLarge, .code = 0, .httpStatus = 0});
				Release(connection);
				return;
			}
			Message message {.text = kind == EventKind::Text, .bytes = std::vector<uint8_t>(size)};
			openblack_ws_pop(connection.socket, message.bytes.data());
			connection.events.emplace_back(std::move(message));
			break;
		}
		case EventKind::Error:
			openblack_ws_pop(connection.socket, nullptr);
			connection.failed = true;
			break;
		case EventKind::Close:
		{
			openblack_ws_pop(connection.socket, nullptr);
			const auto code = static_cast<uint16_t>(value);
			// The browser does not tell the HTTP status of a refused handshake, or why a connection failed
			if (code == k_Abnormal || connection.failed || connection.state == WebSocketState::Connecting)
			{
				connection.End({.end = WebSocketEnd::Network, .code = 0, .httpStatus = 0});
			}
			else
			{
				connection.End({.end = WebSocketEnd::ClosedByServer, .code = code, .httpStatus = 0});
			}
			Release(connection);
			return;
		}
		case EventKind::None:
			return;
		}
	}
}

void BrowserWebSocketClient::Poll()
{
	for (auto& connection : _connections)
	{
		if (!connection->finished)
		{
			Take(*connection);
		}
	}
	Dispatch();
}

void BrowserWebSocketClient::Dispatch()
{
	// A handler can connect, send or close, so the loop finds each connection again by its id before each event
	std::vector<online::WebSocketId> ids;
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

online::WebSocketState BrowserWebSocketClient::State(online::WebSocketId id) const
{
	const auto* connection = Find(id);
	return connection != nullptr ? connection->state : WebSocketState::None;
}

size_t BrowserWebSocketClient::Count() const
{
	return static_cast<size_t>(
	    std::ranges::count_if(_connections, [](const auto& connection) { return connection->state != WebSocketState::None; }));
}

} // namespace openblack::web
