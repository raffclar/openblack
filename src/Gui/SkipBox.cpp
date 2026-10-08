/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkipBox.h"

#include <algorithm>

#include "Common/HelpText.h"
#include "Gui/DialogPainter.h"

namespace openblack::gui
{

namespace
{
/// HELP_TEXT_REQUESTER_BOXES_04, the button's label (entry 0 when the text database is shorter)
constexpr uint32_t k_OkText = 0xA24;
/// The patch database's entries (entry 0 when the database is shorter)
constexpr uint32_t k_PatchQuestion = 4;
constexpr uint32_t k_PatchFirstLabel = 5;
/// The box's fade-in time
constexpr float k_FadeSeconds = 0.5f;

SkipBox::Rect CheckRect(int i)
{
	return {{SkipBox::k_CheckX, SkipBox::k_CheckY + SkipBox::k_CheckStep * i}, {SkipBox::k_CheckSize, SkipBox::k_CheckSize}};
}
} // namespace

SkipBox::SkipBox(float textSize)
    : _question(helptext::GetPatch(k_PatchQuestion))
    , _ok(helptext::Get(k_OkText))
    , _textSize(textSize)
{
	for (int i = 0; i < 4; ++i)
	{
		_labels.at(static_cast<size_t>(i)) = helptext::GetPatch(k_PatchFirstLabel + static_cast<uint32_t>(i));
	}
	// The fade goes to 1 and the scale to 0, both in half a second
	_fade.SetPosition(0.0f);
	_fade.SetDestinationWithSpeedAndTime(1.0f, 0.0f, k_FadeSeconds);
	_scale.SetPosition(0.0f);
	_scale.SetDestinationWithSpeedAndTime(0.0f, 0.0f, k_FadeSeconds);
}

void SkipBox::Update(float seconds)
{
	_fade.Update(seconds);
	_scale.Update(seconds);
}

void SkipBox::MouseMove(glm::ivec2 point)
{
	_hover.reset();
	for (int i = 0; i < 4; ++i)
	{
		if (CheckRect(i).Contains(point))
		{
			_hover = 60 + i;
		}
	}
	// (pending) the button's hover: its own 32 x 32 extent
}

std::optional<SkipBox::Answer> SkipBox::Click(glm::ivec2 point)
{
	// (pending) the original's control hit tests; here the check box squares and a (inferred) 32 x 32 button square
	for (int i = 0; i < 4; ++i)
	{
		if (CheckRect(i).Contains(point))
		{
			_selection = i; // the check box id modulo 10
			return std::nullopt;
		}
	}
	if (Rect {k_ButtonAt, {32, 32}}.Contains(point))
	{
		_visible = false; // unpausing the game is the caller's job
		return _selection;
	}
	return std::nullopt;
}

void SkipBox::Draw(const DialogPainter& painter) const
{
	if (!_visible)
	{
		return;
	}
	// (pending) the box's own draw: the background over k_Box, then the controls in list order (texts, button, check
	// boxes). Until they are confirmed, DialogPainter's equivalents:
	const int size = static_cast<int>(_textSize);
	// The alpha is fade - 0.75 x scale, clamped to 0..1; the background is white, centred on the box, see-through and
	// with every edge
	painter.SetAlpha(std::clamp((_fade.value - _scale.value * 0.75f), 0.0f, 1.0f));
	painter.DrawBackground({.min = k_Box.min, .max = k_Box.min + k_Box.size}, glm::vec3(1.0f), false,
	                       DialogPainter::Edges::All);
	// Static texts are wrapped (left or centred), their size made one smaller while the wrapped text is higher than the
	// rect and the size above 10; a shadow pass at (+2, +2), then the text. (inferred) raffclar's shadow and text
	// colours stand for the original's two
	const auto fitted = [&painter](const Rect& rect, const std::u16string& text, int start) {
		int fit = start;
		while (fit > 10 && painter.GetTextHeight(rect.size.x, text, fit) > static_cast<float>(rect.size.y))
		{
			--fit;
		}
		return fit;
	};
	const auto staticText = [&painter, &fitted](const Rect& rect, const std::u16string& text, bool centred, int start) {
		const int fit = fitted(rect, text, start);
		const DialogRect shadow {.min = rect.min + 2, .max = rect.min + rect.size + 2};
		painter.DrawTextWrapped(shadow, centred, text, fit, DialogPainter::k_ShadowColour);
		painter.DrawTextWrapped({.min = rect.min, .max = rect.min + rect.size}, centred, text, fit,
		                        DialogPainter::k_TextColour);
	};
	staticText(k_Question, _question, true, size); // centred
	for (int i = 0; i < 4; ++i)
	{
		const auto check = CheckRect(i);
		painter.DrawSquare(check.min, k_CheckSize, i == _selection, _hover == 60 + i, false);
		staticText({{k_LabelX, k_CheckY + k_CheckStep * i}, k_LabelSize}, _labels.at(static_cast<size_t>(i)), false,
		           size); // left
	}
	painter.DrawArrow(k_ButtonAt, 32, DialogPainter::Arrow::Right, _hover == k_ButtonId, false);
	// The button's label sits on the right of the 32 x 32 button, left-justified at x1 + 2, its shadow first, in the
	// box's mid text size. (approximate) the vertical place as BigButton's labels: the centre less half the size.
	// (inferred) raffclar's text and hover colours stand for the original's. (pending) the original asks for the left arrow
	// look
	const int top = k_ButtonAt.y + 16 - (size / 2);
	painter.DrawText({k_ButtonAt.x + 32 + 2, top + 2}, 1000, DialogPainter::Justify::Left, _ok, size,
	                 DialogPainter::k_ShadowColour);
	painter.DrawText({k_ButtonAt.x + 32, top}, 1000, DialogPainter::Justify::Left, _ok, size,
	                 _hover == k_ButtonId ? DialogPainter::k_HoverColour : DialogPainter::k_TextColour);
}

} // namespace openblack::gui
