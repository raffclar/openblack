/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Lighting.h"

#include <cmath>

#include <algorithm>

using namespace openblack::landgen;
using namespace openblack::landgen::lighting;

namespace
{
constexpr float k_MetresPerAltitude = 0.67f;
constexpr float k_MetresPerCorner = 10.0f;
constexpr uint8_t k_SeaLevelAltitude = 3;

/// The drawn height of a corner in metres
float Metres(const Grid<uint8_t>& altitudes, int x, int z)
{
	x = std::clamp(x, 0, altitudes.side - 1);
	z = std::clamp(z, 0, altitudes.side - 1);
	const auto altitude = altitudes.At(x, z);
	return altitude <= k_SeaLevelAltitude ? 0.0f : static_cast<float>(altitude) * k_MetresPerAltitude;
}

float MetresBetween(const Grid<uint8_t>& altitudes, float x, float z)
{
	const float fx = std::floor(x);
	const float fz = std::floor(z);
	const auto ix = static_cast<int>(fx);
	const auto iz = static_cast<int>(fz);
	const float tx = x - fx;
	const float tz = z - fz;
	const float h00 = Metres(altitudes, ix, iz);
	const float h10 = Metres(altitudes, ix + 1, iz);
	const float h01 = Metres(altitudes, ix, iz + 1);
	const float h11 = Metres(altitudes, ix + 1, iz + 1);
	return (h00 * (1.0f - tx) * (1.0f - tz)) + (h10 * tx * (1.0f - tz)) + (h01 * (1.0f - tx) * tz) + (h11 * tx * tz);
}
} // namespace

Grid<uint8_t> lighting::Luminosity(const Grid<uint8_t>& altitudes, const Settings& settings)
{
	Grid<uint8_t> luminosity(altitudes.side, 255);
	const float across = std::sqrt((k_Sun.x * k_Sun.x) + (k_Sun.z * k_Sun.z));
	const float towardsX = k_Sun.x / across;
	const float towardsZ = k_Sun.z / across;
	// How much the line to the sun climbs for each corner that it goes towards the sun
	const float climb = k_Sun.y / across * k_MetresPerCorner;

	for (int x = 0; x < altitudes.side; ++x)
	{
		for (int z = 0; z < altitudes.side; ++z)
		{
			const float slopeX = (Metres(altitudes, x + 1, z) - Metres(altitudes, x - 1, z)) / (2.0f * k_MetresPerCorner);
			const float slopeZ = (Metres(altitudes, x, z + 1) - Metres(altitudes, x, z - 1)) / (2.0f * k_MetresPerCorner);
			const float length = std::sqrt((slopeX * slopeX) + 1.0f + (slopeZ * slopeZ));
			const float facing = ((-slopeX * k_Sun.x) + k_Sun.y + (-slopeZ * k_Sun.z)) / length;
			float value = settings.base + (settings.gain * std::max(facing, 0.0f));

			const float own = Metres(altitudes, x, z);
			for (int step = 1; step <= settings.shadowReach; ++step)
			{
				const float distance = static_cast<float>(step);
				const float sx = static_cast<float>(x) + (towardsX * distance);
				const float sz = static_cast<float>(z) + (towardsZ * distance);
				if (sx < 0.0f || sz < 0.0f)
				{
					break;
				}
				if (MetresBetween(altitudes, sx, sz) > own + (climb * distance) + 0.5f)
				{
					value *= 0.5f;
					break;
				}
			}
			const float rounded = std::floor(value + 0.5f);
			luminosity.At(x, z) = static_cast<uint8_t>(std::clamp(rounded, static_cast<float>(settings.darkest), 255.0f));
		}
	}
	return luminosity;
}
