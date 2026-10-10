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

/// The moon: its phase and place in the moon month, where it stands in the sky, and the hour and date that drive them,
/// with controls to scrub the hour and the day of the moon month, step through the phases and go back to the game's own
/// values
class Moon final: public Window
{
public:
	Moon() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void ProcessEventOpen(const SDL_Event&) noexcept override {}
	void ProcessEventAlways(const SDL_Event&) noexcept override {}

private:
	void DrawPhase() noexcept;
	void DrawSky() noexcept;
	void DrawTimeControls() noexcept;
	void DrawDateControls() noexcept;
};

} // namespace openblack::debug::gui
