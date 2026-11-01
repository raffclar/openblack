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
/// (deviation 92). With more than 4 gods, the boxes show 4 gods at a time, in the same places and style as the game,
/// and the player scrolls the window along the gods (deviation 195, MI-7). With 4 gods or fewer there is no window and
/// the boxes are as in the game. Free of the game's state so they can be tested on their own.
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

/// The first god of the window after the player moves it some pages of 4 gods, back with a negative count. The window
/// stops at each end and always shows 4 gods, so the last page can show some gods of the page before it.
[[nodiscard]] size_t Move(size_t gods, size_t first, int pages) noexcept;

/// Whether the window can move further back, or further on
[[nodiscard]] bool CanMoveBack(size_t gods, size_t first) noexcept;
[[nodiscard]] bool CanMoveOn(size_t gods, size_t first) noexcept;

/// The gods in the window, by their places among all the gods, in order
[[nodiscard]] std::vector<size_t> Shown(size_t gods, size_t first);

/// The text beside the window's arrows, for example "Players 5-8 of 32"
[[nodiscard]] std::u16string Label(size_t gods, size_t first);

} // namespace openblack::gui::god_window
