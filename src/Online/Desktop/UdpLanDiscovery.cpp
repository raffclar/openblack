/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UdpLanDiscovery.h"

#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>

namespace openblack::online
{
namespace
{

/// The broadcast address of the network. A game server on this computer also gets the search at this address.
constexpr std::string_view k_Broadcast = "255.255.255.255";

} // namespace

UdpLanDiscovery::UdpLanDiscovery(Clock clock)
    : _clock(std::move(clock))
{
	if (!_socket.Open(0, true))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The search for games on the local network cannot open its socket");
	}
}

bool UdpLanDiscovery::IsAvailable() const
{
	return _socket.IsOpen();
}

void UdpLanDiscovery::Start()
{
	_servers.Clear();
	_searching = true;
	SendSearch(_clock());
}

void UdpLanDiscovery::Stop()
{
	_searching = false;
}

void UdpLanDiscovery::Poll()
{
	if (!_searching)
	{
		return;
	}
	const auto now = _clock();
	if (now >= _nextSearch)
	{
		SendSearch(now);
	}
	while (auto datagram = _socket.Receive())
	{
		if (const auto answer = lan_discovery::ParseAnswer(datagram->bytes); answer.has_value())
		{
			_servers.Heard(lan_discovery::EntryOf(*answer, datagram->address), now);
		}
	}
	_servers.Forget(now);
}

void UdpLanDiscovery::SendSearch(int64_t now)
{
	_nextSearch = now + lan_discovery::k_SearchEveryMicroseconds;
	const auto search = lan_discovery::Search();
	_socket.SendTo(k_Broadcast, lan_discovery::k_Port, search);
}

} // namespace openblack::online
