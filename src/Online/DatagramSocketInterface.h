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

#include "Online/LanDiscovery.h"

namespace openblack::online
{

/// A datagram that came, with the address and the port of the sender
struct Datagram
{
	std::vector<uint8_t> bytes;
	lan_discovery::Address from;
	uint16_t port {0};
};

/// A UDP socket of one address family that does not block. The search of the local network uses it, and tests give a
/// fake.
class DatagramSocketInterface
{
public:
	virtual ~DatagramSocketInterface() = default;

	[[nodiscard]] virtual bool IsOpen() const = 0;
	/// Sends a datagram to an address and a port. Gives false when the datagram did not go.
	virtual bool SendTo(const lan_discovery::Address& to, uint16_t port, std::span<const uint8_t> bytes) = 0;
	/// The next datagram that came, none when no datagram waits
	[[nodiscard]] virtual std::optional<Datagram> Receive() = 0;
};

} // namespace openblack::online
