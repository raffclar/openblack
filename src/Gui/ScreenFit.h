/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <glm/vec2.hpp>

/// Where a full-screen picture goes on a screen of any shape: as big as fits, keeping its own shape, centred, the rest
/// of the screen left to black bars. The game was made for 4:3 screens, where its 4:3 pictures fill the screen and its
/// 16:9 films sit between bars; on other shapes they keep their shape the same way.
namespace openblack::gui
{

/// A picture's shape, as its width to its height
struct Aspect
{
	int32_t width {4};
	int32_t height {3};

	bool operator==(const Aspect&) const = default;
};

/// The shape the game's own screens (logos, tips, pre-intro) are shown at
inline constexpr Aspect k_FourByThree {.width = 4, .height = 3};
/// The shape of the game's films
inline constexpr Aspect k_SixteenByNine {.width = 16, .height = 9};

/// A rectangle on the screen in pixels, from its top left corner
struct ScreenRect
{
	int32_t x {0};
	int32_t y {0};
	int32_t width {0};
	int32_t height {0};

	bool operator==(const ScreenRect&) const = default;
};

/// The biggest rectangle of `aspect` inside a screen, centred. The pixels it leaves over are shared between two bars,
/// each half of them rounded towards zero, as the game shares what a 16:9 film leaves of a 4:3 screen. A screen of
/// the picture's own shape is filled exactly
[[nodiscard]] ScreenRect FitInside(Aspect aspect, int32_t screenWidth, int32_t screenHeight) noexcept;

/// The game's default screen (when no resolution has been chosen), which its sizes in pixels were made for
inline constexpr int32_t k_ReferenceWidth = 800;
inline constexpr int32_t k_ReferenceHeight = 600;

/// How many pixels a pixel of the default screen becomes on a screen: the width of a 4:3 screen as tall as it, or its
/// own width when it is narrower than that, over the default screen's width. 1 on the default screen. Sizes in pixels
/// multiplied by it take the same share of a 4:3 screen, or of the 4:3 middle of a wider one, as on the default screen
[[nodiscard]] float ReferenceScale(int32_t screenWidth, int32_t screenHeight) noexcept;
/// A size in pixels on the default screen as whole pixels on a screen, rounded to nearest and at least one
[[nodiscard]] int32_t ScaledPixels(int32_t pixels, float scale) noexcept;

/// The front end's 800 by 600 layout placed on a screen: a layout point lands at offset + point * scale
struct LayoutSpace
{
	float scale {1.0f};
	glm::vec2 offset {0.0f, 0.0f};

	[[nodiscard]] glm::vec2 ToScreen(glm::vec2 layout) const noexcept { return (layout * scale) + offset; }
	[[nodiscard]] glm::vec2 ToLayout(glm::vec2 screen) const noexcept { return (screen - offset) / scale; }
	bool operator==(const LayoutSpace&) const = default;
};
/// Where the front end's layout goes on a screen. On a screen smaller than the default one in either direction it is
/// shrunk as the game shrinks it, by min(W / 850, H / 650) so a margin is left round it, centred. On the default screen
/// or a bigger one the game drew it a pixel a unit in the middle; here it is scaled to fill the screen's 4:3 middle as
/// on the default screen (ReferenceScale), centred, whole pixels from the screen's corner. Exactly the game's on
/// screens 800 or 600 pixels in one direction and at least the other in the other.
[[nodiscard]] LayoutSpace FrontEndSpace(int32_t screenWidth, int32_t screenHeight) noexcept;

} // namespace openblack::gui
