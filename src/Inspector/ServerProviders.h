/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include <InspectorJson.h>
#include <InspectorProvider.h>

#include "Server/ServerAdmin.h"

namespace openblack::inspector
{

/// The name of the provider of the game server's queries
inline constexpr std::string_view k_ServerProviderName = "server";

/// The report of the room as the inspector gives it
[[nodiscard]] Json ToJson(const server::admin::RoomReport& room);

/// The queries of the game server for its operator. They only read: server.room gives the room of the network game. The
/// function gives the report of the room at the time of the query.
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeServerProvider(std::function<server::admin::RoomReport()> room);

} // namespace openblack::inspector
