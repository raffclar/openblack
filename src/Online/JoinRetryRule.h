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

#include "Online/WebSocketInterface.h"

/// The rule for a first join whose socket does not open. The lobby gives a pool server to the game before the server
/// listens. The proxy in front of the server then refuses the handshake for some seconds. The client tries again
/// in that time, and stops at a refusal that the server itself gives.
namespace openblack::online::join_retry
{

/// The HTTP statuses that the proxy gives while its game server does not listen
constexpr int32_t k_BadGateway = 502;
constexpr int32_t k_Unavailable = 503;
constexpr int32_t k_GatewayTimeout = 504;

/// Whether the end of the socket can come from a server that does not listen yet
[[nodiscard]] constexpr bool ServerNotListening(const WebSocketClosed& closed)
{
	switch (closed.end)
	{
	case WebSocketEnd::Network:
	case WebSocketEnd::Timeout:
		return true;
	case WebSocketEnd::HandshakeRefused:
		// The browser cannot read the status (0)
		return closed.httpStatus == 0 || closed.httpStatus == k_BadGateway || closed.httpStatus == k_Unavailable ||
		       closed.httpStatus == k_GatewayTimeout;
	default:
		return false;
	}
}

/// Whether the client tries again: the socket did not open, and the time to try again did not end
[[nodiscard]] constexpr bool TryAgain(const WebSocketClosed& closed, int64_t sinceJoinMicroseconds, int64_t limitMicroseconds)
{
	return ServerNotListening(closed) && sinceJoinMicroseconds < limitMicroseconds;
}

} // namespace openblack::online::join_retry
