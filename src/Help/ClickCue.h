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
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "ControlIcons.h"
#include "Input/KeyBindings.h"

/// The "Continue" cue while a script's text waits for a click: the picture of the input of the move action at the right
/// of the dialogue box's bottom third, the word "Continue" to its left in yellow, both on soft glows. A mouse binding
/// shows the mouse with its buttons that blink. A key binding shows the key cap. With no binding, the cue does not show.
namespace openblack::help::click_cue
{

/// The help text of the word: "Continue"
constexpr uint32_t k_LabelText = 3705;
/// It fades in over this long, in seconds of real time
constexpr float k_FadeSeconds = 1.0f;
/// The cue shows the input of this action, the move action
constexpr int k_Action = 1;

/// A rectangle in pixels
struct Box
{
	glm::vec2 min {0.0f};
	glm::vec2 max {0.0f};
};

/// The cue's pieces for a frame
struct Cue
{
	/// The mouse picture or the key cap
	Box icon;
	glm::vec4 iconColour {0.0f};
	/// The height of the icon, a third of the box's height
	int size {0};
	/// The glows behind the icon and the word, added to the picture
	Box iconGlow;
	glm::vec4 iconGlowColour {0.0f};
	Box labelGlow;
	glm::vec4 labelGlowColour {0.0f};
	/// The word: where it goes, its size and colour, over black shadows a pixel up-left and down-right
	glm::vec2 labelAt {0.0f};
	float labelSize {0.0f};
	glm::vec4 labelColour {0.0f};
	glm::vec4 shadowColour {0.0f};
};

/// Whether the blinking button is lit at a moment of the computer's clock: 200 ms of every 500
[[nodiscard]] bool ButtonLit(uint32_t tickMs);

/// The picture that the cue shows for the bindings: the mouse buttons of the move action, or its key cap when it has no
/// mouse binding. None when the move action has no binding.
[[nodiscard]] std::optional<control_icons::ActionPicture> PictureFor(std::span<const input::KeyBinding> bindings,
                                                                     const control_icons::KeyNameFn& keyName);

/// The cell of the mice picture for the cue's mouse in `row`. The buttons of `click` blink. When they are dark, the cell
/// shows the mouse with no button lit, and the left button keeps its mirrored picture. The wheel turns as it does on
/// the other icons.
[[nodiscard]] control_icons::MouseCell BlinkCellFor(control_icons::MouseClick click, int row, uint32_t tickMs);

/// The cue for a dialogue box between rows `top` and `bottom` on a screen so wide, `share` of the way through its fade.
/// `labelWidth` is the word's width at the label size, which is two thirds of the cue's size (four fifths for the
/// languages that need bigger text). `iconWidth` is the cue's size for the mouse, and the key cap's width for a key.
/// None when it is too faint to draw.
[[nodiscard]] std::optional<Cue> Layout(int screenWidth, int top, int bottom, float share, float labelWidth, bool biggerText,
                                        float iconWidth);
/// The cue's size for a box between two rows: a third of its height
[[nodiscard]] int CueSize(int top, int bottom);
/// The label's size for a box between two rows
[[nodiscard]] int LabelSize(int top, int bottom, bool biggerText);

} // namespace openblack::help::click_cue
