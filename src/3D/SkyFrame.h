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

#include <glm/vec3.hpp>

#include "ECS/Components/Sky.h"

namespace openblack
{
class LandLightPalette;
}

/// The sky of a frame: the sun and moon placed for the hour, the moon's phase taken from the date, both dimmed by the
/// overcast, and the dome's blend following the sky
namespace openblack::sky_frame
{

/// What a frame of the sky is worked out from
struct Inputs
{
	/// The hour of script time, which places the sun and moon
	float scriptHour;
	/// The computer's date and time, in seconds since 1970, which gives the moon's phase
	int64_t unixTime;
	/// The machine's clock in milliseconds, which paces how often the date is read
	uint32_t ticks;
	/// What the land's light of the frame is built from: the sky's type, the alignment it shows, and the clouds and
	/// lightning over the camera
	LandLightInputs landLight;
	/// The land's light palette, which gives the moon its colour for the time of day and alignment. None before it is
	/// loaded: then the moon is white, and the sun and moon go on showing through the overcast they last did.
	const LandLightPalette* palette;
	/// The fog setting: only with it does an overcast dim the sun and moon
	bool fog;
};

/// A 0xRRGGBB colour as 0 to 1
[[nodiscard]] glm::vec3 Colour(uint32_t rgb);

/// The moon at an hour of script time and a date: its place and phase, its colour and strength not yet worked out
[[nodiscard]] ecs::components::Moon MoonAt(float scriptHour, int64_t unixTime);

/// The date is read again for the moon's phase only once more than this has gone by on the machine's clock since it was
/// last read
inline constexpr int32_t k_MoonDateReadInterval = 2000;

/// The date the moon's phase is taken from, in seconds since 1970: the override, or the computer's as last read
[[nodiscard]] int64_t MoonDate(const ecs::components::Moon& moon);

/// Keeps the frame's land light on the dome, places the sun and moon for the frame, gives the moon its colour and works
/// out how strongly they show. Only a moon that shows reads the date
/// (at most once every two seconds) and takes up its phase.
void Update(const Inputs& inputs, ecs::components::SkyDome& dome, ecs::components::Sun& sun, ecs::components::Moon& moon);

/// The rows of the dome to blend again this frame, for the frame's sky type: the blend follows the sky only while
/// the sky is drawn
void AdvanceDome(ecs::components::SkyDome& dome, float skyType, bool drawn);

} // namespace openblack::sky_frame
