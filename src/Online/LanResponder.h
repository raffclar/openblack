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
#include <memory>
#include <vector>

#include <LobbyApi.h>

#include "Online/DatagramSocketInterface.h"
#include "Online/LanDiscovery.h"

namespace openblack::online
{

/// The parts that the answers of a game server to the search of the local network use. Tests give fakes.
struct LanResponderParts
{
	/// The IPv4 socket and the IPv6 socket on the port of the search. One of them can be closed or empty.
	std::unique_ptr<DatagramSocketInterface> v4;
	std::unique_ptr<DatagramSocketInterface> v6;
	/// The network interfaces of the computer
	std::function<std::vector<lan_discovery::NetworkInterface>()> interfaces;
	/// Microseconds of a steady clock
	std::function<int64_t()> clock;
	/// A random number for each call
	std::function<uint64_t()> random;
};

/// The answers of a game server to the search of the local network (deviations 113 and 194). The server answers only a
/// search of its own version from the local network. Each answer goes after a short random time, and one sender gets
/// one answer at most in a short time.
class LanResponder
{
public:
	/// The room of the server as it is when an answer goes
	using Room = std::function<lobby_api::LanAnswer()>;

	explicit LanResponder(LanResponderParts parts);

	/// The number that marks this server in its answers
	[[nodiscard]] uint64_t Server() const noexcept { return _server; }
	/// Reads the searches and sends the answers that are due. Call it often, for example once a loop of the server.
	void Poll(const Room& room);

private:
	struct Waiting
	{
		lan_discovery::Address to;
		uint16_t port {0};
		uint64_t session {0};
		int64_t due {0};
	};
	struct Sent
	{
		lan_discovery::Address to;
		uint16_t port {0};
		int64_t when {0};
	};

	void ReadSearches(DatagramSocketInterface& socket, int64_t now);
	void SendDue(const Room& room, int64_t now);

	LanResponderParts _parts;
	uint64_t _server {0};
	std::vector<lan_discovery::NetworkInterface> _interfaces;
	int64_t _nextInterfaceRead {0};
	std::vector<Waiting> _waiting;
	std::vector<Sent> _sent;
};

} // namespace openblack::online
