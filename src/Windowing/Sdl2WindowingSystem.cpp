/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "Sdl2WindowingSystem.h"

#include <optional>
#include <string>

#include <SDL_syswm.h>
#include <spdlog/spdlog.h>

#include "Common/Platform.h"
#include "DisplayScaling.h"
#include "SystemPointer.h"
#include "Web/WebPointer.h"
#include "WindowFit.h"
#include "WindowPixels.h"
#include "WindowState.h"

#if defined(SDL_VIDEO_DRIVER_WAYLAND)
#include <wayland-egl.h>
#endif // defined(SDL_VIDEO_DRIVER_WAYLAND)
#if defined(SDL_VIDEO_DRIVER_COCOA)
void* cbSetupMetalLayer(void* wnd);
#endif

#include <SDL.h>

using namespace openblack::windowing;

namespace
{
// SDL's own messages go through the game's logger, so the player's log settings apply to them too. SDL's information
// messages are its internals' (one of its builds prints every relative mouse movement), so they are logged as debug.
void LogFromSdl(void* /*userdata*/, int /*category*/, SDL_LogPriority priority, const char* message)
{
	const auto logger = spdlog::get("graphics");
	if (logger == nullptr)
	{
		return;
	}
	switch (priority)
	{
	case SDL_LOG_PRIORITY_VERBOSE:
		logger->trace("SDL: {}", message);
		break;
	case SDL_LOG_PRIORITY_WARN:
		logger->warn("SDL: {}", message);
		break;
	case SDL_LOG_PRIORITY_ERROR:
		logger->error("SDL: {}", message);
		break;
	case SDL_LOG_PRIORITY_CRITICAL:
		logger->critical("SDL: {}", message);
		break;
	default:
		logger->debug("SDL: {}", message);
		break;
	}
}
} // namespace

void Sdl2WindowingSystem::SDLDestroyer::operator()(SDL_Window* window) const
{
	SDL_DestroyWindow(window);
}

Sdl2WindowingSystem::Sdl2WindowingSystem(const std::string& title, int width, int height, DisplayMode requestedDisplayMode,
                                         uint32_t extraFlags, bool background)
    : _background(background)
{
	const DisplayMode displayMode = BackgroundDisplayMode(requestedDisplayMode, background);
	SDL_version compiledVersion;
	SDL_VERSION(&compiledVersion);

	SDL_version linkedVersion;
	SDL_GetVersion(&linkedVersion);

	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Initializing SDL...");
	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "SDL Version/Compiled {}.{}.{}", //
	                   compiledVersion.major, compiledVersion.minor, compiledVersion.patch);
	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "SDL Version/Linked {}.{}.{}", //
	                   linkedVersion.major, linkedVersion.minor, linkedVersion.patch);

	SDL_LogSetOutputFunction(LogFromSdl, nullptr);

#if defined(_WIN32)
	// Windows does not stretch the window above 100% scale, so the picture stays sharp. Set this before SDL starts
	// its video part
	for (const auto& hint : k_WindowsDisplayScalingHints)
	{
		SDL_SetHint(std::string(hint.name).c_str(), std::string(hint.value).c_str());
	}
#endif

	// Initialize SDL
	if (SDL_WasInit(0) == 0)
	{
		SDL_SetMainReady();
		if (SDL_Init(0) != 0)
		{
			throw std::runtime_error("Could not initialize SDL: " + std::string(SDL_GetError()));
		}

		if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		{
			throw std::runtime_error("Could not initialize SDL Video Subsystem: " + std::string(SDL_GetError()));
		}

		if (SDL_InitSubSystem(SDL_INIT_TIMER) != 0)
		{
			throw std::runtime_error("Could not initialize SDL Timer Subsystem: " + std::string(SDL_GetError()));
		}
	}

	// The game draws its own pointer, the hand, so the pointer of the system hides over the window. A background run
	// leaves the pointer of the system to the user.
	system_pointer::Show(false, background);
	SDL_SetHint(SDL_HINT_VIDEO_EXTERNAL_CONTEXT, "1");
	// A second press makes a double click only within the system's small box round the first, as the game's window
	// took them (SDL's own allowance is far wider); the time between them is the system's already
#if defined(_WIN32)
	SDL_SetHint(SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS, std::to_string(GetSystemMetrics(SM_CXDOUBLECLK) / 2).c_str());
#else
	SDL_SetHint(SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS, "2");
#endif

	if (background)
	{
		// The window shows without becoming the active window, and takes no keyboard focus when it opens
		SDL_SetHint(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN, "1");
	}

	uint32_t flags = (background ? 0 : SDL_WINDOW_INPUT_FOCUS) | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | extraFlags;
	if (displayMode == DisplayMode::Fullscreen)
	{
		flags |= SDL_WINDOW_FULLSCREEN;
	}
	else if (displayMode == DisplayMode::Borderless)
	{
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	// Get SDL Window requirements from Renderer
	// A background run is placed by the game: the system's own placing of new windows can cut a window short when other
	// games are open. Other runs leave the placing to the system.
	const int x = background ? SDL_WINDOWPOS_CENTERED : SDL_WINDOWPOS_UNDEFINED;
	const int y = background ? SDL_WINDOWPOS_CENTERED : SDL_WINDOWPOS_UNDEFINED;

	auto window = std::unique_ptr<SDL_Window, SDLDestroyer>(SDL_CreateWindow(title.c_str(), x, y, width, height, flags));

	if (window == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Failed to create SDL2 window: '{}'", SDL_GetError());
		throw std::runtime_error("Failed creating SDL2 window: " + std::string(SDL_GetError()));
	}

	_window = std::move(window);
#if defined(_WIN32)
	float dpi = 0.0f;
	const int display = SDL_GetWindowDisplayIndex(_window.get());
	if (display >= 0 && SDL_GetDisplayDPI(display, &dpi, nullptr, nullptr) == 0)
	{
		// The log shows the scale of the monitor, and if Windows lets the game use the real pixels of each monitor
		const bool realPixels =
		    AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(), DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE;
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "The window opens on a monitor of {} DPI ({}% scale). Real pixels: {}", dpi,
		                   static_cast<int>(dpi * 100.0f / 96.0f + 0.5f), realPixels);
	}
#endif
	// The window listens to the pointer lock of the page from now. Before this, a lock of the pointer would not reach it.
	if constexpr (k_Platform == Platform::Web)
	{
		web::AllowPointerLock();
		const auto pixels = GetSize();
		const auto scale = GetPixelScale();
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "The game draws at {}x{} pixels, {} pixels for each CSS pixel", pixels.x,
		                   pixels.y, scale.x);
	}
	if (displayMode == DisplayMode::Windowed)
	{
		KeepInsideDesktop({width, height});
	}
}

void Sdl2WindowingSystem::KeepInsideDesktop(glm::ivec2 asked)
{
	auto* window = _window.get();
	const int display = SDL_GetWindowDisplayIndex(window);
	SDL_Rect usable;
	WindowBorders borders {};
	if (display < 0 || SDL_GetDisplayUsableBounds(display, &usable) != 0 ||
	    SDL_GetWindowBordersSize(window, &borders.top, &borders.left, &borders.bottom, &borders.right) != 0)
	{
		return;
	}
	DesktopRect picture {};
	SDL_GetWindowPosition(window, &picture.position.x, &picture.position.y);
	// The size that was asked for is the one to fit: the system may have cut the window short while it opened
	picture.size = asked;
	glm::ivec2 current {};
	SDL_GetWindowSize(window, &current.x, &current.y);
	const auto fitted = FitWindow(picture, borders, {.position = {usable.x, usable.y}, .size = {usable.w, usable.h}});
	if (!fitted.has_value())
	{
		if (current != asked)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"),
			                   "The window opened at {}x{} and not at the {}x{} that was asked for: made {}x{}", current.x,
			                   current.y, asked.x, asked.y, asked.x, asked.y);
			SDL_SetWindowSize(window, asked.x, asked.y);
		}
		return;
	}
	if (fitted->size != current)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"),
		                   "The window's {}x{} picture doesn't fit on the desktop with its frame ({}x{} usable): made {}x{}",
		                   picture.size.x, picture.size.y, usable.w, usable.h, fitted->size.x, fitted->size.y);
		SDL_SetWindowSize(window, fitted->size.x, fitted->size.y);
	}
	SDL_SetWindowPosition(window, fitted->position.x, fitted->position.y);
}

void* Sdl2WindowingSystem::GetHandle() const
{
	return _window.get();
}

Sdl2WindowingSystem::NativeHandles Sdl2WindowingSystem::GetNativeHandles() const
{
	NativeHandles result {nullptr, nullptr};
#if defined(__EMSCRIPTEN__)
	// In a web browser the renderer draws into the page's canvas, which it finds by its selector
	static const char* canvas = "#canvas";
	result.nativeWindow = const_cast<void*>(reinterpret_cast<const void*>(canvas));
	return result;
#else

	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);
	if (SDL_GetWindowWMInfo(_window.get(), &wmi) == 0u)
	{
		throw std::runtime_error("Failed getting native window info: " + std::string(SDL_GetError()));
	}

	// clang-format off
	// Linux
#if defined(SDL_VIDEO_DRIVER_WAYLAND)
	if (wmi.subsystem == SDL_SYSWM_WAYLAND)
	{
		auto* winImpl = static_cast<wl_egl_window*>(SDL_GetWindowData(_window.get(), "wl_egl_window"));
		if (winImpl == nullptr)
		{
			int width;
			int height;
			SDL_GetWindowSize(_window.get(), &width, &height);
			struct wl_surface* surface = wmi.info.wl.surface;
			if (surface == nullptr)
			{
				throw std::runtime_error("Failed getting native window handles: " + std::string(SDL_GetError()));
			}
			winImpl = wl_egl_window_create(surface, width, height);
			SDL_SetWindowData(_window.get(), "wl_egl_window", winImpl);
		}
		// NOLINTNEXTLINE(performance-no-int-to-ptr)
		result.nativeWindow = reinterpret_cast<void*>(winImpl);
		result.nativeDisplay = wmi.info.wl.display;
	}
	else
#endif // defined(SDL_VIDEO_DRIVER_WAYLAND)
#if defined(SDL_VIDEO_DRIVER_X11)
	if (wmi.subsystem == SDL_SYSWM_X11)
	{
		// NOLINTNEXTLINE(performance-no-int-to-ptr)
		result.nativeWindow = reinterpret_cast<void*>(wmi.info.x11.window);
		result.nativeDisplay = wmi.info.x11.display;
	}
	else
#endif // defined(SDL_VIDEO_DRIVER_X11)

	// Mac
#if defined(SDL_VIDEO_DRIVER_COCOA)
	if (wmi.subsystem == SDL_SYSWM_COCOA)
	{
		result.nativeWindow = cbSetupMetalLayer(wmi.info.cocoa.window);
		result.nativeDisplay = nullptr;
	}
	else
#endif // defined(SDL_VIDEO_DRIVER_COCOA)

	// Windows
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
	if (wmi.subsystem == SDL_SYSWM_WINDOWS)
	{
		result.nativeWindow = wmi.info.win.window;
		result.nativeDisplay = nullptr;
	}
	else
#endif // defined(SDL_VIDEO_DRIVER_WINDOWS)

	// Steam Link
#if defined(SDL_VIDEO_DRIVER_VIVANTE)
	if (wmi.subsystem == SDL_SYSWM_VIVANTE)
	{
		result.nativeWindow = wmi.info.vivante.window;
		result.nativeDisplay = wmi.info.vivante.display;
	}
	else
#endif // defined(SDL_VIDEO_DRIVER_VIVANTE)

	// Android
#if defined(SDL_VIDEO_DRIVER_ANDROID)
	if (wmi.subsystem == SDL_SYSWM_ANDROID)
	{
		result.nativeWindow = wmi.info.android.window;
		result.nativeDisplay = nullptr; // wmi.info.android.surface;
	}
	else
#endif // defined(SDL_VIDEO_DRIVER_ANDROID)
	{
		throw std::runtime_error("Unsupported platform or window manager: " + std::to_string(wmi.subsystem));
	}
	return result;
	// clang-format on
#endif // defined(__EMSCRIPTEN__)
}

bool Sdl2WindowingSystem::IsOpen() const
{
	return _window != nullptr;
}

float Sdl2WindowingSystem::GetBrightness() const
{
	return SDL_GetWindowBrightness(_window.get());
}

Sdl2WindowingSystem& Sdl2WindowingSystem::SetBrightness(float brightness)
{
	if (SDL_SetWindowBrightness(_window.get(), brightness) != 0)
	{
		throw std::runtime_error("SDL_SetWindowBrightness Error: " + std::string(SDL_GetError()));
	}
	return *this;
}

uint32_t Sdl2WindowingSystem::GetID() const
{
	return SDL_GetWindowID(_window.get());
}

uint32_t Sdl2WindowingSystem::GetFlags() const
{
	return SDL_GetWindowFlags(_window.get());
}

Sdl2WindowingSystem& Sdl2WindowingSystem::GrabInput(bool b)
{
	system_pointer::Grab(_window.get(), b, _background);
	return *this;
}

Sdl2WindowingSystem& Sdl2WindowingSystem::SetMousePosition(glm::ivec2 position)
{
	system_pointer::Warp(_window.get(), position, _background);
	return *this;
}

bool Sdl2WindowingSystem::IsInputGrabbed() const
{
	return SDL_GetWindowGrab(_window.get()) != SDL_FALSE;
}

std::string Sdl2WindowingSystem::GetTitle() const
{
	return SDL_GetWindowTitle(_window.get());
}

Sdl2WindowingSystem& Sdl2WindowingSystem::SetTitle(const std::string& str)
{
	SDL_SetWindowTitle(_window.get(), str.c_str());
	return *this;
}

float Sdl2WindowingSystem::GetAspectRatio() const
{
	const auto size = GetSize();
	return static_cast<float>(size.x) / static_cast<float>(size.y);
}

Sdl2WindowingSystem& Sdl2WindowingSystem::SetPosition(glm::ivec2 position)
{
	SDL_SetWindowPosition(_window.get(), position.x, position.y);
	return *this;
}

glm::ivec2 Sdl2WindowingSystem::GetPosition() const
{
	glm::ivec2 result;
	SDL_GetWindowPosition(_window.get(), &result.x, &result.y);
	return result;
}

Sdl2WindowingSystem& Sdl2WindowingSystem::SetMinimumSize(glm::ivec2 size)
{
	SDL_SetWindowMinimumSize(_window.get(), size.x, size.y);
	return *this;
}

glm::ivec2 Sdl2WindowingSystem::GetMinimumSize() const
{
	glm::ivec2 result;
	SDL_GetWindowMinimumSize(_window.get(), &result.x, &result.y);
	return result;
}

Sdl2WindowingSystem& Sdl2WindowingSystem::SetMaximumSize(glm::ivec2 size)
{
	SDL_SetWindowMaximumSize(_window.get(), size.x, size.y);
	return *this;
}

glm::ivec2 Sdl2WindowingSystem::GetMaximumSize() const
{
	glm::ivec2 result;
	SDL_GetWindowMaximumSize(_window.get(), &result.x, &result.y);
	return result;
}

Sdl2WindowingSystem& Sdl2WindowingSystem::SetSize(glm::ivec2 size)
{
	SDL_SetWindowSize(_window.get(), size.x, size.y);
	return *this;
}

bool Sdl2WindowingSystem::IsMinimised() const
{
	std::optional<bool> systemMinimised;
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);
	if (SDL_GetWindowWMInfo(_window.get(), &wmi) != SDL_FALSE && wmi.subsystem == SDL_SYSWM_WINDOWS)
	{
		systemMinimised = IsIconic(wmi.info.win.window) != FALSE;
	}
#endif // defined(SDL_VIDEO_DRIVER_WINDOWS)
	return WindowMinimised((SDL_GetWindowFlags(_window.get()) & SDL_WINDOW_MINIMIZED) != 0, systemMinimised);
}

bool Sdl2WindowingSystem::IsWindowed() const
{
	// Full screen on the desktop's own resolution has the full screen flag too
	return (SDL_GetWindowFlags(_window.get()) & SDL_WINDOW_FULLSCREEN) == 0;
}

glm::ivec2 Sdl2WindowingSystem::GetSize() const
{
	glm::ivec2 result;
	// In a web browser the window size is in CSS pixels. The game draws at the full resolution of the screen: the CSS
	// size times the device pixel ratio, which SDL follows at each resize event of the page.
	if constexpr (k_Platform == Platform::Web)
	{
		SDL_GetWindowSizeInPixels(_window.get(), &result.x, &result.y);
	}
	else
	{
		SDL_GetWindowSize(_window.get(), &result.x, &result.y);
	}
	return result;
}

glm::vec2 Sdl2WindowingSystem::GetPixelScale() const
{
	if constexpr (k_Platform == Platform::Web)
	{
		glm::ivec2 units;
		SDL_GetWindowSize(_window.get(), &units.x, &units.y);
		return PixelScale(units, GetSize());
	}
	else
	{
		return glm::vec2(1.0f);
	}
}

Sdl2WindowingSystem& Sdl2WindowingSystem::Show()
{
	SDL_ShowWindow(_window.get());
	return *this;
}

Sdl2WindowingSystem& Sdl2WindowingSystem::Hide()
{
	SDL_HideWindow(_window.get());
	return *this;
}

Sdl2WindowingSystem& Sdl2WindowingSystem::Maximise()
{
	SDL_MaximizeWindow(_window.get());
	return *this;
}

Sdl2WindowingSystem& Sdl2WindowingSystem::Minimise()
{
	SDL_MinimizeWindow(_window.get());
	return *this;
}

Sdl2WindowingSystem& Sdl2WindowingSystem::Restore()
{
	SDL_RestoreWindow(_window.get());
	return *this;
}

Sdl2WindowingSystem& Sdl2WindowingSystem::Raise()
{
	SDL_RaiseWindow(_window.get());
	return *this;
}

Sdl2WindowingSystem& Sdl2WindowingSystem::SetBordered(bool b)
{
	SDL_SetWindowBordered(_window.get(), b ? SDL_TRUE : SDL_FALSE);
	return *this;
}

Sdl2WindowingSystem& Sdl2WindowingSystem::SetFullscreen(bool f)
{
	// In a web browser only the page can show the game on the full screen, from its main thread
	if constexpr (k_Platform == Platform::Web)
	{
		web::RequestFullscreen(f);
		return *this;
	}
	// todo: use DisplayMode
	if (SDL_SetWindowFullscreen(_window.get(), f ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0)
	{
		throw std::runtime_error("SDL_SetWindowFullscreen Error: " + std::string(SDL_GetError()));
	}
	return *this;
}

Sdl2WindowingSystem& Sdl2WindowingSystem::Close()
{
	_window.reset(nullptr);
	return *this;
}
