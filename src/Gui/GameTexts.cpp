/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameTexts.h"

#include <array>
#include <filesystem>
#include <vector>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

namespace openblack::gui
{
namespace
{
/// The text scripts of the game, the help texts first
constexpr std::array k_TextScripts = {"InfoScript2.txt", "InfoScriptPatch2.txt", "InfoScriptMultiplayer2.txt"};

std::vector<uint8_t> ReadIfExists(const std::filesystem::path& path)
{
	auto& fileSystem = Locator::filesystem::value();
	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Missing {} for the interface", path.generic_string());
		return {};
	}
	return fileSystem.ReadAll(path);
}
} // namespace

TextDatabase LoadGameTexts()
{
	auto& fileSystem = Locator::filesystem::value();
	TextDatabase texts;
	for (const auto* script : k_TextScripts)
	{
		const auto data = ReadIfExists(fileSystem.GetPath<filesystem::Path::Scripts>() / script);
		if (script == k_TextScripts.front())
		{
			texts.AddHelpScript(data);
		}
		else
		{
			texts.AddScript(data);
		}
	}
	return texts;
}

} // namespace openblack::gui
