/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Editor/Panels/ScriptsPanel.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// The loaded script program, to read and to debug: the editor's Scripts panel in a window of its own. Closed, it
/// does nothing; open, it changes the scripts only through its own controls.
class Scripts final: public Window
{
public:
	Scripts() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	editor::ScriptsPanel _panel;
};

} // namespace openblack::debug::gui
