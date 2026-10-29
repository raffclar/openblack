/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "MistSystem.h"

#include <cstdint>

#include "3D/Mists.h"
#include "ECS/Components/Mist.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

void MistSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	// The game moves a mist on only while it is in view; every mist moves on here, which looks the same
	Locator::entitiesRegistry::value().Each<Mist>(
	    [gameTime](Mist& mist) { mists::Advance(mist.counter, mist.counterRemainder, gameTime.count()); });
}

namespace
{
/// A mist's colour with its alpha byte changed
uint32_t WithAlpha(uint32_t colour, uint8_t alpha)
{
	return (colour & 0x00FFFFFFu) | (static_cast<uint32_t>(alpha) << 24u);
}
} // namespace

void MistSystem::SetFade(entt::entity mist, float startSize, float endSize, float startAlpha, float endAlpha, float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* shown = registry.TryGet<Mist>(mist);
	if (shown == nullptr)
	{
		return;
	}
	const auto fade = mists::StartFade(startSize, endSize, startAlpha, endAlpha, seconds);
	shown->fading = true;
	shown->fadeTurns = fade.turns;
	shown->fadeElapsed = fade.elapsed;
	shown->fadeSizeStep = fade.sizeStep;
	shown->fadeAlphaStep = fade.alphaStep;
	shown->fadeAlpha = fade.alpha;
	shown->size = startSize;
	shown->colour = WithAlpha(shown->colour, mists::AlphaByte(fade.alpha));
}

void MistSystem::ProcessTurn()
{
	Locator::entitiesRegistry::value().Each<Mist>([](Mist& mist) {
		if (!mist.fading)
		{
			return;
		}
		mists::Fade fade {
		    .turns = mist.fadeTurns,
		    .elapsed = mist.fadeElapsed,
		    .sizeStep = mist.fadeSizeStep,
		    .alphaStep = mist.fadeAlphaStep,
		    .alpha = mist.fadeAlpha,
		};
		if (!mists::StepFade(fade, mist.size))
		{
			mist.fading = false;
			return;
		}
		mist.fadeElapsed = fade.elapsed;
		mist.fadeAlpha = fade.alpha;
		mist.colour = WithAlpha(mist.colour, mists::AlphaByte(fade.alpha));
	});
}
