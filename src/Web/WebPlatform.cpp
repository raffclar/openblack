/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WebPlatform.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

#include <emscripten.h>
#include <emscripten/wasmfs.h>
#include <unistd.h>

#include "Common/UserData.h"

// clang-format off
EM_JS(int, openblack_storage_available, (), {
	return globalThis.openblackWeb && globalThis.openblackWeb.storage ? 1 : 0;
});
// The canvas of the game thread is an OffscreenCanvas. Its picture goes to the page as an image, which the page draws.
EM_JS(void, openblack_present_frame, (), {
	const canvas = Module['canvas'];
	if (canvas && canvas.transferToImageBitmap) {
		const frame = canvas.transferToImageBitmap();
		postMessage({ openblackFrame: frame }, [frame]);
	}
});
EM_JS_DEPS(openblack_web_platform, "$UTF8ToString");
// clang-format on

namespace
{

/// The folder in the private storage of the site where the game keeps its profiles
constexpr std::string_view k_StorageFolder = "/storage";
constexpr std::string_view k_StorageProfiles = "/storage/openblack/Profiles";

/// Put the Profiles folder of the working folder in the private storage of the site, so that the profiles and saves
/// stay after the page closes. Returns false when the browser does not have this storage.
bool LinkStoredProfiles(const std::filesystem::path& profiles)
{
	if (openblack_storage_available() == 0)
	{
		return false;
	}
	const auto storage = wasmfs_create_opfs_backend();
	if (storage == nullptr || wasmfs_create_directory(std::string(k_StorageFolder).c_str(), 0777, storage) != 0)
	{
		return false;
	}
	std::error_code error;
	std::filesystem::create_directories(k_StorageProfiles, error);
	if (error)
	{
		return false;
	}
	std::filesystem::create_directory_symlink(k_StorageProfiles, profiles, error);
	return !error;
}

} // namespace

bool openblack::web::PrepareFileSystem()
{
	std::error_code error;
	std::filesystem::create_directories(k_WorkingFolder, error);
	std::filesystem::current_path(k_WorkingFolder, error);
	if (error)
	{
		std::cerr << "The web build cannot use the working folder " << k_WorkingFolder << ": " << error.message() << '\n';
	}

	const auto profiles = std::filesystem::path(k_WorkingFolder) / user_data::k_ProfileFolderName;
	if (!LinkStoredProfiles(profiles))
	{
		std::filesystem::create_directories(profiles, error);
		ShowPageMessage("Saves are not kept",
		                "This browser does not give the site a private storage. The game keeps its profiles and saves "
		                "only until the page closes.");
	}

	if (!MountUserFolder(k_GamePath))
	{
		ShowPageMessage("No game folder", "The page did not give a Black & White folder to the game.");
		return false;
	}
	return true;
}

void openblack::web::PresentFrame()
{
	openblack_present_frame();
}

void openblack::web::ShowPageMessage(std::string_view title, std::string_view text)
{
	const std::string titleText(title);
	const std::string bodyText(text);
	std::cerr << titleText << ": " << bodyText << '\n';
	// The page shows the message on its main thread. The call waits until the page has the text.
	// clang-format off
	MAIN_THREAD_EM_ASM({
		if (globalThis.openblackShowMessage) {
			globalThis.openblackShowMessage(UTF8ToString($0), UTF8ToString($1));
		}
	}, titleText.c_str(), bodyText.c_str());
	// clang-format on
}
