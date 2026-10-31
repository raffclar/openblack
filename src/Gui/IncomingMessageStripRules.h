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

#include "DialogRect.h"
#include "ECS/IncomingMessageRules.h"

/// The layout and the pictures of the incoming message at the bottom left of the screen: a 48 pixel button with the
/// picture of the message in the corner, and the line of text to the right of it. The layout is in the front end's
/// units, with its left edge on the screen's left edge and its bottom edge on the screen's bottom edge.
namespace openblack::gui::incoming_message_strip
{

/// The bottom of the layout, in the front end's units
constexpr int k_LayoutBottom = k_DialogSize.y;
/// The button is a square of this size in the bottom left corner
constexpr int k_ButtonSize = 48;
/// The line of text starts at the right of the button, 34 units above the bottom, 20 units high
constexpr int k_TextTop = k_LayoutBottom - 34;
constexpr int k_TextHeight = 20;
/// The line of text stops 5 units before the right edge of the screen
constexpr int k_TextRightGap = 5;
/// The size of the text
constexpr int k_TextSize = 18;
/// The button and the text are drawn at this opacity
constexpr float k_Alpha = 229.0f / 255.0f;
/// A button that is not pushed in is drawn 2 units up and to the left
constexpr int k_Lift = 2;

[[nodiscard]] constexpr DialogRect ButtonRect() noexcept
{
	return {.min = {0, k_LayoutBottom - k_ButtonSize}, .max = {k_ButtonSize, k_LayoutBottom}};
}
/// The line of text, for a layout of a width
[[nodiscard]] constexpr DialogRect TextRect(int layoutWidth) noexcept
{
	return {.min = {k_ButtonSize, k_TextTop}, .max = {layoutWidth - k_TextRightGap, k_TextTop + k_TextHeight}};
}

/// Where the layout's corner goes on the window, so that its left edge is on the window's left edge and its bottom on
/// the window's bottom. The layout keeps the scale of the front end's layout (FrontEndSpace).
[[nodiscard]] glm::vec2 Origin(glm::u16vec2 resolution) noexcept;
/// The width of the window in the layout's units
[[nodiscard]] int LayoutWidth(glm::u16vec2 resolution) noexcept;

/// Which texture a picture of the button comes from
enum class Sheet : uint8_t
{
	/// The front end's buttons (data/textures/Front_end_buttons.raw)
	Buttons,
	/// The texture of the text bubbles, which has the weather pictures (data/textures/gatheringtext.raw)
	Bubbles,
};

/// A picture of the button: the part of a texture that it shows
struct Picture
{
	Sheet sheet {Sheet::Buttons};
	glm::vec2 uvMin {0.0f};
	glm::vec2 uvMax {0.0f};
	bool operator==(const Picture&) const = default;
};

/// The picture of a look. The speech bubble pictures light up under the pointer; the others do not change.
[[nodiscard]] Picture PictureOf(incoming_messages::ButtonLook look, bool hovered) noexcept;

} // namespace openblack::gui::incoming_message_strip
