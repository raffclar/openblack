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

#include <string>
#include <string_view>

/// The build id of the game for the lobby and the game servers. Two games can play together only when they have the
/// same build id. The lobby list, the lobby, the ticket and the game server compare the full id.
namespace openblack::online
{

/// The build id from its parts: the version with the commit, "/codec", and the version of the turns. For example
/// "0.1.0+0123456789ab/codec34".
[[nodiscard]] std::string BuildIdOf(std::string_view versionText, uint32_t codecVersion);

/// The build id of this program
[[nodiscard]] std::string ThisBuildId();

} // namespace openblack::online
