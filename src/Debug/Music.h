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

/// The music engine (Audio/Engine/MusicEngine.h): the music main volume, the 6 channels, and a player for any MUSIC_TYPE
class Music final: public Window
{
public:
	Music() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	int _selectedType {3}; ///< MUSIC_TYPE_GENERIC_GOOD
};

} // namespace openblack::debug::gui
