/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <exception>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "NetworkLand.h"
#include "OnlineMapLand.h"
#include "OnlineMapStoreInterface.h"
#include "StartRules.h"

namespace openblack::online
{

bool IsStoreFileName(std::string_view file) noexcept
{
	constexpr size_t k_MostCharacters = 64;
	if (file.empty() || file.size() > k_MostCharacters || file.starts_with('.') || !online_map_land::IsOnlineMap(file))
	{
		return false;
	}
	return std::ranges::all_of(file, [](char c) {
		const auto u = static_cast<unsigned char>(c);
		return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || u == '_' || u == '-' || u == '.' ||
		       u == ' ';
	});
}

FolderMapStore::FolderMapStore(filesystem::FileSystemInterface& files, std::optional<std::filesystem::path> profileFolder)
    : _files(files)
    , _folder(profileFolder.has_value() ? std::optional(*profileFolder / k_OnlineMapFolder) : std::nullopt)
{
}

std::optional<std::filesystem::path> FolderMapStore::PathOf(std::string_view file) const
{
	if (!_folder.has_value() || !IsStoreFileName(file))
	{
		return std::nullopt;
	}
	return *_folder / file;
}

void FolderMapStore::Find(std::string file, uint64_t checksum, FoundCallback done)
{
	const auto bytes = Read(file);
	done(bytes.has_value() && start_rules::LandChecksum(*bytes) == checksum);
}

std::optional<std::vector<uint8_t>> FolderMapStore::Read(std::string_view file) const
{
	const auto path = PathOf(file);
	if (!path.has_value())
	{
		return std::nullopt;
	}
	try
	{
		if (!_files.Exists(*path))
		{
			return std::nullopt;
		}
		return _files.ReadAll(*path);
	}
	catch (const std::exception& e)
	{
		if (const auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_WARN(logger, "The map {} does not read: {}", path->generic_string(), e.what());
		}
		return std::nullopt;
	}
}

void FolderMapStore::Write(std::string file, std::vector<uint8_t> bytes, WrittenCallback done)
{
	const auto path = PathOf(file);
	const bool written = path.has_value() && _files.MakeFolder(*_folder) && _files.WriteAll(*path, bytes);
	if (const auto logger = spdlog::get("game"); logger != nullptr && !written)
	{
		SPDLOG_LOGGER_WARN(logger, "The game could not keep the map {}", file);
	}
	done(written);
}

std::optional<std::vector<uint8_t>> ReadStartLand(std::string_view wireName, const OnlineMapStoreInterface* store,
                                                  filesystem::FileSystemInterface& files)
{
	const auto path = network_land::PathOf(wireName, files.GetPath<filesystem::Path::Scripts>());
	if (!path.has_value())
	{
		return std::nullopt;
	}
	if (store != nullptr && online_map_land::IsOnlineMap(wireName))
	{
		if (auto kept = store->Read(path->filename().generic_string()); kept.has_value())
		{
			return kept;
		}
	}
	try
	{
		if (!files.Exists(*path))
		{
			return std::nullopt;
		}
		return files.ReadAll(*path);
	}
	catch (const std::exception& e)
	{
		if (const auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_WARN(logger, "The land {} does not read: {}", path->generic_string(), e.what());
		}
		return std::nullopt;
	}
}

} // namespace openblack::online
