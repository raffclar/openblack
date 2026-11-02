/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScaffoldGhostRules.h"

#include <algorithm>

#include <PackFile.h>

namespace openblack::ecs
{

// The game calculates these values in single precision: each step rounds to a float

scaffold_ghost::HeldLook scaffold_ghost::Held(const components::ScaffoldPhantom& phantom, int32_t msIntoTurn,
                                              float turnPerGameTurn, SoundsPlayed& played)
{
	HeldLook look;
	const int32_t left = phantom.changeMs - std::min(phantom.changeMs, msIntoTurn);
	float scale = phantom.scale;
	if (left < 1)
	{
		// The phantom is small and shows no building until the next offer grows
		if (!phantom.grows)
		{
			return look;
		}
	}
	else if (!phantom.grows)
	{
		if (!played.shrinks)
		{
			played.grows = false;
			look.sound = GhostSound::Shrinks;
			played.shrinks = true;
		}
		const float part = static_cast<float>(left) * scale;
		scale = part * k_SizePerMs;
	}
	else
	{
		if (!played.grows)
		{
			played.shrinks = false;
			look.sound = GhostSound::Grows;
			played.grows = true;
		}
		const float part = static_cast<float>(left) * k_SizePerMs;
		const float shown = 1.0f - part;
		scale = shown * scale;
	}
	// The phantom turns on between the game turns
	const float share = static_cast<float>(msIntoTurn) * k_TurnPerMs;
	const float turn = share * turnPerGameTurn;
	look.pose = HeldPose {.scale = scale, .yAngle = turn + phantom.yAngle};
	return look;
}

int32_t scaffold_ghost::GrowPitchPercent(int32_t abodeNumber)
{
	return abodeNumber < 0 ? k_PlainPitch : k_GrowPitch - (k_GrowPitchStep * abodeNumber);
}

std::optional<uint32_t> scaffold_ghost::SampleOf(GhostSound sound)
{
	switch (sound)
	{
	case GhostSound::Shrinks:
		return k_ShrinkSample;
	case GhostSound::Grows:
		return k_GrowSample;
	case GhostSound::None:
		break;
	}
	return std::nullopt;
}

audio::SoundEffectOptions scaffold_ghost::SoundOptionsOf(GhostSound sound, int32_t abodeNumber)
{
	if (sound == GhostSound::Grows)
	{
		return {.pitchPercent = static_cast<uint32_t>(GrowPitchPercent(abodeNumber))};
	}
	return {.voicePlayType = pack::AudioBankLoop::Once};
}

uint8_t scaffold_ghost::SiteAlpha(float built)
{
	const float part = built * k_SiteFade;
	const float divisor = part + k_SiteFadeStart;
	const float alpha = k_LandAlpha / divisor;
	return static_cast<uint8_t>(static_cast<int32_t>(alpha));
}

float scaffold_ghost::ScaleOnSite(float built, float scale)
{
	const float part = built * k_ShrinkOnSite;
	const float left = 1.0f - part;
	const float size = left * scale;
	return size <= k_SmallestOnSite ? k_SmallestOnSite : size;
}

} // namespace openblack::ecs
