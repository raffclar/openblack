/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/core/fwd.hpp>

#include "Window.h"

namespace openblack::debug::gui
{

/// The camera paths of the resource cache: pick one, see its points and length, and run, stop or pause it through the
/// camera path system
class Camera final: public Window
{
public:
	Camera() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	void DrawControls();
	void DrawCameraResourceList();
	entt::id_type _selectedCameraPath;
};

} // namespace openblack::debug::gui
