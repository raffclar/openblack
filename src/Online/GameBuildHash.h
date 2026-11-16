/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <future>
#include <optional>
#include <string>

/// The build hash of the desktop game: the SHA-256 of its program file. The game sends it with its build id when it
/// makes or joins a lobby, and the lobby refuses a program that is not a program of the live build (when the service
/// lists the hashes of that build). The web build has no program file, so it sends the build id only.
namespace openblack::online::build_hash
{

/// The SHA-256 of a file as 64 hex digits in lower case, or no value when the game cannot read the file
[[nodiscard]] std::optional<std::string> OfFile(const std::filesystem::path& path);

/// The program file of this game, or an empty path when the system does not give it
[[nodiscard]] std::filesystem::path ProgramPath();

/// The build hash of this game. The desktop game reads its program file on a thread of its own, so that the frames do
/// not wait. The web build gives an empty hash at once.
[[nodiscard]] std::shared_future<std::string> OwnLater();

} // namespace openblack::online::build_hash
