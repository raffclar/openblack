/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "NewPlayerBox.h"

#include <algorithm>
#include <utility>

#include <SDL_keycode.h>

#include "Controls.h"
#include "TextDatabase.h"

using namespace openblack::gui;
using openblack::player_profiles::NameCheck;

namespace
{
constexpr float k_FadeInSeconds = 0.5f;
constexpr float k_FadeOutSeconds = 0.2f;
/// The message comes up over half a second, dimming the box behind it to a quarter
constexpr float k_MessageFadeSeconds = 0.5f;
constexpr float k_MessageDims = 0.75f;
/// The message's text starts at the big size and shrinks while it is 125 or more high, down to the small size
constexpr int k_MessageWidth = 400;
constexpr float k_MessageTallest = 125.0f;

constexpr std::string_view k_QuestionText = "HELP_TEXT_DIALOG_PLEASETYPENAME";
constexpr std::string_view k_PlayerText = "HELP_TEXT_DIALOG_PLAYER";
constexpr std::string_view k_SymbolText = "HELP_TEXT_DIALOG_SYMBOL";
constexpr std::string_view k_OkText = "HELP_TEXT_REQUESTER_BOXES_04";
constexpr std::string_view k_CancelText = "HELP_TEXT_REQUESTER_BOXES_03";
constexpr std::string_view k_AlreadyExistsText = "HELP_TEXT_DIALOG_PLAYERALREADYEXISTS";
constexpr std::string_view k_InvalidText = "HELP_TEXT_DIALOG_INVALIDPLAYERNAME";
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

NewPlayerBox::NewPlayerBox(const TextDatabase& texts, const GameFont& font)
    : _texts(texts)
    , _font(font)
{
	// Made in the order the game makes them. Static text takes the mouse, clicking but doing nothing
	_symbol = &_dialog.Add<SymbolPicture>(k_Symbol, 0);

	auto& question = _dialog.Add<StaticText>(k_Question, Text(texts, k_QuestionText), StaticText::Layout::Centre);
	question.takesClicks = true;

	_cancel = &_dialog.Add<BigButton>(font, k_CancelPosition, k_ButtonSize, Text(texts, k_CancelText),
	                                  BigButton::LabelSide::Left, BigButton::Look::RightArrow);
	_cancel->SetLabelSize(DialogPainter::k_BigTextSize);
	_cancel->onClick = [this] { Cancel(); };

	auto& ok = _dialog.Add<BigButton>(font, k_OkPosition, k_ButtonSize, Text(texts, k_OkText), BigButton::LabelSide::Right,
	                                  BigButton::Look::LeftArrow);
	ok.SetLabelSize(DialogPainter::k_BigTextSize);
	ok.onClick = [this] { Submit(); };

	auto& player = _dialog.Add<StaticText>(k_PlayerLabel, Text(texts, k_PlayerText), StaticText::Layout::Right);
	player.takesClicks = true;
	auto& symbol = _dialog.Add<StaticText>(k_SymbolLabel, Text(texts, k_SymbolText), StaticText::Layout::Left);
	symbol.takesClicks = true;

	_name =
	    &_dialog.Add<EditBox>(font, k_Name, std::u16string {}, player_profiles::k_NameBoxLength, DialogPainter::k_BigTextSize);
}

NewPlayerBox::~NewPlayerBox() = default;

void NewPlayerBox::Show(bool canCancel)
{
	_active = true;
	_answer.reset();
	_cancelled = false;
	_message.reset();
	_messageText = nullptr;
	_messageFade.Reset(0.0f);
	_dialog.Reset();
	// The name's box has the focus, empty; the first symbol is picked
	_dialog.SetFocus(_name);
	_name->SetText(u"");
	_cancel->visible = canCancel;
	_symbol->SetSymbol(0);
	_fade.Reset(0.0f);
	_fade.SetDestination(1.0f, k_FadeInSeconds);
}

void NewPlayerBox::Hide()
{
	_active = false;
	_dialog.Reset();
	_fade.SetDestination(0.0f, k_FadeOutSeconds);
}

void NewPlayerBox::Submit()
{
	const auto name = std::u16string(_name->GetText());
	const auto check = checkName ? checkName(name) : NameCheck::Accepted;
	if (check == NameCheck::AlreadyExists)
	{
		ShowMessage(Text(_texts, k_AlreadyExistsText));
		return;
	}
	if (check == NameCheck::Invalid)
	{
		ShowMessage(Text(_texts, k_InvalidText));
		return;
	}
	_answer = Answer {.name = name, .symbol = _symbol->GetSymbol()};
	Hide();
}

void NewPlayerBox::Cancel()
{
	if (!_cancel->visible)
	{
		return;
	}
	_cancelled = true;
	Hide();
}

void NewPlayerBox::ShowMessage(std::u16string text)
{
	// The box behind loses the focus and lights nothing up
	_dialog.Reset();
	auto& message = _message.emplace();
	_messageText = &message.Add<StaticText>(k_Message, std::move(text), StaticText::Layout::Wrapped);
	_messageText->takesClicks = true;
	auto& ok = message.Add<Button>(k_MessageOk, Text(_texts, k_OkText), DialogPainter::k_MidTextSize);
	ok.onClick = [this] { CloseMessage(); };
	_messageFade.Reset(0.0f);
	_messageFade.SetDestination(1.0f, k_MessageFadeSeconds);
}

void NewPlayerBox::CloseMessage()
{
	// The box is back at once, nothing in it holding the focus
	_message.reset();
	_messageText = nullptr;
	_messageFade.Reset(0.0f);
	_fade.Reset(1.0f);
	_dialog.Reset();
}

void NewPlayerBox::MouseMove(glm::ivec2 point)
{
	if (!_active)
	{
		return;
	}
	if (_message.has_value())
	{
		_message->MouseMove(point);
		return;
	}
	_dialog.MouseMove(point);
}

void NewPlayerBox::MouseDown(glm::ivec2 point)
{
	if (!_active)
	{
		return;
	}
	if (_message.has_value())
	{
		_message->MouseDown(point);
		return;
	}
	_dialog.MouseDown(point);
}

bool NewPlayerBox::MouseUp(glm::ivec2 point)
{
	if (!_active)
	{
		return false;
	}
	if (_message.has_value())
	{
		return _message->MouseUp(point);
	}
	return _dialog.MouseUp(point);
}

void NewPlayerBox::TextInput(std::u16string_view text)
{
	if (_active && !_message.has_value())
	{
		_dialog.TextInput(text);
	}
}

void NewPlayerBox::KeyDown(int key)
{
	if (!_active)
	{
		return;
	}
	if (_message.has_value())
	{
		// The message's OK answers Return and Escape
		if (IsReturn(key) || key == SDLK_ESCAPE)
		{
			CloseMessage();
		}
		return;
	}
	if (IsReturn(key))
	{
		Submit();
		return;
	}
	if (key == SDLK_ESCAPE)
	{
		Cancel();
		return;
	}
	_dialog.KeyDown(key);
}

void NewPlayerBox::Update(float deltaSeconds)
{
	_fade.Update(deltaSeconds);
	_messageFade.Update(deltaSeconds);
	if (!_active)
	{
		return;
	}
	if (_message.has_value())
	{
		_message->Update(deltaSeconds);
		return;
	}
	_dialog.Update(deltaSeconds);
}

void NewPlayerBox::Draw(const DialogPainter& painter) const
{
	if (!IsVisible())
	{
		return;
	}
	const auto alpha = std::clamp(_fade.GetValue() - (_messageFade.GetValue() * k_MessageDims), 0.0f, 1.0f);
	painter.SetAlpha(alpha);
	painter.DrawBackground(k_Box, glm::vec3(1.0f), false, DialogPainter::All);
	painter.SetAlpha(alpha);
	// Behind a message, or fading out, nothing lights up
	_dialog.Draw(painter, _active && !_message.has_value());
	if (_message.has_value())
	{
		// The message's text starts as large as it fits in under 125 high
		auto size = DialogPainter::k_BigTextSize + 1;
		do
		{
			--size;
		} while (painter.GetTextHeight(k_MessageWidth, _messageText->text, size) >= k_MessageTallest &&
		         size > DialogPainter::k_SmallTextSize);
		_messageText->size = size;
		const auto messageAlpha = std::clamp(_messageFade.GetValue(), 0.0f, 1.0f);
		painter.SetAlpha(messageAlpha);
		painter.DrawBackground(k_MessageBox, glm::vec3(1.0f), true, DialogPainter::All);
		painter.SetAlpha(messageAlpha);
		_message->Draw(painter, _active);
	}
	painter.SetAlpha(1.0f);
}

std::vector<Dialog::NamedControl> NewPlayerBox::GetNamedControls() const
{
	return _message.has_value() ? _message->GetNamedControls() : _dialog.GetNamedControls();
}

std::u16string_view NewPlayerBox::GetName() const
{
	return _name->GetText();
}
