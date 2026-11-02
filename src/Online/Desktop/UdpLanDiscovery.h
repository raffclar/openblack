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

#include "Online/LanDiscoveryInterface.h"
#include "Online/LanResponder.h"

namespace openblack::online
{

/// The search for game servers on the local network over UDP (deviations 113 and 194), with the sockets, the network
/// interfaces and the random numbers of this computer. The clock gives microseconds of a steady clock.
[[nodiscard]] std::unique_ptr<LanDiscoveryInterface> MakeUdpLanDiscovery(std::function<int64_t()> clock);

/// The answers of a game server to the search of the local network, on the port of the search. Gives none when the IPv4
/// port of the search does not open. The IPv6 socket is optional.
[[nodiscard]] std::unique_ptr<LanResponder> MakeUdpLanResponder(std::function<int64_t()> clock);

} // namespace openblack::online
