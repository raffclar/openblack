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
#include <span>
#include <string_view>
#include <vector>

namespace openblack::filesystem
{
class FileSystemInterface;
}

/// The files of a saved game that are not creature files: the world files, the script files and the information file
/// of a slot. The creature mind component reads and writes the mind files.
namespace openblack::ecs::save_files
{

/// The bytes of the file, through the game's file system. None when the file is not there or cannot be read.
[[nodiscard]] std::optional<std::vector<uint8_t>> ReadFile(const std::filesystem::path& path);

/// The game writes each world file of a save first to a temporary file: the file's own name with this at its end
inline constexpr std::string_view k_TemporaryEnding = ".tmp";
/// The temporary file of a file of a save
[[nodiscard]] std::filesystem::path TemporaryPath(const std::filesystem::path& path);

/// A world file of a save and its contents
struct SaveFile
{
	std::filesystem::path path;
	std::vector<uint8_t> bytes;
};

/// How a write of the world files of a save ended
enum class SaveWrite : uint8_t
{
	/// Each file took the place of its old file
	Written,
	/// The first temporary file did not open: the game wrote nothing
	NotOpened,
	/// A write did not complete: the game wrote no more, the old files stay as they were and so do the temporary files.
	/// The game tells the player that an error occurred.
	Failed,
};

/// Writes the world files of a save as the game does: each file in turn to its temporary file, and no more after a write
/// that did not complete. When all are complete, each old file goes and its temporary file takes its name, in the same
/// order.
[[nodiscard]] SaveWrite WriteThroughTemporaries(filesystem::FileSystemInterface& files, std::span<const SaveFile> group);

} // namespace openblack::ecs::save_files
