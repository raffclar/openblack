/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "IncomingMessageStrip.h"

#include "IncomingMessageStripRules.h"

namespace openblack::gui
{

using namespace incoming_message_strip;

IncomingMessageStrip::IncomingMessageStrip()
    : _text(TextRect(DialogPainter::k_Size.x), {}, StaticText::Layout::Left, k_TextSize)
{
}

void IncomingMessageStrip::Update(std::optional<incoming_messages::ButtonLook> look, std::optional<std::u16string_view> text,
                                  int layoutWidth)
{
	_look = look;
	if (!_look.has_value())
	{
		_hovered = false;
		_pressedWithRight.reset();
		return;
	}
	_text.rect = TextRect(layoutWidth);
	if (text.has_value())
	{
		_text.text = std::u16string(*text);
	}
}

bool IncomingMessageStrip::WantsMouse(bool mouseButtonDown) const
{
	if (!_look.has_value())
	{
		return false;
	}
	if (_pressedWithRight.has_value())
	{
		return true;
	}
	return !mouseButtonDown && _hovered;
}

void IncomingMessageStrip::MouseMove(glm::ivec2 point)
{
	_hovered = _look.has_value() && ButtonRect().Contains(point);
}

void IncomingMessageStrip::MouseDown(glm::ivec2 point, bool rightButton)
{
	MouseMove(point);
	if (_hovered)
	{
		_pressedWithRight = rightButton;
	}
}

std::optional<IncomingMessageStrip::Click> IncomingMessageStrip::MouseUp(glm::ivec2 point)
{
	MouseMove(point);
	const auto pressed = _pressedWithRight;
	_pressedWithRight.reset();
	if (!pressed.has_value() || !_hovered)
	{
		return std::nullopt;
	}
	return Click {.rightButton = *pressed};
}

void IncomingMessageStrip::Draw(const DialogPainter& painter, const graphics::Texture2D* bubbles) const
{
	if (!_look.has_value())
	{
		return;
	}
	painter.SetAlpha(k_Alpha);
	// The picture fills the button, pushed in while a mouse button is held on it
	const auto picture = PictureOf(*_look, _hovered);
	const auto lift = _pressedWithRight.has_value() ? 0 : -k_Lift;
	const auto rect = ButtonRect();
	const DialogRect drawn {.min = rect.min + lift, .max = rect.max + lift};
	if (picture.sheet == Sheet::Buttons)
	{
		painter.DrawBox(drawn, picture.uvMin, picture.uvMax);
	}
	else if (bubbles != nullptr)
	{
		painter.DrawPicture(drawn, *bubbles, picture.uvMin, picture.uvMax, glm::vec4(1.0f));
	}
	_text.Draw(painter, false, false, false);
	painter.SetAlpha(1.0f);
}

} // namespace openblack::gui
