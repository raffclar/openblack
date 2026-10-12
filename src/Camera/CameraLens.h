/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// How a camera's lens becomes the view on a screen of any shape. The game was made for 4:3 screens and gives its
/// lens as the view across the screen's width (70 degrees for the world). On 4:3 screens and narrower ones that is
/// exactly what is seen. A wider screen keeps the 4:3 screen's view up and down and sees further to the sides, so
/// its middle shows exactly what a 4:3 screen shows and the rest is extra picture at the sides. Pure maths, tested on
/// its own.
namespace openblack::camera_lens
{

/// The screen shape the game was made for: 4 across to 3 up
inline constexpr float k_ReferenceAspect = 4.0f / 3.0f;

/// The view up and down the screen, in radians
/// @param lens the view across the width of a 4:3 screen (or a narrower one), in radians, as the game sets it
/// @param aspect the screen's width over its height
[[nodiscard]] float VerticalFieldOfView(float lens, float aspect);

/// The view across the screen's width, in radians: the lens itself on 4:3 screens and narrower ones, wider on wider
/// screens
[[nodiscard]] float HorizontalFieldOfView(float lens, float aspect);

} // namespace openblack::camera_lens
