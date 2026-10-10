/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkyFrame.h"

#include "3D/LandLightTable.h"

namespace openblack::sky_frame
{

glm::vec3 Colour(uint32_t rgb)
{
	return glm::vec3(static_cast<float>((rgb >> 16u) & 0xFFu), static_cast<float>((rgb >> 8u) & 0xFFu),
	                 static_cast<float>(rgb & 0xFFu)) /
	       255.0f;
}

ecs::components::Moon MoonAt(float scriptHour, int64_t unixTime)
{
	return {
	    .phase = graphics::moon::Phase(unixTime),
	    .placement = graphics::moon::Place(scriptHour),
	};
}

void Update(const Inputs& inputs, ecs::components::SkyDome& dome, ecs::components::Sun& sun, ecs::components::Moon& moon)
{
	dome.landLight = inputs.landLight;
	if (inputs.palette != nullptr)
	{
		dome.overcast = inputs.landLight.overcast;
	}

	sun.placement = graphics::sun::Place(inputs.scriptHour);
	sun.strength =
	    sun.placement.has_value() ? sky_dome::ThroughOvercast(sun.placement->alpha, dome.overcast, inputs.fog) : 0.0f;

	const auto placed = MoonAt(inputs.scriptHour, inputs.unixTime);
	moon.phase = placed.phase;
	moon.placement = placed.placement;
	moon.colour = Colour(inputs.palette != nullptr ? LandLightTable::GetMoonColour(*inputs.palette, inputs.landLight.skyType,
	                                                                               inputs.landLight.alignment)
	                                               : 0xFFFFFFu);
	moon.strength =
	    moon.placement.has_value() ? sky_dome::ThroughOvercast(moon.placement->alpha, dome.overcast, inputs.fog) : 0.0f;
}

void AdvanceDome(ecs::components::SkyDome& dome, float skyType, bool drawn)
{
	dome.frameRows = drawn ? dome.follow.Advance(skyType) : sky_dome::FrameRows {};
}

} // namespace openblack::sky_frame
