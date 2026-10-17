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

#include <filesystem>
#include <optional>
#include <vector>

/// The files of a saved game that are not creature files: the world files, the script files and the information file
/// of a slot. The creature mind component reads and writes the mind files.
namespace openblack::ecs::save_files
{

/// The bytes of the file, through the game's file system. None when the file is not there or cannot be read.
[[nodiscard]] std::optional<std::vector<uint8_t>> ReadFile(const std::filesystem::path& path);

} // namespace openblack::ecs::save_files
