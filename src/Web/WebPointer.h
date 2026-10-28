/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec2.hpp>

/// The pointer lock and the full screen of the web page. The page owns the pointer lock: a click on the game canvas
/// takes the pointer, and the browser frees it on Escape. The game thread asks the page through these calls, because
/// the browser lets only the main thread of the page lock the pointer or show the page on the full screen.
///
/// On the desktop these calls do nothing, so that code for all platforms can call them in a branch on k_Platform.
namespace openblack::web
{

#if defined(__EMSCRIPTEN__)

/// True while the page holds the pointer lock for the game canvas
[[nodiscard]] bool PointerLocked() noexcept;

/// The place in the window of the click that took the pointer lock, when the page knows it. The page cannot tell the
/// place of the pointer to the game before the pointer moves over the game, thus the click gives it. The place is in
/// CSS pixels, the units of the window's mouse events.
[[nodiscard]] std::optional<glm::ivec2> PointerLockClick() noexcept;

/// Tell the page that the game window listens to the pointer lock. Before this call, a click does not lock the pointer.
void AllowPointerLock();

/// Ask the page to free the pointer. The page frees it soon after; PointerLocked tells when.
void FreePointer();

/// Ask the page to show the game on the full screen, or to stop it. In full screen the page also asks the browser for
/// the keyboard lock, where the browser has it.
void RequestFullscreen(bool fullscreen);

#else

[[nodiscard]] inline bool PointerLocked() noexcept
{
	return false;
}

[[nodiscard]] inline std::optional<glm::ivec2> PointerLockClick() noexcept
{
	return std::nullopt;
}

inline void AllowPointerLock() {}

inline void FreePointer() {}

inline void RequestFullscreen([[maybe_unused]] bool fullscreen) {}

#endif

} // namespace openblack::web
