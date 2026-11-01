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

namespace openblack::gui
{

class GameFont;

/// The controls of the window of gods (see god_window): a left arrow, the text "Players 5-8 of 32" and a right arrow,
/// along one line of a box. A click on an arrow moves the window a page of 4 gods. The controls show only while the box
/// has more than 4 gods, and each arrow shows only while the window can move that way.
class GodWindow
{
public:
	/// The size of the arrows, as the box's check boxes
	static constexpr int k_ArrowSize = 25;

	/// Adds the controls to a dialog: the left arrow at the left of the line, the right arrow at its right and the text
	/// between them. The box is told each time the window moves.
	GodWindow(Dialog& dialog, const GameFont& font, DialogRect line, std::function<void()> moved);

	/// The box opens with this many gods: the window goes back to the first god
	void SetGods(size_t gods);
	/// Moves the window some pages of 4 gods, back with a negative count
	void Move(int pages);

	[[nodiscard]] size_t GetFirst() const noexcept { return _first; }
	/// The gods that the box shows, by their places among its gods: every god with 4 or fewer
	[[nodiscard]] std::vector<size_t> Shown() const;
	[[nodiscard]] bool IsShown() const noexcept;

private:
	void Show();

	BigButton* _back {nullptr};
	BigButton* _on {nullptr};
	StaticText* _text {nullptr};
	std::function<void()> _moved;
	size_t _gods {0};
	size_t _first {0};
};

} // namespace openblack::gui
