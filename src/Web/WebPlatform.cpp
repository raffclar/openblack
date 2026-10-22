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
#include <functional>
#include <iostream>
#include <optional>
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
// The canvas of the game thread is an OffscreenCanvas. This sends its picture to the page as an image, which the page
// draws on its screen canvas.
EM_JS(void, openblack_send_frame, (), {
	const canvas = Module['canvas'];
	if (canvas && canvas.transferToImageBitmap) {
		try {
			const frame = canvas.transferToImageBitmap();
			postMessage({ openblackFrame: frame }, [frame]);
		} catch (error) {
			if (!globalThis.openblackSendFrameFailed) {
				globalThis.openblackSendFrameFailed = true;
				console.error('openblack: the frame cannot go to the page: ' + error);
			}
		}
	}
});
// This tells the page which canvas shows the game: the game canvas, which the browser shows when the game thread gives
// control back, or the screen canvas, which shows the images that the game thread sends.
EM_JS(void, openblack_set_present, (int canvas), {
	postMessage({ openblackPresent: canvas ? 'canvas' : 'image' });
});
// The game thread waits for the next display frame of the browser. While it waits, the browser shows the frame that the
// canvas has, and completes the WebGPU work that waits for it. The program links with JSPI, which keeps the stack of the
// game thread while it waits.
EM_ASYNC_JS(void, openblack_wait_for_display_frame, (), {
	await new Promise((resolve) => {
		if (typeof requestAnimationFrame === 'function') {
			requestAnimationFrame(() => resolve());
		} else {
			setTimeout(resolve, 0);
		}
	});
});
EM_JS_DEPS(openblack_web_platform, "$UTF8ToString");
// clang-format on

namespace
{

/// How the page shows the frames of the game
enum class Present
{
	Image,  ///< The game thread sends each frame to the page as an image
	Canvas, ///< The browser shows the game canvas when the game thread gives control back
};

/// The state of the frame hand-off to the page. Only the game thread, which owns the canvas, uses it.
struct PresentState
{
	/// The game thread is in a frame of RunFrames
	bool inRunFrame {false};
	/// The frames that the renderer drew in this frame of RunFrames
	int framesDrawn {0};
	/// What the page shows now; nothing before the first frame
	std::optional<Present> shown;
};

PresentState& GetPresentState()
{
	static PresentState state;
	return state;
}

void SetPresent(Present present)
{
	auto& state = GetPresentState();
	if (state.shown != present)
	{
		state.shown = present;
		openblack_set_present(present == Present::Canvas ? 1 : 0);
	}
}

void SendFrame()
{
	openblack_send_frame();
	SetPresent(Present::Image);
}

/// One frame of RunFrames. Returns false after the last frame.
bool RunOneFrame(const std::function<bool()>& frame)
{
	auto& state = GetPresentState();
	state.inRunFrame = true;
	state.framesDrawn = 0;
	const bool goOn = frame();
	state.inRunFrame = false;
	if (state.framesDrawn == 1)
	{
		// The browser shows this frame while the game thread waits for the next display frame
		SetPresent(Present::Canvas);
	}
	else if (state.framesDrawn > 1)
	{
		SendFrame();
	}
	return goOn;
}

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

int openblack::web::RunFrames(const std::function<bool()>& frame, const std::function<int()>& end)
{
	// One frame in each display frame of the browser. A frame can also wait for the browser inside it, for example
	// while WebGPU reads a texture back. Thus the frames run in this loop, which JSPI can suspend at each wait, and not
	// in a main loop callback of Emscripten, which cannot wait.
	while (RunOneFrame(frame))
	{
		openblack_wait_for_display_frame();
	}
	return end();
}

void openblack::web::BeforeFrame()
{
	const auto& state = GetPresentState();
	if (state.inRunFrame && state.framesDrawn > 0)
	{
		// The next frame replaces the frame that the canvas has, before the browser can show it
		SendFrame();
	}
}

void openblack::web::PresentFrame()
{
	auto& state = GetPresentState();
	if (state.inRunFrame)
	{
		++state.framesDrawn;
		return;
	}
	SendFrame();
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
