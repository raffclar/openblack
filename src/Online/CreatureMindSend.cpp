/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMindSend.h"

#include <utility>
#include <vector>

#include <LobbyApiJson.h>
#include <spdlog/spdlog.h>

namespace openblack::online::creature_mind_send
{

bool Sends(ecs::systems::LobbySessionState state, bool guest) noexcept
{
	return state == ecs::systems::LobbySessionState::LoggedIn && !guest;
}

void Send(ecs::systems::LobbyClientSystemInterface& lobby, std::string fileName, std::span<const uint8_t> mind)
{
	if (!lobby.IsConfigured() || !Sends(lobby.GetState(), lobby.IsGuest()) || mind.empty())
	{
		return;
	}
	lobby.PutPlayerFile(std::string(lobby_api::k_CreatureMindFolder), fileName, std::vector<uint8_t>(mind.begin(), mind.end()),
	                    [fileName](const ecs::systems::LobbyResult<ecs::systems::LobbyDone>& result) {
		                    const auto log = spdlog::get("game");
		                    if (log == nullptr)
		                    {
			                    return;
		                    }
		                    if (result.error.kind == ecs::systems::LobbyErrorKind::None)
		                    {
			                    SPDLOG_LOGGER_INFO(log, "The lobby took the creature mind file {}", fileName);
			                    return;
		                    }
		                    SPDLOG_LOGGER_WARN(log, "The lobby did not take the creature mind file {} (status {}, code {})",
		                                       fileName, result.error.status, result.error.code);
	                    });
}

} // namespace openblack::online::creature_mind_send
