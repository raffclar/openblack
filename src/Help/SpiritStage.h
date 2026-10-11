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

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The part of the screen the advisors live in. The game was made for 4:3 screens and lays the advisors out in pixels
/// of the screen, sized by a field of view that runs across the screen's width, so on a wider screen they would come
/// out bigger for its height and wander further out to the sides. Instead they move in a 4:3 area as tall as the
/// screen, in its middle, and are drawn at the size they have on a 4:3 screen of that height. Screens 4:3 or taller
/// are the area themselves, so the advisors behave there exactly as in the game.
namespace openblack::help::spirits
{

/// A screen size in pixels
struct Screen
{
	uint16_t width {640};
	uint16_t height {480};
	[[nodiscard]] int32_t HalfWidth() const { return width >> 1; }
	[[nodiscard]] int32_t HalfHeight() const { return height >> 1; }

	bool operator==(const Screen&) const = default;
};

/// Where the advisors' area lies on the real screen
struct Stage
{
	/// The real screen
	Screen screen {};
	/// The area the advisors are laid out on, as if it were the whole screen
	Screen layout {};
	/// The area's left edge on the real screen, in pixels
	int32_t left {0};
	/// The area's width over the real screen's: how much smaller than the camera alone would draw them the advisors
	/// are drawn, 1 on screens 4:3 or taller
	float share {1.0f};

	/// A pixel of the real screen in the area
	[[nodiscard]] glm::vec2 ToLayout(glm::vec2 pixel) const { return {pixel.x - static_cast<float>(left), pixel.y}; }
	[[nodiscard]] glm::ivec2 ToLayout(glm::ivec2 pixel) const { return {pixel.x - left, pixel.y}; }
	/// A pixel of the area on the real screen
	[[nodiscard]] glm::vec2 ToScreen(glm::vec2 pixel) const { return {pixel.x + static_cast<float>(left), pixel.y}; }
};

/// The advisors' area on a screen: the 4:3 area of the screen's height in its middle on a wider screen (as the game's
/// 4:3 pictures are fitted), else the whole screen
[[nodiscard]] Stage StageOf(Screen screen);

/// An advisor's axes drawn at a share of their size across the screen, as a 4:3 screen of the same height shows them:
/// each axis's part across and up the screen is scaled by the share and its part along the camera's forward axis kept,
/// so that the advisor keeps its depth (and so its distance from the near plane) and, seen through the wider screen's
/// camera, its look on the 4:3 screen: its outline, its perspective and its light
[[nodiscard]] glm::mat3 ShrinkAcrossScreen(const glm::mat3& axes, const glm::vec3& forward, float share);
/// A vector in the world shrunk the same way
[[nodiscard]] glm::vec3 ShrinkAcrossScreen(const glm::vec3& vector, const glm::vec3& forward, float share);

} // namespace openblack::help::spirits
