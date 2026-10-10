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

#include <optional>

#include <glm/vec3.hpp>

#include "ECS/Components/Sky.h"

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
	/// The clouds over the camera, 0 to 1. None without the land's light palette: then the sun and moon go on showing
	/// through the overcast they last did.
	std::optional<float> overcast;
	/// The fog setting: only with it does an overcast dim the sun and moon
	bool fog;
	/// The moon's colour for the time of day and alignment, 0xRRGGBB
	uint32_t moonColour;
};

/// A 0xRRGGBB colour as 0 to 1
[[nodiscard]] glm::vec3 Colour(uint32_t rgb);

/// The moon at an hour of script time and a date: its place and phase, its colour and strength not yet worked out
[[nodiscard]] ecs::components::Moon MoonAt(float scriptHour, int64_t unixTime);

/// Places the sun and moon for the frame and works out how strongly they show
void Update(const Inputs& inputs, ecs::components::SkyDome& dome, ecs::components::Sun& sun, ecs::components::Moon& moon);

/// The rows of the dome to blend again this frame, for the frame's sky type: the blend follows the sky only while
/// the sky is drawn
void AdvanceDome(ecs::components::SkyDome& dome, float skyType, bool drawn);

} // namespace openblack::sky_frame
