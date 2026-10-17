/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandReflection.h"

#include <glm/geometric.hpp>

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

HeldLook HeldLookOf(const HeldThing& thing) noexcept
{
	if (thing.tree)
	{
		// The haze is at the tree's place on the map, at the height of the sea
		const glm::vec3 hazeAt {thing.takenAt.x, 0.0f, thing.takenAt.z};
		return {
		    .point = thing.takenAt,
		    .viewDepth = glm::dot(hazeAt - thing.eye, thing.forward),
		    .treeGrey = thing.onFire ? thing.treeGrey : 0u,
		};
	}
	HeldLook look {.point = thing.drawnAt, .viewDepth = glm::dot(thing.drawnAt - thing.eye, thing.forward)};
	if (thing.onFire)
	{
		const uint32_t grey = thing.charredGrey;
		look.tint = (grey << 16u) | (grey << 8u) | grey;
		look.added = thing.glow;
	}
	else if (thing.poisonedPot)
	{
		look.tint = k_PoisonTint;
		look.added = k_PoisonAdded;
	}
	return look;
}

} // namespace openblack::graphics::hand_reflection
