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
#include <functional>
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

/// A file of a save that the writer makes from a copy that the game took at the save, such as a creature's mind file.
/// The step tells whether it wrote its file.
using SaveStep = std::function<bool(filesystem::FileSystemInterface& files)>;

/// All the folder and file work of one save, which the writer does on its own thread. The game only takes the copies.
struct SlotWrite
{
	/// The folders that the save writes into, made first in this order if they are missing
	std::vector<std::filesystem::path> folders;
	/// Writes the pictures of the slot. None when the pictures come later, from a picture of the screen.
	SaveStep pictures;
	/// The information file of the slot: its name and its date
	SaveFile info;
	/// The world file. Its temporary file must open before the game writes the files that come with the world.
	std::filesystem::path world;
	/// The files that the game writes with the world, such as the minds of the creatures
	std::vector<SaveStep> withWorld;
	/// Makes the world files, the world file first. The writer calls it after the files before it are written.
	std::function<std::vector<SaveFile>()> worldFiles;
};

/// How a write of all the files of a save ended
struct SlotWriteResult
{
	/// How the write of the world files ended
	SaveWrite world {SaveWrite::NotOpened};
	/// The other files that were not written: the pictures, the information file and the files with the world
	size_t otherFailures {0};
};

/// Writes all the files of a save in the game's order: the folders, the pictures, the information file, then the world.
/// When the temporary world file does not open, the game writes nothing more and tells the player nothing. Else the
/// files with the world are written, then the world files go through their temporary files.
[[nodiscard]] SlotWriteResult WriteSlot(filesystem::FileSystemInterface& files, const SlotWrite& slot);

} // namespace openblack::ecs::save_files
