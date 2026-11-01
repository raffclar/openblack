/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Window.h"

namespace openblack::debug::gui
{

/// The challenges of the land scripts: the player's record of each challenge that started (its title, how far it has
/// got, its alignment and its reminder script), and the scrolls and signs that the scripts put up on the land
class Challenges final: public Window
{
public:
	Challenges() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void ProcessEventOpen(const SDL_Event&) noexcept override {}
	void ProcessEventAlways(const SDL_Event&) noexcept override {}

private:
	void DrawRecords() noexcept;
	void DrawScrolls() noexcept;
};

} // namespace openblack::debug::gui
