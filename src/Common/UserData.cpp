/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UserData.h"

#include <string>
#include <system_error>

#include <ComputerSettings.h>
#include <SDL.h>

std::optional<std::filesystem::path> openblack::user_data::Folder()
{
	char* folder = SDL_GetPrefPath("openblack", "openblack");
	if (folder == nullptr)
	{
		return std::nullopt;
	}
	const std::string text(folder);
	SDL_free(folder);
	// The folder comes as UTF-8
	return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

std::optional<std::filesystem::path> openblack::user_data::ProfileFolder()
{
	std::error_code error;
	auto folder = std::filesystem::current_path(error);
	if (error)
	{
		return std::nullopt;
	}
	return folder / k_ProfileFolderName;
}

std::optional<std::filesystem::path> openblack::user_data::SettingsFile()
{
	const auto profiles = ProfileFolder();
	if (!profiles.has_value())
	{
		return std::nullopt;
	}
	std::error_code error;
	auto folder = *profiles;
	if (std::filesystem::is_symlink(folder, error))
	{
		const auto target = std::filesystem::read_symlink(folder, error);
		if (!error)
		{
			folder = target.is_absolute() ? target : profiles->parent_path() / target;
		}
	}
	return folder.parent_path() / openblack::computer_settings::k_FileName;
}
