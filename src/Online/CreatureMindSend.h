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

#include <span>
#include <string>

#include "ECS/Systems/LobbyClientSystemInterface.h"

/// The desktop game sends the mind file of the player's creature to the files that the lobby keeps for the account, each
/// time that it writes the file. The creature vault of the site then shows the creature of a desktop game too.
namespace openblack::online::creature_mind_send
{

/// Whether the game sends the mind file: only with a session of an account that is not a guest, because the lobby keeps
/// no files for a guest
[[nodiscard]] bool Sends(ecs::systems::LobbySessionState state, bool guest) noexcept;

/// Sends the profile's mind file to the folder of the creature minds in the account's files, when the rule lets it. The
/// log tells the outcome with the name of the file and the status only.
void Send(ecs::systems::LobbyClientSystemInterface& lobby, std::string fileName, std::span<const uint8_t> mind);

} // namespace openblack::online::creature_mind_send
