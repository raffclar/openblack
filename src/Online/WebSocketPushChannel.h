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

#include <optional>
#include <string>
#include <string_view>

#include "Online/PushChannelInterface.h"
#include "Online/WebSocketInterface.h"

namespace openblack::online
{

/// How the push channel gives the access token to the lobby in the handshake
enum class PushAuth : uint8_t
{
	/// The header "Authorization: Bearer <token>" (desktop)
	Header,
	/// The subprotocols "bwgame.v1" and "bearer.<token>", because a browser cannot send headers
	Subprotocol,
};

/// The rules of the push channel of bwgame-service
namespace push_channel
{
/// The subprotocol of version 1 of the channel
constexpr std::string_view k_Subprotocol = "bwgame.v1";
/// The start of the subprotocol that holds the access token
constexpr std::string_view k_BearerPrefix = "bearer.";
/// The close code when the lobby ended the session, or the account is no longer active
constexpr uint16_t k_CloseSignedOut = 4001;
/// The close code when the lobby banned the account
constexpr uint16_t k_CloseBanned = 4003;
/// The HTTP status of a handshake with no access token or an access token that is not valid
constexpr int32_t k_StatusUnauthorized = 401;

/// The handshake of a channel to an address with an access token
[[nodiscard]] WebSocketRequest Handshake(std::string url, std::string_view accessToken, PushAuth auth);
/// Why the channel closed, from how its connection ended
[[nodiscard]] PushClose CloseReason(const WebSocketClosed& closed) noexcept;
} // namespace push_channel

/// The push channel of the lobby on a WSS connection. The lobby client does the protocol of the channel (hello,
/// subscribe, resume and resync). This class opens the connection with the access token, gives the text frames to the
/// client, and tells the client why the connection ended.
class WebSocketPushChannel final: public PushChannelInterface
{
public:
	WebSocketPushChannel(WebSocketClientInterface& sockets, PushAuth auth);
	~WebSocketPushChannel() override;
	WebSocketPushChannel(const WebSocketPushChannel&) = delete;
	WebSocketPushChannel& operator=(const WebSocketPushChannel&) = delete;
	WebSocketPushChannel(WebSocketPushChannel&&) = delete;
	WebSocketPushChannel& operator=(WebSocketPushChannel&&) = delete;

	void Open(std::string url, std::string accessToken, PushHandlers handlers) override;
	void Send(std::string_view text) override;
	void Close() override;
	void Poll() override;
	[[nodiscard]] bool IsOpen() const override;

private:
	WebSocketClientInterface& _sockets;
	PushAuth _auth;
	std::optional<WebSocketId> _connection;
};

} // namespace openblack::online
