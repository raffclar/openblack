/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UdpLanDiscovery.h"

#include <cstdint>

#include <algorithm>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "Online/Desktop/UdpSocket.h"
#include "Online/LanSearcher.h"

namespace openblack::online
{
namespace
{

/// Random numbers from a generator with a seed from the system. The search is not part of the game world, so it does
/// not use the random numbers of the game.
std::function<uint64_t()> SystemRandom()
{
	std::random_device device;
	const auto seed = (static_cast<uint64_t>(device()) << 32) | device();
	return [generator = std::make_shared<std::mt19937_64>(seed)] { return (*generator)(); };
}

/// The indexes of the interfaces that can get the search group
std::vector<uint32_t> GroupInterfaces(const std::vector<lan_discovery::NetworkInterface>& interfaces)
{
	std::vector<uint32_t> indexes;
	for (const auto& item : interfaces)
	{
		if (item.address.family == lan_discovery::Family::V6 && item.multicast && item.index != 0 &&
		    std::ranges::find(indexes, item.index) == indexes.end())
		{
			indexes.push_back(item.index);
		}
	}
	return indexes;
}

void Warn(const char* text)
{
	if (auto logger = spdlog::get("game"))
	{
		logger->warn(text);
	}
}

} // namespace

std::unique_ptr<LanDiscoveryInterface> MakeUdpLanDiscovery(std::function<int64_t()> clock)
{
	auto v4 = std::make_unique<UdpSocket>();
	if (!v4->Open({.family = lan_discovery::Family::V4, .port = 0, .broadcast = true, .groups = {}}))
	{
		Warn("The search for games on the local network cannot open its IPv4 socket");
	}
	auto v6 = std::make_unique<UdpSocket>();
	if (!v6->Open({.family = lan_discovery::Family::V6, .port = 0, .broadcast = false, .groups = {}}))
	{
		Warn("The search for games on the local network cannot open its IPv6 socket");
	}
	return std::make_unique<LanSearcher>(LanSearchParts {
	    .v4 = std::move(v4),
	    .v6 = std::move(v6),
	    .interfaces = &LocalInterfaces,
	    .clock = std::move(clock),
	    .random = SystemRandom(),
	});
}

std::unique_ptr<LanResponder> MakeUdpLanResponder(std::function<int64_t()> clock)
{
	auto v4 = std::make_unique<UdpSocket>();
	if (!v4->Open({.family = lan_discovery::Family::V4, .port = lan_discovery::k_Port, .broadcast = false, .groups = {}}))
	{
		return nullptr;
	}
	auto v6 = std::make_unique<UdpSocket>();
	const auto groups = GroupInterfaces(LocalInterfaces());
	if (!v6->Open({.family = lan_discovery::Family::V6, .port = lan_discovery::k_Port, .broadcast = false, .groups = groups}))
	{
		Warn("The game server cannot answer the search of the local network on IPv6");
	}
	return std::make_unique<LanResponder>(LanResponderParts {
	    .v4 = std::move(v4),
	    .v6 = std::move(v6),
	    .interfaces = &LocalInterfaces,
	    .clock = std::move(clock),
	    .random = SystemRandom(),
	});
}

} // namespace openblack::online
