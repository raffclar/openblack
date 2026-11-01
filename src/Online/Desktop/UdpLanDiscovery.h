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
#include <span>

#include "Online/Desktop/UdpSocket.h"
#include "Online/LanDiscovery.h"
#include "Online/LanDiscoveryInterface.h"

namespace openblack::online
{

/// The search for game servers on the local network over UDP (deviation 113). The search goes to the broadcast address
/// of the network again and again while the search runs.
class UdpLanDiscovery final: public LanDiscoveryInterface
{
public:
	/// Microseconds of a steady clock
	using Clock = std::function<int64_t()>;

	explicit UdpLanDiscovery(Clock clock);

	[[nodiscard]] bool IsAvailable() const override;
	void Start() override;
	void Stop() override;
	void Poll() override;
	[[nodiscard]] std::span<const lobby_api::ServerEntry> GetServers() const override { return _servers.Servers(); }

private:
	void SendSearch(int64_t now);

	Clock _clock;
	UdpSocket _socket;
	bool _searching {false};
	int64_t _nextSearch {0};
	lan_discovery::ServerList _servers;
};

} // namespace openblack::online
