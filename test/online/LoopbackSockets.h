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

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Online/WebSocketInterface.h"
#include "Server/WebSocketServerInterface.h"

/// The sockets of a game server and of its clients in one process, for the tests. Each message waits until the side
/// that gets it polls. A test can cut a connection, as a network that stops.
namespace openblack::test_fakes
{

class LoopbackSockets final: public server::WebSocketServerInterface
{
public:
	struct Message
	{
		bool text {false};
		std::vector<uint8_t> bytes;
	};

	struct Connection
	{
		online::WebSocketHandlers client;
		bool serverKnows {false};
		bool clientOpen {false};
		bool ended {false};
		std::deque<Message> toServer;
		std::deque<Message> toClient;
		/// The close code that the server sent, which the client gets at its next poll
		std::optional<uint16_t> serverClosed;
		/// The client closed, or the network cut the connection: the server gets the code at its next poll
		std::optional<uint16_t> clientClosed;
		bool cutForClient {false};
	};

	/// The WSS client of one computer
	class Client final: public online::WebSocketClientInterface
	{
	public:
		explicit Client(LoopbackSockets& network)
		    : _network(network)
		{
		}

		online::WebSocketId Connect(online::WebSocketRequest /*request*/, online::WebSocketHandlers handlers) override
		{
			const auto id = _network._nextId++;
			auto& connection = _network._connections[id];
			connection.client = std::move(handlers);
			_ids.push_back(id);
			return id;
		}
		bool SendText(online::WebSocketId id, std::string_view text) override
		{
			return Push(id, true, {reinterpret_cast<const uint8_t*>(text.data()), text.size()});
		}
		bool SendBinary(online::WebSocketId id, std::span<const uint8_t> bytes) override { return Push(id, false, bytes); }
		void Close(online::WebSocketId id, uint16_t code) override
		{
			auto* connection = _network.Find(id);
			if (connection == nullptr || connection->ended)
			{
				return;
			}
			connection->ended = true;
			connection->client = {};
			connection->clientClosed = code;
		}
		void Poll() override
		{
			for (const auto id : std::vector(_ids))
			{
				auto* connection = _network.Find(id);
				if (connection == nullptr || connection->ended)
				{
					continue;
				}
				if (!connection->cutForClient)
				{
					if (!connection->clientOpen && (connection->serverKnows || connection->serverClosed.has_value()))
					{
						connection->clientOpen = true;
						if (connection->client.opened)
						{
							connection->client.opened();
						}
					}
					while (connection != nullptr && connection->clientOpen && !connection->toClient.empty() &&
					       !connection->ended)
					{
						auto message = std::move(connection->toClient.front());
						connection->toClient.pop_front();
						if (message.text && connection->client.text)
						{
							connection->client.text(
							    {reinterpret_cast<const char*>(message.bytes.data()), message.bytes.size()});
						}
						else if (!message.text && connection->client.binary)
						{
							connection->client.binary(message.bytes);
						}
						connection = _network.Find(id);
					}
				}
				if (connection == nullptr || connection->ended)
				{
					continue;
				}
				if (connection->cutForClient || connection->serverClosed.has_value())
				{
					connection->ended = true;
					auto handler = std::move(connection->client.closed);
					connection->client = {};
					if (handler)
					{
						handler(connection->cutForClient
						            ? online::WebSocketClosed {.end = online::WebSocketEnd::Network, .code = 0, .httpStatus = 0}
						            : online::WebSocketClosed {.end = online::WebSocketEnd::ClosedByServer,
						                                       .code = *connection->serverClosed,
						                                       .httpStatus = 0});
					}
				}
			}
		}
		[[nodiscard]] online::WebSocketState State(online::WebSocketId id) const override
		{
			const auto* connection = _network.Find(id);
			if (connection == nullptr || connection->ended)
			{
				return online::WebSocketState::None;
			}
			return connection->clientOpen ? online::WebSocketState::Open : online::WebSocketState::Connecting;
		}
		[[nodiscard]] size_t Count() const override
		{
			size_t count = 0;
			for (const auto id : _ids)
			{
				count += State(id) != online::WebSocketState::None ? 1 : 0;
			}
			return count;
		}
		[[nodiscard]] const std::vector<online::WebSocketId>& Ids() const { return _ids; }

	private:
		bool Push(online::WebSocketId id, bool text, std::span<const uint8_t> bytes)
		{
			auto* connection = _network.Find(id);
			if (connection == nullptr || connection->ended || !connection->clientOpen)
			{
				return false;
			}
			connection->toServer.push_back(Message {.text = text, .bytes = {bytes.begin(), bytes.end()}});
			return true;
		}

		LoopbackSockets& _network;
		std::vector<online::WebSocketId> _ids;
	};

	[[nodiscard]] std::unique_ptr<Client> MakeClient() { return std::make_unique<Client>(*this); }

	/// The network stops for this connection: both sides get a close with no close frame
	void Cut(uint32_t id)
	{
		auto* connection = Find(id);
		if (connection == nullptr || connection->ended)
		{
			return;
		}
		connection->cutForClient = true;
		connection->clientClosed = server::server_close::k_NoCloseFrame;
	}

	void SetHandlers(server::ServerSocketHandlers handlers) override { _handlers = std::move(handlers); }
	bool SendText(server::ConnectionId id, std::string_view text) override
	{
		return ServerPush(id, true, {reinterpret_cast<const uint8_t*>(text.data()), text.size()});
	}
	bool SendBinary(server::ConnectionId id, std::span<const uint8_t> bytes) override { return ServerPush(id, false, bytes); }
	void Close(server::ConnectionId id, uint16_t code) override
	{
		auto* connection = Find(id);
		if (connection == nullptr || !connection->serverKnows || connection->serverClosed.has_value())
		{
			return;
		}
		connection->serverClosed = code;
		connection->serverKnows = false;
	}
	void Poll() override
	{
		for (auto& [id, connection] : _connections)
		{
			if (connection.serverClosed.has_value() || (connection.ended && !connection.clientClosed.has_value()))
			{
				continue;
			}
			if (!connection.serverKnows && !connection.ended)
			{
				connection.serverKnows = true;
				if (_handlers.opened)
				{
					_handlers.opened(id);
				}
			}
			while (connection.serverKnows && !connection.toServer.empty())
			{
				auto message = std::move(connection.toServer.front());
				connection.toServer.pop_front();
				if (message.text && _handlers.text)
				{
					_handlers.text(id, {reinterpret_cast<const char*>(message.bytes.data()), message.bytes.size()});
				}
				else if (!message.text && _handlers.binary)
				{
					_handlers.binary(id, message.bytes);
				}
			}
			if (connection.clientClosed.has_value() && connection.serverKnows)
			{
				connection.serverKnows = false;
				const auto code = *connection.clientClosed;
				connection.clientClosed.reset();
				if (_handlers.closed)
				{
					_handlers.closed(id, code);
				}
			}
		}
	}
	[[nodiscard]] size_t Count() const override
	{
		size_t count = 0;
		for (const auto& [id, connection] : _connections)
		{
			count += connection.serverKnows ? 1 : 0;
		}
		return count;
	}

	[[nodiscard]] Connection* Find(uint32_t id)
	{
		const auto found = _connections.find(id);
		return found != _connections.end() ? &found->second : nullptr;
	}
	[[nodiscard]] const Connection* Find(uint32_t id) const
	{
		const auto found = _connections.find(id);
		return found != _connections.end() ? &found->second : nullptr;
	}

private:
	bool ServerPush(server::ConnectionId id, bool text, std::span<const uint8_t> bytes)
	{
		auto* connection = Find(id);
		if (connection == nullptr || !connection->serverKnows)
		{
			return false;
		}
		connection->toClient.push_back(Message {.text = text, .bytes = {bytes.begin(), bytes.end()}});
		return true;
	}

	server::ServerSocketHandlers _handlers;
	std::map<uint32_t, Connection> _connections;
	uint32_t _nextId {1};
};

} // namespace openblack::test_fakes
