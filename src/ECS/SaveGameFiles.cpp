/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SaveGameFiles.h"

#include <exception>
#include <tuple>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

using namespace openblack;

std::optional<std::vector<uint8_t>> ecs::save_files::ReadFile(const std::filesystem::path& path)
{
	if (!Locator::filesystem::has_value() || !Locator::filesystem::value().Exists(path))
	{
		return std::nullopt;
	}
	try
	{
		return Locator::filesystem::value().ReadAll(path);
	}
	catch (const std::exception& error)
	{
		if (const auto log = spdlog::get("game"); log != nullptr)
		{
			SPDLOG_LOGGER_WARN(log, "The game could not read {}: {}", path.generic_string(), error.what());
		}
		return std::nullopt;
	}
}

std::filesystem::path ecs::save_files::TemporaryPath(const std::filesystem::path& path)
{
	auto temporary = path;
	temporary += k_TemporaryEnding;
	return temporary;
}

ecs::save_files::SaveWrite ecs::save_files::WriteThroughTemporaries(filesystem::FileSystemInterface& files,
                                                                    std::span<const SaveFile> group)
{
	using WriteResult = filesystem::FileSystemInterface::WriteResult;
	for (size_t i = 0; i < group.size(); ++i)
	{
		const auto result = files.WriteFile(TemporaryPath(group[i].path), group[i].bytes);
		if (result == WriteResult::Written)
		{
			continue;
		}
		// Only the first file not opening means that the game wrote nothing
		return i == 0 && result == WriteResult::NotOpened ? SaveWrite::NotOpened : SaveWrite::Failed;
	}
	// The game removes the old file first and then gives the temporary file its name, and looks at neither result
	for (const auto& file : group)
	{
		std::ignore = files.Remove(file.path);
		std::ignore = files.Rename(TemporaryPath(file.path), file.path);
	}
	return SaveWrite::Written;
}

ecs::save_files::SlotWriteResult ecs::save_files::WriteSlot(filesystem::FileSystemInterface& files, const SlotWrite& slot)
{
	SlotWriteResult result;
	// The game makes each folder that is missing and does not look at the result. A folder that is not there makes the
	// writes into it fail.
	for (const auto& folder : slot.folders)
	{
		if (!files.MakeFolder(folder))
		{
			if (const auto log = spdlog::get("game"); log != nullptr)
			{
				SPDLOG_LOGGER_WARN(log, "The game could not make the folder {}", folder.generic_string());
			}
		}
	}
	if (slot.pictures && !slot.pictures(files))
	{
		++result.otherFailures;
	}
	if (!files.WriteAll(slot.info.path, slot.info.bytes))
	{
		++result.otherFailures;
	}
	// The temporary world file opens before the game writes the world and the files that come with it
	if (files.WriteFile(TemporaryPath(slot.world), {}) == filesystem::FileSystemInterface::WriteResult::NotOpened)
	{
		result.world = SaveWrite::NotOpened;
		return result;
	}
	for (const auto& step : slot.withWorld)
	{
		if (!step(files))
		{
			++result.otherFailures;
		}
	}
	const auto group = slot.worldFiles ? slot.worldFiles() : std::vector<SaveFile> {};
	result.world = group.empty() ? SaveWrite::Failed : WriteThroughTemporaries(files, group);
	return result;
}
