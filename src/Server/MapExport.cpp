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
#include <string>
#include <vector>

#include <OnlineMapFile.h>

#include "FileSystem/FileSystemInterface.h"
#include "Online/CustomMapPackage.h"
#include "Online/DownloadTestMap.h"
#include "Online/GodsRingMap.h"
#include "Online/GodsRingMapFile.h"
#include "Online/GodsRingMapPackage.h"
#include "Online/HttpClientInterface.h"
#include "Online/OnlineMapLand.h"
#include "Online/OnlineMapStoreInterface.h"
#include "Online/Sha256.h"

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

std::variant<std::filesystem::path, std::string> WriteMapPackage(const std::filesystem::path& folder)
{
	std::error_code error;
	std::filesystem::create_directories(folder, error);
	if (error)
	{
		return "the folder " + folder.generic_string() + " could not be made: " + error.message();
	}
	const auto package = gods_ring_map_package::Make();
	if (package.empty())
	{
		return std::string("the map package could not be made");
	}
	auto out = folder / gods_ring_map_package::FileName(package);
	if (const auto written = WriteBytes(out, package); written.has_value())
	{
		return *written;
	}
	return out;
}

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
	auto file = online_map_land::MapName(name);
	return CustomMapRead {.file = file, .name = file, .bytes = std::move(*bytes)};
}

std::variant<CustomMapRead, std::string> ReadMapPackage(const std::filesystem::path& path)
{
	const auto package = ReadBytes(path);
	if (!package.has_value())
	{
		return "the file " + path.generic_string() + " does not read";
	}
	if (package->size() > online::k_MostLargeAnswerBytes)
	{
		return "the file " + path.generic_string() + " is larger than 64 MiB";
	}
	auto assembled = custom_map_package::Assemble(*package);
	if (const auto* failure = std::get_if<gods_ring_map_package::Failure>(&assembled); failure != nullptr)
	{
		return "the map package " + path.generic_string() +
		       " gives no land: " + std::string(gods_ring_map_package::TextOf(*failure));
	}
	auto& land = std::get<custom_map_package::Assembled>(assembled);
	return CustomMapRead {
	    .file = land.manifest.file,
	    .name = land.manifest.name,
	    .bytes = std::move(land.map),
	    .packageSha256 = online::sha256::HexOf(*package),
	    .packageSize = static_cast<int64_t>(package->size()),
	};
}

std::variant<std::filesystem::path, std::string> WriteCustomMapPackage(const std::filesystem::path& customMap,
                                                                       std::string_view name, int32_t players,
                                                                       const std::filesystem::path& folder)
{
	auto read = ReadCustomMap(customMap);
	if (const auto* error = std::get_if<std::string>(&read); error != nullptr)
	{
		return *error;
	}
	const auto& map = std::get<CustomMapRead>(read);
	const auto package =
	    custom_map_package::Make(map.file, name.empty() ? std::string_view(map.file) : name, players, map.bytes);
	if (!package.has_value())
	{
		return "the map package of " + customMap.generic_string() + " could not be made: the script must load the landscape " +
		       map.file + ".lnd, the name has 1 to 64 characters, and the gods are 1 to " +
		       std::to_string(gods_ring_map::k_Slots);
	}
	std::error_code error;
	std::filesystem::create_directories(folder, error);
	if (error)
	{
		return "the folder " + folder.generic_string() + " could not be made: " + error.message();
	}
	auto out = folder / gods_ring_map_package::FileName(*package);
	if (const auto written = WriteBytes(out, *package); written.has_value())
	{
		return *written;
	}
	return out;
}

std::optional<std::string> WritePackageMap(const std::filesystem::path& package, const std::filesystem::path& out)
{
	auto read = ReadMapPackage(package);
	if (const auto* error = std::get_if<std::string>(&read); error != nullptr)
	{
		return *error;
	}
	return WriteBytes(out, std::get<CustomMapRead>(read).bytes);
}

} // namespace openblack::server
