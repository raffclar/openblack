/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OnlineMapTexts.h"

#include <filesystem>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

openblack::gui::OnlineMapTexts openblack::gui::ReadOnlineMapTexts()
{
	if (!Locator::filesystem::has_value())
	{
		return {};
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto readText = [&fileSystem](const std::filesystem::path& path) {
		if (!fileSystem.Exists(path))
		{
			return std::string();
		}
		const auto bytes = fileSystem.ReadAll(path);
		return std::string(bytes.begin(), bytes.end());
	};
	const std::filesystem::path folder = "Online Maps";
	return {.localMaps = readText(folder / "localmaps.txt"), .conditionTemplate = readText(folder / "ConditionTemplate.txt")};
}
