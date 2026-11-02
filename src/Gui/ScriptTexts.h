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

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/gtc/type_precision.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The words the scripts write across the screen for their cut scenes, such as "Game Over" as the player's temple falls:
// each a box on the screen, its words broken into lines that fit and centred in it, fading in and out.

namespace openblack::gui::script_text
{

/// The most characters a text keeps; any more are cut off
constexpr size_t k_MaxCharacters = 1023;
/// A text whose alpha has fallen to this or below is gone
constexpr float k_GoneAlpha = 0.001f;

/// A text the scripts put on the screen
struct ScreenText
{
	std::u16string words;
	/// The box's top left corner and its width and height, as shares of the screen's width and height
	glm::vec2 corner {0.0f, 0.0f};
	glm::vec2 extent {0.0f, 0.0f};
	/// The height of a line, as a share of the screen's height
	float size {0.0f};
	/// The colour the scripts set when the text was written
	glm::u8vec3 colour {0, 0, 0};
	/// How much it shows, from 0 to 1, moving towards `target` by `rate` a second
	float alpha {0.0f};
	float target {0.0f};
	float rate {0.0f};

	/// Gives the values that a saved game keeps to a visitor
	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self.words, self.corner, self.extent, self.size, self.colour, self.alpha, self.target, self.rate);
	}
};

/// A text in the scripts' colour, fading in over `fadeSeconds`, or shown at once for none
[[nodiscard]] ScreenText Write(std::u16string_view words, glm::vec2 corner, glm::vec2 extent, float size, float fadeSeconds,
                               glm::u8vec3 colour);

/// Fades a text out over `seconds`, or takes it away at once for none
void FadeOut(ScreenText& text, float seconds);

/// Moves a text's fade on by the game's time of a frame. False once it has faded away, when it is gone.
[[nodiscard]] bool Step(ScreenText& text, int32_t milliseconds);

/// The alpha a text is drawn with, from 0 to 255
[[nodiscard]] uint8_t AlphaByte(float alpha);

/// The colour of the scripts' red, green and blue: each cut to a whole number, of which the bottom byte is kept
[[nodiscard]] glm::u8vec3 ColourFromScript(float red, float green, float blue);

/// A text's box on a screen, in pixels
struct ScreenBox
{
	glm::vec2 min {0.0f, 0.0f};
	glm::vec2 max {0.0f, 0.0f};
	/// The height of a line
	float size {0.0f};
};

/// Where a text's box is on a screen of a size in pixels: across by the width, down and the line's height by the height
[[nodiscard]] ScreenBox Place(const ScreenText& text, glm::vec2 screen);

/// A line of a text as it is drawn
struct DrawnLine
{
	std::u16string_view words;
	/// Its top left, in pixels
	glm::vec2 at {0.0f, 0.0f};
	/// How much of the text's alpha its top and its bottom are drawn with, 0 or 1: a line that runs past the bottom of
	/// the box fades out down to it
	float topShare {1.0f};
	float bottomShare {1.0f};
};

/// The width of a line of words at the box's line height, in pixels
using WidthFn = std::function<float(std::u16string_view words)>;

/// The lines a text's words broke into, as they are drawn in its box: each centred across it, one under the other
/// from its top. Nothing when the box is narrower than half a line's height or lower than a line. A line is drawn while
/// its top is within the box.
[[nodiscard]] std::vector<DrawnLine> Lay(const ScreenBox& box, std::span<const std::u16string_view> lines,
                                         const WidthFn& width);

/// The scripts' window: the screen goes black but for a box, the black fading in and out. The scripts open it to show a
/// scene in a part of the screen.
struct ClipWindow
{
	/// The box's left, top, width and height, as shares of the screen's width and height
	glm::vec2 corner {0.0f, 0.0f};
	glm::vec2 extent {0.0f, 0.0f};
	/// How much the black shows, from 0 to 1, moving towards `target` by `rate` a second
	float alpha {0.0f};
	float target {0.0f};
	float rate {0.0f};
};

/// The black shows only while its alpha is above this
constexpr float k_ClipWindowGoneAlpha = 0.001f;

/// Opens the window on a box, the black fading in over `seconds`, or shown at once for none
void OpenClipWindow(ClipWindow& window, glm::vec2 corner, glm::vec2 extent, float seconds);
/// Fades the black out over `seconds`, or removes it at once for none. The box stays where it was.
void CloseClipWindow(ClipWindow& window, float seconds);
/// Moves the fade of the black on by the game's time of a frame
void Step(ClipWindow& window, int32_t milliseconds);

/// A black box drawn round the window, in whole pixels from its top left corner to its bottom right corner
struct ClipBox
{
	glm::ivec2 min {0, 0};
	glm::ivec2 max {0, 0};
};
/// What the window draws on a screen of a size in pixels: the alpha of its black, from 0 to 255, and four boxes round
/// the window (above it, under it, to its left and to its right). Nothing while the black does not show.
struct ClipDraw
{
	uint8_t alpha {0};
	std::array<ClipBox, 4> boxes {};
};
[[nodiscard]] std::optional<ClipDraw> Draw(const ClipWindow& window, glm::u16vec2 screen);

} // namespace openblack::gui::script_text
