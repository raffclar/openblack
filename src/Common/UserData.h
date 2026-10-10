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

/// openblack's own folder for what it writes for the player, apart from the game's folder, which it never writes to
namespace openblack::user_data
{

/// The user's folder for openblack, made if it isn't there yet; none when the system has none to give
[[nodiscard]] std::optional<std::filesystem::path> Folder();

/// Where the player's creature is kept between lands, laid out as the game lays out its own folder
[[nodiscard]] std::optional<std::filesystem::path> CreatureMindFolder();

} // namespace openblack::user_data
