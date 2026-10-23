/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(__EMSCRIPTEN__)
#error "The web platform code is only for the web build"
#endif

#include <functional>
#include <string_view>

/// The parts of the web build that connect the game to the browser page
namespace openblack::web
{

/// The folder where the game finds the user's install of Black & White. The page gives it, and the game only reads it.
constexpr std::string_view k_GamePath = "/game";

/// The working folder of the game. Its Profiles folder goes to the private storage of the site in the browser, when the
/// browser has this storage.
constexpr std::string_view k_WorkingFolder = "/work";

/// Prepare the file system of the game before the game starts:
/// - Put the user's install that the page gives at k_GamePath. The game only reads it.
/// - Make k_WorkingFolder the working folder, with its Profiles folder in the private storage of the site.
/// Returns false when the page gave no install.
[[nodiscard]] bool PrepareFileSystem();

/// Run the frames of the game, one frame in each display frame of the browser. Between two frames the game thread waits
/// for the browser, which then shows the frame that the canvas has. When frame returns false, end runs, and the call
/// returns the exit code that end gives.
[[nodiscard]] int RunFrames(const std::function<bool()>& frame, const std::function<int()>& end);

/// Call before the renderer draws a frame. When the renderer draws more than one frame in a frame of RunFrames, as a
/// loading screen does, the frame that the canvas has goes to the page as an image before the next frame replaces it.
/// With a render thread (see StartRenderThread), this function does nothing.
void BeforeFrame();

/// Call after the renderer drew a frame. When a frame of RunFrames draws one frame only, the browser shows it when the
/// game thread gives control back. All other frames go to the page as images, because the game thread does not give
/// control back to the browser while it draws them. With a render thread, the render thread shows all frames. After a
/// frame that the game thread does not give control back for, the game thread takes the input events of the page that
/// wait for it.
void PresentFrame();

/// Start the thread that draws the frames of the renderer. This thread owns the canvas, and it gives control back to
/// the browser after each frame, so that the browser shows each frame. Call before the renderer starts. Returns false
/// when the thread did not start: the game thread then draws.
[[nodiscard]] bool StartRenderThread();

/// Wait until the render thread draws no more frames. Call after the renderer stops. The thread stays until the program
/// ends, because it owns the canvas.
void WaitForRenderThreadStop();

/// Show a message from the game on the page, under the game's picture
void ShowPageMessage(std::string_view title, std::string_view text);

/// Put the user's install that the page gives at the path. Returns false when the page gave no install.
[[nodiscard]] bool MountUserFolder(std::string_view path);

} // namespace openblack::web
