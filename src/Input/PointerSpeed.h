/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>

namespace openblack::input::pointer_speed
{

/// The screen height the pointer's speed is matched to: the game's default 800x600 screen
inline constexpr float k_ReferenceHeight = 600.0f;

/// How many pixels the pointer moves on this screen for each pixel the same mouse movement moved it on the 800x600
/// screen, so that the same movement sweeps the same share of the view. The view's middle keeps the 4:3 picture (its
/// height on 4:3 and wider screens, its width on narrower ones), so the scale is the height of the screen's 4:3 middle
/// over 600: 1 at 800x600, 1.6 at 1280x1024, 1.8 at 1920x1080, 2.4 at 1440 pixels high. A screen of no size is 1.
[[nodiscard]] float Scale(glm::ivec2 screenSize);

/// The pointer as the mouse moves it at a scale: it keeps the parts of a pixel the scale leaves, so slow movements add
/// up, and it stays on the screen
class ScaledPointer
{
public:
	/// Puts the pointer on a pixel
	void Place(glm::ivec2 position);
	/// Moves the pointer by the mouse's movement times the scale, kept on the screen; returns the pixel it is now on
	glm::ivec2 Move(glm::ivec2 movement, float scale, glm::ivec2 screenSize);
	/// The pixel the pointer is on
	[[nodiscard]] glm::ivec2 Position() const;

private:
	glm::vec2 _position {0.0f};
};

} // namespace openblack::input::pointer_speed
