/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ModLoader.h"

#include <charconv>

#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace
{
constexpr std::string_view k_Whitespace = " \t\r\n";

/// The integer a version.txt holds, with whitespace around it at most; nothing when it holds anything else
std::optional<int> ParseVersion(std::string_view text)
{
	const auto first = text.find_first_not_of(k_Whitespace);
	if (first == std::string_view::npos)
	{
		return std::nullopt;
	}
	text = text.substr(first, text.find_last_not_of(k_Whitespace) - first + 1);
	int version = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), version);
	if (error != std::errc {} || end != text.data() + text.size())
	{
		return std::nullopt;
	}
	return version;
}

ModLoaderResult Check(const char* modsFolder, std::string& line)
{
	if (modsFolder == nullptr)
	{
		line = "no mods folder given: no mods read";
		return ModLoaderResult::NoVersionFile;
	}
	const auto path = std::filesystem::path(modsFolder) / "version.txt";
	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error))
	{
		line = "version.txt missing: no mods read";
		return ModLoaderResult::NoVersionFile;
	}
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		line = "version.txt cannot be read: no mods read";
		return ModLoaderResult::UnreadableVersion;
	}
	const std::string text {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	const auto version = ParseVersion(text);
	if (!version)
	{
		line = "version.txt does not hold a version number: no mods read";
		return ModLoaderResult::UnreadableVersion;
	}
	if (*version != k_ModLoaderVersion)
	{
		line = "version.txt is version " + std::to_string(*version) + ", the loader is version " +
		       std::to_string(k_ModLoaderVersion) + ": no mods read";
		return ModLoaderResult::VersionMismatch;
	}
	// The future mod SDK reads the mods here: each mod's folder, through the game's interfaces only
	line = "version.txt matches version " + std::to_string(k_ModLoaderVersion) + ": no mods are loaded yet";
	return ModLoaderResult::Matched;
}
} // namespace

int ModLoader_Version() noexcept
{
	return k_ModLoaderVersion;
}

int ModLoader_LoadMods(const char* modsFolder, ModLoaderLogFn log, void* context) noexcept
{
	std::string line;
	auto result = ModLoaderResult::UnreadableVersion;
	try
	{
		result = Check(modsFolder, line);
	}
	catch (const std::exception& e)
	{
		line = std::string("version.txt cannot be read (") + e.what() + "): no mods read";
	}
	if (log != nullptr)
	{
		log(line.c_str(), context);
	}
	return static_cast<int>(result);
}
