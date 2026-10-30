/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The devices that the game and the dedicated server share and that use no hardware: the renderer that draws nothing,
// the audio that plays nothing and the sound requests that go to the audio. The simulation does not use this file, so
// that it does not use the interfaces of the renderer and the audio of the game.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include "Audio/AudioManagerNoOp.h"
#include "Audio/GameSoundRequests.h"
#include "Graphics/HeadlessRenderer.h"
#include "Graphics/RendererInterface.h"
#include "Locator.h"

void openblack::EmplaceRenderer(std::unique_ptr<graphics::RendererInterface> renderer)
{
	if (renderer == nullptr)
	{
		Locator::rendererRequests::reset();
		Locator::rendererInterface::reset();
		return;
	}
	// The two services own the renderer together. The renderer stops when the last of the two lets it go.
	std::shared_ptr<graphics::RendererInterface> owner = std::move(renderer);
	Locator::rendererInterface::reset(owner.get(), [owner](graphics::RendererInterface* /*renderer*/) {});
	Locator::rendererRequests::reset(owner.get(), [owner](graphics::RendererRequestsInterface* /*renderer*/) {});
}

bool openblack::InitializeHeadlessEngine() noexcept
{
	EmplaceRenderer(std::make_unique<graphics::HeadlessRenderer>());
	Locator::audio::emplace<audio::AudioManagerNoOp>();
	Locator::sounds::emplace<audio::GameSoundRequests>();
	return true;
}
