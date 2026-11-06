/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DownloadTestMap.h"

#include <cctype>

#include <algorithm>

#include <OnlineMapFile.h>

#include "OnlineMapLand.h"

namespace openblack::download_test_map
{

namespace
{
constexpr std::string_view k_LandscapeCommand = "LOAD_LANDSCAPE";

[[nodiscard]] bool StartsWithIgnoringCase(std::string_view text, std::string_view start)
{
	return text.size() >= start.size() &&
	       std::ranges::equal(text.substr(0, start.size()), start,
	                          [](unsigned char a, unsigned char b) { return std::toupper(a) == std::toupper(b); });
}

[[nodiscard]] std::string Upper(std::string_view text)
{
	std::string upper(text);
	std::ranges::transform(upper, upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	return upper;
}
} // namespace

std::string FileName()
{
	return std::string(k_File) + std::string(online_map_land::k_Extension);
}

std::optional<std::string> ScriptFor(std::string_view script, std::string_view file)
{
	// The lines keep the line end of the script
	const std::string_view lineEnd = script.find("\r\n") != std::string_view::npos ? "\r\n" : "\n";
	std::string out = std::string(k_FirstLine) + std::string(lineEnd);
	bool landscape = false;
	size_t at = 0;
	while (at < script.size())
	{
		auto end = script.find('\n', at);
		end = end == std::string_view::npos ? script.size() : end + 1;
		const auto line = script.substr(at, end - at);
		const auto first = line.find_first_not_of(" \t");
		const auto text = first == std::string_view::npos ? std::string_view {} : line.substr(first);
		if (!landscape && StartsWithIgnoringCase(text, k_LandscapeCommand))
		{
			// The landscape of a land of a network game has the land's name, so that the land loads its own landscape
			out += std::string(k_LandscapeCommand) + "(\".\\" + std::string(online_map_land::k_Folder) + "\\" + Upper(file) +
			       ".lnd\")" + std::string(lineEnd);
			landscape = true;
		}
		else
		{
			out += line;
		}
		at = end;
	}
	if (!landscape)
	{
		return std::nullopt;
	}
	return out;
}

std::optional<std::vector<uint8_t>> Make(std::span<const uint8_t> templateMap)
{
	const auto file = onlinemap::Parse(std::as_bytes(templateMap));
	if (!file.has_value())
	{
		return std::nullopt;
	}
	const auto script = ScriptFor(file->script, k_File);
	if (!script.has_value())
	{
		return std::nullopt;
	}
	const auto* landscape = reinterpret_cast<const uint8_t*>(file->landscape.data());
	return onlinemap::Write(*script, std::span(landscape, file->landscape.size()));
}

} // namespace openblack::download_test_map
