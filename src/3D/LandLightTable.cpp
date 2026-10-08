/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandLightTable.h"

#include <algorithm>
#include <stdexcept>

#include "3D/SkyType.h"

namespace openblack
{
namespace
{
constexpr size_t k_PaletteSide = LandLightPalette::k_Side;
constexpr uint32_t k_DarkLevels = 48;
/// The ramps divide by this rather than 255
constexpr uint32_t k_RampDivisor = 200;

// The haze's distances as their inverses, exactly as the game's floats: 400 and 900 from the camera, drawn in by dusk
// to 100 and 800
constexpr float k_NearInverse = 0x1.47AE14p-9f;     // 0.0025
constexpr float k_NearInverseDusk = 0x1.EB851Ep-8f; // 0.0075
constexpr float k_FarInverse = 0x1.234568p-10f;     // 1 / 900
constexpr float k_FarInverseDusk = 0x1.234560p-13f; // 0.00013888883
// Under a full overcast the haze closes in to 15 and 350
constexpr float k_NearInverseStorm = 0x1.111112p-4f; // 1 / 15
constexpr float k_FarInverseStorm = 0x1.767DCEp-9f;  // 1 / 350
/// How far a full overcast darkens the land's colour
constexpr float k_OvercastDarkening = 96.0f;

enum Row : size_t
{
	k_Good = 0,
	k_Neutral = 1,
	k_Evil = 2,
	k_Dark = 3,
	k_Moon = 5,
	k_Warm = 6,
	k_RowCount = 8,
};

/// Each channel of a towards b by t of 256, in whole steps; the alpha of b
uint32_t Lerp(uint32_t a, uint32_t b, uint32_t t)
{
	const uint32_t r = (((((b & 0xFF0000u) - (a & 0xFF0000u)) * t) >> 8) + (a & 0xFFFF0000u)) & 0xFF0000u;
	const uint32_t g = (((((b & 0xFF00u) - (a & 0xFF00u)) * t) >> 8) + (a & 0xFFFFFF00u)) & 0xFF00u;
	const uint32_t bl = (((((b & 0xFFu) - (a & 0xFFu)) * t) >> 8) + a) & 0xFFu;
	return r | g | bl | (b & 0xFF000000u);
}

/// Each channel (a (255 - t) + b t) / 200, at most 255; the alpha of a. The game's light boost setting, which lowers
/// the divisor, is left at its default
uint32_t Ramp(uint32_t a, uint32_t b, uint32_t t)
{
	uint32_t result = a & 0xFF000000u;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		const uint32_t value = (((a >> shift) & 0xFFu) * (255 - t) + ((b >> shift) & 0xFFu) * t) / k_RampDivisor;
		result |= std::min(255u, value) << shift;
	}
	return result;
}
} // namespace

LandLightPalette::LandLightPalette(std::span<const uint8_t> bytes)
{
	if (bytes.size() != k_Side * k_Side * 4)
	{
		throw std::runtime_error("The land's light palette isn't 32 by 32 colours");
	}
	_colours.resize(k_Side * k_Side);
	for (size_t i = 0; i < _colours.size(); ++i)
	{
		// Red, green, blue and alpha bytes into 0xAARRGGBB
		const auto texel = bytes.subspan(i * 4, 4);
		_colours.at(i) = static_cast<uint32_t>(texel[0]) << 16 | static_cast<uint32_t>(texel[1]) << 8 |
		                 static_cast<uint32_t>(texel[2]) | static_cast<uint32_t>(texel[3]) << 24;
	}
}

void LandLightTable::Build(const LandLightPalette& palette, float skyType, float alignment, float overcast,
                           uint8_t flash) noexcept
{
	// The palette's columns: the time of day from midnight to noon, (2 - T) * 15 (sky_type::LightColumn), and the
	// alignment from good to evil
	const float timeColumn = sky_type::LightColumn(skyType);
	const float x = std::clamp(1.0f - alignment, 0.0f, 2.0f);
	const float alignmentColumn = x * 15.0f;

	std::array<uint32_t, k_RowCount> colours {};
	for (size_t row = 0; row < colours.size(); ++row)
	{
		const float column = row <= k_Evil ? timeColumn : alignmentColumn;
		const auto index = std::min(static_cast<size_t>(column), k_PaletteSide - 2);
		const auto t = static_cast<uint32_t>((column - static_cast<float>(index)) * 256.0f);
		colours[row] = Lerp(palette.At(row, index), palette.At(row, index + 1), t);
	}

	_moonColour = colours[k_Moon];
	_warmColour = colours[k_Warm];

	// The land's colour, from good through neutral to evil
	const auto k = static_cast<int>(x * 255.0f);
	uint32_t base = x < 1.0f ? Lerp(colours[k_Good], colours[k_Neutral], static_cast<uint32_t>(k))
	                         // At an alignment of exactly 0 the weight is -1, which wraps round as an unsigned
	                         // number, as the game's does
	                         : Lerp(colours[k_Neutral], colours[k_Evil], static_cast<uint32_t>(k - 256));
	// No channel brighter than trunc(255 - 96 * overcast), worked out in floats and not clamped (the haze below clamps
	// the overcast to 1 afterwards)
	const auto limit = static_cast<int32_t>(255.0f - overcast * k_OvercastDarkening);
	uint32_t capped = base & 0xFF000000u;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		const auto channel = static_cast<int32_t>((base >> shift) & 0xFFu);
		capped |= static_cast<uint32_t>(std::clamp(std::min(channel, limit), 0, 255)) << shift;
	}
	base = capped;
	_landColour = base;

	// The haze: a third of the land's colour, k by its brightness, and its distances drawn in at dusk; then the storm
	// and the lightning
	{
		const uint32_t r = (base >> 16) & 0xFFu;
		const uint32_t g = (base >> 8) & 0xFFu;
		const uint32_t b = base & 0xFFu;
		_haze.k = static_cast<float>(std::min(255u, (r + 4 * g + 3 * b) / 8 + 8));
		_haze.colour = glm::vec3(static_cast<float>(r / 3), static_cast<float>(g / 3), static_cast<float>(b / 3));
		// sky_type::HazeFactor: 0 by day and at night, 1 at dusk
		const float v2 = sky_type::HazeFactor(skyType);
		float nearInverse = k_NearInverse;
		float farInverse = k_FarInverse;
		if (v2 > 0.0f)
		{
			nearInverse = v2 * k_NearInverseDusk + k_NearInverse;
			farInverse = v2 * k_FarInverseDusk + k_FarInverse;
		}
		if (overcast > 0.0f)
		{
			// An overcast takes the haze towards a storm's: dark, thick and close, by the overcast clamped to 1
			const float w = std::min(overcast, 1.0f);
			const glm::vec3 storm(static_cast<float>((r >> 3) + 32), static_cast<float>((g >> 3) + 32),
			                      static_cast<float>((b >> 3) + 32));
			_haze.colour += (storm - _haze.colour) * w;
			const float k = _haze.k;
			_haze.k = k + static_cast<float>(static_cast<int32_t>((48.0f - k) * w)); // truncated
			nearInverse += (k_NearInverseStorm - nearInverse) * w;
			farInverse += (k_FarInverseStorm - farInverse) * w;
		}
		if (flash != 0)
		{
			// A flash of lightning takes the haze towards white; k in whole steps of 256
			const float f = static_cast<float>(flash);
			_haze.colour += (glm::vec3(255.0f) - _haze.colour) * f * 0.00390625f;
			const auto k = static_cast<int32_t>(_haze.k);
			_haze.k = static_cast<float>(k + ((255 - k) * static_cast<int32_t>(flash)) / 256);
		}
		_haze.nearDistance = 1.0f / nearInverse;
		_haze.farDistance = 1.0f / farInverse;
	}

	// The darkest levels reach the land's colour at a level by its green, then go on to the warm colour
	const uint32_t landLevel = (((base >> 8) & 0xFFu) * k_DarkLevels) >> 8;
	for (uint32_t i = 0; i < landLevel; ++i)
	{
		_table[i] = Ramp(colours[k_Dark], base, (i * 256) / landLevel);
	}
	for (uint32_t i = landLevel; i < k_DarkLevels; ++i)
	{
		_table[i] = Ramp(base, colours[k_Warm], ((i - landLevel) * 256) / (k_DarkLevels - landLevel));
	}
	for (uint32_t i = k_DarkLevels; i < k_Size; ++i)
	{
		_table[i] = Ramp(colours[k_Dark], base, i);
	}
	// A flash of lightning takes every light towards white: c + (((0xFF - c) * flash) >> 8), alpha 0xFF
	if (flash != 0)
	{
		for (auto& light : _table)
		{
			light = Lerp(light, 0xFFFFFFFFu, flash) | 0xFF000000u;
		}
	}

	for (size_t i = 0; i < k_Size; ++i)
	{
		const uint32_t c = _table[i];
		_texels[i] = ((c >> 16) & 0xFFu) | (c & 0xFF00u) | ((c & 0xFFu) << 16) | 0xFF000000u;
	}
}

glm::vec3 LandLightTable::ToColour(uint32_t colour) noexcept
{
	return glm::vec3((colour >> 16) & 0xFFu, (colour >> 8) & 0xFFu, colour & 0xFFu) / 255.0f;
}

glm::vec3 LandLightTable::GetColour(size_t index) const noexcept
{
	return ToColour(_table.at(index));
}

LandLightTable LandLightTable::Unset() noexcept
{
	LandLightTable table;
	table._table.fill(0xFFFFFFFFu);
	return table;
}

} // namespace openblack
