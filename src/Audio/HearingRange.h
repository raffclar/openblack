/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <glm/vec3.hpp>

namespace openblack::audio
{

/// The square of the distance from the camera to a point, summed in the game's order: the height, then up, then across
[[nodiscard]] constexpr float DistanceSquared(const glm::vec3& camera, const glm::vec3& point)
{
	const glm::vec3 way = camera - point;
	return (way.z * way.z + way.y * way.y) + way.x * way.x;
}

/// The furthest from the camera a placed sound effect may start. The bank's maximum distance for the sample counts
/// whether or not the bank's header applies it to the voice; a sample whose bank gives none (most of those that keep
/// the default distances) falls back on the play's own maximum distance, by default the voice's 9999.
[[nodiscard]] constexpr float SoundEffectStartRange(float bankMaxDistance, float playMaxDistance)
{
	return bankMaxDistance != 0.0f ? bankMaxDistance : playMaxDistance;
}

/// An animation effect (a knock, a crash, a footstep) never starts further than this from the camera, in metres
inline constexpr float k_AnimEffectHearingLimit = 800.0f;

/// How an animation effect that does not start a sample ends the samples of its keys that its owner plays
enum class AnimEffectEnd : uint8_t
{
	/// The sample stops at once
	Stop,
	/// A looping sample stops at the end of the pass that it plays
	ReleaseLoop,
};

/// How the thing that plays an animation effect measures its distance from the camera
enum class AnimEffectMeasure : uint8_t
{
	/// The straight distance. The squares add in this order: z, then y, then x. Most things use this
	Straight,
	/// The straight distance. The squares add in this order: x, then z, then y. A tree that bends and a kick use this
	AcrossFirst,
	/// No distance: the effect counts as at the camera. A creature's spell sound uses this
	AtCamera,
};

/// The distance from the camera to a point that plays an animation effect, as the measure finds it
[[nodiscard]] inline float AnimEffectDistance(const glm::vec3& camera, const glm::vec3& point, AnimEffectMeasure measure)
{
	const glm::vec3 way = camera - point;
	switch (measure)
	{
	case AnimEffectMeasure::Straight:
		return std::sqrt(DistanceSquared(camera, point));
	case AnimEffectMeasure::AcrossFirst:
		return std::sqrt((way.x * way.x + way.z * way.z) + way.y * way.y);
	case AnimEffectMeasure::AtCamera:
		break;
	}
	return 0.0f;
}

/// Whether an animation effect at this distance from the camera may start. The distance must not be more than the
/// hearing limit, and not more than the bank's maximum distance for the sample. There is no fallback, so a sample
/// whose bank gives no distance starts only at the camera. A distance that is not a number does not stop it.
[[nodiscard]] constexpr bool IsAnimEffectWithinReach(float distance, float bankMaxDistance)
{
	return !(distance > k_AnimEffectHearingLimit) && !(distance > bankMaxDistance);
}

/// Whether a placed sound at this square of the distance from the camera may start. Only a square of the distance
/// that is more than the square of the range stops it, so a sound right at its range starts.
[[nodiscard]] constexpr bool IsWithinStartRange(float distanceSquared, float range)
{
	return !(distanceSquared > range * range);
}

/// How far a sound travels in the turns since it started, in metres: at 347 metres a second
inline constexpr float k_SpeedOfSound = 347.0f;
[[nodiscard]] constexpr float SoundTravelled(uint32_t turns, uint32_t millisecondsPerTurn)
{
	const float seconds = static_cast<float>(turns) * static_cast<float>(millisecondsPerTurn) * 0.001f;
	return k_SpeedOfSound * seconds;
}

/// Whether a sound that waits to travel to the camera has arrived: it has gone at least as far as the camera is
[[nodiscard]] constexpr bool SoundArrived(float travelled, float distanceSquared)
{
	return !(travelled * travelled < distanceSquared);
}

/// Whether a sound that arrived at the camera starts: only nearer than its range, so a range of nothing never starts
[[nodiscard]] constexpr bool ArrivedSoundStarts(float distanceSquared, float range)
{
	return distanceSquared < range * range;
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
