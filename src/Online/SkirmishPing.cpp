/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkirmishPing.h"

#include <utility>

#include <spdlog/spdlog.h>

#include "ECS/Systems/LobbyClientSystemInterface.h"

namespace openblack::online
{
using ecs::systems::LobbyClientSystemInterface;
using ecs::systems::LobbyDone;
using ecs::systems::LobbyResult;
using ecs::systems::LobbySessionState;

namespace
{
void Send(LobbyClientSystemInterface& lobby, size_t count)
{
	for (size_t i = 0; i < count; ++i)
	{
		lobby.SendSkirmishPing([](LobbyResult<LobbyDone>&& result) {
			// A failure changes nothing in the game
			if (const auto logger = spdlog::get("game"))
			{
				SPDLOG_LOGGER_INFO(logger, "The lobby {} the message of the decided skirmish",
				                   result.Ok() ? "got" : "did not get");
			}
		});
	}
}
} // namespace

void SendSkirmishPings(LobbyClientSystemInterface& lobby, size_t count)
{
	if (count == 0 || !lobby.IsConfigured())
	{
		return;
	}
	if (lobby.GetState() == LobbySessionState::LoggedIn)
	{
		Send(lobby, count);
		return;
	}
	// The lobby lives as long as the game, and it keeps the callback
	lobby.ResumeSession([&lobby, count](auto&& result) {
		if (result.Ok())
		{
			Send(lobby, count);
		}
	});
}

} // namespace openblack::online
