/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SaveGameBoxes.h"

#include <algorithm>

#include <SDL_keyboard.h>
#include <SDL_keycode.h>

#include "Controls.h"
#include "DialogKeyRules.h"
#include "Temple/SaveGameRoomRules.h"
#include "TextDatabase.h"

using namespace openblack::gui;

namespace
{
constexpr float k_FadeInSeconds = 0.5f;
constexpr float k_FadeOutSeconds = 0.2f;
/// Behind the question the box that holds it is dimmed by three quarters
constexpr float k_HoldDims = 0.75f;
/// The question's text starts at the big size and shrinks while it is 125 or more high, down to the small size
constexpr int k_QuestionWidth = 400;
constexpr float k_QuestionTallest = 125.0f;

constexpr std::string_view k_YesText = "HELP_TEXT_REQUESTER_BOXES_01";
constexpr std::string_view k_NoText = "HELP_TEXT_REQUESTER_BOXES_02";
constexpr std::string_view k_OkText = "HELP_TEXT_REQUESTER_BOXES_04";
constexpr std::string_view k_CancelText = "HELP_TEXT_REQUESTER_BOXES_03";
constexpr std::string_view k_PromptText = "HELP_TEXT_ROOM_CITADEL_SAVEGAME_ROOM_SAVEMESSAGE";
/// The words whose first letters answer a question by the keyboard
constexpr std::string_view k_YesKeyText = "HELP_TEXT_PAUSE_STATS_145";
constexpr std::string_view k_NoKeyText = "HELP_TEXT_PAUSE_STATS_144";
/// What the table gives for a text it doesn't have
constexpr std::string_view k_MissingText = "HELP_TEXT_NONE";

std::u16string Text(const TextDatabase& texts, std::string_view name)
{
	const auto text = texts.Get(name);
	return std::u16string(text.empty() ? texts.Get(k_MissingText) : text);
}

bool IsReturn(int key)
{
	return key == SDLK_RETURN || key == SDLK_KP_ENTER;
}
} // namespace

SaveGameBoxes::SaveGameBoxes(const TextDatabase& texts, const GameFont& font)
    : _texts(texts)
    , _font(font)
{
	// The name box's controls, in the order the game makes them
	auto& prompt = _nameDialog.Add<StaticText>(k_NamePrompt, Text(texts, k_PromptText), StaticText::Layout::Wrapped);
	prompt.takesClicks = true;
	auto& ok = _nameDialog.Add<Button>(k_Ok, Text(texts, k_OkText));
	ok.onClick = [this] { SubmitName(); };
	_ok = &ok;
	_nameDialog.Add<Button>(k_Cancel, Text(texts, k_CancelText)).onClick = [this] { CancelName(); };
	_name = &_nameDialog.Add<EditBox>(font, k_Name, std::u16string {}, temple::save_game_room::k_NameLength,
	                                  DialogPainter::k_BigTextSize);
}

SaveGameBoxes::~SaveGameBoxes() = default;

SaveGameBoxes::Question& SaveGameBoxes::Show(std::u16string text)
{
	auto& shown = _question.emplace();
	shown.text = text;
	shown.textControl = &shown.answers.Add<StaticText>(k_Question, std::move(text), StaticText::Layout::Wrapped);
	shown.textControl->takesClicks = true;
	_answer.reset();
	_holdingFade.Reset(0.0f);
	_holdingFade.SetDestination(1.0f, k_FadeInSeconds);
	_questionFade.Reset(0.0f);
	_questionFade.SetDestination(1.0f, k_FadeInSeconds);
	return shown;
}

void SaveGameBoxes::Ask(std::u16string question)
{
	auto& asked = Show(std::move(question));
	asked.answers
	    .Add<BigButton>(_font, k_YesPosition, k_ArrowSize, Text(_texts, k_YesText), BigButton::LabelSide::Right,
	                    BigButton::Look::LeftArrow)
	    .onClick = [this] { Answered(Answer::Yes); };
	asked.answers
	    .Add<BigButton>(_font, k_NoPosition, k_ArrowSize, Text(_texts, k_NoText), BigButton::LabelSide::Left,
	                    BigButton::Look::RightArrow)
	    .onClick = [this] { Answered(Answer::No); };
}

void SaveGameBoxes::Tell(std::u16string message)
{
	auto& told = Show(std::move(message));
	told.message = true;
	told.answers.Add<Button>(k_MessageOk, Text(_texts, k_OkText), DialogPainter::MidTextSize()).onClick = [this] {
		Answered(Answer::Ok);
	};
}

void SaveGameBoxes::AskName(std::u16string_view name)
{
	_nameActive = true;
	_answer.reset();
	_nameDialog.Reset();
	_name->SetText(name);
	_nameDialog.SetFocus(_name);
	_nameFade.Reset(0.0f);
	_nameFade.SetDestination(1.0f, k_FadeInSeconds);
}

std::u16string_view SaveGameBoxes::GetQuestion() const noexcept
{
	return _question.has_value() ? std::u16string_view(_question->text) : std::u16string_view();
}

std::u16string_view SaveGameBoxes::GetName() const
{
	return _name->GetText();
}

void SaveGameBoxes::Answered(Answer answer)
{
	// The question goes at once
	_question.reset();
	_holdingFade.Reset(0.0f);
	_questionFade.Reset(0.0f);
	_answer = answer;
}

void SaveGameBoxes::WithdrawMessage()
{
	if (_question.has_value() && _question->message)
	{
		_question.reset();
		_holdingFade.Reset(0.0f);
		_questionFade.Reset(0.0f);
	}
}

void SaveGameBoxes::SubmitName()
{
	_nameActive = false;
	_nameDialog.Reset();
	_nameFade.SetDestination(0.0f, k_FadeOutSeconds);
	_answer = Answer::Named;
}

void SaveGameBoxes::CancelName()
{
	_nameActive = false;
	_nameDialog.Reset();
	_nameFade.SetDestination(0.0f, k_FadeOutSeconds);
	_answer = Answer::Cancelled;
}

void SaveGameBoxes::MouseMove(glm::ivec2 point)
{
	if (_question.has_value())
	{
		_question->answers.MouseMove(point);
	}
	else if (_nameActive)
	{
		_nameDialog.MouseMove(point);
	}
}

void SaveGameBoxes::MouseDown(glm::ivec2 point)
{
	if (_question.has_value())
	{
		_question->answers.MouseDown(point);
	}
	else if (_nameActive)
	{
		_nameDialog.MouseDown(point);
	}
}

bool SaveGameBoxes::MouseUp(glm::ivec2 point)
{
	if (_question.has_value())
	{
		return _question->answers.MouseUp(point);
	}
	if (_nameActive)
	{
		return _nameDialog.MouseUp(point);
	}
	return false;
}

void SaveGameBoxes::TextInput(std::u16string_view text)
{
	if (_question.has_value() && _question->message)
	{
		return;
	}
	if (_question.has_value())
	{
		// The first letters of the words "yes" and "no" answer the question, unless they are the same
		const auto yes = _texts.Get(k_YesKeyText);
		const auto no = _texts.Get(k_NoKeyText);
		if (text.empty() || yes.empty() || no.empty())
		{
			return;
		}
		switch (dialog_keys::AnswerTyped(dialog_keys::Buttons::YesNo, text.front(), yes.front(), no.front()))
		{
		case dialog_keys::Answer::Yes:
			Answered(Answer::Yes);
			break;
		case dialog_keys::Answer::No:
			Answered(Answer::No);
			break;
		default:
			break;
		}
		return;
	}
	if (_nameActive)
	{
		_nameDialog.TextInput(text);
	}
}

void SaveGameBoxes::KeyDown(int key)
{
	if (_question.has_value() && _question->message)
	{
		// A message's OK answers Return and Escape
		if (IsReturn(key) || key == SDLK_ESCAPE)
		{
			Answered(Answer::Ok);
		}
		return;
	}
	if (_question.has_value() || !_nameActive)
	{
		// The question takes neither Return nor Escape
		return;
	}
	if (IsReturn(key))
	{
		// Return in the name's box gives the focus to OK; Return on OK saves
		if (_nameDialog.GetFocus() == _name)
		{
			_nameDialog.SetFocus(_ok);
		}
		else
		{
			SubmitName();
		}
		return;
	}
	if (key == SDLK_ESCAPE)
	{
		CancelName();
		return;
	}
	_nameDialog.KeyDown(key, (SDL_GetModState() & KMOD_SHIFT) != 0);
}

void SaveGameBoxes::Update(float deltaSeconds)
{
	_holdingFade.Update(deltaSeconds);
	_questionFade.Update(deltaSeconds);
	_nameFade.Update(deltaSeconds);
	if (_question.has_value())
	{
		_question->answers.Update(deltaSeconds);
	}
	else if (_nameActive)
	{
		_nameDialog.Update(deltaSeconds);
	}
}

void SaveGameBoxes::Draw(const DialogPainter& painter) const
{
	if (const auto nameAlpha = std::clamp(_nameFade.GetValue(), 0.0f, 1.0f); nameAlpha > 0.0f)
	{
		painter.SetAlpha(nameAlpha);
		painter.DrawBackground(k_NameBox, glm::vec3(1.0f), false, DialogPainter::All);
		painter.SetAlpha(nameAlpha);
		_nameDialog.Draw(painter, _nameActive && !_question.has_value());
	}
	if (_question.has_value())
	{
		// The box that holds the question, dimmed behind it
		const auto holding = std::clamp(_holdingFade.GetValue() - (_questionFade.GetValue() * k_HoldDims), 0.0f, 1.0f);
		painter.SetAlpha(holding);
		painter.DrawBackground(k_HoldingBox, glm::vec3(1.0f), false, DialogPainter::All);
		// The question's text starts as large as it fits in under 125 high
		auto size = DialogPainter::k_BigTextSize + 1;
		do
		{
			--size;
		} while (painter.GetTextHeight(k_QuestionWidth, _question->text, size) >= k_QuestionTallest &&
		         size > DialogPainter::SmallTextSize());
		_question->textControl->size = size;
		const auto alpha = std::clamp(_questionFade.GetValue(), 0.0f, 1.0f);
		painter.SetAlpha(alpha);
		painter.DrawBackground(k_QuestionBox, glm::vec3(1.0f), true, DialogPainter::All);
		painter.SetAlpha(alpha);
		_question->answers.Draw(painter, true);
	}
	painter.SetAlpha(1.0f);
}

std::vector<Dialog::NamedControl> SaveGameBoxes::GetNamedControls() const
{
	if (_question.has_value())
	{
		return _question->answers.GetNamedControls();
	}
	return _nameActive ? _nameDialog.GetNamedControls() : std::vector<Dialog::NamedControl> {};
}
