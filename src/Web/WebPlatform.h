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

/// Show the frame that the renderer drew on the page. The game thread never gives control back to the browser, so the
/// browser does not show the frames of the canvas of the game thread by itself.
void PresentFrame();

/// Show a message from the game on the page, under the game's picture
void ShowPageMessage(std::string_view title, std::string_view text);

/// Put the user's install that the page gives at the path. Returns false when the page gave no install.
[[nodiscard]] bool MountUserFolder(std::string_view path);

} // namespace openblack::web
