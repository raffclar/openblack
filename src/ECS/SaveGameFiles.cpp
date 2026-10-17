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
