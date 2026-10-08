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
#include <string>

#include "Window.h"

namespace openblack::debug::gui
{

/// The good and evil spirits: each one's state, its clips to play, its voices to say, the help's message sets to run,
/// and a spirit silenced or saying every text. Everything happens on a press of the window's controls. The speaker
/// override stays as chosen with the window closed, until it is changed or the next land loads.
class Consciences final: public Window
{
public:
	Consciences() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	void DrawSpeaker() noexcept;
	void DrawSpirit(int dude) noexcept;
	void DrawMessageSets() noexcept;

	/// Per spirit: the clip's speed and the voice sample to say
	std::array<float, 2> _clipSpeed {1.0f, 1.0f};
	std::array<int, 2> _sample {1, 1};
	std::string _search;
	/// What became of the last message set run
	std::string _last;
};

} // namespace openblack::debug::gui
