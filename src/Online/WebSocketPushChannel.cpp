/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WebSocketPushChannel.h"

#include <utility>

namespace openblack::online
{

namespace push_channel
{

WebSocketRequest Handshake(std::string url, std::string_view accessToken, PushAuth auth)
{
	WebSocketRequest request {.url = std::move(url), .headers = {}, .subprotocols = {}};
	switch (auth)
	{
	case PushAuth::Header:
		request.headers.emplace_back("Authorization", "Bearer " + std::string(accessToken));
		break;
	case PushAuth::Subprotocol:
		request.subprotocols.emplace_back(k_Subprotocol);
		request.subprotocols.push_back(std::string(k_BearerPrefix) + std::string(accessToken));
		break;
	}
	return request;
}

PushClose CloseReason(const WebSocketClosed& closed) noexcept
{
	switch (closed.end)
	{
	case WebSocketEnd::ClosedByServer:
		if (closed.code == k_CloseSignedOut)
		{
			return PushClose::SignedOut;
		}
		if (closed.code == k_CloseBanned)
		{
			return PushClose::Banned;
		}
		return PushClose::Lost;
	case WebSocketEnd::HandshakeRefused:
		// Only a 401 tells that the access token is the problem. The lobby refuses too many sockets or a wrong origin
		// with other codes, and a later attempt can work.
		return closed.httpStatus == k_StatusUnauthorized ? PushClose::Refused : PushClose::Lost;
	case WebSocketEnd::AddressRefused:
		// The address is wrong for this build. Another attempt cannot work.
		return PushClose::Unavailable;
	case WebSocketEnd::Network:
	case WebSocketEnd::Tls:
	case WebSocketEnd::Timeout:
	case WebSocketEnd::TooLarge:
		return PushClose::Lost;
	}
	return PushClose::Lost;
}

} // namespace push_channel

WebSocketPushChannel::WebSocketPushChannel(WebSocketClientInterface& sockets, PushAuth auth)
    : _sockets(sockets)
    , _auth(auth)
{
}

WebSocketPushChannel::~WebSocketPushChannel()
{
	Close();
}

void WebSocketPushChannel::Open(std::string url, std::string accessToken, PushHandlers handlers)
{
	Close();
	auto request = push_channel::Handshake(std::move(url), accessToken, _auth);
	WebSocketHandlers socketHandlers {
	    .opened = std::move(handlers.opened),
	    .text = std::move(handlers.message),
	    // The lobby sends only text frames
	    .binary = {},
	    .closed =
	        [this, closed = std::move(handlers.closed)](const WebSocketClosed& end) {
		        _connection.reset();
		        if (closed)
		        {
			        closed(push_channel::CloseReason(end));
		        }
	        },
	};
	_connection = _sockets.Connect(std::move(request), std::move(socketHandlers));
}

void WebSocketPushChannel::Send(std::string_view text)
{
	if (_connection.has_value())
	{
		_sockets.SendText(*_connection, text);
	}
}

void WebSocketPushChannel::Close()
{
	if (_connection.has_value())
	{
		_sockets.Close(*_connection, websocket_close::k_Normal);
		_connection.reset();
	}
}

void WebSocketPushChannel::Poll()
{
	// The transport runs the handlers in its own Poll, which the game calls before the lobby client moves on
}

bool WebSocketPushChannel::IsOpen() const
{
	return _connection.has_value() && _sockets.State(*_connection) == WebSocketState::Open;
}

} // namespace openblack::online
