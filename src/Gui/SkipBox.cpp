/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkipBox.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

#include "TextDatabase.h"

using namespace openblack::gui;
using openblack::new_game_choice::Choice;

namespace
{
constexpr float k_FadeInSeconds = 0.5f;
constexpr float k_FadeOutSeconds = 0.2f;

/// The question and the four answers come from the patch's texts, the button's label from the game's
constexpr std::string_view k_QuestionText = "HELP_TEXT_PATCH_04";
constexpr std::array<std::string_view, openblack::new_game_choice::k_ChoiceCount> k_AnswerTexts = {
    "HELP_TEXT_PATCH_05",
    "HELP_TEXT_PATCH_06",
    "HELP_TEXT_PATCH_07",
    "HELP_TEXT_PATCH_08",
    "", // Debug: Start with Cow, a literal label
};
constexpr std::u16string_view k_DebugCowAnswer = u"Debug: Start with Cow";
constexpr std::string_view k_ButtonText = "HELP_TEXT_REQUESTER_BOXES_04";
/// What each table gives for a text it doesn't have
constexpr std::string_view k_MissingPatchText = "HELP_TEXT_PATCH_NONE";
constexpr std::string_view k_MissingText = "HELP_TEXT_NONE";
/// The radio buttons' label: a single space, which is all of them that takes the mouse beside the button
constexpr std::u16string_view k_RadioLabel = u" ";

std::u16string Text(const TextDatabase& texts, std::string_view name, std::string_view missing)
{
	const auto text = texts.Get(name);
	return std::u16string(text.empty() ? texts.Get(missing) : text);
}
} // namespace

SkipBox::SkipBox(const TextDatabase& texts, const GameFont& font, int textSize)
{
	// Made in the order the game makes them, the latest made taking the mouse first
	auto& question = _dialog.Add<StaticText>(k_Question, Text(texts, k_QuestionText, k_MissingPatchText),
	                                         StaticText::Layout::Wrapped, textSize);
	question.takesClicks = true;

	auto& button = _dialog.Add<BigButton>(font, k_ButtonPosition, k_ButtonSize, Text(texts, k_ButtonText, k_MissingText),
	                                      BigButton::LabelSide::Right, BigButton::Look::LeftArrow);
	button.onClick = [this] {
		_answer = _selection;
		_active = false;
		_fade.SetDestination(0.0f, k_FadeOutSeconds);
	};

	for (size_t i = 0; i < new_game_choice::k_ChoiceCount; ++i)
	{
		auto& radio = _dialog.Add<CheckBox>(font, GetRadioRect(i).min, k_RadioSize, std::u16string(k_RadioLabel),
		                                    BigButton::LabelSide::Right, i == 0, true);
		radio.onClick = [this, i] {
			_selection = new_game_choice::ClampChoice(static_cast<int>(i));
			ShowSelection();
		};
		_radios.at(i) = &radio;

		const auto top = k_FirstAnswer + glm::ivec2(0, k_RadioStep * static_cast<int>(i));
		_answers.at(i) = k_AnswerTexts.at(i).empty() ? std::u16string(k_DebugCowAnswer)
		                                             : Text(texts, k_AnswerTexts.at(i), k_MissingPatchText);
		auto& answer = _dialog.Add<StaticText>(DialogRect {.min = top, .max = top + k_AnswerSize}, _answers.at(i),
		                                       StaticText::Layout::WrappedLeft, textSize);
		answer.takesClicks = true;
		_answerTexts.at(i) = &answer;
	}
}

DialogRect SkipBox::GetRadioRect(size_t index)
{
	const auto min = k_FirstRadio + glm::ivec2(0, k_RadioStep * static_cast<int>(index));
	return {.min = min, .max = min + k_RadioSize};
}

std::vector<Dialog::NamedControl> SkipBox::GetNamedControls() const
{
	// The radio buttons' own label is a single space: they go by their answers instead
	std::vector<Dialog::NamedControl> named;
	std::ranges::copy_if(_dialog.GetNamedControls(), std::back_inserter(named),
	                     [](const Dialog::NamedControl& control) { return control.name != k_RadioLabel; });
	for (size_t i = 0; i < _answers.size(); ++i)
	{
		// A hidden choice can't be pressed by tools either
		if (_radios.at(i)->visible)
		{
			named.push_back({.name = _answers.at(i), .kind = "radio", .rect = GetRadioRect(i)});
		}
	}
	return named;
}

void SkipBox::ShowSelection()
{
	for (size_t i = 0; i < _radios.size(); ++i)
	{
		_radios.at(i)->SetChecked(i == static_cast<size_t>(_selection));
	}
}

bool SkipBox::IsOffered(Choice choice) const noexcept
{
	return _radios.at(static_cast<size_t>(choice))->visible;
}

void SkipBox::Show(bool currentProfileHasCreature)
{
	// A choice that can't be taken is hidden, the others keeping their places
	for (size_t i = 0; i < new_game_choice::k_ChoiceCount; ++i)
	{
		const bool offered = new_game_choice::IsAvailable(static_cast<Choice>(i), currentProfileHasCreature);
		_radios.at(i)->visible = offered;
		_answerTexts.at(i)->visible = offered;
	}
	_selection = new_game_choice::AvailableChoice(_selection, currentProfileHasCreature);
	_active = true;
	_answer.reset();
	_dialog.Reset();
	ShowSelection();
	_fade.Reset(0.0f);
	_fade.SetDestination(1.0f, k_FadeInSeconds);
}

bool SkipBox::Withdraw()
{
	const bool wasActive = std::exchange(_active, false);
	_answer.reset();
	if (wasActive)
	{
		_fade.SetDestination(0.0f, k_FadeOutSeconds);
	}
	return wasActive;
}

void SkipBox::MouseMove(glm::ivec2 point)
{
	if (_active)
	{
		_dialog.MouseMove(point);
	}
}

void SkipBox::MouseDown(glm::ivec2 point)
{
	if (_active)
	{
		_dialog.MouseDown(point);
	}
}

SkipBox::MouseUpResult SkipBox::MouseUp(glm::ivec2 point)
{
	if (!_active)
	{
		return {};
	}
	const bool clicked = _dialog.MouseUp(point);
	return {.clicked = clicked, .answer = std::exchange(_answer, std::nullopt)};
}

void SkipBox::Update(float deltaSeconds)
{
	_fade.Update(deltaSeconds);
	if (_active)
	{
		_dialog.Update(deltaSeconds);
	}
}

void SkipBox::Draw(const DialogPainter& painter) const
{
	if (!IsVisible())
	{
		return;
	}
	const auto alpha = std::clamp(_fade.GetValue(), 0.0f, 1.0f);
	painter.SetAlpha(alpha);
	painter.DrawBackground(k_Box, glm::vec3(1.0f), false, DialogPainter::All);
	painter.SetAlpha(alpha);
	// Fading out, nothing lights up
	_dialog.Draw(painter, _active);
	painter.SetAlpha(1.0f);
}
