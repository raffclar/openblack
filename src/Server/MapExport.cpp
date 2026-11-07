/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapExport.h"

#include <cstdint>

#include <fstream>
#include <iterator>
#include <span>
#include <vector>

#include <OnlineMapFile.h>

#include "FileSystem/FileSystemInterface.h"
#include "Online/DownloadTestMap.h"
#include "Online/GodsRingMap.h"
#include "Online/GodsRingMapFile.h"
#include "Online/HttpClientInterface.h"
#include "Online/OnlineMapLand.h"
#include "Online/OnlineMapStoreInterface.h"

namespace openblack::server
{

std::optional<std::string> WriteGodsRingMap(const std::filesystem::path& gamePath, const std::filesystem::path& out)
{
	const auto templatePath =
	    filesystem::FileSystemInterface::FindIgnoringCase(gamePath, std::filesystem::path(gods_ring_map::k_TemplateMap));
	if (!templatePath.has_value())
	{
		return "the game folder has no " + std::string(gods_ring_map::k_TemplateMap);
	}
	std::ifstream in(*templatePath, std::ios::binary);
	const std::vector<uint8_t> templateMap((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	const auto file = gods_ring_map_file::Make(templateMap);
	if (!file.has_value())
	{
		return "the land " + templatePath->generic_string() + " gives no village for the map";
	}
	std::ofstream stream(out, std::ios::binary | std::ios::trunc);
	stream.write(reinterpret_cast<const char*>(file->data()), static_cast<std::streamsize>(file->size()));
	if (!stream.good())
	{
		return "the file " + out.generic_string() + " could not be written";
	}
	return std::nullopt;
}

namespace
{
[[nodiscard]] std::optional<std::vector<uint8_t>> ReadBytes(const std::filesystem::path& path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

[[nodiscard]] std::optional<std::string> WriteBytes(const std::filesystem::path& out, const std::vector<uint8_t>& bytes)
{
	std::ofstream stream(out, std::ios::binary | std::ios::trunc);
	stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if (!stream.good())
	{
		return "the file " + out.generic_string() + " could not be written";
	}
	return std::nullopt;
}
} // namespace

std::optional<std::string> WriteDownloadTestMap(const std::filesystem::path& gamePath, const std::filesystem::path& out)
{
	const auto templatePath =
	    filesystem::FileSystemInterface::FindIgnoringCase(gamePath, std::filesystem::path(download_test_map::k_TemplateMap));
	const auto templateMap = templatePath.has_value() ? ReadBytes(*templatePath) : std::nullopt;
	if (!templateMap.has_value())
	{
		return "the game folder has no " + std::string(download_test_map::k_TemplateMap);
	}
	const auto file = download_test_map::Make(*templateMap);
	if (!file.has_value())
	{
		return "the land " + templatePath->generic_string() + " is not a land of a network game with a landscape";
	}
	return WriteBytes(out, *file);
}

std::variant<CustomMapRead, std::string> ReadCustomMap(const std::filesystem::path& path)
{
	const auto name = path.filename().generic_string();
	if (!online::IsStoreFileName(name))
	{
		return "the name " + name + " is not a plain file name that ends with .map";
	}
	auto bytes = ReadBytes(path);
	if (!bytes.has_value())
	{
		return "the file " + path.generic_string() + " does not read";
	}
	if (bytes->size() > online::k_MostLargeAnswerBytes || !onlinemap::Parse(std::as_bytes(std::span(*bytes))).has_value())
	{
		return "the file " + path.generic_string() + " is not a land of a network game of at most 64 MiB";
	}
	return CustomMapRead {.file = online_map_land::MapName(name), .bytes = std::move(*bytes)};
}

} // namespace openblack::server
