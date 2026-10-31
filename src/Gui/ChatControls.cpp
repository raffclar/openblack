/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ChatControls.h"

#include <algorithm>
#include <utility>

#include <SDL.h>

#include "Controls.h"
#include "GameFont.h"
#include "Gui/MultiplayerFrontEndRules.h"
#include "PlayerSlots.h"

namespace openblack::gui
{

using namespace chat_controls;

namespace
{
/// The name of each button, for tools that press them
constexpr std::u16string_view k_OpenName = u"Chat";
constexpr std::u16string_view k_SendName = u"Send";
constexpr std::u16string_view k_QuickName = u"Quick chat";
/// The quick chat list scrolls this far each frame while the pointer is above or below it
constexpr float k_QuickScrollStep = 4.0f;
/// The two spaces before the name of each player in the list
constexpr std::u16string_view k_PlayerIndent = u"  ";

/// A picture of a big button in the part of the button that the clip lets show, from top to bottom
void DrawClipped(const DialogPainter& painter, DialogRect rect, Picture picture, int clipTop, int clipBottom)
{
	const auto top = std::max(rect.min.y, clipTop);
	const auto bottom = std::min(rect.max.y, clipBottom);
	if (bottom <= top)
	{
		return;
	}
	const auto height = static_cast<float>(rect.Height());
	const auto span = picture.uvMax.y - picture.uvMin.y;
	const glm::vec2 uvMin {picture.uvMin.x, picture.uvMin.y + (span * static_cast<float>(top - rect.min.y) / height)};
	const glm::vec2 uvMax {picture.uvMax.x, picture.uvMin.y + (span * static_cast<float>(bottom - rect.min.y) / height)};
	painter.DrawBox({.min = {rect.min.x, top}, .max = {rect.max.x, bottom}}, uvMin, uvMax);
}
} // namespace

class ChatControls::PictureButton final: public Control
{
public:
	PictureButton(DialogRect rect, Look look, std::u16string_view name)
	    : Control(rect)
	    , look(look)
	    , _name(name)
	{
	}

	void Draw(const DialogPainter& painter, bool hovered, bool focused, bool pressed) const override
	{
		if (!visible)
		{
			return;
		}
		// A button with no label lights up under the pointer or with the focus
		const bool lit = hovered || focused;
		if (look == Look::RightArrow)
		{
			painter.DrawArrow(rect.min, rect.Width(), DialogPainter::Arrow::Right, lit, pressed);
			return;
		}
		const auto picture = PictureOf(look, lit);
		const auto lift = pressed ? 0 : -k_Lift;
		painter.DrawBox({.min = rect.min + lift, .max = rect.max + lift}, picture.uvMin, picture.uvMax);
	}
	[[nodiscard]] std::u16string_view GetName() const override { return _name; }
	[[nodiscard]] std::string_view GetKind() const override { return "button"; }

	Look look;

private:
	std::u16string_view _name;
};

ChatControls::ChatControls(const GameFont& font)
    : _font(font)
{
	const auto width = _layoutSize.x;
	_sendButton = &_dialog.Add<PictureButton>(CornerButtonRect(width), Look::NoSpeech, k_SendName);
	_sendButton->onClick = [this]() { _clicked = Clicked::Send; };
	_quickButton = &_dialog.Add<PictureButton>(ButtonAt({width / 2, 0}), Look::ExclaimArrow, k_QuickName);
	_edit = &_dialog.Add<EditBox>(_font, EditRect(width, (width / 2) - 40), std::u16string(), chat::k_MostTypedCharacters,
	                              k_TextSize);
	_players = &_dialog.Add<List>(_font, PlayerListRect(width, _layoutSize.y, 0), k_TextSize, false, false, false);
	_players->scrollBarWidth = k_ScrollBarWidth;
	_players->onClick = [this]() { _clicked = Clicked::Players; };
	_players->drawOver = [this](const DialogPainter& painter, const List::ItemPlace& place) {
		// The heading has the speech button that turns the messages of its players on or off
		if (place.index != 0)
		{
			return;
		}
		const auto left = HeadingButtonLeft(place.right);
		const auto picture = PictureOf(_people.messagesOn ? Look::Speech : Look::NoSpeech, false);
		DrawClipped(painter, ButtonAt({left, place.shownTop}), picture, place.top + 2, place.bottom - 2);
	};
	_quickList = &_dialog.Add<List>(_font, QuickListRect(width, _layoutSize.y), k_TextSize, false, false, true);
	_quickList->scrollBarWidth = k_ScrollBarWidth;
	_quickList->onClick = [this]() { _clicked = Clicked::QuickList; };
	_openButton = &_dialog.Add<PictureButton>(CornerButtonRect(width), Look::Speech, k_OpenName);
	_openButton->onClick = [this]() { _clicked = Clicked::Open; };
	Layout();
}

void ChatControls::SetTexts(std::vector<std::u16string> taunts, std::u16string heading)
{
	_taunts = std::move(taunts);
	_heading = std::move(heading);
	std::vector<List::Item> items;
	items.reserve(_taunts.size());
	for (const auto& taunt : _taunts)
	{
		items.push_back({.text = taunt, .mouseCell = std::nullopt, .colour = std::nullopt, .bar = std::nullopt});
	}
	_quickList->SetItems(std::move(items));
	FitQuickList();
	RebuildRows();
}

void ChatControls::SetPlayers(std::vector<chat::people::Person> players)
{
	const auto same = [this, &players]() {
		if (players.size() != _people.players.size())
		{
			return false;
		}
		auto sorted = players;
		std::ranges::stable_sort(sorted, {}, &chat::people::Person::slot);
		return std::ranges::equal(sorted, _people.players,
		                          [](const auto& a, const auto& b) { return a.slot == b.slot && a.name == b.name; });
	};
	if (same())
	{
		return;
	}
	// The heading keeps whether it is expanded and whether its messages are on
	const auto expanded = _people.expanded;
	const auto messagesOn = _people.messagesOn;
	_people = chat::people::MakeList(std::move(players));
	_people.expanded = expanded;
	_people.messagesOn = messagesOn;
	RebuildRows();
}

void ChatControls::RebuildRows()
{
	std::vector<List::Item> items;
	if (!_people.players.empty())
	{
		auto heading = _heading;
		if (!_people.expanded)
		{
			heading += u"...";
		}
		items.push_back(
		    {.text = std::move(heading), .mouseCell = std::nullopt, .colour = ToColour(k_HeadingColour), .bar = std::nullopt});
		if (_people.expanded)
		{
			for (const auto& player : _people.players)
			{
				// The chat colours count the gods of the room from 1, in the order of their slots
				const auto god = player_slots::SlotOfPlayer(player.slot).value_or(0) + 1;
				const auto colour = PlayerColour(multiplayer::GodColourBits(static_cast<int32_t>(god)));
				items.push_back({.text = std::u16string(k_PlayerIndent) + player.name,
				                 .mouseCell = std::nullopt,
				                 .colour = ToColour(colour),
				                 .bar = player.selected ? std::optional(ToColour(colour | k_SelectedBits)) : std::nullopt});
			}
		}
	}
	const auto scroll = _players->GetScroll();
	// The list is as high as its items, up to half of the screen, with a scroll bar when they do not fit
	_players->SetScrollBar(false);
	_players->rect = PlayerListRect(_layoutSize.x, _layoutSize.y, 0);
	_players->SetItems(items);
	_players->rect = PlayerListRect(_layoutSize.x, _layoutSize.y, _players->GetContentHeight());
	if (_players->GetContentHeight() > _players->rect.Height())
	{
		_players->SetScrollBar(true);
		_players->SetItems(std::move(items));
	}
	_players->ScrollTo(scroll);
}

void ChatControls::Layout()
{
	const auto width = _layoutSize.x;
	_openButton->rect = CornerButtonRect(width);
	_sendButton->rect = CornerButtonRect(width);
	const auto textWidth = _font.GetWidth(_edit->GetText(), static_cast<float>(k_TextSize));
	_edit->rect = EditRect(width, chat::EditWidth(textWidth, width));
	_quickButton->rect = QuickButtonRect(_edit->rect);
	const auto quickRect = QuickListRect(width, _layoutSize.y);
	if (quickRect.min != _quickList->rect.min || quickRect.max != _quickList->rect.max)
	{
		_quickList->rect = quickRect;
		FitQuickList();
	}
}

void ChatControls::FitQuickList()
{
	// The list has a scroll bar when its items do not fit
	_quickList->SetScrollBar(false);
	_quickList->SetItems(_quickList->GetItems());
	if (_quickList->GetContentHeight() > _quickList->rect.Height())
	{
		_quickList->SetScrollBar(true);
		_quickList->SetItems(_quickList->GetItems());
	}
}

bool ChatControls::Update(const Frame& frame)
{
	_shown = ShowsNext(_shown, frame.show);
	const bool wasShown = _boxShown;
	_boxShown = frame.show.inGame && _shown && !frame.otherBox;
	_onlyShift = frame.onlyShift;
	if (frame.layoutSize != _layoutSize)
	{
		_layoutSize = frame.layoutSize;
		RebuildRows();
	}
	// While the quick chat list is held, the pointer picks a taunt and the line shows it
	if (_quickOpen && _dialog.IsHolding())
	{
		const auto& rect = _quickList->rect;
		if (_pointer.y >= rect.min.y && _pointer.y < rect.max.y)
		{
			_quickList->SetSelection(_quickList->ItemAt(_pointer.y));
		}
		const auto selection = _quickList->GetSelection();
		_edit->SetText(selection.has_value() && *selection < _taunts.size() ? _taunts[*selection] : u"");
		if (_pointer.y > rect.max.y)
		{
			_quickList->ScrollTo(_quickList->GetScroll() + k_QuickScrollStep);
		}
		if (_pointer.y < rect.min.y)
		{
			_quickList->ScrollTo(_quickList->GetScroll() - k_QuickScrollStep);
		}
	}
	Layout();
	_openButton->look = OpenLook(_onlyShift);
	_sendButton->look = SendLook(_edit->GetText().empty());
	_quickList->visible = _quickOpen;
	_edit->visible = _open;
	_players->visible = _open && !_players->GetItems().empty() && !_quickOpen;
	_sendButton->visible = _open;
	_quickButton->visible = _open;
	_openButton->visible = _shown && !_open;
	if (!_boxShown)
	{
		_dialog.Reset();
	}
	_dialog.Update(0.0f);
	return _boxShown && !wasShown;
}

bool ChatControls::WantsKeys(bool wideScreen) const
{
	return !wideScreen && _boxShown && _open && _dialog.GetFocus() == _edit && _edit->visible;
}

bool ChatControls::WantsMouse(bool mouseButtonDown) const
{
	if (!_boxShown)
	{
		return false;
	}
	return _dialog.IsHolding() || (!mouseButtonDown && _dialog.IsPointerOverControl());
}

void ChatControls::MouseMove(glm::ivec2 point)
{
	_pointer = point;
	_dialog.MouseMove(point);
}

void ChatControls::StartQuickChat(glm::ivec2 point)
{
	_quickOpen = true;
	_quickList->visible = true;
	_players->visible = false;
	_dialog.Hold(*_quickList);
	_quickList->MouseDown(point);
}

void ChatControls::MouseDown(glm::ivec2 point)
{
	_pointer = point;
	auto* control = _dialog.HitTest(point);
	// With Shift, the speech button opens the chat to close after the taunt, and the press goes on in the quick chat
	if (control == _openButton && _openButton->look != Look::Speech)
	{
		OpenChat(true);
		_openButton->visible = false;
		_sendButton->visible = true;
		_quickButton->visible = true;
		_edit->visible = true;
		control = _quickButton;
	}
	if (control == _quickButton)
	{
		_dialog.MouseMove(point);
		StartQuickChat(point);
		return;
	}
	_dialog.MouseDown(point);
}

bool ChatControls::MouseUp(glm::ivec2 point, bool shift, bool ctrl, bool otherModifier)
{
	_pointer = point;
	_clicked = Clicked::None;
	const bool acted = _dialog.MouseUp(point);
	// The quick chat closes as the mouse button goes up, before the control that the button went up on acts
	if (_closeAfterSend && _quickOpen)
	{
		_open = false;
	}
	_quickOpen = false;
	_quickList->visible = false;
	const auto clicked = std::exchange(_clicked, Clicked::None);
	if (clicked == Clicked::None)
	{
		return acted;
	}
	_closeAfterSend = false;
	switch (clicked)
	{
	case Clicked::QuickList:
		if (const auto selection = _quickList->GetSelection(); selection.has_value())
		{
			Send(static_cast<uint32_t>(*selection));
		}
		break;
	case Clicked::Send:
		Send(std::nullopt);
		break;
	case Clicked::Open:
		OpenChat(false);
		break;
	case Clicked::Players:
		ClickPlayers(point, shift, ctrl, otherModifier);
		break;
	case Clicked::None:
		break;
	}
	return acted;
}

void ChatControls::ClickPlayers(glm::ivec2 point, bool shift, bool ctrl, bool otherModifier)
{
	const auto row = _players->ItemAt(point.y);
	if (!row.has_value())
	{
		return;
	}
	// The speech button of the heading: from 22 units to 2 units from the right of the box of the items, both ends out
	const auto right = _players->GetBoxRight() - 2;
	const auto left = HeadingButtonLeft(_players->GetBoxRight());
	chat::people::Apply(_people, {.row = *row,
	                              .onSpeechButton = point.x > left && point.x < right,
	                              .shift = shift,
	                              .ctrl = ctrl,
	                              .otherModifier = otherModifier});
	RebuildRows();
}

void ChatControls::Wheel(glm::ivec2 point, int steps)
{
	_dialog.Wheel(point, steps);
}

bool ChatControls::TextInput(std::u16string_view text)
{
	return _dialog.TextInput(text);
}

bool ChatControls::KeyDown(int key, bool shift)
{
	if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
	{
		Send(std::nullopt);
		return true;
	}
	if (key == SDLK_ESCAPE)
	{
		_open = false;
		return true;
	}
	_dialog.KeyDown(key, shift);
	// The game sees no key while the line has the keys
	return true;
}

void ChatControls::OpenChat(bool closeAfterSend)
{
	_closeAfterSend = closeAfterSend;
	_shown = true;
	_open = true;
	_edit->SetText(u"");
	_dialog.SetFocus(_edit);
}

void ChatControls::Send(std::optional<uint32_t> taunt)
{
	const auto& line = _edit->GetText();
	if (taunt.has_value())
	{
		_sent.push_back({.line = std::nullopt, .taunt = taunt});
	}
	else if (!line.empty())
	{
		_sent.push_back({.line = line, .taunt = std::nullopt});
	}
	if (line.empty() || _closeAfterSend)
	{
		_open = false;
	}
	_edit->SetText(u"");
	_dialog.SetFocus(nullptr);
}

std::vector<ChatControls::Sent> ChatControls::TakeSent()
{
	return std::exchange(_sent, {});
}

std::vector<PlayerNames> ChatControls::Recipients() const
{
	const auto listed = chat::people::Listed(_people);
	return chat::Recipients(listed);
}

std::u16string_view ChatControls::GetLine() const noexcept
{
	return _edit->GetText();
}

void ChatControls::Draw(const DialogPainter& painter) const
{
	if (!_boxShown)
	{
		return;
	}
	const auto alpha = painter.GetAlpha();
	painter.SetAlpha(k_Alpha);
	_dialog.Draw(painter, true);
	painter.SetAlpha(alpha);
}

std::vector<Dialog::NamedControl> ChatControls::GetNamedControls() const
{
	return _boxShown ? _dialog.GetNamedControls() : std::vector<Dialog::NamedControl> {};
}

} // namespace openblack::gui
