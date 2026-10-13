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

#include <array>
#include <functional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureLayers.h"

/// The rules for a creature fart. The creature squats as for a poo. On each fifth frame of the squat loop, a cloud of
/// dust moves out behind it and a smoke starts at its groin. If the creature looks evil, the villagers near it faint.
namespace openblack::creature_fart
{
/// The squat loop makes a cloud on each fifth frame that the game draws
constexpr uint32_t k_FramesPerBurst = 5;
/// The number of dust puffs in each cloud
constexpr size_t k_Puffs = 15;
/// The speed of the cloud behind the creature, and the width of its spread, for each unit of the shown size
constexpr float k_CloudSpeed = 15.0f;
/// The size of each puff for each unit of the shown size, and its transparent grey colour (0xAARRGGBB)
constexpr float k_PuffSize = 4.0f;
constexpr uint32_t k_PuffColour = 0x40808080;
/// If the evil to good value of the drawn body is less than this, the fart makes villagers faint
constexpr float k_StinksBelow = -0.5f;

/// Gives a random number from 0 to a limit, from the shared random numbers of the game
using FloatRandom = std::function<float(float)>;

/// Tells if the body plays the squat loop, for a poo or for a fart
[[nodiscard]] bool PlaysSquatLoop(const creature_layers::BodyAction& body);
/// Counts a frame of the squat loop. Returns true on the frame that makes a cloud or a lump. Then the count starts again
[[nodiscard]] bool CountFrame(uint32_t& frames);
/// What the squat makes on that frame: a cloud for a fart, else a lump of poo
enum class Burst : uint8_t
{
	Cloud,
	Lump,
};
[[nodiscard]] constexpr Burst BurstOf(bool farting)
{
	return farting ? Burst::Cloud : Burst::Lump;
}
/// Gives the velocity of each puff. Each puff moves straight back from the direction that the creature faces. A random
/// offset of up to half the spread changes each velocity: first on z, then up, then on x
[[nodiscard]] std::array<glm::vec3, k_Puffs> PuffVelocities(glm::vec2 ahead, float shownSize, const FloatRandom& random);
/// Tells if the fart makes villagers faint, from the evil to good value of the drawn body (-1 evil to 1 good)
[[nodiscard]] constexpr bool Stinks(float evilGood)
{
	return evilGood < k_StinksBelow;
}
} // namespace openblack::creature_fart
