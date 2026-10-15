/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StoryHistoryBox.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "Controls.h"
#include "TextDatabase.h"

using namespace openblack::gui;
using openblack::help::StoryTurn;

namespace
{
constexpr float k_FadeInSeconds = 0.5f;
constexpr float k_FadeOutSeconds = 0.2f;

std::u16string Text(const TextDatabase& texts, std::string_view name)
{
	return std::u16string(texts.Get(name));
}
} // namespace

StoryHistoryBox::StoryHistoryBox(const TextDatabase& texts, const GameFont& font)
{
	// The arrows' labels are the big size of text, the middle button's label the middle size
	auto& previous = _dialog.Add<BigButton>(font, k_PreviousPosition, k_ArrowSize, Text(texts, "HELP_TEXT_PREVIOUS_LINE"),
	                                        BigButton::LabelSide::Right, BigButton::Look::LeftArrow);
	previous.SetLabelSize(DialogPainter::k_BigTextSize);
	previous.onClick = [this]() { _turn = StoryTurn::Previous; };

	auto& next = _dialog.Add<BigButton>(font, k_NextPosition, k_ArrowSize, Text(texts, "HELP_TEXT_NEXT_LINE"),
	                                    BigButton::LabelSide::Left, BigButton::Look::RightArrow);
	next.SetLabelSize(DialogPainter::k_BigTextSize);
	next.onClick = [this]() { _turn = StoryTurn::Next; };

	auto& latest = _dialog.Add<Button>(k_Latest, Text(texts, "HELP_TEXT_DIALOG_ADDITION_81"), DialogPainter::k_MidTextSize);
	latest.onClick = [this]() { _turn = StoryTurn::Latest; };
}

void StoryHistoryBox::SetShown(bool shown)
{
	if (shown == _active)
	{
		return;
	}
	_active = shown;
	_turn.reset();
	if (shown)
	{
		_dialog.Reset();
		_fade.SetDestination(1.0f, k_FadeInSeconds);
	}
	else
	{
		_fade.SetDestination(0.0f, k_FadeOutSeconds);
	}
}

bool StoryHistoryBox::WantsMouse() const
{
	return _active && (_dialog.IsHolding() || _dialog.IsPointerOverControl());
}

void StoryHistoryBox::MouseMove(glm::ivec2 point)
{
	if (_active)
	{
		_dialog.MouseMove(point);
	}
}

void StoryHistoryBox::MouseDown(glm::ivec2 point)
{
	if (_active)
	{
		_dialog.MouseDown(point);
	}
}

bool StoryHistoryBox::MouseUp(glm::ivec2 point)
{
	return _active && _dialog.MouseUp(point);
}

void StoryHistoryBox::Update(float deltaSeconds)
{
	_fade.Update(deltaSeconds);
	if (_active)
	{
		_dialog.Update(deltaSeconds);
	}
}

void StoryHistoryBox::Draw(const DialogPainter& painter) const
{
	if (!IsVisible())
	{
		return;
	}
	painter.SetAlpha(std::clamp(_fade.GetValue(), 0.0f, 1.0f));
	// While the buttons fade out, no button lights up
	_dialog.Draw(painter, _active);
	painter.SetAlpha(1.0f);
}

std::vector<Dialog::NamedControl> StoryHistoryBox::GetNamedControls() const
{
	return _active ? _dialog.GetNamedControls() : std::vector<Dialog::NamedControl> {};
}
