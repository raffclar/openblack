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
#include <initializer_list>
#include <optional>

/// The colours that the draw of an object multiplies its light by and adds after its texture: the colours of poison on
/// a pot, and how an instance of an object carries a colour added together with the flow of its grain
namespace openblack::graphics::object_tint
{

/// The colour that poison multiplies the light of a poisoned pot by, 0xRRGGBB
inline constexpr uint32_t k_PoisonTint = 0xE8FFDDu;
/// The colour that poison adds to a poisoned pot, 0xRRGGBB
inline constexpr uint32_t k_PoisonAdded = 0x001000u;

/// A colour that multiplies the light of an object and a colour added after its texture, each 0xRRGGBB
struct Colours
{
	uint32_t tint;
	uint32_t added;
};

/// The colours of a pot or a pile, on the ground and in the hand. Poison tints a poisoned pot that has no fire on it. A
/// fire on a pot takes the place of the poison: the pot then takes the colours of the fire, as each other object does.
[[nodiscard]] constexpr std::optional<Colours> PotColours(bool poisoned, bool onFire) noexcept
{
	if (!poisoned || onFire)
	{
		return std::nullopt;
	}
	return Colours {.tint = k_PoisonTint, .added = k_PoisonAdded};
}

/// Two added colours together, each channel at most 255
[[nodiscard]] constexpr uint32_t SumAdded(uint32_t first, uint32_t second) noexcept
{
	uint32_t sum = 0;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		sum |= std::min(((first >> shift) & 0xFFu) + ((second >> shift) & 0xFFu), 0xFFu) << shift;
	}
	return sum;
}

/// How far the grain of a pile can flow down it, as a share of its texture
inline constexpr float k_MostGrainFlow = 0.25f;

/// The value that an instance carries for a colour added after its texture (0xRRGGBB) and the flow of its grain: minus
/// the colour, plus the flow as a fraction. The whole part of the value is always the colour. The flow keeps as many
/// steps as the value can hold next to the colour.
[[nodiscard]] constexpr float PackAdded(uint32_t added, float grainFlow) noexcept
{
	return -(static_cast<float>(added) + std::clamp(grainFlow, 0.0f, k_MostGrainFlow));
}

} // namespace openblack::graphics::object_tint
