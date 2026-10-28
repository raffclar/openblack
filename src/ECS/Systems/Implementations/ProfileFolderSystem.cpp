/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ProfileFolderSystem.h"

#include <exception>
#include <fstream>
#include <utility>

#include <spdlog/spdlog.h>

#include "Common/PlayerProfiles.h"
#include "FileSystem/FileSystemInterface.h"

using namespace openblack::ecs::systems;

ProfileFolderSystem::ProfileFolderSystem(filesystem::FileSystemInterface& fileSystem,
                                         std::optional<std::filesystem::path> folder)
    : _fileSystem(fileSystem)
    , _folder(std::move(folder))
{
}

std::optional<std::filesystem::path> ProfileFolderSystem::GetPlayerFolder(std::u16string_view player) const
{
	if (!_folder.has_value() || player.empty())
	{
		return std::nullopt;
	}
	return *_folder / player_profiles::ProfileKey(player);
}

std::optional<std::filesystem::path> ProfileFolderSystem::MakeFolder(const std::filesystem::path& relative)
{
	if (!_folder.has_value())
	{
		return std::nullopt;
	}
	auto path = *_folder / relative;
	if (!_fileSystem.MakeFolder(path))
	{
		if (const auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_WARN(logger, "The game could not make the folder {}", path.generic_string());
		}
		return std::nullopt;
	}
	return path;
}

std::optional<std::vector<uint8_t>> ProfileFolderSystem::ReadPlayerFile(std::u16string_view player, std::string_view file) const
{
	const auto folder = GetPlayerFolder(player);
	if (!folder.has_value())
	{
		return std::nullopt;
	}
	const auto path = *folder / file;
	if (!_fileSystem.Exists(path))
	{
		return std::nullopt;
	}
	try
	{
		return _fileSystem.ReadAll(path);
	}
	catch (const std::exception& error)
	{
		if (const auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_WARN(logger, "The game could not read {}: {}", path.generic_string(), error.what());
		}
		return std::nullopt;
	}
}

bool ProfileFolderSystem::WritePlayerFile(std::u16string_view player, std::string_view file, std::span<const uint8_t> bytes)
{
	const auto folder = GetPlayerFolder(player);
	if (!folder.has_value())
	{
		return false;
	}
	if (!MakeFolder(folder->lexically_relative(*_folder)).has_value())
	{
		return false;
	}
	const auto path = *folder / file;
	const bool written = _fileSystem.WriteAll(path, bytes);
	if (const auto logger = spdlog::get("game"); !written && logger != nullptr)
	{
		SPDLOG_LOGGER_WARN(logger, "The game could not write {}", path.generic_string());
	}
	return written;
}

bool ProfileFolderSystem::AppendPlayerFile(std::u16string_view player, std::string_view file, std::span<const uint8_t> bytes)
{
	const auto folder = GetPlayerFolder(player);
	if (!folder.has_value() || !MakeFolder(folder->lexically_relative(*_folder)).has_value())
	{
		return false;
	}
	const auto path = *folder / file;
	std::ofstream stream(path, std::ios::binary | std::ios::app);
	stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	const bool written = stream.good();
	if (const auto logger = spdlog::get("game"); !written && logger != nullptr)
	{
		SPDLOG_LOGGER_WARN(logger, "The game could not write {}", path.generic_string());
	}
	return written;
}

bool ProfileFolderSystem::EmptyPlayerFolder(std::u16string_view player)
{
	const auto folder = GetPlayerFolder(player);
	if (!folder.has_value())
	{
		return false;
	}
	const bool emptied = _fileSystem.EmptyFolder(*folder);
	if (const auto logger = spdlog::get("game"); !emptied && logger != nullptr)
	{
		SPDLOG_LOGGER_WARN(logger, "The game could not empty the folder {}", folder->generic_string());
	}
	return emptied;
}
