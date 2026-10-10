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

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

/// The "Continue" cue while a script's text waits for a click: the mouse with its left button blinking at the right of
/// the dialogue box's bottom third, the word "Continue" to its left in yellow, both on soft glows
namespace openblack::help::click_cue
{

/// The help text of the word: "Continue"
constexpr uint32_t k_LabelText = 3705;
/// It fades in over this long, in seconds of real time
constexpr float k_FadeSeconds = 1.0f;
/// The mice picture's rows: two buttons, three buttons, a wheel mouse
constexpr int k_WheelMouseRow = 2;

/// A rectangle in pixels
struct Box
{
	glm::vec2 min {0.0f};
	glm::vec2 max {0.0f};
};

/// The cue's pieces for a frame
struct Cue
{
	/// The mouse picture, its place on the texture (left and right swapped for the left button)
	Box mouse;
	glm::vec2 uvMin {0.0f};
	glm::vec2 uvMax {0.0f};
	/// The glows behind the mouse and the word, added to the picture
	Box mouseGlow;
	glm::vec4 mouseGlowColour {0.0f};
	Box labelGlow;
	glm::vec4 labelGlowColour {0.0f};
	/// The word: where it goes, its size and colour, over black shadows a pixel up-left and down-right
	glm::vec2 labelAt {0.0f};
	float labelSize {0.0f};
	glm::vec4 labelColour {0.0f};
	glm::vec4 shadowColour {0.0f};
	glm::vec4 mouseColour {0.0f};
};

/// Whether the blinking button is lit at a moment of the computer's clock: 200 ms of every 500
[[nodiscard]] bool ButtonLit(uint32_t tickMs);

/// The cue for a dialogue box between rows `top` and `bottom` on a screen so wide, `share` of the way through its fade;
/// `labelWidth` the word's width at the label size, which is two thirds of the cue's (four fifths for the languages
/// that need bigger text). None when it is too faint to draw.
[[nodiscard]] std::optional<Cue> Layout(int screenWidth, int top, int bottom, float share, float labelWidth, bool biggerText,
                                        uint32_t tickMs);
/// The label's size for a box between two rows
[[nodiscard]] int LabelSize(int top, int bottom, bool biggerText);

} // namespace openblack::help::click_cue
