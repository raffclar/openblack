/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandInfoBox.h"

#include <algorithm>

#include <SDL_keycode.h>

#include "Controls.h"
#include "TextDatabase.h"

using namespace openblack::gui;

namespace
{
constexpr float k_FadeInSeconds = 0.5f;
constexpr float k_FadeOutSeconds = 0.2f;
/// The band under the title: light grey at half the opacity of the box
constexpr glm::vec4 k_BandColour {224.0f / 255.0f, 224.0f / 255.0f, 224.0f / 255.0f, 0.5f};

constexpr std::string_view k_GoText = "HELP_TEXT_DIALOG_ADDITION_62";
constexpr std::string_view k_BackText = "HELP_TEXT_DIALOG_BACK";
} // namespace

LandInfoBox::LandInfoBox(const TextDatabase& texts, const GameFont& font)
{
	using Look = BigButton::Look;
	using LabelSide = BigButton::LabelSide;
	// The box makes its controls in the order of the game. The box's text is the big size.
	_title = &_dialog.Add<StaticText>(k_Title, std::u16string(), StaticText::Layout::Wrapped);

	auto& go = _dialog.Add<Button>(k_Go, std::u16string(texts.Get(k_GoText)));
	go.onClick = [this] { Hide(Choice::Go); };

	// The list shows no selection. Its scroll bar shows only while the lines do not fit the box.
	_list = &_dialog.Add<List>(font, k_List, DialogPainter::k_BigTextSize, false, true, false);
	_list->ShowScrollBarWhenNeeded();

	auto& back = _dialog.Add<BigButton>(font, k_BackPosition, k_ArrowSize, std::u16string(texts.Get(k_BackText)),
	                                    LabelSide::Right, Look::LeftArrow);
	back.SetLabelSize(DialogPainter::k_BigTextSize);
	back.onClick = [this] { Hide(Choice::Back); };
}

LandInfoBox::~LandInfoBox() = default;

void LandInfoBox::Show(std::u16string title)
{
	_title->text = std::move(title);
	_list->SetItems({});
	_choice = Choice::None;
	if (!_active)
	{
		_fade.Reset(0.0f);
		_fade.SetDestination(1.0f, k_FadeInSeconds);
	}
	_active = true;
	_covered = false;
	_dialog.Reset();
}

void LandInfoBox::AddLine(std::u16string line)
{
	auto items = _list->GetItems();
	items.push_back({.text = std::move(line), .mouseCell = std::nullopt, .mouseMirrored = false, .colour = std::nullopt});
	_list->SetItems(std::move(items));
}

void LandInfoBox::Hide(Choice choice)
{
	_choice = choice;
	_active = false;
	_covered = false;
	_dialog.Reset();
	_fade.SetDestination(0.0f, k_FadeOutSeconds);
}

void LandInfoBox::SetCovered(bool covered)
{
	if (_covered == covered || !_active)
	{
		return;
	}
	_covered = covered;
	_dialog.Reset();
	_fade.SetDestination(covered ? 0.0f : 1.0f, covered ? k_FadeOutSeconds : k_FadeInSeconds);
}

void LandInfoBox::MouseMove(glm::ivec2 point)
{
	if (TakesInput())
	{
		_dialog.MouseMove(point);
	}
}

void LandInfoBox::MouseDown(glm::ivec2 point)
{
	if (TakesInput())
	{
		_dialog.MouseDown(point);
	}
}

bool LandInfoBox::MouseUp(glm::ivec2 point)
{
	return TakesInput() && _dialog.MouseUp(point);
}

void LandInfoBox::Wheel(glm::ivec2 point, int steps)
{
	if (TakesInput())
	{
		_dialog.Wheel(point, steps);
	}
}

void LandInfoBox::KeyDown(int key, uint16_t modifiers)
{
	if (!TakesInput())
	{
		return;
	}
	// Escape with no modifier key closes the box, as Go! does
	if (key == SDLK_ESCAPE && (modifiers & (KMOD_SHIFT | KMOD_CTRL | KMOD_ALT)) == 0)
	{
		Hide(Choice::Go);
		return;
	}
	_dialog.KeyDown(key, (modifiers & KMOD_SHIFT) != 0);
}

void LandInfoBox::Update(float deltaSeconds)
{
	_fade.Update(deltaSeconds);
	if (TakesInput())
	{
		_dialog.Update(deltaSeconds);
	}
}

void LandInfoBox::Draw(const DialogPainter& painter) const
{
	if (!IsVisible())
	{
		return;
	}
	const auto alpha = std::clamp(_fade.GetValue(), 0.0f, 1.0f);
	painter.SetAlpha(alpha);
	// The band under the title comes first, under the box
	const auto& band = k_Title;
	painter.DrawShape(
	    {glm::vec2(band.min), glm::vec2(band.max.x, band.min.y), glm::vec2(band.max), glm::vec2(band.min.x, band.max.y)},
	    {k_BandColour, k_BandColour, k_BandColour, k_BandColour});
	painter.DrawBackground(k_Box, glm::vec3(1.0f), false, DialogPainter::All);
	painter.SetAlpha(alpha);
	// While the box fades out, or another box covers it, no control lights up
	_dialog.Draw(painter, TakesInput());
	painter.SetAlpha(1.0f);
}

std::vector<Dialog::NamedControl> LandInfoBox::GetNamedControls() const
{
	if (!_active)
	{
		return {};
	}
	return _dialog.GetNamedControls();
}
