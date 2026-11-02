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
#include <vector>

#include "Online/DatagramSocketInterface.h"
#include "Online/LanDiscovery.h"

namespace openblack::online
{

/// A UDP socket on IPv4 or IPv6 that does not block: the search for game servers on the local network uses it. Its
/// datagrams cross no router, because each datagram can make one hop only.
class UdpSocket final: public DatagramSocketInterface
{
public:
	struct Options
	{
		lan_discovery::Family family {lan_discovery::Family::V4};
		/// The port on all the addresses of the computer, 0 for a free port
		uint16_t port {0};
		/// The socket can send to the broadcast address of a network (IPv4 only)
		bool broadcast {false};
		/// The socket gets the datagrams of the search group on these interfaces (IPv6 only)
		std::vector<uint32_t> groups;
	};

	UdpSocket();
	~UdpSocket() override;
	UdpSocket(const UdpSocket&) = delete;
	UdpSocket& operator=(const UdpSocket&) = delete;
	UdpSocket(UdpSocket&&) = delete;
	UdpSocket& operator=(UdpSocket&&) = delete;

	/// Opens the socket. Gives false when the socket does not open.
	[[nodiscard]] bool Open(const Options& options);
	[[nodiscard]] bool IsOpen() const override;
	bool SendTo(const lan_discovery::Address& to, uint16_t port, std::span<const uint8_t> bytes) override;
	[[nodiscard]] std::optional<Datagram> Receive() override;

private:
	lan_discovery::Family _family {lan_discovery::Family::V4};
	intptr_t _socket {-1};
};

/// The addresses of the network interfaces of this computer that are on
[[nodiscard]] std::vector<lan_discovery::NetworkInterface> LocalInterfaces();

} // namespace openblack::online
