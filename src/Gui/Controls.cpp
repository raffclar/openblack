/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Controls.h"

#include <cmath>

#include <algorithm>
#include <chrono>
#include <string_view>
#include <utility>

#include <SDL_keyboard.h>
#include <SDL_keycode.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include "Common/MachineClock.h"
#include "GameFont.h"

using namespace openblack::gui;

namespace
{
using Justify = DialogPainter::Justify;

/// The game's colour for a control's text: orange under the pointer, white with the focus, grey otherwise
glm::vec4 TextColour(bool hovered, bool focused)
{
	if (hovered)
	{
		return DialogPainter::k_HoverColour;
	}
	return focused ? DialogPainter::k_FocusColour : DialogPainter::k_TextColour;
}

/// Big buttons' and sliders' labels are at the mid text size
constexpr int k_LabelSize = DialogPainter::k_MidTextSize;
/// How far a click on a slider beside the knob moves it
constexpr float k_SliderStep = 0.1f;
/// A list's gap below each item's text
constexpr int k_ItemGap = 6;
} // namespace

StaticText::StaticText(DialogRect rect, std::u16string text, Layout layout, int size)
    : Control(rect)
    , text(std::move(text))
    , layout(layout)
    , size(size)
{
}

void StaticText::Draw(const DialogPainter& painter, bool /*hovered*/, bool /*focused*/, bool /*pressed*/) const
{
	if (!visible || text.empty())
	{
		return;
	}
	// Static text shrinks the text until it fits: wrapped text in the height, a line of text in the width
	const bool wrapped = layout == Layout::Wrapped || layout == Layout::WrappedLeft;
	auto fitted = size;
	const auto overflows = [&](int textSize) {
		if (wrapped)
		{
			return painter.GetTextHeight(rect.Width(), text, textSize) > static_cast<float>(rect.Height());
		}
		return painter.GetTextWidth(text, textSize) > static_cast<float>(rect.Width());
	};
	while (fitted > 10 && overflows(fitted))
	{
		--fitted;
	}

	const auto shadow = DialogPainter::k_ShadowColour;
	const auto white = DialogPainter::k_FocusColour;
	if (wrapped)
	{
		const bool centred = layout == Layout::Wrapped;
		painter.DrawTextWrapped({.min = rect.min + 2, .max = rect.max + 2}, centred, text, fitted, shadow);
		painter.DrawTextWrapped(rect, centred, text, fitted, white);
		return;
	}
	const auto y = rect.Centre().y - (fitted / 2);
	auto x = rect.Centre().x;
	auto justify = Justify::Centre;
	if (layout == Layout::Left)
	{
		x = rect.min.x;
		justify = Justify::Left;
	}
	else if (layout == Layout::Right)
	{
		x = rect.max.x;
		justify = Justify::Right;
	}
	painter.DrawText({x + 2, y + 2}, rect.Width(), justify, text, fitted, shadow);
	painter.DrawText({x, y}, rect.Width(), justify, text, fitted, white);
}

Button::Button(DialogRect rect, std::u16string label, int size)
    : Control(rect)
    , label(std::move(label))
    , size(size)
{
}

void Button::Draw(const DialogPainter& painter, bool hovered, bool focused, bool /*pressed*/) const
{
	if (!visible)
	{
		return;
	}
	// A button draws the second style of box, the third under the pointer, and the label shrunk to fit
	painter.DrawBevelBox(rect, hovered ? 2 : 1, DialogPainter::All, glm::vec4(1.0f));
	auto fitted = size;
	while (fitted > 10 && painter.GetTextWidth(label, fitted) > static_cast<float>(rect.Width()))
	{
		--fitted;
	}
	painter.DrawText({rect.Centre().x, rect.Centre().y - (fitted / 2)}, rect.Width(), Justify::Centre, label, fitted,
	                 TextColour(hovered, focused));
}

BigButton::BigButton(const GameFont& font, glm::ivec2 position, int size, std::u16string label, LabelSide side, Look look)
    : Control({.min = position, .max = position + size})
    , label(std::move(label))
    , _font(font)
    , _side(side)
    , _look(look)
{
}

DialogRect BigButton::GetLabelRect() const
{
	// The label's text as it is drawn, its width cut to whole pixels
	const auto width = static_cast<int>(_font.GetWidth(label, static_cast<float>(_labelSize)));
	const auto top = rect.Centre().y - (_labelSize / 2);
	switch (_side)
	{
	case LabelSide::Right:
		return {.min = {rect.max.x + _labelGap, top}, .max = {rect.max.x + _labelGap + width, top + _labelSize}};
	case LabelSide::Left:
		return {.min = {rect.min.x - _labelGap - width, top}, .max = {rect.min.x - _labelGap, top + _labelSize}};
	case LabelSide::Below:
	default:
	{
		const auto left = rect.Centre().x - (width / 2);
		return {.min = {left, rect.max.y + 2}, .max = {left + width, rect.max.y + 2 + _labelSize}};
	}
	}
}

bool BigButton::HitTest(glm::ivec2 point) const
{
	return visible && (rect.Contains(point) || (!label.empty() && GetLabelRect().Contains(point)));
}

void BigButton::Draw(const DialogPainter& painter, bool hovered, bool /*focused*/, bool pressed) const
{
	if (!visible)
	{
		return;
	}
	switch (_look)
	{
	case Look::Square:
		painter.DrawSquare(rect.min, rect.Width(), _checked, hovered, pressed && hovered);
		break;
	case Look::LeftArrow:
	case Look::RightArrow:
		painter.DrawArrow(rect.min, rect.Width(),
		                  _look == Look::LeftArrow ? DialogPainter::Arrow::Left : DialogPainter::Arrow::Right, hovered,
		                  pressed && hovered);
		break;
	case Look::Rotation:
		painter.DrawRotation(rect.min, rect.Width(), hovered);
		break;
	}
	DrawLabel(painter, hovered);
}

void BigButton::DrawLabel(const DialogPainter& painter, bool hovered) const
{
	if (label.empty())
	{
		return;
	}
	// A big button draws a shadow two pixels down and right, then the label in white, orange under the pointer
	const auto colour = hovered ? DialogPainter::k_HoverColour : DialogPainter::k_FocusColour;
	const auto shadow = DialogPainter::k_ShadowColour;
	const auto top = rect.Centre().y - (_labelSize / 2);
	switch (_side)
	{
	case LabelSide::Right:
		painter.DrawText({rect.max.x + _labelGap + 2, top + 2}, 1000, Justify::Left, label, _labelSize, shadow);
		painter.DrawText({rect.max.x + _labelGap, top}, 1000, Justify::Left, label, _labelSize, colour);
		break;
	case LabelSide::Left:
		painter.DrawText({rect.min.x - _labelGap + 2, top + 2}, 1000, Justify::Right, label, _labelSize, shadow);
		painter.DrawText({rect.min.x - _labelGap, top}, 1000, Justify::Right, label, _labelSize, colour);
		break;
	case LabelSide::Below:
		painter.DrawText({rect.Centre().x + 2, rect.max.y + 4}, 1000, Justify::Centre, label, _labelSize, shadow);
		painter.DrawText({rect.Centre().x, rect.max.y + 2}, 1000, Justify::Centre, label, _labelSize, colour);
		break;
	}
}

CheckBox::CheckBox(const GameFont& font, glm::ivec2 position, std::u16string label, bool checked)
    : CheckBox(font, position, k_DefaultSize, std::move(label), LabelSide::Below, checked, false)
{
}

CheckBox::CheckBox(const GameFont& font, glm::ivec2 position, int size, std::u16string label, LabelSide side, bool checked,
                   bool radio)
    : BigButton(font, position, size, std::move(label), side, Look::Square)
    , _radio(radio)
{
	_checked = checked;
	// A check box's label beside it stands four pixels further off than a big button's
	_labelGap = 4;
}

bool CheckBox::HitTest(glm::ivec2 point) const
{
	if (!visible)
	{
		return false;
	}
	const auto offset = point - rect.Centre();
	const auto radius = rect.Width() / 2;
	return (offset.x * offset.x) + (offset.y * offset.y) < radius * radius ||
	       (!label.empty() && GetLabelRect().Contains(point));
}

void CheckBox::Activate(glm::ivec2 point)
{
	_checked = _radio || !_checked;
	if (onChange)
	{
		onChange(_checked);
	}
	BigButton::Activate(point);
}

Slider::Slider(DialogRect rect, std::u16string label, float value)
    : Control(rect)
    , label(std::move(label))
    , _value(std::clamp(value, 0.0f, 1.0f))
{
}

void Slider::SetValue(float value) noexcept
{
	_value = std::clamp(value, 0.0f, 1.0f);
}

int Slider::GetKnobLeft(float value) const
{
	// The knob is as wide as the bar is high
	return rect.min.x + static_cast<int>(static_cast<float>(rect.Width() - rect.Height()) * value);
}

void Slider::Draw(const DialogPainter& painter, bool hovered, bool focused, bool /*pressed*/) const
{
	if (!visible)
	{
		return;
	}
	painter.DrawBevelBox(rect, 1, DialogPainter::All, glm::vec4(1.0f));
	const auto knob = GetKnobLeft(_value);
	painter.DrawSquare({knob + 3, rect.min.y + 3}, rect.Height() - 6, false, hovered || focused, true);
	painter.DrawText({rect.Centre().x, rect.Centre().y - (k_LabelSize / 2)}, rect.Width(), Justify::Centre, label, k_LabelSize,
	                 focused ? DialogPainter::k_FocusColour : DialogPainter::k_TextColour);
}

void Slider::MouseDown(glm::ivec2 point)
{
	_downX = point.x;
	_downValue = _value;
}

void Slider::Drag(glm::ivec2 point)
{
	// Dragging a slider: the knob follows the pointer from where it was grabbed, or steps a tenth towards it
	const auto knob = GetKnobLeft(_downValue);
	const auto range = static_cast<float>(rect.Width() - rect.Height());
	const bool onKnob = _downX >= knob && _downX < knob + rect.Height();
	const auto step = _downX < knob ? -k_SliderStep : k_SliderStep;
	const auto moved = onKnob ? static_cast<float>(point.x - _downX) / range : step;
	const auto value = std::clamp(_downValue + moved, 0.0f, 1.0f);
	if (value != _value)
	{
		_value = value;
		if (onChange)
		{
			onChange(_value);
		}
	}
}

List::List(const GameFont& font, DialogRect rect, int size, bool centred, bool scrollBar, bool showSelection)
    : Control(rect)
    , _font(font)
    , _size(size)
    , _centred(centred)
    , _scrollBar(scrollBar)
    , _showSelection(showSelection)
{
}

int List::GetBoxRight() const
{
	return _scrollBar ? rect.max.x - 2 - GetScrollBarWidth() : rect.max.x;
}

int List::GetContentHeight() const
{
	int height = 0;
	for (const auto h : _heights)
	{
		height += h;
	}
	return height;
}

void List::SetItems(std::vector<Item> items)
{
	_items = std::move(items);
	_heights.clear();
	const auto width = static_cast<float>(GetBoxRight() - rect.min.x - 8);
	for (const auto& item : _items)
	{
		const auto lines = std::max<size_t>(1, _font.Wrap(item.text, static_cast<float>(_size), width).size());
		// The game measures the item's text with its text drawing, which leaves a gap below it
		// TODO(raffclar): the gap is measured from the original's screens, not worked out from its code
		_heights.push_back((static_cast<int>(lines) * _size) + k_ItemGap);
	}
	if (_selection && *_selection >= _items.size())
	{
		_selection.reset();
	}
	ScrollTo(_scroll);
}

std::optional<size_t> List::ItemAt(int y) const
{
	auto top = rect.min.y - static_cast<int>(_scroll);
	for (size_t i = 0; i < _items.size(); ++i)
	{
		if (y >= top && y < top + _heights[i])
		{
			return i;
		}
		top += _heights[i];
	}
	return std::nullopt;
}

void List::ScrollTo(float scroll)
{
	const auto most = static_cast<float>(std::max(0, GetContentHeight() - rect.Height()));
	_scroll = std::clamp(scroll, 0.0f, most);
}

void List::Draw(const DialogPainter& painter, bool hovered, bool focused, bool /*pressed*/) const
{
	if (!visible)
	{
		return;
	}
	const auto boxRight = GetBoxRight();
	painter.DrawBevelBox({.min = rect.min, .max = {boxRight, rect.max.y}}, 1, DialogPainter::All, glm::vec4(1.0f));

	const auto hoveredItem = hovered && _pointer.x < boxRight ? ItemAt(_pointer.y) : std::nullopt;
	const auto alpha = painter.GetAlpha();
	auto top = rect.min.y - static_cast<int>(_scroll);
	for (size_t i = 0; i < _items.size(); ++i)
	{
		const auto bottom = top + _heights[i];
		if (bottom > rect.min.y && top < rect.max.y)
		{
			const auto& item = _items[i];
			const auto selected = _showSelection && _selection == i;
			if (selected)
			{
				painter.DrawBevelBox(
				    {.min = {rect.min.x, std::max(top, rect.min.y)}, .max = {boxRight, std::min(bottom, rect.max.y)}}, 0,
				    DialogPainter::All, glm::vec4(1.0f));
			}
			const auto colour = selected ? DialogPainter::k_FocusColour : TextColour(hoveredItem == i, focused);

			// The list fades the lines the box cuts through
			const auto left = rect.min.x + 4;
			const auto right = boxRight - 4;
			const auto lines = painter.GetLines(right - left, item.text, _size);
			for (size_t l = 0; l < lines.size(); ++l)
			{
				const auto lineTop = top + 2 + (static_cast<int>(l) * _size);
				const auto inside = std::min(lineTop + _size, rect.max.y) - std::max(lineTop, rect.min.y);
				if (inside <= 0)
				{
					continue;
				}
				painter.SetAlpha(alpha * static_cast<float>(inside) / static_cast<float>(_size));
				const auto lineWidth = painter.GetTextWidth(lines[l], _size);
				const auto lineLeft = _centred ? ((left + right) / 2) - (static_cast<int>(lineWidth) / 2) : left;
				painter.DrawText({lineLeft, lineTop}, right - left, Justify::Left, lines[l], _size, colour);

				// The key binding dialog's mice, in the gap of five spaces before the button's name
				if (l == 0 && item.mouseCell && lineTop >= rect.min.y && lineTop + _size <= rect.max.y)
				{
					const auto bracket = lines[l].find(u'(');
					const auto prefix =
					    bracket != std::u16string_view::npos && bracket >= 6 ? lines[l].substr(0, bracket - 6) : lines[l];
					const auto x = lineLeft + static_cast<int>(painter.GetTextWidth(prefix, _size));
					painter.DrawMouse({.min = {x, lineTop}, .max = {x + _size, lineTop + _size}}, *item.mouseCell,
					                  item.mouseMirrored);
				}
			}
			painter.SetAlpha(alpha);
		}
		top = bottom;
	}

	if (_scrollBar)
	{
		const auto barLeft = boxRight + 2;
		painter.DrawBevelBox({.min = {barLeft, rect.min.y}, .max = {barLeft + GetScrollBarWidth(), rect.max.y}}, 1,
		                     DialogPainter::All, glm::vec4(1.0f));
		const auto track = static_cast<float>(rect.Height() - 6);
		const auto content = static_cast<float>(std::max(GetContentHeight(), rect.Height()));
		const auto thumbTop = static_cast<int>(track * _scroll / content);
		const auto thumbBottom = static_cast<int>(track * (_scroll + static_cast<float>(rect.Height())) / content);
		painter.DrawBevelBox({.min = {barLeft + 3, rect.min.y + 3 + thumbTop},
		                      .max = {barLeft + GetScrollBarWidth() - 3, rect.min.y + 3 + thumbBottom}},
		                     0, DialogPainter::All, glm::vec4(1.0f));
	}
}

void List::MouseDown(glm::ivec2 point)
{
	_draggingThumb = false;
	if (_scrollBar && point.x >= GetBoxRight())
	{
		const auto track = static_cast<float>(rect.Height() - 6);
		const auto content = static_cast<float>(std::max(GetContentHeight(), rect.Height()));
		const auto thumbTop = rect.min.y + 3 + static_cast<int>(track * _scroll / content);
		const auto thumbBottom =
		    rect.min.y + 3 + static_cast<int>(track * (_scroll + static_cast<float>(rect.Height())) / content);
		if (point.y >= thumbTop && point.y < thumbBottom)
		{
			_draggingThumb = true;
			_grabY = point.y;
			_grabScroll = _scroll;
		}
		else
		{
			// A page up or down
			ScrollTo(_scroll + static_cast<float>(point.y < thumbTop ? -rect.Height() : rect.Height()));
		}
		return;
	}
	if (const auto item = ItemAt(point.y))
	{
		_selection = item;
	}
}

void List::Drag(glm::ivec2 point)
{
	if (_draggingThumb)
	{
		const auto track = static_cast<float>(rect.Height() - 6);
		const auto content = static_cast<float>(std::max(GetContentHeight(), rect.Height()));
		ScrollTo(_grabScroll + (static_cast<float>(point.y - _grabY) * content / track));
	}
}

void List::Wheel(int steps)
{
	ScrollTo(_scroll - static_cast<float>(steps * _size));
}

EditBox::EditBox(const GameFont& font, DialogRect rect, std::u16string text, size_t maxLength, int size)
    : Control(rect)
    , _font(font)
    , _state {.text = std::move(text), .maxLength = maxLength}
    , _size(size)
{
	SetText(std::u16string(_state.text));
}

int EditBox::Width(std::u16string_view text) const
{
	return static_cast<int>(_font.GetWidth(text, static_cast<float>(_size)));
}

int EditBox::CharacterAt(glm::ivec2 point) const
{
	return text_edit::CharacterAt(_state, rect.min.x, point.x, [this](std::u16string_view text) { return Width(text); });
}

void EditBox::Changed(bool textChanged)
{
	text_edit::Scroll(_state, rect.Width(), [this](std::u16string_view text) { return Width(text); });
	if (textChanged && onChange)
	{
		onChange(_state.text);
	}
}

void EditBox::SetText(std::u16string_view text)
{
	_state.text = std::u16string(text.substr(0, std::min(text.size(), _state.maxLength)));
	const auto end = static_cast<int>(_state.text.size());
	_state.caret = end;
	_state.selectionStart = end;
	_state.selectionEnd = end;
	_state.scroll = 0;
	Changed(false);
}

void EditBox::Draw(const DialogPainter& painter, bool /*hovered*/, bool focused, bool /*pressed*/) const
{
	if (!visible)
	{
		return;
	}
	painter.DrawBevelBox(rect, 1, DialogPainter::All, glm::vec4(1.0f));
	const auto text = std::u16string_view(_state.text);
	const auto scroll = static_cast<size_t>(std::clamp(_state.scroll, 0, static_cast<int>(text.size())));
	const auto shown = text.substr(scroll);
	const int left = rect.min.x + 4;
	const int top = rect.Centre().y - (_size / 2);
	const int room = rect.Width() - 8;
	painter.DrawText({left, top}, room, Justify::Left, shown, _size, DialogPainter::k_FocusColour);
	if (!focused)
	{
		return;
	}
	// Widths from the first character shown
	const auto widthTo = [this, &text, scroll](int place) {
		const auto end = static_cast<size_t>(std::clamp(place, static_cast<int>(scroll), static_cast<int>(text.size())));
		return Width(text.substr(scroll, end - scroll));
	};
	const auto start = widthTo(_state.selectionStart);
	const auto end = widthTo(_state.selectionEnd);
	const auto caret = widthTo(_state.caret);
	const auto low = std::min({start, end, room});
	const auto high = std::min(std::max(start, end) + 1, room);
	if (low >= high)
	{
		return;
	}
	const auto pale = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f);
	const auto band = [&painter, &pale, this](int from, int to) {
		const auto y0 = static_cast<float>(rect.min.y + 2);
		const auto y1 = static_cast<float>(rect.max.y - 2);
		const auto x0 = static_cast<float>(from);
		const auto x1 = static_cast<float>(to);
		painter.DrawShape({glm::vec2(x0, y0), {x1, y0}, {x1, y1}, {x0, y1}}, {pale, pale, pale, pale});
	};
	// The caret shows every other fifth of a second
	if ((machine_clock::Ticks() / 200u) % 2u == 1u && caret >= 0 && caret < room)
	{
		band(rect.min.x + 5 + caret, rect.min.x + 7 + caret);
	}
	if (_state.selectionStart != _state.selectionEnd)
	{
		band(left + low, rect.min.x + 5 + high);
		const auto first =
		    static_cast<size_t>(std::max(std::min(_state.selectionStart, _state.selectionEnd), static_cast<int>(scroll)));
		const auto last =
		    static_cast<size_t>(std::max(std::max(_state.selectionStart, _state.selectionEnd), static_cast<int>(scroll)));
		if (first < last && last <= text.size())
		{
			painter.DrawText({left + low, top}, room - low, Justify::Left, text.substr(first, last - first), _size,
			                 DialogPainter::k_TextColour);
		}
	}
}

void EditBox::MouseDown(glm::ivec2 point)
{
	text_edit::MouseDown(_state, CharacterAt(point));
	Changed(false);
}

void EditBox::Drag(glm::ivec2 point)
{
	text_edit::Drag(_state, CharacterAt(point));
	Changed(false);
}

void EditBox::Release(glm::ivec2 point)
{
	text_edit::MouseUp(_state, CharacterAt(point));
	Changed(false);
}

void EditBox::SetFocus(bool focused)
{
	text_edit::SetFocus(_state, focused);
	Changed(false);
}

bool EditBox::TextInput(std::u16string_view text)
{
	for (const auto c : text)
	{
		text_edit::Type(_state, c);
	}
	Changed(true);
	return true;
}

bool EditBox::KeyDown(int key)
{
	const bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
	switch (key)
	{
	case SDLK_BACKSPACE:
		text_edit::Type(_state, u'');
		Changed(true);
		return true;
	case SDLK_DELETE:
		text_edit::Press(_state, text_edit::Key::Delete, shift);
		Changed(true);
		return true;
	case SDLK_LEFT:
		text_edit::Press(_state, text_edit::Key::Left, shift);
		break;
	case SDLK_RIGHT:
		text_edit::Press(_state, text_edit::Key::Right, shift);
		break;
	case SDLK_HOME:
		text_edit::Press(_state, text_edit::Key::Home, shift);
		break;
	case SDLK_END:
		text_edit::Press(_state, text_edit::Key::End, shift);
		break;
	default:
		return false;
	}
	Changed(false);
	return true;
}

namespace
{
// The picture's ring of symbols comes up over half a second and goes over a second
constexpr float k_RingOpenSeconds = 0.5f;
constexpr float k_RingCloseSeconds = 1.0f;
// A third of the symbols are on the inner ring, the others on the outer
constexpr int k_InnerCount = (SymbolPicture::k_SymbolCount / 3) + 1;
// The disc is cut into 32 slices, its edges fading out over three pixels
constexpr int k_DiscSlices = 32;
constexpr float k_DiscEdge = 3.0f;
// As the ring comes up the inner ring turns into place one way and the outer the other, from a quarter turn
constexpr float k_RingTurn = 0.006159986f;
constexpr int k_Opaque = 255;
// Symbols can only be picked once the ring is all the way up
constexpr int k_Pickable = 0xFD;

constexpr float k_TwoPi = glm::two_pi<float>();
} // namespace

namespace
{
// The colour pickers: the strip of colours or the bar is drawn 16 pixels in from each side, the box round it 14 in and
// 2 beyond its top and bottom, and the arrow marking the height picked is 16 pixels square
constexpr int k_PickerInset = 16;
constexpr int k_PickerBoxInset = 14;
constexpr int k_PickerArrowSize = 16;
// The palette's strip of the front end atlas, down its left
constexpr glm::vec2 k_PaletteUvMin {1.0f / 512.0f, 65.0f / 512.0f};
constexpr glm::vec2 k_PaletteUvMax {63.0f / 512.0f, 511.0f / 512.0f};
// A dragged symbol over somewhere it can go flashes every 400 milliseconds
constexpr uint32_t k_FlashMilliseconds = 400;

} // namespace

ColourPicker::ColourPicker(DialogRect rect, Kind kind)
    : Control(rect)
    , _kind(kind)
{
}

void ColourPicker::Drag(glm::ivec2 point)
{
	const auto position = static_cast<float>(point.y - rect.min.y) / static_cast<float>(rect.Height());
	_position = position > 0.0f ? std::min(position, 1.0f) : 0.0f;
	if (onDrag)
	{
		onDrag(point);
	}
}

void ColourPicker::Draw(const DialogPainter& painter, bool hovered, bool /*focused*/, bool /*pressed*/) const
{
	if (!visible)
	{
		return;
	}
	painter.DrawBevelBox(
	    {.min = {rect.min.x + k_PickerBoxInset, rect.min.y - 2}, .max = {rect.max.x - k_PickerBoxInset, rect.max.y + 2}},
	    hovered ? 2 : 1, DialogPainter::All, glm::vec4(1.0f));
	const auto y = static_cast<int>((_position * static_cast<float>(rect.Height())) + static_cast<float>(rect.min.y));
	const auto left = static_cast<float>(rect.min.x + k_PickerInset);
	const auto right = static_cast<float>(rect.max.x - k_PickerInset);
	if (_kind == Kind::Brightness)
	{
		const auto top = static_cast<float>(rect.min.y);
		const auto middle = static_cast<float>((rect.min.y + rect.max.y) / 2);
		const auto bottom = static_cast<float>(rect.max.y);
		// Fading in from see-through black at the top
		const auto black = glm::vec4(0.0f);
		const auto white = glm::vec4(1.0f);
		const auto centre = glm::vec4(colour) / 255.0f;
		painter.DrawShape({glm::vec2 {left, top}, {right, top}, {right, middle}, {left, middle}},
		                  {black, black, centre, centre});
		painter.DrawShape({glm::vec2 {left, middle}, {right, middle}, {right, bottom}, {left, bottom}},
		                  {centre, centre, white, white});
		const auto arrowHovered = hovered && _pointer.x > rect.max.x - k_PickerArrowSize;
		painter.DrawArrowAlone({rect.max.x - k_PickerArrowSize, y - (k_PickerArrowSize / 2)}, k_PickerArrowSize,
		                       DialogPainter::Arrow::Left, arrowHovered);
		return;
	}
	painter.DrawBox({.min = {rect.min.x + k_PickerInset, rect.min.y}, .max = {rect.max.x - k_PickerInset, rect.max.y}},
	                k_PaletteUvMin, k_PaletteUvMax);
	const auto arrowHovered = hovered && _pointer.x < rect.min.x + k_PickerArrowSize;
	painter.DrawArrowAlone({rect.min.x, y - (k_PickerArrowSize / 2)}, k_PickerArrowSize, DialogPainter::Arrow::Right,
	                       arrowHovered);
}

DraggedSymbol::DraggedSymbol(glm::ivec2 position, int symbol)
    : Control({.min = position, .max = position + k_Size})
    , _symbol(symbol)
{
}

void DraggedSymbol::Draw(const DialogPainter& painter, bool hovered, bool focused, bool /*pressed*/) const
{
	if (!visible)
	{
		return;
	}
	const auto untinted = tint == glm::u8vec4(0);
	const auto colour = untinted ? TextColour(false, focused) : glm::vec4(tint) / 255.0f;
	if (untinted)
	{
		painter.DrawSymbol({.min = rect.min + 2, .max = rect.max + 2}, _symbol, DialogPainter::k_ShadowColour);
	}
	painter.DrawSymbol(rect, _symbol, hovered ? DialogPainter::k_HoverColour : colour);
	if (!_dragging)
	{
		return;
	}
	const auto min = _pointer - (k_Size / 2);
	const auto max = min + (rect.max - rect.min);
	if (untinted)
	{
		painter.DrawSymbol({.min = {min.x - 2, min.y + 2}, .max = {max.x - 2, max.y + 2}}, _symbol,
		                   DialogPainter::k_ShadowColour);
	}
	auto shown = colour;
	if (overTarget && ((milliseconds / k_FlashMilliseconds) & 1u) != 0)
	{
		// The colour's opposite, as opaque as it is
		shown = glm::vec4(glm::vec3(1.0f) - glm::vec3(colour), colour.a);
	}
	painter.DrawSymbol({.min = min, .max = max}, _symbol, shown);
}

void DraggedSymbol::MouseDown(glm::ivec2 /*point*/)
{
	_dragging = true;
}

void DraggedSymbol::Release(glm::ivec2 /*point*/)
{
	if (std::exchange(_dragging, false) && onDrop)
	{
		onDrop();
	}
}

SymbolPicture::SymbolPicture(DialogRect rect, int symbol)
    : Control(rect)
    , _symbol(std::clamp(symbol, 0, k_SymbolCount - 1))
{
}

std::pair<glm::vec2, float> SymbolPicture::GetRingSymbol(int symbol, float opening) const
{
	// The symbols' step is three quarters of half the picture: the inner ring a step out from the picture's edge and
	// the outer three, each symbol a step either way of its centre
	const auto halfSize = rect.Height() / 2;
	const auto step = static_cast<int>(static_cast<float>(halfSize) * 0.75f);
	const auto alpha = static_cast<int>(opening * static_cast<float>(k_Opaque));
	const auto turn = static_cast<float>(k_Opaque - alpha) * k_RingTurn;
	const auto inner = symbol < k_InnerCount;
	const auto count = inner ? k_InnerCount : k_SymbolCount - k_InnerCount;
	const auto index = inner ? symbol : symbol - k_InnerCount;
	const auto radius = static_cast<float>(halfSize + (inner ? step : step * 3));
	const auto angle = (static_cast<float>(index) * k_TwoPi / static_cast<float>(count)) + (inner ? turn : -turn);
	const auto centre = glm::vec2(rect.Centre()) + glm::vec2(std::sin(angle), -std::cos(angle)) * radius;
	return {glm::floor(centre), static_cast<float>((step * alpha) >> 8)};
}

DialogRect SymbolPicture::GetRingRect(int symbol) const
{
	const auto [centre, half] = GetRingSymbol(symbol, 1.0f);
	return {.min = glm::ivec2(centre - half), .max = glm::ivec2(centre + half)};
}

std::optional<int> SymbolPicture::RingSymbolAt(glm::ivec2 point) const
{
	if (static_cast<int>(GetRingOpening() * static_cast<float>(k_Opaque)) <= k_Pickable)
	{
		return std::nullopt;
	}
	for (int s = 0; s < k_SymbolCount; ++s)
	{
		// Within the circle around the symbol
		const auto [centre, half] = GetRingSymbol(s, GetRingOpening());
		const auto offset = glm::vec2(point) - centre;
		if (glm::dot(offset, offset) < half * half)
		{
			return s;
		}
	}
	return std::nullopt;
}

bool SymbolPicture::HitTest(glm::ivec2 point) const
{
	if (!visible)
	{
		return false;
	}
	if (rect.Contains(point))
	{
		return true;
	}
	// While the button is held the whole disc is the picture's
	const auto halfSize = rect.Height() / 2;
	const auto outer = static_cast<float>(halfSize + (static_cast<int>(static_cast<float>(halfSize) * 0.75f) * 4)) + k_DiscEdge;
	const auto offset = glm::vec2(point - rect.Centre());
	return _ringOpen && glm::dot(offset, offset) <= outer * outer;
}

void SymbolPicture::Draw(const DialogPainter& painter, bool hovered, bool focused, bool /*pressed*/) const
{
	if (!visible)
	{
		return;
	}
	const auto shadow = DialogPainter::k_ShadowColour;
	painter.DrawSymbol({.min = rect.min + 2, .max = rect.max + 2}, _symbol, shadow);
	painter.DrawSymbol(rect, _symbol, TextColour(hovered, focused));

	const auto opening = GetRingOpening();
	const auto alpha = static_cast<int>(opening * static_cast<float>(k_Opaque));
	if (alpha <= 5)
	{
		return;
	}

	// The disc: black at half the ring's opacity from the picture's edge to a step beyond the outer ring
	const auto centre = glm::vec2(rect.Centre());
	const auto halfSize = rect.Height() / 2;
	const auto step = static_cast<int>(static_cast<float>(halfSize) * 0.75f);
	const auto outer = static_cast<float>(halfSize + (step * 4));
	const auto inner = static_cast<float>(halfSize);
	// The disc is half as opaque as the symbols, an integer halving like the game's
	const int halfAlpha = alpha / 2;
	const auto disc = glm::vec4(0.0f, 0.0f, 0.0f, static_cast<float>(halfAlpha) / 255.0f);
	const auto clear = glm::vec4(0.0f);
	for (int i = 0; i < k_DiscSlices; ++i)
	{
		const auto a0 = static_cast<float>(i) * k_TwoPi / static_cast<float>(k_DiscSlices);
		const auto a1 = static_cast<float>(i + 1) * k_TwoPi / static_cast<float>(k_DiscSlices);
		const auto d0 = glm::vec2(std::cos(a0), std::sin(a0));
		const auto d1 = glm::vec2(std::cos(a1), std::sin(a1));
		const auto band = [&](float r0, float r1, glm::vec4 c0, glm::vec4 c1) {
			painter.DrawShape({centre + d0 * r0, centre + d1 * r0, centre + d1 * r1, centre + d0 * r1}, {c0, c0, c1, c1});
		};
		band(outer, inner, disc, disc);
		band(outer + k_DiscEdge, outer, clear, disc);
		band(inner, inner - k_DiscEdge, disc, clear);
	}

	// The symbols, growing and turning into place, the one under the pointer orange
	const auto saved = painter.GetAlpha();
	painter.SetAlpha(saved * static_cast<float>(alpha) / 256.0f);
	const auto under = RingSymbolAt(_pointer);
	for (int s = 0; s < k_SymbolCount; ++s)
	{
		const auto [symbolCentre, half] = GetRingSymbol(s, opening);
		const auto min = glm::ivec2(symbolCentre - half);
		const auto max = glm::ivec2(symbolCentre + half);
		painter.DrawSymbol({.min = min + 2, .max = max + 2}, s, shadow);
		painter.DrawSymbol({.min = min, .max = max}, s,
		                   under == s ? DialogPainter::k_HoverColour : DialogPainter::k_FocusColour);
	}
	painter.SetAlpha(saved);
}

void SymbolPicture::MouseDown(glm::ivec2 /*point*/)
{
	_ringOpen = true;
	_ring.SetDestination(1.0f, k_RingOpenSeconds);
}

void SymbolPicture::Release()
{
	_ringOpen = false;
	_ring.SetDestination(0.0f, k_RingCloseSeconds);
}

void SymbolPicture::Activate(glm::ivec2 point)
{
	// Releasing the mouse picks the symbol under the pointer
	if (const auto symbol = RingSymbolAt(point); _ringOpen && symbol)
	{
		_symbol = *symbol;
		if (onChange)
		{
			onChange(_symbol);
		}
	}
	Release();
	Control::Activate(point);
}
