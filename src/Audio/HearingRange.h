/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::audio
{

/// The furthest from the camera a placed sound effect may start. The bank's maximum distance for the sample counts
/// whether or not the bank's header applies it to the voice; a sample whose bank gives none (most of those that keep
/// the default distances) falls back on the play's own maximum distance, by default the voice's 9999.
[[nodiscard]] constexpr float SoundEffectStartRange(float bankMaxDistance, float playMaxDistance)
{
	return bankMaxDistance != 0.0f ? bankMaxDistance : playMaxDistance;
}

/// The furthest from the camera an animation effect (a knock, a crash, a footstep) may start: the bank's maximum
/// distance for the sample, with no fallback, so a sample whose bank gives none is only heard from right on top of it
[[nodiscard]] constexpr float AnimEffectStartRange(float bankMaxDistance)
{
	return bankMaxDistance;
}

/// Whether a placed sound this far from the camera may start, up to and including its range
[[nodiscard]] constexpr bool IsWithinStartRange(float distance, float range)
{
	return distance <= range;
}

/// The distances that a bank header gives a placed voice. The scale is the roll-off: how fast the volume falls past
/// the minimum distance.
struct DistanceSettings
{
	float minDistance;
	float maxDistance;
	float rolloff;
};

/// The part of a placed voice's volume that its distance from the listener gives. The voice has full volume up to its
/// minimum distance (and everywhere when the roll-off is 0). Between the minimum and the maximum distance the volume
/// is minimum / (minimum + roll-off * (distance - minimum)). Past the maximum distance the voice is silent.
[[nodiscard]] constexpr float DistanceGain(float distance, const DistanceSettings& settings)
{
	if (distance > settings.maxDistance)
	{
		return 0.0f;
	}
	if (distance <= settings.minDistance || settings.rolloff == 0.0f)
	{
		return 1.0f;
	}
	return settings.minDistance / ((distance - settings.minDistance) * settings.rolloff + settings.minDistance);
}

/// Whether a placed voice this far from the listener is silent
[[nodiscard]] constexpr bool IsBeyondMaxDistance(float distance, float maxDistance)
{
	return distance > maxDistance;
}

/// Whether a placed sound that plays and is attached to a thing plays on this turn. The game stops it when the camera
/// is at or beyond the maximum distance of its voice.
[[nodiscard]] constexpr bool IsWithinPlayRange(float distance, float voiceMaxDistance)
{
	return distance < voiceMaxDistance;
}

} // namespace openblack::audio
