/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TattooEditorDialog.h"

#include <algorithm>
#include <string>
#include <string_view>

#include <SDL_keyboard.h>
#include <SDL_keycode.h>

#include "Controls.h"
#include "Creature/CreatureTattoo.h"
#include "Creature/TattooEditor.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "TextDatabase.h"

using namespace openblack;
using namespace openblack::gui;

namespace
{
constexpr float k_FadeInSeconds = 0.5f;
constexpr float k_FadeOutSeconds = 0.2f;

/// The line of help along the top, broken to fit and shrunk to fit its height
constexpr DialogRect k_HelpRect {.min = {150, 30}, .max = {650, 130}};
/// The frame the dialog has of its own from the main menu: 748 by 548 in the middle of the dialog space
constexpr DialogRect k_FrameRect {.min = {26, 26}, .max = {774, 574}};
/// The symbols are 64 pixels square, 65 apart down each column
constexpr int k_SymbolSpacing = 65;
constexpr int k_SymbolsTop = 35;
constexpr int k_LeftColumn = 80;
constexpr int k_RightColumn = 656;
constexpr size_t k_SymbolsPerColumn = 8;
/// The marker on the place under the pointer: a black square 16 pixels across, pushed in
constexpr int k_MarkerSize = 16;
/// The texts of the label of OK and of the warning of OK in a network game
constexpr std::string_view k_OkText = "HELP_TEXT_REQUESTER_BOXES_04";
constexpr std::string_view k_WarningText = "HELP_TEXT_SYMBOL_EXTRA_01";

DialogRect ToDialogRect(const creature_tattoo_editor::PickerRect& rect)
{
	return {.min = rect.min, .max = rect.max};
}

/// The round button that turns the view: holding it down tells the editor where the pointer is every frame
class RotationButton final: public BigButton
{
public:
	RotationButton(const GameFont& font, std::function<void(glm::ivec2)> held)
	    : BigButton(font, TattooEditorDialog::k_RotationPosition, TattooEditorDialog::k_RotationSize, u" ", LabelSide::Right,
	                Look::Rotation)
	    , _held(std::move(held))
	{
		SetLabelSize(DialogPainter::k_BigTextSize);
	}
	void Drag(glm::ivec2 point) override { _held(point); }

private:
	std::function<void(glm::ivec2)> _held;
};
} // namespace

TattooEditorDialog::TattooEditorDialog(const TextDatabase& texts, const GameFont& font)
    : _warningText(texts.Get(k_WarningText))
    , _okText(texts.Get(k_OkText))
{
	auto& palette = _dialog.Add<ColourPicker>(ToDialogRect(creature_tattoo_editor::k_PaletteRect), ColourPicker::Kind::Palette);
	auto& brightness =
	    _dialog.Add<ColourPicker>(ToDialogRect(creature_tattoo_editor::k_BrightnessRect), ColourPicker::Kind::Brightness);
	_palette = &palette;
	_brightness = &brightness;
	palette.onDrag = [this](glm::ivec2 point) {
		// The palette's colour becomes the middle of the brightness bar
		const auto cell = creature_tattoo_editor::PaletteCell(point);
		const auto index = (static_cast<size_t>(cell.y) * creature_tattoo::k_PaletteColumns) + cell.x;
		if (index < _paletteColours.size())
		{
			const auto& colour = _paletteColours[index];
			_brightness->colour = {colour[0], colour[1], colour[2], 255};
		}
		ApplyTint();
	};
	brightness.onDrag = [this](glm::ivec2 /*point*/) { ApplyTint(); };

	_dialog.Add<StaticText>(k_HelpRect, std::u16string(texts.Get("HELP_TEXT_DIALOG_TATOODRAG")), StaticText::Layout::Wrapped);

	auto& ok = _dialog.Add<BigButton>(font, k_OkPosition, k_ArrowSize, _okText, BigButton::LabelSide::Right,
	                                  BigButton::Look::LeftArrow);
	ok.SetLabelSize(DialogPainter::k_BigTextSize);
	ok.onClick = [this]() {
		if (_editor != nullptr)
		{
			Ok(*_editor);
		}
	};
	auto& cancel =
	    _dialog.Add<BigButton>(font, k_CancelPosition, k_ArrowSize, std::u16string(texts.Get("HELP_TEXT_REQUESTER_BOXES_03")),
	                           BigButton::LabelSide::Left, BigButton::Look::RightArrow);
	cancel.SetLabelSize(DialogPainter::k_BigTextSize);
	cancel.onClick = [this]() {
		if (_editor != nullptr)
		{
			Cancel(*_editor);
		}
	};
	_message.onAnswer = [this]() {
		// The answer clears the change. The dialog is back at once, and no control has the focus.
		if (_editor != nullptr)
		{
			_editor->Answered();
		}
		_fade.Reset(1.0f);
		_dialog.Reset();
	};

	for (size_t i = 0; i < _symbols.size(); ++i)
	{
		auto& symbol = _dialog.Add<DraggedSymbol>(GetSymbolPosition(i), static_cast<int>(i));
		symbol.onDrop = [this, i]() { _dropped = i; };
		_symbols.at(i) = &symbol;
	}

	_dialog.Add<RotationButton>(font, [this](glm::ivec2 point) {
		if (!_grab.has_value())
		{
			_grab = point;
		}
		if (_editor != nullptr)
		{
			_editor->Steer(*_grab, point, _frameMilliseconds);
		}
	});
}

TattooEditorDialog::~TattooEditorDialog() = default;

glm::ivec2 TattooEditorDialog::GetSymbolPosition(size_t index)
{
	const auto x = index < k_SymbolsPerColumn ? k_LeftColumn : k_RightColumn;
	return {x, k_SymbolsTop + (static_cast<int>(index % k_SymbolsPerColumn) * k_SymbolSpacing)};
}

void TattooEditorDialog::Open(bool framed)
{
	_framed = framed;
	_open = true;
	_grab.reset();
	_dropped.reset();
	_message.Close();
	// The pickers and the symbols' colour stay as the player last left them
	_dialog.Reset();
	_fade.SetDestination(1.0f, k_FadeInSeconds);
}

void TattooEditorDialog::Close()
{
	_open = false;
	_dialog.Reset();
	_message.Close();
	_fade.SetDestination(0.0f, k_FadeOutSeconds);
}

void TattooEditorDialog::ApplyTint()
{
	// The brightness bar's colour at its arrow's height. Above its middle the colour is opaque; below it keeps the
	// bar's own opacity, which is none until a colour is picked from the palette.
	const auto colour = _brightness->colour;
	const auto position = _brightness->GetPosition();
	const auto rgb = creature_tattoo::Brightened(glm::u8vec3(colour), position);
	SetTint({rgb, position >= 0.5f ? 255 : colour.a});
}

void TattooEditorDialog::SetTint(glm::u8vec4 tint)
{
	_tint = tint;
	for (auto* symbol : _symbols)
	{
		symbol->tint = tint;
	}
}

void TattooEditorDialog::MouseMove(glm::ivec2 point)
{
	// While the message shows, the dialog behind it gets the pointer only for a tattoo that the player lifted off
	if (_message.IsShown())
	{
		_message.MouseMove(point);
		if (!_dialog.IsHolding())
		{
			return;
		}
	}
	_dialog.MouseMove(point);
}

void TattooEditorDialog::MouseDown(glm::ivec2 point)
{
	if (!_open)
	{
		return;
	}
	if (_message.IsShown())
	{
		_message.MouseDown(point);
		return;
	}
	_dialog.MouseDown(point);
}

bool TattooEditorDialog::MouseUp(glm::ivec2 point, ecs::systems::TattooEditorSystemInterface& editor)
{
	if (!_open)
	{
		return false;
	}
	_editor = &editor;
	if (_message.IsShown() && !_dialog.IsHolding())
	{
		const auto answered = _message.MouseUp(point);
		_editor = nullptr;
		return answered;
	}
	const auto acted = _dialog.MouseUp(point);
	_grab.reset();
	if (const auto dropped = std::exchange(_dropped, std::nullopt); dropped.has_value())
	{
		editor.Drop(static_cast<uint8_t>(*dropped), glm::u8vec3(_symbols.at(*dropped)->tint));
	}
	_editor = nullptr;
	return acted;
}

bool TattooEditorDialog::KeyDown(int key, ecs::systems::TattooEditorSystemInterface& editor)
{
	if (!_open)
	{
		return false;
	}
	if (_message.IsShown())
	{
		// Return and Escape press the OK of the message. The message takes every key.
		_editor = &editor;
		_message.KeyDown(key);
		_editor = nullptr;
		return true;
	}
	if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
	{
		Ok(editor);
		return true;
	}
	if (key == SDLK_ESCAPE)
	{
		Cancel(editor);
		return true;
	}
	// Tab moves the focus over the editor's controls
	return _dialog.KeyDown(key, (SDL_GetModState() & KMOD_SHIFT) != 0);
}

void TattooEditorDialog::Ok(ecs::systems::TattooEditorSystemInterface& editor)
{
	// In a network game with a change, OK shows the warning first
	if (editor.Ok(_networkGame) == creature_tattoo_editor::Accept::Close)
	{
		Close();
		return;
	}
	_dialog.Reset();
	_message.Show(_warningText, _okText);
}

void TattooEditorDialog::Cancel(ecs::systems::TattooEditorSystemInterface& editor)
{
	editor.Cancel();
	Close();
}

void TattooEditorDialog::Update(float deltaSeconds, ecs::systems::TattooEditorSystemInterface& editor,
                                std::span<const std::array<uint8_t, 3>> palette, bool leftButtonDown)
{
	_fade.Update(deltaSeconds);
	_message.Update(deltaSeconds);
	_paletteColours = palette;
	if (!_open)
	{
		return;
	}
	if (!editor.IsOpen())
	{
		Close();
		return;
	}
	_editor = &editor;
	_frameMilliseconds = deltaSeconds * 1000.0f;
	_dialog.Update(deltaSeconds);

	const auto site = editor.GetHoveredSite();
	const auto dragging = std::ranges::any_of(_symbols, [](const auto* symbol) { return symbol->IsDragging(); });
	// Pressing on a place, not on one of the controls, lifts its tattoo off to drag. This also occurs while the
	// message shows, but not while the button is down on the OK of the message.
	if (leftButtonDown && site.has_value() && !dragging && !_dialog.IsHolding() && !_message.IsHolding())
	{
		if (const auto lifted = editor.Lift(); lifted.has_value())
		{
			auto& symbol = *_symbols.at(lifted->design);
			symbol.StartDragging();
			_dialog.Hold(symbol);
			SetTint({lifted->colour, 255});
		}
	}
	for (auto* symbol : _symbols)
	{
		symbol->overTarget = symbol->IsDragging() && site.has_value();
	}
	_editor = nullptr;
}

void TattooEditorDialog::Draw(const DialogPainter& painter, uint32_t milliseconds) const
{
	const auto fade = std::clamp(_fade.GetValue(), 0.0f, 1.0f);
	if (fade <= 0.0f)
	{
		return;
	}
	// Behind the message the dialog dims
	const auto alpha = _message.BehindAlpha(fade);
	painter.SetAlpha(alpha);
	if (_framed)
	{
		painter.DrawBackground(k_FrameRect, glm::vec3(1.0f), false, DialogPainter::All);
		painter.SetAlpha(alpha);
	}
	for (auto* symbol : _symbols)
	{
		symbol->milliseconds = milliseconds;
	}
	_dialog.Draw(painter, _open && !_message.IsShown());
	painter.SetAlpha(1.0f);
}

void TattooEditorDialog::DrawOverCreature(const DialogPainter& painter, std::optional<glm::ivec2> marker) const
{
	const auto fade = std::clamp(_fade.GetValue(), 0.0f, 1.0f);
	if (fade <= 0.0f)
	{
		return;
	}
	painter.SetAlpha(_message.BehindAlpha(fade));
	if (marker.has_value())
	{
		const auto half = k_MarkerSize / 2;
		painter.DrawSquare(*marker - half, k_MarkerSize, false, false, true);
	}
	for (const auto* symbol : _symbols)
	{
		if (symbol->IsDragging())
		{
			symbol->Draw(painter, true, true, false);
		}
	}
	// The message is in front of all of the editor
	_message.Draw(painter, _open);
	painter.SetAlpha(1.0f);
}
