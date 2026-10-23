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
#include <optional>
#include <string_view>

/// openblack's own folder for what it writes for the player, apart from the game's folder, which it never writes to
namespace openblack::user_data
{

/// The user's folder for openblack, made if it isn't there yet; none when the system has none to give
[[nodiscard]] std::optional<std::filesystem::path> Folder();

/// The name of the profile folder
constexpr std::string_view k_ProfileFolderName = "Profiles";

/// The profile folder, which keeps the files that the game writes for its players. It is in the working folder of the
/// game. This does not make the folder. There is no folder when the system cannot give the working folder.
[[nodiscard]] std::optional<std::filesystem::path> ProfileFolder();

/// The file of the settings of this computer, next to the profile folder. When the profile folder is a link to a folder
/// in another place (as in the web build, to the site's private storage), the file is next to that folder. There is no
/// file when the system cannot give the working folder.
[[nodiscard]] std::optional<std::filesystem::path> SettingsFile();

} // namespace openblack::user_data
