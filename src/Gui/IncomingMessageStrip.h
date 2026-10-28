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
#include <string>

#include <glm/vec2.hpp>

#include "Controls.h"
#include "ECS/IncomingMessageRules.h"

namespace openblack::graphics
{
class Texture2D;
}

namespace openblack::gui
{

/// The incoming message at the bottom left of the screen, with no box behind it: the button with the picture of the top
/// message, and its line of text to the right. The button and the text show only while a message is in the list and
/// the screen lets them show. A click on the button removes the message; the game decides what else the click does.
///
/// The button takes the mouse only when the pointer is over it, or while a mouse button that went down on it is held.
class IncomingMessageStrip
{
public:
	/// A click on the button
	struct Click
	{
		/// The click was with the right mouse button
		bool rightButton {false};
	};

	IncomingMessageStrip();
	IncomingMessageStrip(const IncomingMessageStrip&) = delete;
	IncomingMessageStrip& operator=(const IncomingMessageStrip&) = delete;

	/// Every frame: the look of the button, none when the button and the text do not show, the line of text, and the
	/// width of the window in the layout's units. With no text the line keeps the text that it had.
	void Update(std::optional<incoming_messages::ButtonLook> look, std::optional<std::u16string_view> text, int layoutWidth);
	/// The button and the text show
	[[nodiscard]] bool IsActive() const noexcept { return _look.has_value(); }
	/// Whether the button takes the mouse: a mouse button that went down on it is held, or, with no mouse button down,
	/// the pointer is over it
	[[nodiscard]] bool WantsMouse(bool mouseButtonDown) const;
	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point, bool rightButton);
	/// The click, when the mouse button goes up over the button that it went down on
	std::optional<Click> MouseUp(glm::ivec2 point);
	/// Draws the button and the text in the layout space, the weather pictures from the texture of the text bubbles. The
	/// caller puts the layout's bottom left corner on the screen's bottom left corner (incoming_message_strip::Origin).
	void Draw(const DialogPainter& painter, const graphics::Texture2D* bubbles) const;

	/// The look of the button that shows, none when nothing shows
	[[nodiscard]] const std::optional<incoming_messages::ButtonLook>& GetLook() const noexcept { return _look; }
	[[nodiscard]] std::u16string_view GetText() const noexcept { return _text.text; }

private:
	StaticText _text;
	std::optional<incoming_messages::ButtonLook> _look;
	bool _hovered {false};
	/// A mouse button went down on the button and is held. The button is pushed in until it goes up.
	std::optional<bool> _pressedWithRight;
};

} // namespace openblack::gui
