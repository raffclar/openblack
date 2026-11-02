/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <functional>
#include <vector>

#include "Dialog.h"
#include "GodWindowRules.h"

namespace openblack::gui
{

class GameFont;

/// The controls of the window of gods (see god_window) of a box with more than 4 gods:
/// - A left arrow, the text "Players 5-8 of 32" and a right arrow, along one line of the box. A click on an arrow
///   scrolls the view one god.
/// - A scroll bar under the god columns, in the look of the scroll bar of a list. The player drags its thumb, or clicks
///   the bar before or after the thumb to scroll the view one view width.
/// - The mouse wheel over the god columns or the bar scrolls the view, also with Shift held and with a horizontal wheel.
/// The controls show only while the box has more than 4 gods. Each arrow shows only while the view can scroll that way.
class GodWindow
{
public:
	/// The size of the arrows, as the box's check boxes
	static constexpr int k_ArrowSize = 25;

	/// Where the controls go in the box
	struct Layout
	{
		/// The line of the arrows and the text
		DialogRect line;
		/// The god columns that the view shows: the box cuts the columns at its edges
		DialogRect view;
		/// The width of a god column. The view is 4 columns wide.
		int column {0};
		/// The scroll bar, under the god columns
		DialogRect bar;
		/// Where the mouse wheel scrolls the view
		DialogRect wheel;
	};

	/// Adds the controls to a dialog. The box is told each time the view scrolls, with true when the 4 gods of the
	/// window changed.
	GodWindow(Dialog& dialog, const GameFont& font, const Layout& layout, std::function<void(bool)> scrolled);

	/// The box opens with this many gods: the view goes back to the first god
	void SetGods(size_t gods);
	/// Scrolls the view some gods on, or back with a negative count, as clicks on the arrows do
	void Step(int steps);
	/// Scrolls the view as the mouse wheel does (see god_window::Wheel)
	void Wheel(int notches, int across);
	/// Scrolls the view to a scroll, kept in its range
	void ScrollTo(float scroll);

	[[nodiscard]] size_t GetGods() const noexcept { return _gods; }
	[[nodiscard]] float GetScroll() const noexcept { return _scroll; }
	[[nodiscard]] const Layout& GetLayout() const noexcept { return _layout; }
	/// The first god of the window
	[[nodiscard]] size_t GetFirst() const noexcept;
	/// The 4 gods of the window, by their places among the gods of the box: every god with 4 or fewer
	[[nodiscard]] std::vector<size_t> Shown() const;
	/// The god columns that the view shows all or part of
	[[nodiscard]] std::vector<god_window::VisibleColumn> Visible() const;
	[[nodiscard]] bool IsShown() const noexcept;

private:
	void Show();

	Layout _layout;
	BigButton* _back {nullptr};
	BigButton* _on {nullptr};
	StaticText* _text {nullptr};
	Control* _bar {nullptr};
	Control* _wheel {nullptr};
	std::function<void(bool)> _scrolled;
	size_t _gods {0};
	float _scroll {0.0f};
};

} // namespace openblack::gui
