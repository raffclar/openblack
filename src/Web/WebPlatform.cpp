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
#include <memory>
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
		const frame = canvas.transferToImageBitmap();
		postMessage({ openblackFrame: frame }, [frame]);
	}
});
// This tells the page which canvas shows the game: the game canvas, which the browser shows when the game thread gives
// control back, or the screen canvas, which shows the images that the game thread sends.
EM_JS(void, openblack_set_present, (int canvas), {
	postMessage({ openblackPresent: canvas ? 'canvas' : 'image' });
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

/// The frames of the game that RunFrames runs, and their state
struct FrameRun
{
	std::function<bool()> frame;
	std::function<int()> end;
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

/// One frame of RunFrames, which the browser calls once in each of its display frames
void RunOneFrame(void* argument)
{
	auto& run = *static_cast<FrameRun*>(argument);
	auto& state = GetPresentState();
	state.inRunFrame = true;
	state.framesDrawn = 0;
	const bool goOn = run.frame();
	state.inRunFrame = false;
	if (state.framesDrawn == 1)
	{
		// The browser shows this frame when this function returns
		SetPresent(Present::Canvas);
	}
	else if (state.framesDrawn > 1)
	{
		SendFrame();
	}
	if (!goOn)
	{
		emscripten_cancel_main_loop();
		std::unique_ptr<FrameRun> owned(&run);
		const int code = owned->end();
		// This destroys the game, as the end of main does on the desktop
		owned.reset();
		emscripten_force_exit(code);
	}
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

void openblack::web::RunFrames(std::function<bool()> frame, std::function<int()> end)
{
	auto run = std::make_unique<FrameRun>(FrameRun {.frame = std::move(frame), .end = std::move(end)});
	// The browser calls the frames at its display rate. The run frees itself after its last frame.
	emscripten_set_main_loop_arg(RunOneFrame, run.release(), 0, false);
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
