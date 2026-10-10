/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include "Window.h"

namespace openblack::debug::gui
{

/// The player's two advisors: sending them out and home as the scripts do, pointing, clinging, flying and acting, and
/// what each is doing
class Advisors final: public Window
{
public:
	Advisors() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void ProcessEventOpen(const SDL_Event&) noexcept override {}
	void ProcessEventAlways(const SDL_Event&) noexcept override {}

private:
	void DrawAdvisor(int advisor) noexcept;

	/// Where on the screen each advisor is sent to, across and down from 0 to 1
	std::array<std::array<float, 2>, 2> _place {{{0.3f, 0.6f}, {0.7f, 0.6f}}};
	std::array<int, 2> _anim {20, 24};
	std::array<float, 2> _animSpeed {1.0f, 1.0f};
	std::array<int, 2> _emotion {1, 7};
	std::array<bool, 2> _inWorld {false, false};
};

} // namespace openblack::debug::gui
