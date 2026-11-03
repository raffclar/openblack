/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StaffQuestionBox.h"

#include <algorithm>

#include <SDL_keycode.h>

#include "Controls.h"
#include "TextDatabase.h"

using namespace openblack::gui;
using namespace openblack::gathering_box;

namespace
{

constexpr float k_FadeInSeconds = 0.5f;

bool IsReturn(int key)
{
	return key == SDLK_RETURN || key == SDLK_KP_ENTER;
}

} // namespace

StaffQuestionBox::StaffQuestionBox(const TextDatabase& texts, const GameFont& font)
    : _texts(texts)
{
	// The controls, in the order the game makes them
	_text = &_dialog.Add<StaticText>(k_Question, std::u16string {}, StaticText::Layout::Wrapped, k_QuestionSize);
	_first = &_dialog.Add<Button>(k_First, std::u16string(question::k_YesAnswer));
	_first->onClick = [this] { Pushed(question::Push::First); };
	_no = &_dialog.Add<Button>(k_No, std::u16string(question::k_NoLabel));
	_no->onClick = [this] { Pushed(question::Push::No); };
	_line =
	    &_dialog.Add<EditBox>(font, k_Line, std::u16string {}, question::k_MostAnswerCharacters, DialogPainter::k_BigTextSize);
}

StaffQuestionBox::~StaffQuestionBox() = default;

void StaffQuestionBox::Ask(question::Question asked)
{
	const bool line = asked.kind == question::Kind::Line;
	_text->text = asked.text;
	_first->label = _texts.GetHelpText(question::FirstButtonText(asked.kind)).text;
	_no->visible = !line;
	_line->visible = line;
	_question = std::move(asked);
	_answer.reset();
	_dialog.Reset();
	_dialog.SetFocus(_line);
	_fade.Reset(0.0f);
	_fade.SetDestination(1.0f, k_FadeInSeconds);
}

void StaffQuestionBox::Pushed(question::Push push)
{
	if (!_question.has_value())
	{
		return;
	}
	auto answer = question::AnswerOf(_question->kind, push, _line->GetText());
	_answer.emplace(std::move(*_question), std::move(answer));
	// The box goes at once
	_question.reset();
	_dialog.Reset();
	_fade.Reset(0.0f);
}

void StaffQuestionBox::MouseMove(glm::ivec2 point)
{
	if (_question.has_value())
	{
		_dialog.MouseMove(point);
	}
}

void StaffQuestionBox::MouseDown(glm::ivec2 point)
{
	if (_question.has_value())
	{
		_dialog.MouseDown(point);
	}
}

bool StaffQuestionBox::MouseUp(glm::ivec2 point)
{
	return _question.has_value() && _dialog.MouseUp(point);
}

void StaffQuestionBox::TextInput(std::u16string_view text)
{
	if (_question.has_value())
	{
		_dialog.TextInput(text);
	}
}

void StaffQuestionBox::KeyDown(int key, bool modifier)
{
	if (!_question.has_value())
	{
		return;
	}
	if ((IsReturn(key) || key == SDLK_ESCAPE) && !modifier)
	{
		const auto pushed =
		    question::PushOf(IsReturn(key) ? question::Key::Return : question::Key::Escape, _dialog.GetFocus() == _line);
		if (pushed == question::Push::FocusFirstButton)
		{
			_dialog.SetFocus(_first);
		}
		else
		{
			Pushed(pushed);
		}
		return;
	}
	_dialog.KeyDown(key, false);
}

void StaffQuestionBox::Update(float deltaSeconds)
{
	_fade.Update(deltaSeconds);
	if (_question.has_value())
	{
		_dialog.Update(deltaSeconds);
	}
}

void StaffQuestionBox::Draw(const DialogPainter& painter) const
{
	if (!_question.has_value())
	{
		return;
	}
	const auto alpha = std::clamp(_fade.GetValue(), 0.0f, 1.0f);
	painter.SetAlpha(alpha);
	painter.DrawBackground(k_Box, glm::vec3(1.0f), false, DialogPainter::All);
	painter.SetAlpha(alpha);
	_dialog.Draw(painter, true);
	painter.SetAlpha(1.0f);
}

std::vector<Dialog::NamedControl> StaffQuestionBox::GetNamedControls() const
{
	return _question.has_value() ? _dialog.GetNamedControls() : std::vector<Dialog::NamedControl> {};
}
