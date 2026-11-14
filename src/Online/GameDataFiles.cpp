/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameDataFiles.h"

#include <cstdint>

#include <exception>
#include <filesystem>
#include <optional>
#include <system_error>

#include "FileSystem/FileSystemInterface.h"

namespace openblack::online::game_data
{

std::vector<FilePrint> PrintFiles(filesystem::FileSystemInterface& files, std::span<const std::string> paths, PrintCache* cache)
{
	std::vector<FilePrint> print;
	const auto& game = files.GetGamePath();
	for (const auto& path : paths)
	{
		if (print.size() >= k_MostPrintFiles)
		{
			break;
		}
		if (!IsPrintPath(path))
		{
			continue;
		}
		// The file is read from the game's folder only, not from the working folder or another folder
		std::optional<std::vector<uint8_t>> bytes;
		const std::filesystem::path relative(path);
		std::error_code error;
		auto full = std::optional(game / relative);
		if (!std::filesystem::exists(*full, error))
		{
			// The server's name of the file can have another letter case than the player's file
			full = filesystem::FileSystemInterface::FindIgnoringCase(game, relative);
		}
		// A file with the same size and time of change as before keeps its print
		std::optional<std::pair<uint64_t, std::filesystem::file_time_type>> stamp;
		if (!game.empty() && full.has_value())
		{
			const auto size = std::filesystem::file_size(*full, error);
			const auto changed = error ? std::filesystem::file_time_type {} : std::filesystem::last_write_time(*full, error);
			if (!error)
			{
				stamp.emplace(size, changed);
			}
		}
		if (cache != nullptr && stamp.has_value())
		{
			const auto found = cache->entries.find(path);
			if (found != cache->entries.end() && found->second.size == stamp->first && found->second.changed == stamp->second)
			{
				print.push_back(found->second.print);
				continue;
			}
		}
		if (!game.empty() && full.has_value())
		{
			try
			{
				bytes = files.ReadAll(*full);
			}
			catch (const std::exception&)
			{
				bytes.reset();
			}
		}
		print.push_back(PrintOf(path, bytes));
		if (cache != nullptr && stamp.has_value() && print.back().present)
		{
			cache->entries[path] = {.size = stamp->first, .changed = stamp->second, .print = print.back()};
		}
	}
	return print;
}

std::vector<FilePrint> PrintOfReadFiles(filesystem::FileSystemInterface& files)
{
	const auto read = files.ReadGameFiles();
	const auto paths = PrintPaths(read);
	return PrintFiles(files, paths);
}

InstallHints HintsOfGameFolder(const filesystem::FileSystemInterface& files)
{
	std::vector<std::string> names;
	const auto& game = files.GetGamePath();
	if (game.empty())
	{
		return {};
	}
	try
	{
		files.Iterate(game, false, [&names](const std::filesystem::path& path) { names.push_back(path.filename().string()); });
	}
	catch (const std::exception&)
	{
		return {};
	}
	return HintsOf(names);
}

} // namespace openblack::online::game_data
