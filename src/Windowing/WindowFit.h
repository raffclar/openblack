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

/// Keeps a window's picture where the player can reach all of it. The camera is turned and tilted from the very edges
/// of the picture, so a window hanging off the screen or under the taskbar loses those controls.
namespace openblack::windowing
{

/// A rectangle of the desktop, in its pixels
struct DesktopRect
{
	glm::ivec2 position;
	glm::ivec2 size;
};

/// The frame a window's system decorations add round its picture
struct WindowBorders
{
	int top;
	int left;
	int bottom;
	int right;
};

/// Where a window's picture goes, and its size, so that the window and its frame fit in the usable part of the desktop
/// (the screen less the taskbar). Nothing is given when the window already fits where it is. Otherwise it is centred
/// there, made smaller first, keeping its shape, if it is too big.
[[nodiscard]] std::optional<DesktopRect> FitWindow(DesktopRect picture, WindowBorders borders, DesktopRect usable);

} // namespace openblack::windowing
