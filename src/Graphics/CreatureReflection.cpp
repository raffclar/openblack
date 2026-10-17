/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureReflection.h"

#include "Creature/CreatureFizzLook.h"

namespace openblack::graphics::creature_reflection
{

namespace
{
/// One byte of a colour, from 0 to 1
float Channel(uint32_t colour, uint32_t shift) noexcept
{
	constexpr float k_ByteMax = 255.0f;
	return static_cast<float>((colour >> shift) & 0xFFu) / k_ByteMax;
}

glm::vec3 Rgb(uint32_t colour) noexcept
{
	return {Channel(colour, 16), Channel(colour, 8), Channel(colour, 0)};
}
} // namespace

glm::vec3 Colour() noexcept
{
	return Rgb(k_Colour);
}

glm::vec3 Added() noexcept
{
	return Rgb(k_Added);
}

std::optional<glm::ivec2> BlockUnder(glm::vec3 position) noexcept
{
	const auto x = static_cast<int32_t>(position.x * k_PerCell);
	const auto z = static_cast<int32_t>(position.z * k_PerCell);
	if (x < 0 || x >= k_MapCells || z < 0 || z >= k_MapCells)
	{
		return std::nullopt;
	}
	return glm::ivec2(x >> k_BlockCellsShift, z >> k_BlockCellsShift);
}

bool Shown(const Situation& situation) noexcept
{
	// The block must be drawn this frame and near, then the body low and not yet fizzed far out of sight
	return situation.primaryCreature && situation.onBlock && situation.blockInView && situation.blockDistance <= k_Range &&
	       situation.height < k_HighestStand && creature_fizz_look::ReflectionShown(situation.fizz);
}

} // namespace openblack::graphics::creature_reflection
