/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FileSystemInterface.h"

#include <cctype>

#include <algorithm>
#include <array>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <vector>

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

namespace
{
bool SameIgnoringCase(const std::string& left, const std::string& right)
{
	return std::ranges::equal(left, right, [](char a, char b) {
		return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	});
}
} // namespace

std::optional<std::filesystem::path> FileSystemInterface::FindIgnoringCase(const std::filesystem::path& root,
                                                                           const std::filesystem::path& relative)
{
	std::error_code error;
	auto found = root;
	for (const auto& part : relative)
	{
		const auto name = part.generic_string();
		if (name.empty() || name == ".")
		{
			continue;
		}
		if (name == "..")
		{
			return std::nullopt;
		}
		// The part as written first, then any entry of the folder that differs only in letter case
		if (std::filesystem::exists(found / part, error))
		{
			found /= part;
			continue;
		}
		std::optional<std::filesystem::path> match;
		for (std::filesystem::directory_iterator it(found, error), end; !error && it != end; it.increment(error))
		{
			if (SameIgnoringCase(it->path().filename().generic_string(), name))
			{
				match = it->path();
				break;
			}
		}
		if (!match.has_value())
		{
			return std::nullopt;
		}
		found = *match;
	}
	return found;
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
	return WriteFile(path, bytes) == WriteResult::Written;
}

FileSystemInterface::WriteResult FileSystemInterface::WriteFile(const std::filesystem::path& path,
                                                                std::span<const uint8_t> bytes)
{
	if (InGameFolder(path))
	{
		return WriteResult::NotOpened;
	}
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open())
	{
		return WriteResult::NotOpened;
	}
	stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	stream.close();
	return stream ? WriteResult::Written : WriteResult::Incomplete;
}

bool FileSystemInterface::WriteAt(const std::filesystem::path& path, uint64_t offset, std::span<const uint8_t> bytes)
{
	if (InGameFolder(path))
	{
		return false;
	}
	// Opened to read and write, the file keeps its bytes and is not made when it is not there
	std::fstream stream(path, std::ios::binary | std::ios::in | std::ios::out);
	if (!stream.is_open())
	{
		return false;
	}
	stream.seekp(static_cast<std::streamoff>(offset));
	stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	stream.close();
	return static_cast<bool>(stream);
}

bool FileSystemInterface::Rename(const std::filesystem::path& from, const std::filesystem::path& to)
{
	if (InGameFolder(from) || InGameFolder(to))
	{
		return false;
	}
	std::error_code error;
	std::filesystem::rename(from, to, error);
	return !error;
}

bool FileSystemInterface::Remove(const std::filesystem::path& path)
{
	if (InGameFolder(path))
	{
		return false;
	}
	std::error_code error;
	return std::filesystem::remove(path, error);
}

bool FileSystemInterface::EmptyFolder(const std::filesystem::path& path)
{
	if (InGameFolder(path))
	{
		return false;
	}
	std::error_code error;
	if (!std::filesystem::is_directory(path, error))
	{
		return false;
	}
	std::vector<std::filesystem::path> entries;
	for (const auto& entry : std::filesystem::directory_iterator(path, error))
	{
		entries.push_back(entry.path());
	}
	for (const auto& entry : entries)
	{
		std::filesystem::remove_all(entry, error);
	}
	return std::filesystem::is_empty(path, error);
}
