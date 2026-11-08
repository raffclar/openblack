/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>

#include "Graphics/RendererInterface.h"

namespace openblack::ecs::systems
{

/// What the work behind the loading screen needs from the game that loads the land
struct LoadFinishHooks
{
	/// Gives true when the game must stop, for example after the player closed the window
	std::function<bool()> quitRequested;
	/// Sets the scene's view and the sea's reflection to the window's size, as the game's frames draw them
	std::function<void()> sizeSceneToWindow;
	/// Gives true when the game draws the hand of this computer
	std::function<bool()> handDrawn;
	/// How the game draws its scene into the window at the given time, with or without the hand
	std::function<graphics::RendererInterface::DrawSceneDesc(uint32_t time, bool drawHand)> sceneDrawDesc;
};

/// The work that the game does behind the loading screen at the end of a load, so that the first frames of the land
/// are not slow: the resources that the loading threads still load, the work of the first frames that only the loaded
/// world can do, and a first frame that the loading screen hides.
class LoadFinishSystemInterface
{
public:
	virtual ~LoadFinishSystemInterface() = default;

	/// Waits behind the loading screen until the loading threads have loaded all the prefetched resources. Then it does
	/// the work of the first frame that the game can do before, and draws one more loading frame.
	virtual void FinishBehindScreen() = 0;
	/// Does behind the loading screen the work that the first frames of a land do once: the land's shadows, the land
	/// where creatures can walk, and the particle files
	virtual void PrepareFirstFrames() = 0;
	/// Ends a load that showed no loading screen. The renderer made the land's pipelines and resources before the load,
	/// thus only the work of the first frames that needs the loaded world occurs.
	virtual void EndWithoutScreen() = 0;
};

} // namespace openblack::ecs::systems
