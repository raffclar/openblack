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
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace openblack::online
{

/// A UDP socket on IPv4 that does not block: the search for game servers on the local network uses it
class UdpSocket
{
public:
	/// A datagram that came, with the IPv4 address and the port of the sender
	struct Datagram
	{
		std::vector<uint8_t> bytes;
		std::string address;
		uint16_t port {0};
	};

	UdpSocket();
	~UdpSocket();
	UdpSocket(const UdpSocket&) = delete;
	UdpSocket& operator=(const UdpSocket&) = delete;
	UdpSocket(UdpSocket&&) = delete;
	UdpSocket& operator=(UdpSocket&&) = delete;

	/// Opens the socket on a port of all the addresses of the computer, 0 for a free port. With broadcast, the socket can
	/// send to the broadcast address of the network. Gives false when the socket does not open.
	[[nodiscard]] bool Open(uint16_t port, bool broadcast);
	[[nodiscard]] bool IsOpen() const noexcept;
	/// Sends a datagram to an IPv4 address and a port. Gives false when the datagram did not go.
	bool SendTo(std::string_view address, uint16_t port, std::span<const uint8_t> bytes);
	/// The next datagram that came, none when no datagram waits
	[[nodiscard]] std::optional<Datagram> Receive();

private:
	intptr_t _socket {-1};
};

} // namespace openblack::online
