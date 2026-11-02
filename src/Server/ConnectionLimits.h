/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <map>
#include <string>
#include <string_view>
#include <vector>

/// The limits of the game server on its connections, so that one client cannot use all of its memory or its time. These
/// rules keep no sockets, so that the tests give them values.
namespace openblack::server::connection_limits
{

/// The most connections of the server at a time. A room has at most 32 gods and its observers.
constexpr size_t k_MostConnections = 160;
/// The most connections from one address at a time. A computer can have a game and a few tools open.
constexpr size_t k_MostConnectionsPerAddress = 8;
/// The most bytes that wait to go to one client. A world state of a land is a few megabytes.
constexpr size_t k_MostQueuedBytes = 32 * 1024 * 1024;
/// The most bytes that the server reads from one connection in one poll. The rest waits in the socket.
constexpr size_t k_MostReadBytesPerPoll = 256 * 1024;
/// The time in which a connection must join the game or come back into it. A client sends its join as it connects.
constexpr int64_t k_JoinMicroseconds = 15 * 1'000'000;

/// True for an address of 127.0.0.0/8, or the name "localhost"
[[nodiscard]] bool IsLoopback(std::string_view address) noexcept;

/// The address of the client of a connection. A connection from this computer that has an X-Forwarded-For header
/// comes from the reverse proxy in front of the server, and the last address of the header is the address of the
/// client that the proxy saw. Any other connection is from its own address.
[[nodiscard]] std::string ClientAddress(std::string_view peer, std::string_view forwardedFor);

/// The connections of each address. A connection from this computer with no proxy in front of it has no limit for its
/// address, as the tools of the game start many clients on one computer.
class AddressCounts
{
public:
	/// Counts a connection of the address. False, and no count, when the address has as many connections as it can have.
	[[nodiscard]] bool Add(const std::string& address);
	/// The connection of the address ended
	void Remove(const std::string& address);
	[[nodiscard]] size_t Of(const std::string& address) const;

private:
	std::map<std::string, size_t> _counts;
};

/// The bytes that wait to go to a client, in the order that they go. The bytes that went are removed from the front in
/// large steps, so that each byte is moved only a few times.
class OutQueue
{
public:
	/// Adds the bytes at the end. False, and nothing added, when the queue would have more than the limit.
	[[nodiscard]] bool Add(const uint8_t* data, size_t size, size_t most = k_MostQueuedBytes);
	/// The bytes that wait, from the first one
	[[nodiscard]] const uint8_t* Data() const { return _bytes.data() + _first; }
	[[nodiscard]] size_t Size() const { return _bytes.size() - _first; }
	[[nodiscard]] bool Empty() const { return Size() == 0; }
	/// The first bytes went
	void Sent(size_t count);

private:
	std::vector<uint8_t> _bytes;
	size_t _first {0};
};

} // namespace openblack::server::connection_limits
