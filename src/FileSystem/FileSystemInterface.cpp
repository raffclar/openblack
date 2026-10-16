/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FileSystemInterface.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <ios>
#include <system_error>

using namespace openblack::filesystem;

FileSystemInterface::~FileSystemInterface() = default;

std::filesystem::path FileSystemInterface::FixPath(const std::filesystem::path& path)
{
	std::string result = path.generic_string();

	constexpr std::array<std::string_view, 3> caseFixTable = {
	    "\\Data\\",
	    "\\Landscape\\",
	    "\\Multi_Player\\",
	};
	for (const auto& pattern : caseFixTable)
	{
		auto foundIter = std::search(result.cbegin(), result.cend(), pattern.cbegin(), pattern.cend(),
		                             [](char left, char right) { return std::toupper(left) == std::toupper(right); });
		if (foundIter != result.cend())
		{
			result.replace(foundIter, foundIter + pattern.size(), pattern.data());
		}
	}

	for (auto pos = result.find('\\'); pos != std::string::npos; pos = result.find('\\', pos + 1))
	{
		result[pos] = '/';
	}

	return result;
}

bool FileSystemInterface::InGameFolder(const std::filesystem::path& path) const
{
	const auto& gamePath = GetGamePath();
	if (gamePath.empty())
	{
		return false;
	}
	std::error_code error;
	auto game = std::filesystem::weakly_canonical(gamePath, error);
	if (error)
	{
		return false;
	}
	if (!game.has_filename())
	{
		game = game.parent_path();
	}
	const auto target = std::filesystem::weakly_canonical(path, error);
	if (error)
	{
		return false;
	}
	// The path is in the game's folder when each part of the game's folder starts it
	return std::ranges::mismatch(game, target).in1 == game.end();
}

bool FileSystemInterface::MakeFolder(const std::filesystem::path& path)
{
	if (InGameFolder(path))
	{
		return false;
	}
	std::error_code error;
	std::filesystem::create_directories(path, error);
	return std::filesystem::is_directory(path, error);
}

bool FileSystemInterface::WriteAll(const std::filesystem::path& path, std::span<const uint8_t> bytes)
{
	if (InGameFolder(path))
	{
		return false;
	}
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	return static_cast<bool>(stream);
}
