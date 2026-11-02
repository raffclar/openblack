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

#include <string>
#include <vector>

/// The window of gods of the boxes that show the gods side by side: the Game Statistics box and the box of the winning
/// conditions. The game shows at most 4 players in these boxes. A network game of openblack can have up to 32 gods
/// (deviation 92). With more than 4 gods, the god columns of a box make one strip, and the box shows 4 columns of the
/// strip at a time, in the same places and style as the game. The player scrolls the strip along the gods (deviation
/// 195, MI-7). With 4 gods or fewer there is no window and the boxes are as in the game. Free of the game's state so
/// they can be tested on their own.
///
/// The scroll is the distance in units of the dialog space from the left edge of the strip to the left edge of the
/// view. The 4 gods of the window are the 4 gods whose columns are nearest to the view. They give the totals, the
/// graphs and the creatures of the box.
namespace openblack::gui::god_window
{

/// The gods that a box shows at a time, as the game does
inline constexpr size_t k_Shown = 4;

/// Whether the box needs the window: it has more gods than it can show at a time
[[nodiscard]] constexpr bool Scrolls(size_t gods) noexcept
{
	return gods > k_Shown;
}

/// The first god of the window, kept in the window's range: from 0 to the last start that still shows 4 gods
[[nodiscard]] size_t Clamp(size_t gods, std::ptrdiff_t first) noexcept;

/// The gods in the window, by their places among all the gods, in order
[[nodiscard]] std::vector<size_t> Shown(size_t gods, size_t first);

/// The text beside the window's arrows, for example "Players 5-8 of 32"
[[nodiscard]] std::u16string Label(size_t gods, size_t first);

/// The width of the view: 4 columns
[[nodiscard]] constexpr int ViewWidth(int column) noexcept
{
	return column * static_cast<int>(k_Shown);
}

/// The largest scroll: the view then shows the last 4 columns. It is 0 with 4 gods or fewer.
[[nodiscard]] float MostScroll(size_t gods, int column) noexcept;

/// A scroll kept between 0 and the largest scroll
[[nodiscard]] float ClampScroll(size_t gods, int column, float scroll) noexcept;

/// The first god of the window at a scroll: the god whose column edge is nearest to the left edge of the view
[[nodiscard]] size_t FirstAt(size_t gods, int column, float scroll) noexcept;

/// The scroll after some clicks on an arrow: one god further on for each click, or one god back with a negative count.
/// The view stops with the edge of a column at its left edge.
[[nodiscard]] float Step(size_t gods, int column, float scroll, int steps) noexcept;

/// The scroll after the mouse wheel turns. A notch moves the view as far as a notch moves a list (notchLength). A notch
/// away from the player (a positive count) moves the view back, as it moves a list up. A notch to the right of a
/// horizontal wheel (a positive across count) moves the view on.
[[nodiscard]] float Wheel(size_t gods, int column, float scroll, int notches, int across, int notchLength) noexcept;

/// A god column that the view shows all or part of: the god and the left edge of its column
struct VisibleColumn
{
	size_t god {0};
	int left {0};

	bool operator==(const VisibleColumn&) const = default;
};
/// The god columns that the view shows at a scroll, with the left edge of the view at viewLeft. The view cuts the
/// columns at its edges, so it can show parts of 5 columns.
[[nodiscard]] std::vector<VisibleColumn> Visible(size_t gods, int column, float scroll, int viewLeft);

/// The scroll bar under the god columns has the look of the scroll bar of a list of the game: a box as thick as the
/// list's scroll bar, and in it a thumb 3 units in from each side of the box
inline constexpr int k_BarThickness = 24;
inline constexpr int k_ThumbInset = 3;

/// The thumb along the bar, from the inner edge of the bar. Its length is the share of the strip that the view shows.
/// It starts at the share of the strip before the view.
struct Thumb
{
	int start {0};
	int end {0};

	bool operator==(const Thumb&) const = default;
};
[[nodiscard]] Thumb ThumbOf(size_t gods, int column, float scroll, int barLength) noexcept;

/// The part of the bar at a distance (along) from the left edge of the bar
enum class BarPart
{
	Before,
	Thumb,
	After,
};
[[nodiscard]] BarPart PartAt(size_t gods, int column, float scroll, int barLength, int along) noexcept;

/// The scroll while the player drags the thumb: the scroll when the button went down, moved as far along the strip as
/// the pointer moved the thumb along the bar
[[nodiscard]] float Drag(size_t gods, int column, float grabScroll, int moved, int barLength) noexcept;

/// The scroll after a click on the bar before or after the thumb: the view moves one view width back or on, as a click
/// on the bar of a list moves the list one page
[[nodiscard]] float Page(size_t gods, int column, float scroll, bool on) noexcept;

} // namespace openblack::gui::god_window
