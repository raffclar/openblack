/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/LoadFinishSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class LoadFinishSystem final: public LoadFinishSystemInterface
{
public:
	explicit LoadFinishSystem(LoadFinishHooks hooks);

	void FinishBehindScreen() override;
	void PrepareFirstFrames() override;
	void EndWithoutScreen() override;

private:
	/// Draws the first frame of the land under a loading frame, which hides it, so that the renderer makes the
	/// pipelines and render targets of the scene behind the loading screen. Nothing that the game's frames use changes.
	void WarmUpFirstFrame();
	/// Starts in the background the pipelines of the land, the sky and the sea, before the other resources load. The
	/// pipelines of the land take the longest time to make. Only renderers that make pipelines in the background do it.
	void StartLandPipelines();

	LoadFinishHooks _hooks;
};

} // namespace openblack::ecs::systems
