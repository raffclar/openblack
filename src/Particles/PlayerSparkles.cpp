/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerSparkles.h"

#include <array>

namespace openblack::player_sparkles
{

namespace
{
/// A channel at half way to a value
constexpr uint32_t Toward(uint32_t channel, uint32_t value)
{
	return (channel + value) >> 1u;
}

/// Three quarters of a channel
constexpr uint32_t Darker(uint32_t channel)
{
	return (channel * 3u) >> 2u;
}
} // namespace

uint32_t Colour(uint32_t playerColour, uint32_t tint)
{
	std::array<uint32_t, 3> channels {
	    ((playerColour >> 16u) & 0xFFu) >> 1u,
	    ((playerColour >> 8u) & 0xFFu) >> 1u,
	    (playerColour & 0xFFu) >> 1u,
	};
	auto& [red, green, blue] = channels;
	constexpr uint32_t k_Full = 0xFFu;
	constexpr uint32_t k_Pale = 0x90u;
	switch (tint)
	{
	case 0:
		red = Toward(red, k_Full);
		green = Toward(green, k_Pale);
		blue = Toward(blue, k_Pale);
		break;
	case 1:
		red = Toward(red, k_Full);
		green = Toward(green, k_Full);
		blue = Toward(blue, k_Pale);
		break;
	case 2:
		red = Toward(red, k_Pale);
		green = Toward(green, k_Full);
		blue = Toward(blue, k_Pale);
		break;
	case 3:
		red = Darker(red);
		green = Darker(green);
		blue = Darker(blue);
		break;
	default:
		break;
	}
	return (k_Alpha << 24u) | (red << 16u) | (green << 8u) | blue;
}

liquid_particles::Kind KindOf(uint32_t roll)
{
	return roll == 0 ? liquid_particles::Kind::MoteReversed : liquid_particles::Kind::Mote;
}

} // namespace openblack::player_sparkles
