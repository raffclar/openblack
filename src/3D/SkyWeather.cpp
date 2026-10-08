/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkyWeather.h"

#include "Camera/Camera.h"
#include "ECS/Weather/LightningFlash.h"
#include "Locator.h"

namespace openblack::sky_weather
{

uint8_t LightningFlash()
{
	// the storm flash at the camera's position
	if (!Locator::camera::has_value())
	{
		return 0;
	}
	return weather::LightningFlashAtCamera(Locator::camera::value().GetOrigin());
}

} // namespace openblack::sky_weather
