/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TurnStream.h"

#include <utility>

namespace openblack::online
{

TurnStream::TurnStream(WebSocketClientInterface& sockets, MicrosecondClock clock)
    : _sockets(sockets)
    , _clock(std::move(clock))
{
}

TurnStream::~TurnStream()
{
	Close();
}

void TurnStream::Open(WebSocketRequest request, TurnStreamHandlers handlers)
{
	Close();
	_handlers = std::move(handlers);
	_nextSequence = 0;
	_roundTrip.reset();
	_connection =
	    _sockets.Connect(std::move(request), WebSocketHandlers {
	                                             .opened =
	                                                 [this]() {
		                                                 if (_handlers.opened)
		                                                 {
			                                                 _handlers.opened();
		                                                 }
	                                                 },
	                                             .text =
	                                                 [this](std::string_view text) {
		                                                 if (_handlers.text)
		                                                 {
			                                                 _handlers.text(text);
		                                                 }
	                                                 },
	                                             .binary = [this](std::span<const uint8_t> bytes) { Received(bytes); },
	                                             .closed =
	                                                 [this](const WebSocketClosed& closed) {
		                                                 _connection.reset();
		                                                 auto handler = std::move(_handlers.closed);
		                                                 _handlers = {};
		                                                 if (handler)
		                                                 {
			                                                 handler(closed);
		                                                 }
	                                                 },
	                                         });
}

void TurnStream::Close()
{
	if (_connection.has_value())
	{
		_sockets.Close(*_connection, websocket_close::k_Normal);
		_connection.reset();
	}
	_handlers = {};
}

bool TurnStream::IsOpen() const
{
	return _connection.has_value() && _sockets.State(*_connection) == WebSocketState::Open;
}

bool TurnStream::SendFrame(const turn_codec::Frame& frame)
{
	if (!IsOpen())
	{
		return false;
	}
	_buffer.clear();
	if (!turn_codec::Append(frame, _buffer))
	{
		return false;
	}
	return _sockets.SendBinary(*_connection, _buffer);
}

bool TurnStream::SendCommands(uint32_t turn, std::span<const player_commands::Action> actions)
{
	const turn_codec::CommandBlock block {
	    .sequence = _nextSequence,
	    .turn = turn,
	    .actions = {actions.begin(), actions.end()},
	};
	if (!SendFrame(block))
	{
		return false;
	}
	++_nextSequence;
	return true;
}

bool TurnStream::SendChecksum(uint32_t turn, uint64_t checksum)
{
	return SendFrame(turn_codec::TurnChecksum {.turn = turn, .checksum = checksum});
}

bool TurnStream::SendPing()
{
	return SendFrame(turn_codec::Ping {.id = _nextPing++, .time = _clock()});
}

bool TurnStream::SendText(std::string_view text)
{
	return IsOpen() && _sockets.SendText(*_connection, text);
}

void TurnStream::Received(std::span<const uint8_t> bytes)
{
	auto decoded = turn_codec::Decode(bytes);
	if (!decoded.frame.has_value())
	{
		++_rejected;
		return;
	}
	auto& frame = *decoded.frame;
	if (const auto* turn = std::get_if<turn_codec::TurnBundle>(&frame))
	{
		if (_handlers.turn)
		{
			_handlers.turn(*turn);
		}
	}
	else if (const auto* ping = std::get_if<turn_codec::Ping>(&frame))
	{
		// The server measures the round trip of each client with its own pings
		SendFrame(turn_codec::Pong {.id = ping->id, .time = ping->time});
	}
	else if (const auto* pong = std::get_if<turn_codec::Pong>(&frame))
	{
		const auto now = _clock();
		if (now >= pong->time)
		{
			_roundTrip = now - pong->time;
		}
	}
	else
	{
		// Commands and checksums go only from a client to the server
		++_rejected;
	}
}

} // namespace openblack::online
