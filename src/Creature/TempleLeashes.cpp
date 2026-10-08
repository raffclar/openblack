/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleLeashes.h"

#include <cmath>

using namespace openblack;
using namespace openblack::temple_leashes;

namespace
{
/// What is left over a whole number of rounds, the whole rounds cut off towards zero as the game does
float Wrap(float value, float round)
{
	return value - (std::trunc(value * (1.0f / round)) * round);
}
} // namespace

Look temple_leashes::Start(const std::array<int32_t, 4>& draws)
{
	// Each is a draw scaled to its range
	const auto draw = [](float range, int32_t value) { return range * static_cast<float>(value) * k_DrawShare; };
	return {
	    .scroll = draw(1.0f, draws[0]),
	    .pitch = draw(k_FullTurn, draws[1]),
	    .roll = draw(k_FullTurn, draws[2]),
	    .glow = draw(k_GlowPictures, draws[3]),
	};
}

Look temple_leashes::Advance(const Look& look, float seconds)
{
	const auto pitch = look.pitch + (k_PitchRate * seconds);
	const auto roll = look.roll + (k_RollRate * seconds);
	const auto scroll = look.scroll + (k_ScrollRate * seconds);
	const auto glow = look.glow + (k_GlowRate * seconds);
	return {
	    .scroll = scroll - std::trunc(scroll),
	    .pitch = Wrap(pitch, k_FullTurn),
	    .roll = Wrap(roll, k_FullTurn),
	    .glow = Wrap(glow, k_GlowPictures),
	};
}

glm::mat3 temple_leashes::Turn(const Look& look)
{
	const auto c2 = std::cos(look.pitch);
	const auto s2 = std::sin(look.pitch);
	const auto c3 = std::cos(look.roll);
	const auto s3 = std::sin(look.roll);
	// Each column is one of the leash's own axes in the world
	return {
	    glm::vec3(c3, -s3 * c2, s3 * s2),
	    glm::vec3(s3, c3 * c2, -c3 * s2),
	    glm::vec3(0.0f, s2, c2),
	};
}

uint8_t temple_leashes::GlowPicture(const Look& look)
{
	constexpr uint32_t k_PictureMask = 0x3F;
	return static_cast<uint8_t>(static_cast<uint32_t>(std::trunc(look.glow)) & k_PictureMask);
}

float temple_leashes::Band(LeashType type)
{
	// Each band is an eighth of the texture: the rope second, the rainbow third, the spiked blade fourth
	constexpr float k_BandHeight = 0.125f;
	switch (type)
	{
	case LeashType::Evil:
		return 3.0f * k_BandHeight;
	case LeashType::Rope:
		return k_BandHeight;
	case LeashType::Good:
		return 2.0f * k_BandHeight;
	case LeashType::None:
		break;
	}
	return 0.0f;
}

uint8_t temple_leashes::GlowAlpha(uint32_t light)
{
	const auto sum = ((light >> 16u) & 0xFFu) + ((light >> 8u) & 0xFFu) + (light & 0xFFu);
	return static_cast<uint8_t>(std::trunc(static_cast<float>(sum) * 0.16666667f));
}

Glow temple_leashes::GlowOf(bool picked, uint8_t alpha)
{
	const auto a = static_cast<float>(alpha) / 255.0f;
	if (picked)
	{
		constexpr glm::vec3 k_Orange {0xC1 / 255.0f, 0x81 / 255.0f, 0x19 / 255.0f};
		return {.tint = glm::vec4(k_Orange, a), .additive = true};
	}
	return {.tint = glm::vec4(1.0f, 1.0f, 1.0f, a), .additive = false};
}

uint32_t temple_leashes::ToolTipOf(LeashType type)
{
	switch (type)
	{
	case LeashType::Evil:
		return k_AggressionTip;
	case LeashType::Good:
		return k_CompassionTip;
	case LeashType::Rope:
	case LeashType::None:
		break;
	}
	return k_LearningTip;
}
