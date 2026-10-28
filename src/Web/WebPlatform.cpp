/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WebPlatform.h"

#include <cstdlib>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <system_error>

#include <bgfx/bgfx.h>
#include <emscripten.h>
#include <emscripten/threading.h>
#include <emscripten/wasmfs.h>
#include <pthread.h>
#include <unistd.h>

#include "Common/BuildVersion.h"
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
// The render thread gives control back to the browser for one task. The browser then shows the frame that the canvas
// has. A message on a channel does not wait for a display frame or for the minimum time of a timer.
EM_ASYNC_JS(void, openblack_yield_to_browser, (), {
	if (!globalThis.openblackYieldChannel) {
		globalThis.openblackYieldChannel = new MessageChannel();
	}
	const channel = globalThis.openblackYieldChannel;
	await new Promise((resolve) => {
		channel.port1.onmessage = () => resolve();
		channel.port2.postMessage(0);
	});
});
// The text of the version file of the page (version.txt), which the page gives to the game threads. Null without it.
EM_JS(char*, openblack_page_version, (), {
	const text = globalThis.openblackWeb && globalThis.openblackWeb.version;
	return typeof text === 'string' ? stringToNewUTF8(text) : 0;
});
// The host of the page, with its port. The game thread reads its own location, which has the host of the page. The
// game does not use emscripten_run_script for this, because the page's content security policy refuses eval.
EM_JS(char*, openblack_page_host, (), {
	return stringToNewUTF8(typeof location === 'object' && location ? location.host : '');
});
// The Join Online Game button of the page writes 1 into the first slot of the shared requests array. The game thread
// reads the slot and puts 0 back in one step, so that it takes each press one time.
EM_JS(int, openblack_take_join_online, (), {
	const requests = globalThis.openblackWeb && globalThis.openblackWeb.requests;
	return requests ? Atomics.exchange(requests, 0, 0) : 0;
});
EM_JS_DEPS(openblack_web_platform, "$UTF8ToString,$stringToNewUTF8");
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

/// The thread that draws the frames of the renderer and owns the canvas
struct RenderThread
{
	/// The game uses the render thread
	std::atomic<bool> running {false};
	/// The thread is the render thread of bgfx
	std::atomic<bool> ready {false};
	/// The renderer stopped, and the thread draws no more frames
	std::atomic<bool> stopped {false};
};

RenderThread& GetRenderThread()
{
	static RenderThread thread;
	return thread;
}

void* RenderThreadMain(void* /*unused*/)
{
	auto& renderThread = GetRenderThread();
	// A call before the renderer starts makes this thread the render thread of bgfx
	bgfx::renderFrame();
	renderThread.ready.store(true);
	renderThread.ready.notify_all();
	openblack_set_present(1);
	while (bgfx::renderFrame() != bgfx::RenderFrame::Exiting)
	{
		// The browser shows the frame when this thread gives control back
		openblack_yield_to_browser();
	}
	renderThread.stopped.store(true);
	renderThread.stopped.notify_all();
	// The thread owns the canvas. Thus it stays until the program ends, because the browser changes the size of the
	// canvas through this thread, also when the game closes its window.
	emscripten_exit_with_live_runtime();
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
	if (GetRenderThread().running.load())
	{
		return goOn;
	}
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

/// Put the version file of the page into the working folder, where the game reads it as the version file next to the
/// game. Without the file of the page, the game finds no version file.
void WritePageVersion()
{
	const std::unique_ptr<char, decltype(&free)> text(openblack_page_version(), free);
	if (!text)
	{
		return;
	}
	std::ofstream file(std::filesystem::path(openblack::web::k_WorkingFolder) / openblack::build_version::k_FileName,
	                   std::ios::binary);
	file << text.get();
}

} // namespace

std::string openblack::web::PageHost()
{
	const std::unique_ptr<char, decltype(&free)> host(openblack_page_host(), free);
	return host ? std::string(host.get()) : std::string();
}

bool openblack::web::TakeJoinOnlineRequest()
{
	return openblack_take_join_online() != 0;
}

bool openblack::web::PrepareFileSystem()
{
	std::error_code error;
	std::filesystem::create_directories(k_WorkingFolder, error);
	std::filesystem::current_path(k_WorkingFolder, error);
	if (error)
	{
		std::cerr << "The web build cannot use the working folder " << k_WorkingFolder << ": " << error.message() << '\n';
	}

	WritePageVersion();

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

bool openblack::web::StartRenderThread()
{
	auto& renderThread = GetRenderThread();
	pthread_attr_t attributes;
	pthread_attr_init(&attributes);
	pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
	// The canvas goes from the game thread to the render thread
	emscripten_pthread_attr_settransferredcanvases(&attributes, "#canvas");
	pthread_t thread {};
	const bool started = pthread_create(&thread, &attributes, RenderThreadMain, nullptr) == 0;
	pthread_attr_destroy(&attributes);
	if (!started)
	{
		return false;
	}
	renderThread.running.store(true);
	renderThread.ready.wait(false);
	return true;
}

void openblack::web::WaitForRenderThreadStop()
{
	auto& renderThread = GetRenderThread();
	if (renderThread.running.exchange(false))
	{
		renderThread.stopped.wait(false);
	}
}

void openblack::web::BeforeFrame()
{
	if (GetRenderThread().running.load())
	{
		return;
	}
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
	}
	else if (!GetRenderThread().running.load())
	{
		SendFrame();
	}
	// The page sends its input events to the game thread as calls that wait in the queue of the thread. The thread does
	// these calls when it gives control back to the browser, which it does only between two frames of RunFrames. A
	// start-up screen, such as the new player's name box, and a loading screen draw their frames in a loop of their own
	// and read the input between them. Thus, after such a frame, the thread does the waiting calls at once.
	if (!state.inRunFrame || state.framesDrawn > 1)
	{
		emscripten_current_thread_process_queued_calls();
	}
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
