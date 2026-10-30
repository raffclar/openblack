/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UserData.h"

#include <cerrno>
#include <cstdlib>

#include <string>
#include <system_error>

#include <ComputerSettings.h>

#if defined(_WIN32)
// clang-format off: the shell folders need the types of windows.h first
#include <windows.h>
#include <shlobj.h>
// clang-format on
#elif defined(__ANDROID__)
#include <SDL.h>
#elif !defined(__APPLE__)
#include <sys/stat.h>
#endif

// The folder is the folder that SDL gives for the organisation "openblack" and the application "openblack". The test
// test_input_device compares the two. The macOS version is in UserDataApple.mm.
#if defined(_WIN32)
std::optional<std::filesystem::path> openblack::user_data::Folder()
{
	// The roaming application data of the user, made if it is not there yet
	std::wstring path(MAX_PATH, L'\0');
	if (!SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, 0, path.data())))
	{
		return std::nullopt;
	}
	path.resize(path.find(L'\0'));
	constexpr std::wstring_view k_Name = L"openblack";
	// The folder has a length limit of MAX_PATH, with its terminator
	if (path.size() + (2 * k_Name.size()) + 3 + 1 > MAX_PATH)
	{
		return std::nullopt;
	}
	for (int level = 0; level < 2; ++level)
	{
		path += L'\\';
		path += k_Name;
		if (CreateDirectoryW(path.c_str(), nullptr) == FALSE && GetLastError() != ERROR_ALREADY_EXISTS)
		{
			return std::nullopt;
		}
	}
	path += L'\\';
	return std::filesystem::path(path);
}
#elif defined(__ANDROID__)
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
#elif !defined(__APPLE__)
std::optional<std::filesystem::path> openblack::user_data::Folder()
{
	std::string folder;
#if defined(__EMSCRIPTEN__)
	// The file system of the web page has the folder at its root
	folder = "/libsdl/";
#else
	// The data folder of the user by the XDG specification, else the one in the home folder
	if (const char* data = std::getenv("XDG_DATA_HOME"); data != nullptr)
	{
		folder = data;
		folder += folder.ends_with('/') ? "" : "/";
	}
	else if (const char* home = std::getenv("HOME"); home != nullptr)
	{
		folder = home;
		folder += folder.ends_with('/') ? ".local/share/" : "/.local/share/";
	}
	else
	{
		return std::nullopt;
	}
#endif
	folder += "openblack/openblack/";
	// Make each folder of the path that is not there yet
	for (size_t slash = folder.find('/', 1); slash != std::string::npos; slash = folder.find('/', slash + 1))
	{
		if (mkdir(folder.substr(0, slash).c_str(), 0700) != 0 && errno != EEXIST)
		{
			return std::nullopt;
		}
	}
	if (mkdir(folder.c_str(), 0700) != 0 && errno != EEXIST)
	{
		return std::nullopt;
	}
	// The folder is UTF-8
	return std::filesystem::path(std::u8string(folder.begin(), folder.end()));
}
#endif

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
