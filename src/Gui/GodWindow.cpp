/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GodWindow.h"

#include <utility>

#include "GameFont.h"
#include "GodWindowRules.h"

using namespace openblack::gui;

GodWindow::GodWindow(Dialog& dialog, const GameFont& font, DialogRect line, std::function<void()> moved)
    : _moved(std::move(moved))
{
	_back = &dialog.Add<BigButton>(font, line.min, k_ArrowSize, std::u16string(), BigButton::LabelSide::Right,
	                               BigButton::Look::LeftArrow);
	_back->onClick = [this] { Move(-1); };
	_text = &dialog.Add<StaticText>(
	    DialogRect {.min = {line.min.x + k_ArrowSize, line.min.y}, .max = {line.max.x - k_ArrowSize, line.min.y + k_ArrowSize}},
	    std::u16string(), StaticText::Layout::Centre, DialogPainter::SmallTextSize());
	_on = &dialog.Add<BigButton>(font, glm::ivec2 {line.max.x - k_ArrowSize, line.min.y}, k_ArrowSize, std::u16string(),
	                             BigButton::LabelSide::Left, BigButton::Look::RightArrow);
	_on->onClick = [this] { Move(1); };
	Show();
}

void GodWindow::SetGods(size_t gods)
{
	_gods = gods;
	_first = 0;
	Show();
}

void GodWindow::Move(int pages)
{
	const auto first = god_window::Move(_gods, _first, pages);
	if (first == _first)
	{
		return;
	}
	_first = first;
	Show();
	if (_moved)
	{
		_moved();
	}
}

std::vector<size_t> GodWindow::Shown() const
{
	return god_window::Shown(_gods, _first);
}

bool GodWindow::IsShown() const noexcept
{
	return god_window::Scrolls(_gods);
}

void GodWindow::Show()
{
	const bool shown = IsShown();
	_text->visible = shown;
	_text->text = shown ? god_window::Label(_gods, _first) : std::u16string();
	_back->visible = shown && god_window::CanMoveBack(_gods, _first);
	_on->visible = shown && god_window::CanMoveOn(_gods, _first);
}
