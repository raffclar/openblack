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

using namespace openblack::gui;

namespace
{

/// The scroll bar under the god columns: a box with a thumb in it, as the scroll bar of a list
class GodScrollBar final: public Control
{
public:
	GodScrollBar(DialogRect rect, GodWindow& window)
	    : Control(rect)
	    , _window(window)
	{
	}

	void Draw(const DialogPainter& painter, bool /*hovered*/, bool /*focused*/, bool /*pressed*/) const override
	{
		if (!visible)
		{
			return;
		}
		painter.DrawBevelBox(rect, 1, DialogPainter::All, glm::vec4(1.0f));
		const auto thumb =
		    god_window::ThumbOf(_window.GetGods(), _window.GetLayout().column, _window.GetScroll(), rect.Width());
		constexpr int k_Inset = god_window::k_ThumbInset;
		painter.DrawBevelBox({.min = {rect.min.x + k_Inset + thumb.start, rect.min.y + k_Inset},
		                      .max = {rect.min.x + k_Inset + thumb.end, rect.max.y - k_Inset}},
		                     0, DialogPainter::All, glm::vec4(1.0f));
	}
	[[nodiscard]] bool IsTabStop() const override { return false; }

	void MouseDown(glm::ivec2 point) override
	{
		const auto gods = _window.GetGods();
		const auto column = _window.GetLayout().column;
		const auto scroll = _window.GetScroll();
		_dragging = false;
		switch (god_window::PartAt(gods, column, scroll, rect.Width(), point.x - rect.min.x))
		{
		case god_window::BarPart::Thumb:
			_dragging = true;
			_grabX = point.x;
			_grabScroll = scroll;
			break;
		case god_window::BarPart::Before:
			_window.ScrollTo(god_window::Page(gods, column, scroll, false));
			break;
		case god_window::BarPart::After:
			_window.ScrollTo(god_window::Page(gods, column, scroll, true));
			break;
		}
	}
	void Drag(glm::ivec2 point) override
	{
		if (_dragging)
		{
			_window.ScrollTo(
			    god_window::Drag(_window.GetGods(), _window.GetLayout().column, _grabScroll, point.x - _grabX, rect.Width()));
		}
	}
	void Release(glm::ivec2 /*point*/) override { _dragging = false; }
	void Wheel(int steps) override { _window.Wheel(steps, 0); }
	void WheelAcross(int steps) override { _window.Wheel(0, steps); }

private:
	GodWindow& _window;
	bool _dragging {false};
	int _grabX {0};
	float _grabScroll {0.0f};
};

/// The place over the god columns where the mouse wheel scrolls the view. It takes no click, so the controls under it
/// still take the mouse button.
class GodWheelArea final: public Control
{
public:
	GodWheelArea(DialogRect rect, GodWindow& window)
	    : Control(rect)
	    , _window(window)
	{
	}

	void Draw(const DialogPainter& /*painter*/, bool /*hovered*/, bool /*focused*/, bool /*pressed*/) const override {}
	[[nodiscard]] bool HitTest(glm::ivec2 /*point*/) const override { return false; }
	[[nodiscard]] bool WheelHitTest(glm::ivec2 point) const override { return visible && rect.Contains(point); }
	[[nodiscard]] bool IsTabStop() const override { return false; }
	void Wheel(int steps) override { _window.Wheel(steps, 0); }
	void WheelAcross(int steps) override { _window.Wheel(0, steps); }

private:
	GodWindow& _window;
};

} // namespace

GodWindow::GodWindow(Dialog& dialog, const GameFont& font, const Layout& layout, std::function<void(bool)> scrolled)
    : _layout(layout)
    , _scrolled(std::move(scrolled))
{
	const auto& line = _layout.line;
	_back = &dialog.Add<BigButton>(font, line.min, k_ArrowSize, std::u16string(), BigButton::LabelSide::Right,
	                               BigButton::Look::LeftArrow);
	_back->onClick = [this] { Step(-1); };
	_text = &dialog.Add<StaticText>(
	    DialogRect {.min = {line.min.x + k_ArrowSize, line.min.y}, .max = {line.max.x - k_ArrowSize, line.min.y + k_ArrowSize}},
	    std::u16string(), StaticText::Layout::Centre, DialogPainter::SmallTextSize());
	_on = &dialog.Add<BigButton>(font, glm::ivec2 {line.max.x - k_ArrowSize, line.min.y}, k_ArrowSize, std::u16string(),
	                             BigButton::LabelSide::Left, BigButton::Look::RightArrow);
	_on->onClick = [this] { Step(1); };
	_wheel = &dialog.Add<GodWheelArea>(_layout.wheel, *this);
	_bar = &dialog.Add<GodScrollBar>(_layout.bar, *this);
	Show();
}

void GodWindow::SetGods(size_t gods)
{
	_gods = gods;
	_scroll = 0.0f;
	Show();
}

void GodWindow::Step(int steps)
{
	ScrollTo(god_window::Step(_gods, _layout.column, _scroll, steps));
}

void GodWindow::Wheel(int notches, int across)
{
	// A notch moves the view as far as a notch moves a list of small text
	ScrollTo(god_window::Wheel(_gods, _layout.column, _scroll, notches, across, DialogPainter::SmallTextSize()));
}

void GodWindow::ScrollTo(float scroll)
{
	const auto clamped = god_window::ClampScroll(_gods, _layout.column, scroll);
	if (clamped == _scroll)
	{
		return;
	}
	const auto first = GetFirst();
	_scroll = clamped;
	const bool windowMoved = GetFirst() != first;
	Show();
	if (_scrolled)
	{
		_scrolled(windowMoved);
	}
}

size_t GodWindow::GetFirst() const noexcept
{
	return god_window::FirstAt(_gods, _layout.column, _scroll);
}

std::vector<size_t> GodWindow::Shown() const
{
	return god_window::Shown(_gods, GetFirst());
}

std::vector<god_window::VisibleColumn> GodWindow::Visible() const
{
	return god_window::Visible(_gods, _layout.column, _scroll, _layout.view.min.x);
}

bool GodWindow::IsShown() const noexcept
{
	return god_window::Scrolls(_gods);
}

void GodWindow::Show()
{
	const bool shown = IsShown();
	_text->visible = shown;
	_text->text = shown ? god_window::Label(_gods, GetFirst()) : std::u16string();
	_back->visible = shown && _scroll > 0.0f;
	_on->visible = shown && _scroll < god_window::MostScroll(_gods, _layout.column);
	_bar->visible = shown;
	_wheel->visible = shown;
}
