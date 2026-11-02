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
#include <span>
#include <vector>

#include "Online/DatagramSocketInterface.h"
#include "Online/LanDiscovery.h"
#include "Online/LanDiscoveryInterface.h"

namespace openblack::online
{

/// The parts that the search of the local network uses. Tests give fakes.
struct LanSearchParts
{
	/// The IPv4 socket and the IPv6 socket. One of them can be closed or empty.
	std::unique_ptr<DatagramSocketInterface> v4;
	std::unique_ptr<DatagramSocketInterface> v6;
	/// The network interfaces of the computer
	std::function<std::vector<lan_discovery::NetworkInterface>()> interfaces;
	/// Microseconds of a steady clock
	std::function<int64_t()> clock;
	/// A random number for each call
	std::function<uint64_t()> random;
};

/// The search for game servers on the local network (deviations 113 and 194). While the search runs, it sends the search
/// on IPv4 broadcast and IPv6 link-local multicast, with a longer time after each search up to a limit. It lists only the
/// answers to its own session from the local network.
class LanSearcher final: public LanDiscoveryInterface
{
public:
	explicit LanSearcher(LanSearchParts parts);

	[[nodiscard]] bool IsAvailable() const override;
	void Start() override;
	void Stop() override;
	void Poll() override;
	[[nodiscard]] std::span<const lobby_api::ServerEntry> GetServers() const override { return _servers.Servers(); }

private:
	[[nodiscard]] static bool IsOpen(const std::unique_ptr<DatagramSocketInterface>& socket);
	void ReadInterfaces(int64_t now);
	void SendSearch(int64_t now);
	void ReadAnswers(DatagramSocketInterface& socket, int64_t now);

	LanSearchParts _parts;
	std::vector<lan_discovery::NetworkInterface> _interfaces;
	int64_t _nextInterfaceRead {0};
	bool _searching {false};
	uint64_t _session {0};
	int _searchIndex {0};
	int64_t _nextSearch {0};
	lan_discovery::ServerList _servers;
};

} // namespace openblack::online
