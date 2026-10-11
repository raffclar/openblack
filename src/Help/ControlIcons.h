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
#include <variant>

#include <SDL_scancode.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Input/KeyBindings.h"

/// The control icons a help text asks for with $m<n>, n an action numbered as input::BindableActionMap's bits: the
/// keys of the keyboard camera actions as one key cap in the top third of the dialogue box ("Shift + Left / Right"),
/// and the button of any other action as the mouse picture (or its key cap) in the bottom third, where the "Continue"
/// cue goes. The codes print nothing in the text; the icons sit at the right of the screen, 4 pixels in, a third of
/// the box high, fade in over 0.3 seconds on a white glow and go the moment the text changes.
namespace openblack::help::control_icons
{

/// The keyboard camera actions, zoom on to rotate on, show their keys in the top third
constexpr int k_FirstCameraKey = 6;
constexpr int k_LastCameraKey = 15;
/// The icons fade in over this long, in seconds of real time
constexpr float k_FadeSeconds = 0.3f;

/// The mouse picture's buttons, lit steadily (the wheel's turning shown frame by frame)
enum class MouseClick : uint8_t
{
	None,
	LeftButton,
	RightButton,
	BothButtons,
	MiddleButton,
	WheelDown,
	WheelUp,
};

/// What the bottom icon shows: a mouse picture, or the words of a key cap
using ActionPicture = std::variant<MouseClick, std::u16string>;

/// The name of a key as the keyboard shows it ("Shift", "Left", "Space"), Ctrl, Shift and Alt not telling the sides
/// apart
using KeyNameFn = std::function<std::u16string(SDL_Scancode)>;

/// Whether a code asks for the top icon's keys
[[nodiscard]] bool IsCameraKey(int action) noexcept;

/// The top icon's words for the codes of the newest text, in their order: the bound keys of the camera actions, the
/// second after " + " and the rest after " / ". The help system rebuilds them each frame from the action that made the
/// icon (the first bound one), so codes before its next mention are dropped. None when no camera action has a key.
[[nodiscard]] std::optional<std::u16string> KeyIconText(std::span<const int> codes, std::span<const input::KeyBinding> bindings,
                                                        const KeyNameFn& keyName);

/// The mouse buttons an action's binding shows. The zooms, with no mouse binding of their own, show both buttons while
/// move and action are on the left and right buttons. None when the action has no mouse binding.
[[nodiscard]] std::optional<MouseClick> ClickFor(int action, std::span<const input::KeyBinding> bindings);

/// The bottom icon for the codes of the newest text: the last code (other than the camera keys) whose action is bound,
/// its mouse picture, or failing that its key cap ("Ctrl + S"). None when there is none.
[[nodiscard]] std::optional<ActionPicture> ActionIcon(std::span<const int> codes, std::span<const input::KeyBinding> bindings,
                                                      const KeyNameFn& keyName);

/// A rectangle in pixels
struct Box
{
	glm::vec2 min {0.0f};
	glm::vec2 max {0.0f};

	[[nodiscard]] bool operator==(const Box&) const = default;
};

/// A cell of the mice picture (4 by 4), its corners on the texture, left and right swapped for the left button
struct MouseCell
{
	glm::vec2 uvMin {0.0f};
	glm::vec2 uvMax {0.0f};
};

/// The cell showing the buttons lit on the mice picture's row (the wheel mouse's on openblack), the wheel's frame
/// picked by the computer's clock
[[nodiscard]] MouseCell CellFor(MouseClick click, int row, uint32_t tickMs);

/// One icon placed for a frame
struct Placed
{
	/// The picture, or the whole key cap
	Box icon;
	/// The glow added behind it, white at up to half strength
	glm::vec4 glowColour {0.0f};
	/// The icon's strength
	float alpha {0.0f};
	/// The icon's size, a third of the box's height
	int size {0};
};

/// The size of the icons for a dialogue box between rows `top` and `bottom`
[[nodiscard]] int IconSize(int top, int bottom);
/// The top row of the key cap: the box's top third
[[nodiscard]] int KeyIconTop(int top, int bottom);
/// The top row of the bottom icon: the box's bottom third, where the "Continue" cue goes
[[nodiscard]] int ActionIconTop(int top, int bottom);

/// The key cap's width for its words, `textWidth` their width at KeyTextSize: the words and a third of the cap on
/// either side, never narrower than square
[[nodiscard]] float KeyCapWidth(int size, float textWidth);
/// The key cap's words' size, four tenths of the cap's height
[[nodiscard]] float KeyTextSize(int size);

/// An icon `iconWidth` wide with its top at `y` on a screen so wide, `share` of the way through its fade. None when it
/// is too faint to draw.
[[nodiscard]] std::optional<Placed> Place(int screenWidth, int y, int size, float iconWidth, float share);

/// The key cap drawn over a placed icon's box: three slices of the bevelled key of atmos.raw, a quarter of its height
/// at each end, and its words in black, a fifth of the height in and a seventh down
struct KeyCap
{
	std::array<Box, 3> slices {};
	std::array<Box, 3> uvs {};
	glm::vec2 textAt {0.0f};
	float textSize {0.0f};
	glm::vec4 textColour {0.0f};
};
[[nodiscard]] KeyCap LayoutKeyCap(const Placed& placed);

} // namespace openblack::help::control_icons
