/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

#include "3D/DayNightClock.h"

namespace openblack::graphics
{

/// Where the god hand's shadow is cast from. The shadow itself is one of the moving shadows laid on the land (see
/// GroundShadow): the hand's silhouette counts half as many coverage samples as other bodies, so it darkens by at most
/// 4 of 15 levels, while what it holds counts them all, and it falls on the land and the objects on it, fading out as
/// the camera pulls away from the ground beneath the hand. Unlike the game, whose hand is lit from just above it, the
/// shadow here is cast by the sun as it crosses the sky, onto the ground beneath the hand, and fades out at night.
struct HandShadow
{
	/// Direction sunlight travels in at a time of day in hours. The sun rises and sets with the sky's dusk and climbs
	/// highest at midday, coming from the same side of the island as Black & White's fixed light.
	[[nodiscard]] static glm::vec3 SunlightDirection(float hours, const DayNightTimes& times);
	/// 1 in full day, 0 at night, ramping through dawn and dusk
	[[nodiscard]] static float Daylight(float hours, const DayNightTimes& times);
	/// The point the hand is lit from: far up the sunlight from the ground beneath it, as far as the game's own sun
	[[nodiscard]] static glm::vec3 LightPoint(const glm::vec3& groundBelow, float hours, const DayNightTimes& times);
};

} // namespace openblack::graphics
