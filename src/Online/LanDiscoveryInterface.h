/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>
#include <vector>

#include <LobbyApi.h>

namespace openblack::online
{

/// Finds the game servers on the local network, with no lobby: the front end's list of LAN games. Each server that it
/// finds is a line of the list in the lobby's shape, with the address of its game socket.
class LanDiscoveryInterface
{
public:
	virtual ~LanDiscoveryInterface() = default;

	/// Whether this build can look for servers on the local network. A browser cannot.
	[[nodiscard]] virtual bool IsAvailable() const = 0;
	/// Starts to look for servers, and forgets the servers that it found before
	virtual void Start() = 0;
	virtual void Stop() = 0;
	/// Once a frame while it looks
	virtual void Poll() = 0;
	[[nodiscard]] virtual std::span<const lobby_api::ServerEntry> GetServers() const = 0;
};

/// A build with no search of the local network: the front end shows no LAN connection
class NoLanDiscovery final: public LanDiscoveryInterface
{
public:
	[[nodiscard]] bool IsAvailable() const override { return false; }
	void Start() override {}
	void Stop() override {}
	void Poll() override {}
	[[nodiscard]] std::span<const lobby_api::ServerEntry> GetServers() const override { return {}; }
};

} // namespace openblack::online
