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

#include <algorithm>
#include <array>

namespace openblack::graphics
{

/// The original's graphics detail levels (registry "detailidx", default 4; level 5 is "custom", where every key is
/// read from the registry and the table's values are the custom defaults).
struct DetailLevel
{
	float waterTiling;     ///< sea texture period = 2000 - 1800 * WaterTiling (0: a nearly static quad)
	bool landReflection;   ///< "LandRef": the land mirrored under the sea
	bool fog;              ///< "Fog": software distance haze
	bool clouds;           ///< "Clouds" and "CloudShadows"
	bool useHighTexture;   ///< 256 px landscape textures (else 128)
	uint8_t rainSplash;    ///< "RainSplash"
	bool shadowsOnObjects; ///< "ShadowsOnObjects" (startup only): the hand's dynamic shadow also falls on objects
	/// Per level, not read from the registry at level 5: the sky dome without the day / dusk / night blend, 128 rows.
	/// Not ported: openblack's dome always blends (sky_type::DomeBlend)
	bool skyNoBlend;

	[[nodiscard]] float SeaPeriod() const { return 2000.0f - 1800.0f * waterTiling; }
};

inline constexpr std::array<DetailLevel, 7> k_DetailLevels = {{
    {0.0f, false, false, false, false, 0, false, true},
    {0.2f, false, false, false, false, 0, false, true},
    {0.4f, false, false, false, false, 3, false, false},
    {0.6f, true, true, true, false, 5, true, false},
    {0.8f, true, true, true, true, 8, true, false},
    {0.5f, true, true, true, true, 8, true, false},
    {1.0f, true, true, true, true, 8, true, false},
}};

inline constexpr uint8_t k_DefaultDetailLevel = 4;

inline const DetailLevel& GetDetailLevel(uint8_t level)
{
	return k_DetailLevels.at(std::min<size_t>(level, k_DetailLevels.size() - 1));
}

} // namespace openblack::graphics
