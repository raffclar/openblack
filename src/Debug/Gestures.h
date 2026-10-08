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

/// The gestures drawn with the hand: what the interface is waiting for, the hand's path drawn over the screen with its
/// corners, the gestures it matches now and the last gesture recognised. A gesture's template can be drawn through the
/// recogniser from here, as mouse positions.
class Gestures final: public Window
{
public:
	Gestures() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override {}
	void ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept override {}
	void ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept override {}

private:
	/// The path and its key points over the screen
	void DrawOverlay() const noexcept;
	void DrawState() noexcept;
	void DrawTools() noexcept;

	bool _overlay {true};
	/// The gesture picked to draw
	int _gesture {4};
};

} // namespace openblack::debug::gui
