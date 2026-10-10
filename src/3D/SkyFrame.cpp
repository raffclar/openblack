/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkyFrame.h"

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
	if (inputs.overcast.has_value())
	{
		dome.overcast = *inputs.overcast;
	}

	sun.placement = graphics::sun::Place(inputs.scriptHour);
	sun.strength =
	    sun.placement.has_value() ? sky_dome::ThroughOvercast(sun.placement->alpha, dome.overcast, inputs.fog) : 0.0f;

	moon.placement = graphics::moon::Place(inputs.scriptHour);
	moon.colour = Colour(inputs.moonColour);
	moon.strength =
	    moon.placement.has_value() ? sky_dome::ThroughOvercast(moon.placement->alpha, dome.overcast, inputs.fog) : 0.0f;
	if (moon.strength <= 0.0f)
	{
		return;
	}
	// The date is kept between reads, and read again once two seconds have gone by (or before it was ever read)
	if (static_cast<int32_t>(inputs.ticks - moon.dateReadAt) > k_MoonDateReadInterval)
	{
		moon.dateReadAt = inputs.ticks;
		moon.date = 0;
	}
	if (moon.date == 0)
	{
		moon.date = inputs.unixTime;
	}
	moon.phase = graphics::moon::Phase(MoonDate(moon));
}

int64_t MoonDate(const ecs::components::Moon& moon)
{
	return moon.dateOverride.value_or(moon.date);
}

void AdvanceDome(ecs::components::SkyDome& dome, float skyType, bool drawn)
{
	dome.frameRows = drawn ? dome.follow.Advance(skyType) : sky_dome::FrameRows {};
}

} // namespace openblack::sky_frame
