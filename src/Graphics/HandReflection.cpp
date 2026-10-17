/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandReflection.h"

namespace openblack::graphics::hand_reflection
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

} // namespace openblack::graphics::hand_reflection
