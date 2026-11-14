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
#include <map>
#include <span>
#include <string>
#include <vector>

#include "Online/GameDataPrint.h"

namespace openblack::filesystem
{
class FileSystemInterface;
} // namespace openblack::filesystem

/// The print of the game data from the files of the game's folder
namespace openblack::online::game_data
{

/// The prints that the computer made before, by the path, with the size and the time of change of the file. A file with
/// the same size and time keeps its print, so that a rejoin does not read the files again.
struct PrintCache
{
	struct Entry
	{
		uint64_t size {0};
		std::filesystem::file_time_type changed;
		FilePrint print;
	};
	std::map<std::string, Entry> entries;
};

/// The print of each file of the list from the game's folder, in the order of the list. A path that cannot be in a
/// print is left out, and the list stops at k_MostPrintFiles. The cache is optional.
[[nodiscard]] std::vector<FilePrint> PrintFiles(filesystem::FileSystemInterface& files, std::span<const std::string> paths,
                                                PrintCache* cache = nullptr);

/// The print of the files that the program read from the game's folder so far
[[nodiscard]] std::vector<FilePrint> PrintOfReadFiles(filesystem::FileSystemInterface& files);

/// The hints of the copy of the game from the names of the files at the top of the game's folder
[[nodiscard]] InstallHints HintsOfGameFolder(const filesystem::FileSystemInterface& files);

} // namespace openblack::online::game_data
