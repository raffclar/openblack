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

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "Online/WebSocketInterface.h"

/// A WSS client for the tests. The test plays the server: it accepts or refuses each connection, and sends messages and
/// close frames. The handlers run from Poll, as in the real clients.
namespace openblack::test_fakes
{

class FakeWebSocketClient final: public online::WebSocketClientInterface
{
public:
	struct Sent
	{
		bool text {false};
		std::vector<uint8_t> bytes;

		[[nodiscard]] std::string Text() const { return {bytes.begin(), bytes.end()}; }
	};

	struct Socket
	{
		online::WebSocketRequest request;
		online::WebSocketHandlers handlers;
		online::WebSocketState state {online::WebSocketState::Connecting};
		std::vector<Sent> sent;
		/// The close code of the client, when the client closed the connection
		std::optional<uint16_t> closedWith;
	};

	online::WebSocketId Connect(online::WebSocketRequest request, online::WebSocketHandlers handlers) override
	{
		const auto id = _nextId++;
		sockets[id] = Socket {.request = std::move(request), .handlers = std::move(handlers), .sent = {}, .closedWith = {}};
		order.push_back(id);
		return id;
	}

	bool SendText(online::WebSocketId id, std::string_view text) override
	{
		return Record(id, true, {reinterpret_cast<const uint8_t*>(text.data()), text.size()});
	}

	bool SendBinary(online::WebSocketId id, std::span<const uint8_t> bytes) override { return Record(id, false, bytes); }

	void Close(online::WebSocketId id, uint16_t code) override
	{
		auto found = sockets.find(id);
		if (found == sockets.end() || found->second.state == online::WebSocketState::None)
		{
			return;
		}
		found->second.state = online::WebSocketState::None;
		found->second.closedWith = code;
		std::erase_if(_events, [id](const auto& event) { return event.first == id; });
	}

	void Poll() override
	{
		// A handler can add events, which then wait for the next Poll
		auto events = std::move(_events);
		_events.clear();
		for (auto& [id, event] : events)
		{
			auto found = sockets.find(id);
			if (found == sockets.end() || found->second.state == online::WebSocketState::None)
			{
				continue;
			}
			auto& socket = found->second;
			if (std::holds_alternative<std::monostate>(event))
			{
				socket.state = online::WebSocketState::Open;
				if (socket.handlers.opened)
				{
					socket.handlers.opened();
				}
			}
			else if (auto* sent = std::get_if<Sent>(&event))
			{
				if (sent->text && socket.handlers.text)
				{
					socket.handlers.text(sent->Text());
				}
				else if (!sent->text && socket.handlers.binary)
				{
					socket.handlers.binary(sent->bytes);
				}
			}
			else
			{
				socket.state = online::WebSocketState::None;
				auto closed = std::move(socket.handlers.closed);
				socket.handlers = {};
				if (closed)
				{
					closed(std::get<online::WebSocketClosed>(event));
				}
			}
		}
	}

	[[nodiscard]] online::WebSocketState State(online::WebSocketId id) const override
	{
		const auto found = sockets.find(id);
		return found != sockets.end() ? found->second.state : online::WebSocketState::None;
	}

	[[nodiscard]] size_t Count() const override
	{
		return static_cast<size_t>(std::ranges::count_if(
		    sockets, [](const auto& entry) { return entry.second.state != online::WebSocketState::None; }));
	}

	/// The server accepts the handshake. The opened handler runs at the next Poll.
	void Accept(online::WebSocketId id) { _events.emplace_back(id, std::monostate {}); }
	/// The server sends a text message
	void ServerText(online::WebSocketId id, std::string_view text)
	{
		_events.emplace_back(id, Sent {.text = true, .bytes = {text.begin(), text.end()}});
	}
	/// The server sends a binary message
	void ServerBinary(online::WebSocketId id, std::span<const uint8_t> bytes)
	{
		_events.emplace_back(id, Sent {.text = false, .bytes = {bytes.begin(), bytes.end()}});
	}
	/// The connection ends
	void End(online::WebSocketId id, online::WebSocketClosed closed) { _events.emplace_back(id, closed); }

	/// The last connection that the client started
	[[nodiscard]] online::WebSocketId Last() const { return order.back(); }
	/// The messages that the client sent on a connection, and removes them
	std::vector<Sent> TakeSent(online::WebSocketId id) { return std::exchange(sockets[id].sent, {}); }

	std::map<online::WebSocketId, Socket> sockets;
	std::vector<online::WebSocketId> order;

private:
	using Event = std::variant<std::monostate, Sent, online::WebSocketClosed>;

	bool Record(online::WebSocketId id, bool text, std::span<const uint8_t> bytes)
	{
		auto found = sockets.find(id);
		if (found == sockets.end() || found->second.state != online::WebSocketState::Open)
		{
			return false;
		}
		found->second.sent.push_back({.text = text, .bytes = {bytes.begin(), bytes.end()}});
		return true;
	}

	std::deque<std::pair<online::WebSocketId, Event>> _events;
	online::WebSocketId _nextId {1};
};

} // namespace openblack::test_fakes
