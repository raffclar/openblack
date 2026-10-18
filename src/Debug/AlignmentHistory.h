/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "Window.h"

namespace openblack::debug::gui
{

/// The totals of the changes to the alignment of the player at this computer, one line for each cause. The game keeps
/// these totals but does not show them. The window also shows the alignment of player one, and how far it moved since
/// the window opened for the first time.
class AlignmentHistory final: public Window
{
public:
	AlignmentHistory() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void ProcessEventOpen(const SDL_Event&) noexcept override {}
	void ProcessEventAlways(const SDL_Event&) noexcept override {}

private:
	/// The alignment of player one when the window opened for the first time
	std::optional<float> _start;
};

} // namespace openblack::debug::gui
