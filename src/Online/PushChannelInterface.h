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

#include <functional>
#include <string>
#include <string_view>
#include <utility>

/// The push channel of the lobby: one WSS connection, on which the lobby sends JSON events to the client. The client
/// keeps it open during play. The client sends only a few frames on it, and does each action with a REST call.
namespace openblack::online
{

/// Why the channel closed
enum class PushClose : uint8_t
{
	/// The game closed it
	Closed,
	/// The connection failed or stopped. The client can open it again.
	Lost,
	/// The lobby refused the access token
	Refused,
	/// This build has no transport for the channel. The client does not try again.
	Unavailable,
};

struct PushHandlers
{
	std::function<void()> opened;
	/// A text frame from the lobby
	std::function<void(std::string_view text)> message;
	std::function<void(PushClose reason)> closed;
};

class PushChannelInterface
{
public:
	virtual ~PushChannelInterface() = default;

	/// Opens the channel to a wss:// address, or on the browser to a path of the origin of the page. The access token
	/// goes with the opening handshake. The handlers run from Poll, never in Open.
	virtual void Open(std::string url, std::string accessToken, PushHandlers handlers) = 0;
	/// Sends a text frame. Nothing happens when the channel is not open.
	virtual void Send(std::string_view text) = 0;
	/// Closes the channel. The closed handler runs with Closed.
	virtual void Close() = 0;
	/// Once a frame: moves the connection on without a wait, and runs the handlers
	virtual void Poll() = 0;
	[[nodiscard]] virtual bool IsOpen() const = 0;
};

/// The channel of a build with no WSS transport. Each open closes with Unavailable at the next Poll.
class NoPushChannel final: public PushChannelInterface
{
public:
	void Open(std::string /*url*/, std::string /*accessToken*/, PushHandlers handlers) override
	{
		_handlers = std::move(handlers);
		_closing = true;
	}
	void Send(std::string_view /*text*/) override {}
	void Close() override {}
	void Poll() override
	{
		if (!_closing)
		{
			return;
		}
		_closing = false;
		auto handlers = std::move(_handlers);
		_handlers = {};
		if (handlers.closed)
		{
			handlers.closed(PushClose::Unavailable);
		}
	}
	[[nodiscard]] bool IsOpen() const override { return false; }

private:
	PushHandlers _handlers;
	bool _closing {false};
};

} // namespace openblack::online
