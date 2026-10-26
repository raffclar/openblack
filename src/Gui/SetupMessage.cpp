/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SetupMessage.h"

#include <algorithm>
#include <utility>

#include <SDL_keycode.h>

#include "Controls.h"
#include "DialogKeyRules.h"

using namespace openblack::gui;

namespace
{
/// The text starts at the big size and gets smaller while it is this high or more
constexpr int k_TextWidth = 400;
constexpr float k_TextTallest = 125.0f;

dialog_keys::Key KeyOf(int key)
{
	if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
	{
		return dialog_keys::Key::Return;
	}
	return key == SDLK_ESCAPE ? dialog_keys::Key::Escape : dialog_keys::Key::Other;
}
} // namespace

SetupMessage::SetupMessage() = default;

SetupMessage::~SetupMessage() = default;

void SetupMessage::Show(std::u16string text, std::u16string okLabel)
{
	auto& dialog = _dialog.emplace();
	_text = &dialog.Add<StaticText>(k_Text, std::move(text), StaticText::Layout::Wrapped);
	_text->takesClicks = true;
	dialog.Add<Button>(k_Ok, std::move(okLabel), DialogPainter::k_MidTextSize).onClick = [this] { _answered = true; };
	_answered = false;
	_fade.Reset(0.0f);
	_fade.SetDestination(1.0f, k_FadeSeconds);
}

void SetupMessage::Close()
{
	_dialog.reset();
	_text = nullptr;
	_answered = false;
	_fade.Reset(0.0f);
}

void SetupMessage::Answer()
{
	Close();
	if (onAnswer)
	{
		onAnswer();
	}
}

float SetupMessage::BehindAlpha(float boxAlpha) const
{
	return std::clamp(boxAlpha - (_fade.GetValue() * k_Dims), 0.0f, 1.0f);
}

void SetupMessage::MouseMove(glm::ivec2 point)
{
	if (_dialog.has_value())
	{
		_dialog->MouseMove(point);
	}
}

void SetupMessage::MouseDown(glm::ivec2 point)
{
	if (_dialog.has_value())
	{
		_dialog->MouseDown(point);
	}
}

bool SetupMessage::MouseUp(glm::ivec2 point)
{
	if (!_dialog.has_value())
	{
		return false;
	}
	const auto acted = _dialog->MouseUp(point);
	if (_answered)
	{
		Answer();
	}
	return acted;
}

bool SetupMessage::KeyDown(int key)
{
	if (!_dialog.has_value() || dialog_keys::AnswerKey(dialog_keys::Buttons::Ok, KeyOf(key)) != dialog_keys::Answer::Ok)
	{
		return false;
	}
	Answer();
	return true;
}

void SetupMessage::Update(float deltaSeconds)
{
	_fade.Update(deltaSeconds);
	if (_dialog.has_value())
	{
		_dialog->Update(deltaSeconds);
	}
}

void SetupMessage::Draw(const DialogPainter& painter, bool active) const
{
	if (!_dialog.has_value())
	{
		return;
	}
	auto size = DialogPainter::k_BigTextSize + 1;
	do
	{
		--size;
	} while (painter.GetTextHeight(k_TextWidth, _text->text, size) >= k_TextTallest && size > DialogPainter::k_SmallTextSize);
	_text->size = size;
	const auto alpha = std::clamp(_fade.GetValue(), 0.0f, 1.0f);
	painter.SetAlpha(alpha);
	painter.DrawBackground(k_Box, glm::vec3(1.0f), true, DialogPainter::All);
	painter.SetAlpha(alpha);
	_dialog->Draw(painter, active);
	painter.SetAlpha(1.0f);
}

std::vector<Dialog::NamedControl> SetupMessage::GetNamedControls() const
{
	return _dialog.has_value() ? _dialog->GetNamedControls() : std::vector<Dialog::NamedControl> {};
}
