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
#include <functional>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "Server/WebSocketServerInterface.h"

/// Tests and tools only: faults of the network between the game server and its clients, as the switches
/// --test-net-delay-ms and --test-net-loss of the server give them. A normal game never has them.
namespace openblack::server
{

struct NetworkFaults
{
	/// The time that each message takes in each direction, in milliseconds
	uint32_t delayMilliseconds {0};
	/// The chance in percent that a packet is lost. The game socket is TCP, so a lost packet comes late: TCP sends it
	/// again after its time to send again, which doubles at each loss. The messages after it wait, as in TCP.
	uint32_t lossPercent {0};
	/// The seed of the draws of the losses. These draws are not synced.
	uint32_t seed {1};

	[[nodiscard]] bool Any() const noexcept { return delayMilliseconds > 0 || lossPercent > 0; }
};

/// The first time of TCP to send a lost packet again (the least time of Linux), in microseconds
inline constexpr int64_t k_FirstResendMicroseconds = 200'000;
/// The most losses of one packet in a row. After this many the packet goes through.
inline constexpr uint32_t k_MostLossesInARow = 6;

/// The extra time of a packet that was lost this many times in a row: each loss waits for the time to send again, and
/// this time doubles after each loss. Pure.
[[nodiscard]] constexpr int64_t ResendMicroseconds(uint32_t losses) noexcept
{
	int64_t total = 0;
	int64_t wait = k_FirstResendMicroseconds;
	for (uint32_t i = 0; i < losses && i < k_MostLossesInARow; ++i)
	{
		total += wait;
		wait *= 2;
	}
	return total;
}

/// The WebSocket end of the server with the faults of the network. Each message in each direction comes after the delay
/// and the time of its losses. A message never passes the message before it on the same connection, as in TCP. A close
/// goes after the messages before it. With no fault, each call goes to the real sockets at once.
class FaultyWebSocketServer final: public WebSocketServerInterface
{
public:
	/// Microseconds of a steady clock
	using Clock = std::function<int64_t()>;

	FaultyWebSocketServer(WebSocketServerInterface& sockets, NetworkFaults faults, Clock clock);

	void SetHandlers(ServerSocketHandlers handlers) override;
	bool SendText(ConnectionId connection, std::string_view text) override;
	bool SendBinary(ConnectionId connection, std::span<const uint8_t> bytes) override;
	void Close(ConnectionId connection, uint16_t code) override;
	void Poll() override;
	[[nodiscard]] size_t Count() const override { return _sockets.Count(); }
	[[nodiscard]] std::string AddressOf(ConnectionId connection) const override { return _sockets.AddressOf(connection); }

	/// The packets that were lost and sent again, in both directions
	[[nodiscard]] uint64_t Losses() const noexcept { return _losses; }

private:
	enum class Kind : uint8_t
	{
		Text,
		Binary,
		Close,
	};
	struct Message
	{
		int64_t due {0};
		Kind kind {Kind::Text};
		std::vector<uint8_t> bytes;
		/// The close code of a close
		uint16_t code {0};
	};
	/// The messages of one direction of one connection, in their order
	struct Queue
	{
		std::deque<Message> messages;
		int64_t lastDue {0};
	};

	/// The time when a message that goes now comes to the other end
	[[nodiscard]] int64_t DueTime(Queue& queue);
	void Push(std::map<ConnectionId, Queue>& queues, ConnectionId connection, Kind kind, std::span<const uint8_t> bytes,
	          uint16_t code);
	void DeliverIncoming(int64_t now);
	void SendOutgoing(int64_t now);

	WebSocketServerInterface& _sockets;
	NetworkFaults _faults;
	Clock _clock;
	ServerSocketHandlers _handlers;
	std::mt19937 _random;
	/// From the clients to the server
	std::map<ConnectionId, Queue> _incoming;
	/// From the server to the clients
	std::map<ConnectionId, Queue> _outgoing;
	/// The connections that the server sees open
	std::set<ConnectionId> _open;
	uint64_t _losses {0};
};

} // namespace openblack::server
