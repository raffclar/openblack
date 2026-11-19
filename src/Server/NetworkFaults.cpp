/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "NetworkFaults.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace openblack::server
{

FaultyWebSocketServer::FaultyWebSocketServer(WebSocketServerInterface& sockets, NetworkFaults faults, Clock clock)
    : _sockets(sockets)
    , _faults(faults)
    , _clock(std::move(clock))
    , _random(faults.seed)
{
}

void FaultyWebSocketServer::SetHandlers(ServerSocketHandlers handlers)
{
	if (!_faults.Any())
	{
		_sockets.SetHandlers(std::move(handlers));
		return;
	}
	_handlers = std::move(handlers);
	_sockets.SetHandlers(ServerSocketHandlers {
	    .opened =
	        [this](ConnectionId connection) {
		        _open.insert(connection);
		        if (_handlers.opened)
		        {
			        _handlers.opened(connection);
		        }
	        },
	    .text =
	        [this](ConnectionId connection, std::string_view text) {
		        Push(_incoming, connection, Kind::Text, std::span(reinterpret_cast<const uint8_t*>(text.data()), text.size()),
		             0);
	        },
	    .binary = [this](ConnectionId connection,
	                     std::span<const uint8_t> bytes) { Push(_incoming, connection, Kind::Binary, bytes, 0); },
	    .closed = [this](ConnectionId connection,
	                     uint16_t code) { Push(_incoming, connection, Kind::Close, std::span<const uint8_t> {}, code); },
	});
}

bool FaultyWebSocketServer::SendText(ConnectionId connection, std::string_view text)
{
	if (!_faults.Any())
	{
		return _sockets.SendText(connection, text);
	}
	if (!_open.contains(connection))
	{
		return false;
	}
	Push(_outgoing, connection, Kind::Text, std::span(reinterpret_cast<const uint8_t*>(text.data()), text.size()), 0);
	return true;
}

bool FaultyWebSocketServer::SendBinary(ConnectionId connection, std::span<const uint8_t> bytes)
{
	if (!_faults.Any())
	{
		return _sockets.SendBinary(connection, bytes);
	}
	if (!_open.contains(connection))
	{
		return false;
	}
	Push(_outgoing, connection, Kind::Binary, bytes, 0);
	return true;
}

void FaultyWebSocketServer::Close(ConnectionId connection, uint16_t code)
{
	if (!_faults.Any())
	{
		_sockets.Close(connection, code);
		return;
	}
	// No handler of the connection runs after the close, and the close goes after the messages before it
	_open.erase(connection);
	_incoming.erase(connection);
	Push(_outgoing, connection, Kind::Close, std::span<const uint8_t> {}, code);
}

void FaultyWebSocketServer::Poll()
{
	_sockets.Poll();
	if (!_faults.Any())
	{
		return;
	}
	const auto now = _clock();
	DeliverIncoming(now);
	SendOutgoing(now);
}

int64_t FaultyWebSocketServer::DueTime(Queue& queue)
{
	uint32_t losses = 0;
	if (_faults.lossPercent > 0)
	{
		std::uniform_int_distribution<uint32_t> percent(0, 99);
		while (losses < k_MostLossesInARow && percent(_random) < _faults.lossPercent)
		{
			++losses;
		}
	}
	_losses += losses;
	const auto due = _clock() + (static_cast<int64_t>(_faults.delayMilliseconds) * 1'000) + ResendMicroseconds(losses);
	// A message never passes the message before it
	queue.lastDue = std::max(queue.lastDue, due);
	return queue.lastDue;
}

void FaultyWebSocketServer::Push(std::map<ConnectionId, Queue>& queues, ConnectionId connection, Kind kind,
                                 std::span<const uint8_t> bytes, uint16_t code)
{
	auto& queue = queues[connection];
	queue.messages.push_back(
	    Message {.due = DueTime(queue), .kind = kind, .bytes = {bytes.begin(), bytes.end()}, .code = code});
}

void FaultyWebSocketServer::DeliverIncoming(int64_t now)
{
	// The handlers can send and close, so the messages that are due leave their queues first
	std::vector<std::pair<ConnectionId, Message>> due;
	for (auto& [connection, queue] : _incoming)
	{
		while (!queue.messages.empty() && queue.messages.front().due <= now)
		{
			due.emplace_back(connection, std::move(queue.messages.front()));
			queue.messages.pop_front();
		}
	}
	std::erase_if(_incoming, [](const auto& entry) { return entry.second.messages.empty(); });
	for (auto& [connection, message] : due)
	{
		if (!_open.contains(connection))
		{
			continue;
		}
		switch (message.kind)
		{
		case Kind::Text:
			if (_handlers.text)
			{
				_handlers.text(connection,
				               std::string_view(reinterpret_cast<const char*>(message.bytes.data()), message.bytes.size()));
			}
			break;
		case Kind::Binary:
			if (_handlers.binary)
			{
				_handlers.binary(connection, message.bytes);
			}
			break;
		case Kind::Close:
			_open.erase(connection);
			_outgoing.erase(connection);
			if (_handlers.closed)
			{
				_handlers.closed(connection, message.code);
			}
			break;
		}
	}
}

void FaultyWebSocketServer::SendOutgoing(int64_t now)
{
	for (auto it = _outgoing.begin(); it != _outgoing.end();)
	{
		const auto connection = it->first;
		auto& messages = it->second.messages;
		bool closed = false;
		while (!messages.empty() && messages.front().due <= now && !closed)
		{
			const auto& message = messages.front();
			switch (message.kind)
			{
			case Kind::Text:
				_sockets.SendText(connection,
				                  std::string_view(reinterpret_cast<const char*>(message.bytes.data()), message.bytes.size()));
				break;
			case Kind::Binary:
				_sockets.SendBinary(connection, message.bytes);
				break;
			case Kind::Close:
				_sockets.Close(connection, message.code);
				closed = true;
				break;
			}
			messages.pop_front();
		}
		it = closed || messages.empty() ? _outgoing.erase(it) : std::next(it);
	}
}

} // namespace openblack::server
